/*
 * Super block data structure & methods
 */

#pragma once

#include <linux/fs.h>
#include <linux/slab.h>

#include "wlfs.h"

// Intialize the superblock
struct dentry *wlfs_mount (struct file_system_type *type, int flags,
                           char const *dev, void *data);
// Free the superblock
void wlfs_kill_sb (struct super_block *sb);

struct wlfs_super {
    struct wlfs_super_meta meta;
    struct inode_map imap;
    struct segment_map segmap;
    struct kmem_cache *imap_cache;
    struct kmem_cache *segmap_cache;
};
