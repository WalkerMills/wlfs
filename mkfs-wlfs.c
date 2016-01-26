#include <argp.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "wlfs.h"

// Data structure for holding parsed argp parameters
struct arguments {
    char *device;
    struct wlfs_super_disk sb;
};

// Description of argp keyword parameters
static struct argp_option options[] = {
    {"block-size", 'b', "size", 0, "Block size (bytes)"},
    {"buffer-period", 'w', "period", 0, "Write-back period (seconds)"},
    {"checkpoint-period", 'c', "period", 0, "Checkpoint period (seconds)"},
    {"indirection", 'i', "depth", 0, "Indirect block tree depth"},
    {"max-inodes", 'n', "num", 0, "Maximum number of inodes"},
    {"min-clean", 'm', "num", 0, 
     "Clean when the number of clean segments drops below this value"},
    {"segment-size", 's', "size", 0, "Segment size (bytes)"},
    {"target-clean", 't', "num", 0, 
     "Stop cleaning when the number of clean segments rises above this value"},
    {0}
};

// Argp argument parser
static error_t parse_opt (int key, char *arg, struct argp_state *state) {
    struct arguments *arguments = (struct arguments *) state->input;
    int value;
    if (arg) {
        value = atoi(arg);
    }

    switch (key) {
    case 'b':
        if (value < 1) {
            argp_error(state, "Block size of %hu bytes is too small\n", value);
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
        arguments->sb.max_inodes = value;
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
            argp_error(state, "Segment size of %u bytes is too small\n", 
                       value);
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

// Description of positional parameters
static char args_doc[] = "device";

// Helper function for persisting the super block
static int write_super (struct arguments *arguments) {
    ssize_t ret = -1;
    // Open block device for writing with DMA & synchronous writes
    int fd = open(arguments->device, O_WRONLY | O_DIRECT | O_DSYNC);
    if (fd == -1) {
        fprintf(stderr, "Error opening device %s\n", arguments->device);
        goto exit;
    }

    // Write super block to the correct sector of the block device
    if (SUPER_BLOCK_INDEX == 0) {
        ret = write(fd, &arguments->sb, sizeof(struct wlfs_super_disk));
    } else {
        ret = pwrite(fd, &arguments->sb, sizeof(struct wlfs_super_disk),
                     SUPER_BLOCK_INDEX * arguments->sb.block_size);
    }
    if (ret < 0) {
        fprintf(stderr, "Writing superblock failed with error code %zd\n", 
                ret);
        goto exit;
    }

    printf("Sucessfully wrote wlfs super block\n");
    close(fd);
exit:
    return ret;
}

int main (int argc, char **argv) {
    // Initialize argp parser
    struct argp argp = {options, parse_opt, args_doc};
    struct arguments arguments;
    // Set default argument values
    arguments.sb.block_size = WLFS_BLOCK_SIZE;
    arguments.sb.buffer_period = BUFFER_PERIOD;
    arguments.sb.checkpoint_period = CHECKPOINT_PERIOD;
    arguments.sb.indirection = INDIRECTION;
    arguments.sb.magic = WLFS_MAGIC;
    arguments.sb.max_inodes = MAX_INODES;
    arguments.sb.min_clean_segs = MIN_CLEAN_SEGS;
    arguments.sb.segment_size = SEGMENT_SIZE;
    arguments.sb.target_clean_segs = TARGET_CLEAN_SEGS;
    // Parse commandline arguments
    argp_parse(&argp, argc, argv, 0, 0, &arguments);

#ifndef NDEBUG
    printf("Block size: %hu\n"
           "Buffer period: %hhu\n"
           "Checkpoint period: %hhu\n"
           "Indirection: %hhu\n"
           "Max inodes: %u\n"
           "Minimum clean segments: %hhu\n"
           "Segment size: %u\n"
           "Target clean segments: %hhu\n",
           arguments.sb.block_size, arguments.sb.buffer_period, 
           arguments.sb.checkpoint_period, arguments.sb.indirection, 
           arguments.sb.max_inodes, arguments.sb.min_clean_segs, 
           arguments.sb.segment_size, arguments.sb.target_clean_segs);
#endif
    // Write super block to disk
    int ret = write_super(&arguments);
    return ret;
}
