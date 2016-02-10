#define _XOPEN_SOURCE 200809L // pread

#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>

#include <gtest/gtest.h>

#include "wlfs.h"

TEST (test_mkfs_wlfs, test_write_super) {
    int fd = open("m4_DEVICE", O_RDWR);
    struct wlfs_super_meta sb = {0};
    pread(fd, &sb, sizeof(struct wlfs_super_meta), WLFS_OFFSET);

    ASSERT_TRUE(sb.magic == WLFS_MAGIC);
}

int main (int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
