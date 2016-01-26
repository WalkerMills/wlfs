/*
 * Module initialization & cleanup handlers
 */

#include <linux/compiler.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/printk.h>

#include "wlfs.h"
#include "super.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Walker Mills");
MODULE_DESCRIPTION("Walker's log-based filesystem");

static struct file_system_type wlfs_type = {
    .owner = THIS_MODULE,
    .name = "wlfs",
    .mount = wlfs_mount,
    .kill_sb = kill_block_super,
    .fs_flags = FS_REQUIRES_DEV,
};

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
