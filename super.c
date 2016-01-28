#include <asm/bug.h>
#include <linux/buffer_head.h>
#include <linux/compiler.h>
#include <linux/dcache.h> // d_make_root
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/string.h>

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
    printk(KERN_DEBUG "Freeing superblock");
#endif
    kfree(sb);
}

void wlfs_put_super (struct super_block *sb) {
    printk(KERN_INFO "Super block destroyed\n");
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
    struct buffer_head *bh = __bread(sb->s_bdev, offset, 
                                     sizeof(struct wlfs_super_meta));
    BUG_ON(!bh);
    struct wlfs_super *wlfs_sb = 
        (struct wlfs_super *) kzalloc(sizeof(struct wlfs_super), GFP_NOFS);
    memcpy(wlfs_sb, bh->b_data, sizeof(struct wlfs_super_meta));
    brelse(bh);
    // Check that the data was read & copied correctly
    BUG_ON(wlfs_sb->meta.magic != WLFS_MAGIC);

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

exit:
    return ret;
}
