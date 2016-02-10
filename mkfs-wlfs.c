#include <argp.h>
#include <fcntl.h>
#include <linux/fs.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "util.h"
#include "wlfs.h"

// Data structure for holding parsed argp parameters
struct arguments {
    char *device;
    bool round;
    struct wlfs_super_meta sb;
};
// Return codes
enum return_code {
    SUCCESS,
    DEVICE_ERROR,       // Something went wrong with the block device
    INVALID_ARGUMENT,   // An invalid argument was supplied
    ILLEGAL_CONFIG,     // Argument choice caused an illegal configuration
};

// Compute derived filesystem parameters (e.g., segment count) and either
// round or reject misaligned block/segment sizes
static enum return_code build_super (int fd, struct wlfs_super_meta *sb, 
                                     bool round);
// Check if the named value's highest set bit is within the lowest <bits> bits
static bool check_overflow (__u64 value, unsigned bits);
// Calculate the number of segments in the block device; may return -1 if an
// error occurs
static __u32 get_segments (struct wlfs_super_meta *sb, __u64 size);
// Calculate the number of checkpoint blocks; may return -1 if an error occurs
static __u16 get_checkpoint_blocks (struct wlfs_super_meta *sb);
// Argp argument parser
static error_t parse_opt (int key, char *arg, struct argp_state *state);
// Persist the superblock to the block device
static enum return_code write_super (int fd, struct wlfs_super_meta *sb);

// Description of argp keyword parameters
static struct argp_option options[] = {
    {"block-size", 'b', "size", 0, "Block size (bytes)"},
    {"buffer-period", 'w', "period", 0, "Write-back period (seconds)"},
    {"checkpoint-period", 'c', "period", 0, "Checkpoint period (seconds)"},
    {"indirection", 'i', "depth", 0, "Indirect block tree depth"},
    {"inodes", 'n', "num", 0, "Maximum number of inodes"},
    {"min-clean", 'm', "num", 0, 
     "Clean when the number of clean segments drops below this value"},
    {"segment-size", 's', "size", 0, "Segment size (bytes)"},
    {"target-clean", 't', "num", 0, 
     "Stop cleaning when the number of clean segments rises above this value"},
    {"round", 'r', 0, 0, 
     "Round block/segment size to the nearest sector/block boundary"},
    {0}
};
// Description of argp positional parameters
static char args_doc[] = "device";

int main (int argc, char **argv) {
    // Initialize argp parser
    struct argp argp = {options, parse_opt, args_doc};
    struct arguments arguments;
    // Set default argument values
    arguments.sb.block_size = WLFS_BLOCK_SIZE;
    arguments.sb.buffer_period = BUFFER_PERIOD;
    arguments.sb.checkpoint_period = CHECKPOINT_PERIOD;
    arguments.sb.indirection = INDIRECTION;
    arguments.sb.magic = (__u32) WLFS_MAGIC;
    arguments.sb.inodes = MAX_INODES;
    arguments.sb.min_clean_segs = MIN_CLEAN_SEGS;
    arguments.sb.segment_size = SEGMENT_SIZE;
    arguments.sb.target_clean_segs = TARGET_CLEAN_SEGS;
    // Parse commandline arguments
    argp_parse(&argp, argc, argv, 0, 0, &arguments);

    int fd = open(arguments.device, O_RDWR | O_DSYNC);
    if (fd < 0) {
        fprintf(stderr, "Opening %s failed\n", arguments.device);
        return -DEVICE_ERROR;
    }

    // Sanitize input and compute derived values
    int ret = build_super(fd, &arguments.sb, arguments.round);
    if (ret != SUCCESS) {
        fprintf(stderr, "Failed to compute filesystem parameters for %s\n",
                arguments.device);
        goto exit;
    }
#ifndef NDEBUG
    printf("Block size: %hu\n"
           "Buffer period: %hhu\n"
           "Checkpoint blocks: %hu\n"
           "Checkpoint period: %hhu\n"
           "Indirection: %hhu\n"
           "Max inodes: %u\n"
           "Minimum clean segments: %hhu\n"
           "Segments: %u\n"
           "Segment size: %u\n"
           "Target clean segments: %hhu\n",
           arguments.sb.block_size, arguments.sb.buffer_period, 
           arguments.sb.checkpoint_blocks, arguments.sb.checkpoint_period,
           arguments.sb.indirection, arguments.sb.inodes,
           arguments.sb.min_clean_segs, arguments.sb.segments,
           arguments.sb.segment_size, arguments.sb.target_clean_segs);
#endif

    // Write super block to disk
    ret = write_super(fd, &arguments.sb);
    if (ret != SUCCESS) {
        fprintf(stderr, "Failed to write superblock to %s\n", 
                arguments.device);
    }

exit:
    close(fd);
    return ret;
}

