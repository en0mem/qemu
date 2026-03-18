#include <stdint.h>

#define PAGE_SIZE (1 << 12)
#define PAGE_READ_SIZE (512)
#define NVME_SECTOR_SIZE (512)
#define MAX_PAGE_INDEX  (1 << 23)

#define NUM_BLOCKS ((1 << 30) / (512))

static uint64_t (*ebpf_pread)(uint64_t buf, uint64_t bytes, uint64_t offset, uint64_t, uint64_t) = (void *)0;
static uint64_t (*ubpf_get_random_number)(void) = (void*)1;

struct b_tree_arg {
    void *opaque;
    uint8_t *buf;
    uint64_t size;
    uint64_t offset;
};

uint64_t scan_b_tree(void* args, uint64_t depth) {
    struct b_tree_arg* mem = (struct b_tree_arg*)(args);

    uint64_t index = ubpf_get_random_number() % NUM_BLOCKS;
    for (int i = 0; i < depth; i++) {
        if (ebpf_pread((uint64_t)mem->buf, NVME_SECTOR_SIZE, index * NVME_SECTOR_SIZE, 0, 0) != 0) {
            return 1;
        };
    }

    return 0;
}
