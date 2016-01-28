/*
 * Utility functions for wlfs
 */

#pragma once

#include <linux/types.h>

#include "disk.h"

// Number of bytes of data in each block (not including header)
__u16 get_block_bytes (struct wlfs_super_disk *sb);

// Number of entries (disk addresses) per block
__u16 get_imap_entries (struct wlfs_super_disk *sb);

// Number of inode map blocks
__u32 get_imap_blocks (struct wlfs_super_disk *sb);

// Number of bits in a segment usage bitmap
__u16 get_segmap_bits (struct wlfs_super_disk *sb);

// Number of entries (segment usage bitmaps) per block
__u16 get_segmap_entries (struct wlfs_super_disk *sb);

// Number of segmap blocks
__u32 get_segmap_blocks (struct wlfs_super_disk *sb);
