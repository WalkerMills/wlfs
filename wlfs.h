/*
 * WLFS data structures, compile-time constants
 */

#pragma once

// Unique magic number for this filesystem
#define WLFS_MAGIC 0x5CA1AB1E
// Version number
#define WLFS_VERSION '0.0'

// Fixed indexes
// Reserve 0th block for boot loader, just in case
#define SUPER_BLOCK_INDEX 1
// There are two checkpoint blocks
#define CHECKPOINT_BLOCK_INDEX (WLFS_SUPER_BLOCK_NUM + 1)
// First block of the log (location of first segment)
#define LOG_BLOCK_INDEX (WLFS_CHECKPOINT_BLOCK_NUM + 2)
// Root inode number
#define ROOT_INODE_INDEX 1

// Default block size: 4 KiB (assumes advanced format block device)
#define BLOCK_SIZE 1 << 12;

// Period (seconds) between checkpoints
#define CHECKPOINT_PERIOD 45;

// Stop cleaning when the # of clean segments rises above this value
#define ENOUGH_CLEAN_SEGS 1 << 7;

// Default period (seconds) between write buffer flushes
#define FLUSH_PERIOD 30;

// Default level of block indirection
#define INDIRECTION 3;

// Default maximum number of inodes
#define MAX_INODES 1 << 18;

// Start cleaning when the # of clean segments drops below this value
#define MIN_CLEAN_SEGS 1 << 5;

// Number of block pointers locally stored in an inode
#define NBLOCK_PTR (1 << 4)

// Default segment size: 2 MiB
#define SEGMENT_SIZE 1 << 20;
