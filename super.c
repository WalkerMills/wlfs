#include <asm/bug.h>
#include <linux/buffer_head.h>
#include <linux/compiler.h>
#include <linux/dcache.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/string.h>
#include <linux/time.h>

#include "super.h"
#include "util.h"

// Deallocate imap, segmap, and object caches within the superblock
static void wlfs_put_super (struct super_block *sb);
// Populate the in-memory superblock with data from disk & computed fields
static int wlfs_fill_super (struct super_block *sb, void *data, int silent);

static struct super_operations const wlfs_super_ops = {
    .put_super = wlfs_put_super,
};

struct dentry *wlfs_mount (struct file_system_type *type, int flags,
                           char const *dev, void *data) {
#ifndef NDEBUG
    printk(KERN_DEBUG "Mounting wlfs\n");
#endif
    struct dentry *const entry = mount_bdev(type, flags, dev, data, 
                                            wlfs_fill_super);
    if (unlikely(IS_ERR(entry))) {
        printk(KERN_ERR "Failed to mount wlfs\n");
    } else {
        printk(KERN_INFO "Successfully mounted wlfs\n");
    }

    return entry;
}

void wlfs_kill_sb (struct super_block *sb) {
#ifndef NDEBUG
    printk(KERN_DEBUG "Freeing superblock\n");
#endif
    kfree(sb->s_fs_info);
}

void wlfs_put_super (struct super_block *sb) {
#ifndef NDEBUG
    printk(KERN_DEBUG "Destroying superblock members\n");
#endif
    struct wlfs_super *wlfs_sb = (struct wlfs_super *) sb->s_fs_info;
    kmem_cache_destroy(wlfs_sb->imap_cache);
    kmem_cache_destroy(wlfs_sb->segmap_cache);
    kmem_cache_destroy(wlfs_sb->segment_cache);
}

int wlfs_fill_super (struct super_block *sb, void *data, int silent) {
#ifndef NDEBUG
    printk(KERN_DEBUG "Populating super block fields");
#endif
    int ret = -EPERM;

    // If the superblock is not aligned, something is very wrong
    BUG_ON(WLFS_OFFSET % sb->s_bdev->bd_block_size != 0);
    // Read the on-disk superblock into memory
    sector_t offset = WLFS_OFFSET / sb->s_bdev->bd_block_size;
    struct buffer_head *bh = __getblk(sb->s_bdev, offset, 
                                      sb->s_bdev->bd_block_size);
    BUG_ON(!bh);
    struct wlfs_super *wlfs_sb = 
        (struct wlfs_super *) kzalloc(sizeof(struct wlfs_super), GFP_NOFS);
    memcpy(&wlfs_sb->meta, bh->b_data, sizeof(struct wlfs_super_meta));
    brelse(bh);
    // Check that the data was read & copied correctly
    BUG_ON(wlfs_sb->meta.magic != WLFS_MAGIC);
    
    // TODO: read imap, segmap from disk
    wlfs_sb->imap_cache = kmem_cache_create(
        "wlfs_imap_block", wlfs_sb->meta.block_size, 0, 0, NULL);
    wlfs_sb->segmap_cache = kmem_cache_create(
        "wlfs_segments_block", wlfs_sb->meta.block_size, 0, 0, NULL);
    wlfs_sb->segment_cache = kmem_cache_create(
        "wlfs_segment", sizeof(struct segment), 0, 0, NULL);
    sb->s_fs_info = wlfs_sb;

    // Intialize root inode
    struct inode *root = new_inode(sb);
    root->i_ino = ROOT_INODE_INDEX;
    inode_init_owner(root, NULL, S_IFDIR);
    root->i_sb = sb;
    root->i_ctime = root->i_atime = root->i_mtime = CURRENT_TIME;
    sb->s_root = d_make_root(root);
    if (unlikely(!sb->s_root)) {
        printk(KERN_ERR "Error allocating root inode\n");
        ret = -ENOMEM;
        goto exit;
    }

    // Set remaining superblock fields
    sb->s_magic = wlfs_sb->meta.magic;
    sb->s_op = &wlfs_super_ops;
    if (unlikely(sb_set_blocksize(sb, wlfs_sb->meta.block_size)) == 0) {
        printk(KERN_ERR "Error setting block size to %u\n", 
               wlfs_sb->meta.block_size);
        ret = -EINVAL;
        goto exit;
    }
    sb->s_maxbytes = get_max_bytes(&wlfs_sb->meta);

    ret = 0;
exit:
    return ret;
}
