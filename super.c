#include <asm/bug.h>
#include <linux/buffer_head.h>
#include <linux/compiler.h>
#include <linux/dcache.h> // d_make_root
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>

#include "wlfs.h"
#include "super.h"

static struct wlfs_super_disk *wlfs_read_super (struct super_block *sb);
static int wlfs_fill_super (struct super_block *sb, void *data, int silent);

static struct super_operations const wlfs_super_ops = {
    .put_super = wlfs_put_super,
};

static struct wlfs_super_disk *wlfs_read_super (struct super_block *sb) {
    return NULL;
}

/*
 * Max file size = block size * (
 *      local block pointers per inode - 
 *      indirection +
 *      sum_{i=0}^{indirection} (entries per indirect block)^i)
 */
static __u64 max_bytes (struct wlfs_super_disk *sb_meta) {
    __u64 size = NBLOCK_PTR;

    if (unlikely(sb_meta->indirection < 1)) {
        return size;
    }

    __u64 block_entries = 
        (sb_meta->block_size - 2 * sizeof(struct header)) / 8;
    size = size - sb_meta->indirection + block_entries;
    int i = 2;
    for (; i <= sb_meta->indirection; ++i) {
        __u64 base = block_entries;
        __u8 exp = i;
        __u64 result = 1;
        while (exp)
        {
            if (exp & 1)
                result *= base;
            exp >>= 1;
            base *= base;
        }
    }
    size *= sb_meta->block_size;

    return size;
}

int wlfs_fill_super (struct super_block *sb, void *data, int silent) {
#ifndef NDEBUG
    printk(KERN_DEBUG "Populating super block fields");
#endif
    int ret = -EPERM;

    // Read the wlfs super block metadata from disk
    struct buffer_head *bh = sb_bread(sb, SUPER_BLOCK_INDEX);
    BUG_ON(!bh);
    struct wlfs_super_disk *sb_meta = (struct wlfs_super_disk *) bh->b_data;
    BUG_ON(sb_meta->magic != WLFS_MAGIC);

    sb->s_magic = sb_meta->magic;
    sb->s_op = &wlfs_super_ops;
    if (sb_set_blocksize(sb, sb_meta->block_size) == 0) {
        printk(KERN_ERR "Error setting block size to %u\n", 
               sb_meta->block_size);
        ret = -EINVAL;
        goto exit;
    }
    sb->s_maxbytes = max_bytes(sb_meta);

exit:
    brelse(bh);
    return ret;
}

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

void wlfs_put_super (struct super_block *sb) {
    printk(KERN_INFO "Super block destroyed\n");
}

