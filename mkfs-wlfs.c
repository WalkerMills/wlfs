#include <argp.h>
#include <assert.h>
#include <fcntl.h>
#include <linux/fs.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "util.h"
#include "wlfs.h"

// Argp argument parser
static error_t parse_opt (int key, char *arg, struct argp_state *state);
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

// Persist the superblock to the block device
static int write_super (struct arguments *arguments);
// Calculate & store the number of segments in the block device; may return -1
// if something goes wrong with the block device
static __u32 get_segments (struct wlfs_super_meta *sb, __u64 size);
// Calculate & store the number of checkpoint blocks
static __u16 get_checkpoint_blocks (struct wlfs_super_meta *sb);

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

error_t parse_opt (int key, char *arg, struct argp_state *state) {
    struct arguments *arguments = (struct arguments *) state->input;
    int value;
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
#ifndef NDEBUG
        printf("Block size: %hu\n", value);
#endif
        arguments->sb.block_size = value;
        break;

    case 'w':
        if (value < 1) {
            argp_error(state, 
                       "Write-back period of %hhu seconds is too small\n", 
                       value);
        }
#ifndef NDEBUG
        printf("Write-back period: %hhu\n", value);
#endif
        arguments->sb.buffer_period = value;
        break;

    case 'c':
        if (value < 1) {
            argp_error(state, 
                       "Checkpoint period of %hhu seconds is too small\n", 
                       value);
        }
#ifndef NDEBUG
        printf("Checkpoint period: %hhu\n", value);
#endif
        arguments->sb.checkpoint_period = value;
        break;

    case 'i':
        if (value < 0) {
            argp_error(state, "Indirection depth of %hhu is too small\n", 
                       value);
        }
#ifndef NDEBUG
        printf("Indirection: %hhu\n", value);
#endif
        arguments->sb.indirection = value;
        break;

    case 'n':
        if (value < 1) {
            argp_error(state, "%u is not enough inodes\n", value);
        }
#ifndef NDEBUG
        printf("Max inodes: %u\n", value);
#endif
        arguments->sb.inodes = value;
        break;

    case 'm':
        if (value < 1) {
            argp_error(state, 
                       "A threshold of %hhu will never trigger cleaning\n", 
                       value);
        }
#ifndef NDEBUG
        printf("Min clean: %hhu\n", value);
#endif
        arguments->sb.min_clean_segs = value;
        break;

    case 's':
        if (value < 1) {
            argp_error(state, "Segment size of %uB is too small\n", 
                       value);
        } else if (value % LOGICAL_BLOCK_SIZE != 0) {
            argp_error(state, "Segment size must be a multiple of %uB\n",
                       LOGICAL_BLOCK_SIZE);
        } else if (value < WLFS_BLOCK_SIZE) {
            argp_error(state, "Segment size must be at least %dB\n",
                       WLFS_BLOCK_SIZE);
        }
#ifndef NDEBUG
        printf("Segment size: %u\n", value);
#endif
        arguments->sb.segment_size = value;
        break;

    case 't':
        if (value < 1) {
            argp_error(state, "A threshold of %hhu will prevent cleaning\n", 
                       value);
        }
#ifndef NDEBUG
        printf("Target clean: %hhu\n", value);
#endif
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
        fprintf(stderr, "Error: opening %s failed\n", arguments->device);
        return ret;
    }

    // Get the size of the block device; this ioctl will fail when fd refers
    // to a file, so fall back to fstat if necessary
    __u64 size;
    if (ioctl(fd, BLKGETSIZE64, &size) < 0) {
        fprintf(stderr, 
                "Warning: ioctl failed to get size, falling back to fstat\n");
        struct stat buf;
        if (fstat(fd, &buf) < 0) {
            fprintf(stderr, "Error: fstat failed for %s, exiting\n", 
                    arguments->device);
            goto exit;
        }
        size = buf.st_size;
    }
    // Make sure there's at least enough room for the superblock
    if (size < WLFS_OFFSET + arguments->sb.block_size) {
        fprintf(stderr, 
                "Error: %s is too small (%lluB) to write superblock\n", 
                arguments->device,
                size);
        goto exit;
    }
#ifndef NDEBUG
    printf("%s is %llu bytes\n", arguments->device, size);
#endif
    // Set total number of segments
    arguments->sb.segments = get_segments(&arguments->sb, size);
    // Set the number of checkpoint blocks
    arguments->sb.checkpoint_blocks = get_checkpoint_blocks(&arguments->sb);

    // Write super block to the correct sector of the block device
    ret = pwrite(fd, &arguments->sb, sizeof(struct wlfs_super_meta), 
                 WLFS_OFFSET);
    if (ret < 0) {
        fprintf(stderr, 
                "Fatal: writing superblock failed with error code %zd\n", 
                ret);
        goto exit;
    }
    printf("Sucessfully wrote wlfs super block\n");

exit:
    close(fd);
    return ret;
}

__u32 get_segments (struct wlfs_super_meta *sb, __u64 size) {
    __u64 segments = size / sb->segment_size;
    if (sizeof(sb->segments) < sizeof(__u64)) {
        // Make sure the number of segments fits into 32 bits
        __u64 mask = (1ULL << 32) - 1;
        assert((segments & mask) == segments);
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
    // Make sure the number of checkpoint blocks fits into 16 bits
    assert((checkpoint_blocks & ((1 << 16) - 1)) == checkpoint_blocks);
    return checkpoint_blocks;
}

