#define _XOPEN_SOURCE 200809L // pread

#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#include <check.h>

#include "wlfs.h"

START_TEST (test_write_super) {
    int fd = open("m4_DEVICE", O_RDWR);
    struct wlfs_super_meta sb = {0};
    pread(fd, &sb, sizeof(struct wlfs_super_meta), WLFS_OFFSET);

    ck_assert(sb.magic == WLFS_MAGIC);
} END_TEST

Suite *mkfs_suite (void) {
    Suite *suite = suite_create("mkfs-wlfs");
    TCase *test_case = tcase_create("write_super");
    
    tcase_add_test(test_case, test_write_super);
    suite_add_tcase(suite, test_case);

    return suite;
}

int main(void) {
    Suite *suite = mkfs_suite();
    SRunner *runner = srunner_create(suite);

    srunner_run_all(runner, CK_NORMAL);
    int failed = srunner_ntests_failed(runner);
    srunner_free(runner);

    return (failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
