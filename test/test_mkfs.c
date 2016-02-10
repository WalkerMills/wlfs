#define _XOPEN_SOURCE 200809L // pread

#include <fcntl.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
// #include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#include <cmocka.h>

#include "wlfs.h"

void test_mkfs (void **state) {
    int fd = open("m4_DEVICE", O_RDWR);
    struct wlfs_super_meta sb = {0};
    pread(fd, &sb, sizeof(struct wlfs_super_meta), WLFS_OFFSET);

    assert_true(sb.magic == WLFS_MAGIC);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_mkfs),      
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
