/*
 * Module initialization & cleanup handlers
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/printk.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Walker Mills");
MODULE_DESCRIPTION("Walker's log-based filesystem");

static int __init wlfs_init(void) {
    printk(KERN_INFO "Loaded wlfs\n");
    return 0;
}

static void __exit wlfs_exit(void) {
    printk(KERN_INFO "Unloaded wlfs\n");
}

module_init(wlfs_init);
module_exit(wlfs_exit);
