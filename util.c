#include "util.h"

__u16 get_block_bytes (struct wlfs_super_meta *meta) {
    return meta->block_size - sizeof(struct block_meta);
}

__u16 get_imap_entries (struct wlfs_super_meta *meta) {
    return get_block_bytes(meta) / sizeof(__kernel_daddr_t);
}


__u32 get_imap_blocks (struct wlfs_super_meta *meta) {
    return meta->inodes / get_imap_entries(meta);
}

/*
 * Max file size = block size * (
 *      local block pointers per inode - 
 *      indirection +
 *      sum_{i=0}^{indirection} (entries per indirect block)^i)
 */
__u64 get_max_bytes (struct wlfs_super_meta *meta) {
    __u64 size = NBLOCK_PTR;

    if (meta->indirection < 1) {
        return size;
    }

    __u64 block_entries = get_daddr_entries(meta);
    size = size - meta->indirection + block_entries;
    int i = 2;
    for (; i <= meta->indirection; ++i) {
        __u64 base = block_entries;
        __u8 exp = i;
        __u64 result = 1;
        while (exp)
        {
            if (exp & 1)
                result *= base;
            exp >>= 1;
            base *= base;
        }
    }
    size *= meta->block_size;

    return size;
}

__u16 get_segmap_bits (struct wlfs_super_meta *meta) {
    return meta->segment_size / meta->block_size;
}

__u16 get_segmap_entries (struct wlfs_super_meta *meta) {
    return get_block_bytes(meta) / (get_segmap_bits(meta) >> 3);
}

__u32 get_segmap_blocks (struct wlfs_super_meta *meta) {
    return meta->segments / get_segmap_entries(meta);
}