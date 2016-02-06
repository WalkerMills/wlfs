/*
 * WLFS constants, default filesystem parameters, common data structures
 */

#pragma once

#include <linux/types.h>

// Fixed constants
// Unique magic number for this filesystem
#define WLFS_MAGIC 0x5CA1AB1EUL
// Version number
#define WLFS_VERSION '0.1'
// Start the filesystem at LBA 40 to avoid clobbering GPT & 4K align the data
#define WLFS_OFFSET 163840
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
// Default segment size: 2 MiB
#define SEGMENT_SIZE (1 << 20)
// Default block size: 4 KiB (assumes advanced format block device)
#define WLFS_BLOCK_SIZE (1 << 12)

struct header {
    __kernel_time_t wtime;
    // Incremented when the file is deleted/trunctated
    __u8 version;
};

// This is basically a block header; there's always one at the head of a
// block, and the remaining space is data
struct block {
    // Two headers, in case of mid-update crashes
    struct header h0;
    struct header h1;
    // Could be inode #, or map block #
    __u64 index;
};

struct inode_map {
    struct block **blocks;
    __u32 nblocks;
    __u16 entries;
};

struct segment {
    struct segment *prev;
    struct segment *next;
    struct block *block;
};

struct segment_map {
    // Doubly linked list of segment map blocks
    struct segment *blocks;
    // Number of segment map blocks
    __u32 nblocks;
    // Number of bitmaps per segment map block
    __u16 entries;
    // Bits per segment usage bitmap = # blocks/segment
    __u32 bits;
};

struct wlfs_super_meta {
    __u16 block_size;
    __u16 checkpoint_blocks;
    __u32 inodes;
    __u32 magic;
    __u32 segment_size;
    __u32 segments;
    __u8 buffer_period;
    __u8 checkpoint_period;
    __u8 indirection;
    __u8 min_clean_segs;
    __u8 target_clean_segs;
};
