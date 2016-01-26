/*
 * Super block operations
 */

#pragma once

#include <linux/fs.h>

struct dentry *wlfs_mount (struct file_system_type *type, int flags,
                           char const *dev, void *data);
void wlfs_put_super (struct super_block *sb);