/*
 * Helper functions
 */

enum return_code build_super (int fd, struct wlfs_super_meta *sb, bool round) {
    __u64 block_size;
    __u64 size;
    struct stat buf;

    // Find the physical block size & block device size; ioctl's will not work
    // when fd refers to a file, so fall back to fstat
    if (ioctl(fd, BLKPBSZGET, &block_size) < 0 || 
        ioctl(fd, BLKGETSIZE64, &size) < 0) {
        fprintf(stderr, "ioctl failed, falling back to fstat\n");
        if (fstat(fd, &buf) < 0) {
            fprintf(stderr, "fstat failed\n");
            return -DEVICE_ERROR;
        }
        block_size = buf.st_blksize;
        size = buf.st_size;
    }

    if (round) {
        // Round the block & segment sizes
        sb->block_size = (__u16) block_size;
        sb->segment_size += sb->segment_size % sb->block_size;
    // Reject incorrect sizes            
    } else if (sb->segment_size % sb->block_size != 0) {
        fprintf(stderr, 
                "%uB (segment size) %% %huB (block size) != 0, consider "
                "using -r\n", 
                sb->segment_size, 
                sb->block_size);
        return -INVALID_ARGUMENT;
    }
#ifndef NDEBUG
    printf("Device is %lluB, with %huB blocks\n", size, sb->block_size);
#endif

    // Set the number of checkpoint blocks
    __u16 checkpoint_blocks = get_checkpoint_blocks(sb);
    if (checkpoint_blocks < 2) {
        fprintf(stderr, "Failed to get number of checkpoint blocks\n");
        return -ILLEGAL_CONFIG;
    }
    sb->checkpoint_blocks = checkpoint_blocks;

    // Set total number of segments
    __u32 segments = get_segments(sb, size);
    if (segments == 0) {
        fprintf(stderr, "Not enough space for wlfs\n");
        return -ILLEGAL_CONFIG;
    } else if (segments < 1) {
        fprintf(stderr, "Failed to get number of segments\n");
        return -INVALID_ARGUMENT;
    }
    sb->segments = segments;

    return SUCCESS;
}


bool check_overflow (__u64 value, unsigned bits) {
    return (value & ((1ULL << bits) - 1)) == value;
}

// get_checkpoint_blocks must be called first
__u32 get_segments (struct wlfs_super_meta *sb, __u64 size) {
    __u64 segments = 
        (size - WLFS_OFFSET - (sb->checkpoint_blocks + 1) * sb->block_size) / 
        sb->segment_size;
    if (!check_overflow(segments, 32)) {
        fprintf(stderr, "Number of segments doesn't fit into 32 bits\n");
        return -INVALID_ARGUMENT;
    }
    return segments;
}

__u16 get_checkpoint_blocks (struct wlfs_super_meta *sb) {
    // Number of disk addresses that fit in a block (imap entries are disk
    // addresses)
    __u32 entries = get_daddr_entries(sb);
    // Total number of imap blocks
    __u32 imap_blocks = get_imap_blocks(sb);
    // Total number of segment usage bitmap blocks
    __u32 segmap_blocks = get_segmap_blocks(sb);
#ifndef NDEBUG
    printf("%u imap blocks, %u segmap blocks\n", imap_blocks, segmap_blocks);
#endif

    // Checkpoint blocks store disk addresses of imap & segmap blocks; if the
    // number of map blocks is not a multiple of the entries per block, pad
    // the last block instead of mixing imap & segmap addresses
    __u32 checkpoint_blocks = 
        imap_blocks / entries + (imap_blocks % entries != 0) +
        segmap_blocks / entries + (segmap_blocks % entries != 0);
    if (!check_overflow(checkpoint_blocks, 16)) {
        fprintf(stderr, 
                "Number of checkpoint blocks doesn't fit into 16 bits\n");
        return -INVALID_ARGUMENT;
    }
    return checkpoint_blocks;
}

