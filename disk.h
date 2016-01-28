/*
 * On-disk metadata structures
 */

#pragma once

#include <linux/types.h>

struct header {
    __kernel_time_t wtime;
    // Incremented when the file is deleted/trunctated
    __u8 version;
};

struct block_disk {
    struct header h0;
    struct header h1;
    __u64 index;
};

struct wlfs_super_disk {
    __u16 block_size;
    __u16 checkpoint_blocks;
    __u16 magic;
    __u32 inodes;
    __u32 segment_size;
    __u32 segments;
    __u8 buffer_period;
    __u8 checkpoint_period;
    __u8 indirection;
    __u8 min_clean_segs;
    __u8 target_clean_segs;
};
