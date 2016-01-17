/*
 * Module initialization & cleanup handlers
 */

#include <asm/bug.h>
#include <linux/compiler.h>
#include <linux/dcache.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/types.h>

#include "wlfs.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Walker Mills");
MODULE_DESCRIPTION("Walker's log-based filesystem");

// 
static struct dentry *wlfs_mount (struct file_system_type *type, int flags,
                                  char const *dev, void *data);
static int wlfs_fill_super (struct super_block *sb, void *data, int silent);
static void wlfs_put_super (struct super_block *sb);

static struct file_system_type wlfs_type = {
    .owner = THIS_MODULE,
    .name = "wlfs",
    .mount = wlfs_mount,
    .kill_sb = kill_block_super,
    .fs_flags = FS_REQUIRES_DEV,
};

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

static __u64 max_bytes (void) {
    __u64 size = NBLOCK_PTR;

    if (unlikely(INDIRECTION < 1)) {
        return size;
    }

    __u64 block_entries = (WLFS_BLOCK_SIZE - 2 * sizeof(struct header)) / 8;
    size = size - INDIRECTION + block_entries;
    int i = 2;
    for (; i <= INDIRECTION; ++i) {
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
    size *= WLFS_BLOCK_SIZE;

    return size;
}

int wlfs_fill_super (struct super_block *sb, void *data, int silent) {
#ifndef NDEBUG
    printk(KERN_DEBUG "Populating super block fields");
#endif
    // struct inode *root = NULL;

    sb->s_magic = WLFS_MAGIC;
    sb->s_op = &wlfs_super_ops;
    if (sb_set_blocksize(sb, WLFS_BLOCK_SIZE) == 0) {
        printk(KERN_ERR "Error setting block size to %u\n", WLFS_BLOCK_SIZE);
        return -EINVAL;
    }
    sb->s_maxbytes = max_bytes();

    return 0;
}

void wlfs_put_super (struct super_block *sb) {
    printk(KERN_INFO "Super block destroyed\n");
}

static int __init wlfs_init (void) {
#ifndef NDEBUG
    printk(KERN_DEBUG "Loading wlfs\n");
#endif
    if (unlikely(register_filesystem(&wlfs_type))) {
        printk(KERN_ERR "Failed to register filesystem");
        return -1;
    }

    printk(KERN_INFO "Loaded wlfs\n");
    return 0;
}

static void __exit wlfs_exit (void) {
#ifndef NDEBUG
    printk(KERN_DEBUG "Unloading wlfs\n");
#endif
    if (unlikely(unregister_filesystem(&wlfs_type))) {
        printk(KERN_ERR "Failed to unregister filesystem");
    }

    printk(KERN_INFO "Unloaded wlfs\n");
}

module_init(wlfs_init);
module_exit(wlfs_exit);
