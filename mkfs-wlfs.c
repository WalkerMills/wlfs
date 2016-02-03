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
    struct wlfs_super_meta sb;
};
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
    {0}
};
// Description of argp positional parameters
static char args_doc[] = "device";

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
static int write_super (struct arguments *arguments);

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

    // Write super block to disk
    int ret = write_super(&arguments);
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
    return ret;
}

/*
 * Helper functions
 */

bool check_overflow (__u64 value, unsigned bits) {
    return (value & ((1ULL << bits) - 1)) == value;
}

__u32 get_segments (struct wlfs_super_meta *sb, __u64 size) {
    __u64 segments = size / sb->segment_size;
    if (!check_overflow(segments, 32)) {
        fprintf(stderr, "Number of segments doesn't fit into 32 bits\n",
                segments);
        return -1;
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
                "Number of checkpoint blocks doesn't fit into 16 bits\n", 
                checkpoint_blocks);
        return -1;
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
#define LOGICAL_BLOCK_SIZE 512
        if (value < 1) {
            argp_error(state, "Block size of %huB is too small\n", value);
        } else if (value % LOGICAL_BLOCK_SIZE != 0) {
            argp_error(state, "Block size must be a multiple of %uB\n",
                       LOGICAL_BLOCK_SIZE);
        } else if (value > WLFS_BLOCK_SIZE) {
            argp_error(state, "Block size must be <= %dB\n", 
                       WLFS_BLOCK_SIZE);
        }
        arguments->sb.block_size = value;
        break;

    case 'w':
        if (value < 1) {
            argp_error(state, 
                       "Write-back period of %hhu seconds is too small\n", 
                       value);
        } else if (!check_overflow(value, 8)) {
            argp_error(state, "Write-back period doesn't fit into 8 bits");
        }
        arguments->sb.buffer_period = value;
        break;

    case 'c':
        if (value < 1) {
            argp_error(state, 
                       "Checkpoint period of %hhu seconds is too small\n", 
                       value);
        } else if (!check_overflow(value, 8)) {
            argp_error(state, "Checkpoint period doesn't fit into 8 bits");
        }
        arguments->sb.checkpoint_period = value;
        break;

    case 'i':
        if (value < 0) {
            argp_error(state, "Indirection depth of %hhu is too small\n", 
                       value);
        } else if (!check_overflow(value, 8)) {
            argp_error(state, "Indirection doesn't fit into 8 bits");
        }
        arguments->sb.indirection = value;
        break;

    case 'n':
        if (value < 1) {
            argp_error(state, "%u is not enough inodes\n", value);
        } else if (!check_overflow(value, 32)) {
            argp_error(state, "Max inode count doesn't fit into 32 bits");
        }
        arguments->sb.inodes = value;
        break;

    case 'm':
        if (value < 1) {
            argp_error(state, 
                       "A threshold of %hhu will never trigger cleaning\n", 
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
        } else if (value % WLFS_BLOCK_SIZE != 0) {
            argp_error(state, "Segment size must be a multiple of %uB\n",
                       WLFS_BLOCK_SIZE);
        } else if (!check_overflow(value, 32)) {
            argp_error(state, "Segment size doesn't fit into 32 bits");
        }
        arguments->sb.segment_size = value;
        break;

    case 't':
        if (value < 1) {
            argp_error(state, "A threshold of %hhu will prevent cleaning\n", 
                       value);
        } else if (!check_overflow(value, 8)) {
            argp_error(state, "Target clean segments doesn't fit into 8 bits");
        }
        arguments->sb.buffer_period = value;
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

int write_super (struct arguments *arguments) {
    ssize_t ret = -1;
    int fd = open(arguments->device, O_RDWR | O_DSYNC);
    if (fd < 0) {
        fprintf(stderr, "Opening %s failed\n", arguments->device);
        return ret;
    }

    // Get the size of the block device; this ioctl will fail when fd refers
    // to a file, so fall back to fstat if necessary
    __u64 size;
    if (ioctl(fd, BLKGETSIZE64, &size) < 0) {
        fprintf(stderr, "ioctl failed to get size, falling back to fstat\n");
        struct stat buf;
        if (fstat(fd, &buf) < 0) {
            fprintf(stderr, "fstat failed for %s\n", arguments->device);
            goto exit;
        }
        size = buf.st_size;
    }
    // Make sure there's at least enough room for the superblock
    if (size < WLFS_OFFSET + arguments->sb.block_size) {
        fprintf(stderr, "%s is too small (%lluB) to write superblock\n", 
                arguments->device, size);
        goto exit;
    }
#ifndef NDEBUG
    printf("%s is %llu bytes\n", arguments->device, size);
#endif

    // Set total number of segments
    __u32 segments = get_segments(&arguments->sb, size);
    if (segments < 0) {
        fprintf(stderr, "Failed to get number of segments\n");
        goto exit;
    }
    arguments->sb.segments = segments;
    // Set the number of checkpoint blocks
    __u16 checkpoint_blocks = get_checkpoint_blocks(&arguments->sb);
    if (checkpoint_blocks < 0) {
        fprintf(stderr, "Failed to get number of checkpoint blocks\n");
        goto exit;
    }
    arguments->sb.checkpoint_blocks = checkpoint_blocks;

    // Write super block to the correct sector of the block device
    ret = pwrite(fd, &arguments->sb, sizeof(struct wlfs_super_meta), 
                 WLFS_OFFSET);
    if (ret < 0) {
        fprintf(stderr, "Writing superblock failed with error code %zd\n", 
                ret);
        goto exit;
    }

    printf("Sucessfully wrote wlfs super block\n");

exit:
    close(fd);
    return ret;
}
