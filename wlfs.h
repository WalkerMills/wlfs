/*
 * WLFS data structures, compile-time constants
 */

#pragma once

#include <linux/types.h>

// Unique magic number for this filesystem
#define WLFS_MAGIC 0x5CA1AB1E
// Version number
#define WLFS_VERSION '0.0'
// Default block size: 4 KiB (assumes advanced format block device)
#define WLFS_BLOCK_SIZE (1 << 12)
// Default segment size: 2 MiB
#define SEGMENT_SIZE (1 << 20)

// Fixed constants
// Reserve 0th block for boot loader, just in case
#define SUPER_BLOCK_INDEX 1
// There are two checkpoint blocks
#define CHECKPOINT_BLOCK_INDEX (WLFS_SUPER_BLOCK_NUM + 1)
// First block of the log (location of first segment)
#define LOG_BLOCK_INDEX (WLFS_CHECKPOINT_BLOCK_NUM + 2)
// Root inode number
#define ROOT_INODE_INDEX 1
// Number of block pointers locally stored in an inode
#define NBLOCK_PTR (1 << 4)

// Default values for format-time adjustable constants
// Period (seconds) between write buffer flushes
#define BUFFER_PERIOD 30
// Period (seconds) between checkpoints
#define CHECKPOINT_PERIOD 45
// Level of block indirection
#define INDIRECTION 3
// Maximum number of inodes
#define MAX_INODES (1 << 18)
// Start cleaning when the # of clean segments drops below this value
#define MIN_CLEAN_SEGS (1 << 5)
// Stop cleaning when the # of clean segments rises above this value
#define TARGET_CLEAN_SEGS (1 << 7)

struct header {
    __u64 ino;
    __kernel_time_t wtime;
    __u8 version;
};

struct block {
    struct header h0;
    struct header h1;
    __u8 *data;
};

struct inode_map_block {
    __u32 offset;
    __u32 wtime;
    __u64 *locations;
    __u16 *versions;
};

struct inode_map {
    struct inode_map_block **blocks;
    __u32 nblocks;
    __u16 entries;
};

struct segment_map_block {
    __u32 offset;
    __u32 wtime;
    __u8 **usage_maps;
};

struct segment_map {
    struct segment_map_block **blocks;
    __u32 nblocks;
    __u16 entries;
};

struct wlfs_super_meta {
    __u16 block_size;
    __u32 max_inodes;
    __u32 segment_size;
    __u8 buffer_period;
    __u8 checkpoint_period;
    __u8 indirection;
    __u8 min_clean_segs;
    __u8 target_clean_segs;
};