error_t parse_opt (int key, char *arg, struct argp_state *state) {
    struct arguments *arguments = (struct arguments *) state->input;
    __u64 value;
    if (arg) {
        value = atoi(arg);
    }

    switch (key) {
    case 'b':
        if (value < 1) {
            argp_error(state, "Block size of %lluB is too small\n", value);
        } else if (value > (__u64) getpagesize()){
            argp_error(state, "Block size must be <= %dB\n", getpagesize());
        }
        arguments->sb.block_size = value;
        break;

    case 'w':
        if (value < 1) {
            argp_error(state, 
                       "Write-back period of %llu seconds is too small\n", 
                       value);
        } else if (!check_overflow(value, 8)) {
            argp_error(state, "Write-back period doesn't fit into 8 bits");
        }
        arguments->sb.buffer_period = value;
        break;

    case 'c':
        if (value < 1) {
            argp_error(state, 
                       "Checkpoint period of %llu seconds is too small\n", 
                       value);
        } else if (!check_overflow(value, 8)) {
            argp_error(state, "Checkpoint period doesn't fit into 8 bits");
        }
        arguments->sb.checkpoint_period = value;
        break;

    case 'i':
        if (value == 0) {
            argp_error(state, "Indirection depth of %llu is too small\n", 
                       value);
        } else if (!check_overflow(value, 8)) {
            argp_error(state, "Indirection doesn't fit into 8 bits");
        }
        arguments->sb.indirection = value;
        break;

    case 'n':
        if (value < 1) {
            argp_error(state, "%llu is not enough inodes\n", value);
        } else if (!check_overflow(value, 32)) {
            argp_error(state, "Max inode count doesn't fit into 32 bits");
        }
        arguments->sb.inodes = value;
        break;

    case 'm':
        if (value < 1) {
            argp_error(state, 
                       "A threshold of %llu will never trigger cleaning\n", 
                       value);
        } else if (!check_overflow(value, 8)) {
            argp_error(state, 
                       "Minimum clean segment count doesn't fit into 8 bits");
        }
        arguments->sb.min_clean_segs = value;
        break;

    case 's':
        if (value < WLFS_BLOCK_SIZE) {
            argp_error(state, "Segment size must be at least %dB\n",
                       WLFS_BLOCK_SIZE);
        } else if (!check_overflow(value, 32)) {
            argp_error(state, "Segment size doesn't fit into 32 bits");
        }
        arguments->sb.segment_size = value;
        break;

    case 't':
        if (value < 1) {
            argp_error(state, "A threshold of %llu will prevent cleaning\n", 
                       value);
        } else if (!check_overflow(value, 8)) {
            argp_error(state, "Target clean segments doesn't fit into 8 bits");
        }
        arguments->sb.buffer_period = value;
        break;

    case 'r':
        arguments->round = true;
        break;

    case ARGP_KEY_ARG:
        if (state->arg_num > 1) {
            argp_usage(state);
        }
        arguments->device = arg;
        break;

    case ARGP_KEY_END:
        if (state->arg_num < 1) {
            argp_usage(state);
        }
        break;

    default:
        return ARGP_ERR_UNKNOWN;
    }

    return 0;
}

enum return_code write_super (int fd, struct wlfs_super_meta *sb) {
    // Write super block to the correct sector of the block device
    ssize_t ret = pwrite(fd, sb, sizeof(struct wlfs_super_meta), WLFS_OFFSET);
    if (ret < 0) {
        fprintf(stderr, "pwrite failed with error code %zd\n", ret);
        return -DEVICE_ERROR;
    }
#ifndef NDEBUG
    printf("Sucessfully wrote wlfs super block\n");
#endif

    return SUCCESS;
}
