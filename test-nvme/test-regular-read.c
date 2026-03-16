#include <time.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <linux/nvme_ioctl.h>

#define PAGE_SIZE (1 << 12)
#define PAGE_READ_SIZE (512)
#define NVME_SECTOR_SIZE (512)
#define MAX_PAGE_INDEX  (1 << 23)
#define SRAND_SEED 42
#define BILLION 1000000000L

// deterministic rng
static uint32_t ubpf_get_random_number(void) {
    static uint64_t rng_state = 0xDEADBEEFBECAFE;
    uint64_t x = rng_state;


    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    rng_state = x;
    return (uint32_t)x;
}

int main(int argc, char **argv) {
    int scan_depth = -1;
    if (argc < 3) {
        printf("Usage: %s, <ns_dev> <level>\n", argv[0]);
        return 1;
    }

    srand(SRAND_SEED);

    const char *ns_dev_path = argv[1];
    sscanf(argv[2], "%d", &scan_depth);

    if (scan_depth == -1) {
        printf("Scan depth not properly loaded\n");
        return 1;
    }

    int ns_device = open(ns_dev_path, O_RDWR);
    if (ns_device < 0) {
        perror("Open NS dev");
        return 1;
    }

    char buf[0x100];
    readbpf(depth, buf, sizeof(buf));
    int sector_size = 512;
    void *read_data = NULL;
    if (posix_memalign(&read_data, PAGE_SIZE, sector_size) != 0) {
        perror("posix_memalign");
        return 1;
    }

    // Prepare IO Command (Read)
    struct nvme_passthru_cmd io_cmd;
    memset(&io_cmd, 0, sizeof(io_cmd));

    io_cmd.opcode = 0x02;
    io_cmd.nsid = 1;
    // Physical buffer address and length
    io_cmd.addr    = (uint64_t)(uintptr_t)read_data;
    io_cmd.data_len = PAGE_READ_SIZE;  // 512 bytes
    io_cmd.timeout_ms = 1000;

    struct timespec start, stop;
    double elapsed;

    clock_gettime(CLOCK_MONOTONIC, &start);

    // you want to:
    // read 512 bytes at a random offset, for level number of times, to simulate parsing a B+ tree
    for (int i = 0; i < scan_depth; i++) {  
        // LBA to read from (index is already your logical block address)
        uint32_t index = ubpf_get_random_number();
        io_cmd.cdw10 = (uint32_t)(index & 0xffffffff);         // LBA lower 32 bits
        io_cmd.cdw11 = 0;
        
        // Number of logical blocks to read minus 1 (0 = 1 block = 512 bytes)
        io_cmd.cdw12 = 0;

        if (ioctl(ns_device, NVME_IOCTL_IO_CMD, &io_cmd) < 0) {
            perror("ioctl UBPF read");
            return 1;
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &stop);

    elapsed = (stop.tv_sec - start.tv_sec) + (double)(stop.tv_nsec - start.tv_nsec) / BILLION;

    printf("Time taken: %f seconds\n", elapsed);

    close(ns_device);
    free(read_data);

    return 0;
}