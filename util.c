#include "util.h"

__u16 get_block_bytes (struct wlfs_super_disk *sb) {
    return sb->block_size - sizeof(struct block_disk);
}

__u16 get_imap_entries (struct wlfs_super_disk *sb) {
    return get_block_bytes(sb) / sizeof(__kernel_daddr_t);
}

__u32 get_imap_blocks (struct wlfs_super_disk *sb) {
    return sb->inodes / get_imap_entries(sb);
}

__u16 get_segmap_bits (struct wlfs_super_disk *sb) {
    return sb->segment_size / sb->block_size;
}

__u16 get_segmap_entries (struct wlfs_super_disk *sb) {
    return get_block_bytes(sb) / (get_segmap_bits(sb) >> 3);
}

__u32 get_segmap_blocks (struct wlfs_super_disk *sb) {
    return sb->segments / get_segmap_entries(sb);
}