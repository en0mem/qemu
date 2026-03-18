#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <time.h>
#include <linux/nvme_ioctl.h>

#define NVME_ADM_CMD_UBPF_UPLOAD 0xC0
#define NVME_CMD_UBPF_READ       0x90
#define BILLION 1000000000L

int main(int argc, char **argv) {
    if (argc < 6) {
        printf("Usage: %s <admin_dev> <ns_dev> <bpf_prog.o> <depth> <reptitions>\n", argv[0]);
        printf("Example: %s /dev/nvme0 /dev/nvme0n1 ubpf-read-b-tree.bin 5 10\n", argv[0]);
        return 1;
    }

    const char* load_bpf = getenv("LOAD_BPF");
    const char *admin_dev_path = argv[1];
    const char *ns_dev_path = argv[2];
    const char *bpf_prog_path = argv[3];
    int depth = atoi(argv[4]);
    int repetitions = atoi(argv[5]);

    // Read BPF program
    if (load_bpf){
        FILE *f = fopen(bpf_prog_path, "rb");
        if (!f) {
            perror("fopen bpf prog");
            return 1;
        }
        fseek(f, 0, SEEK_END);
        long prog_size = ftell(f);
        fseek(f, 0, SEEK_SET);

        void *bpf_data = malloc(prog_size);
        if (fread(bpf_data, 1, prog_size, f) != prog_size) {
            perror("fread bpf prog");
            free(bpf_data);
            fclose(f);
            return 1;
        }
        fclose(f);

        printf("[1] Loaded %ld bytes of BPF bytecode from %s\n", prog_size, bpf_prog_path);

        // Open Admin Device
        int admin_fd = open(admin_dev_path, O_RDWR);
        if (admin_fd < 0) {
            perror("open admin dev");
            free(bpf_data);
            return 1;
        }

        // Prepare Setup Command (Upload)
        struct nvme_passthru_cmd admin_cmd;
        memset(&admin_cmd, 0, sizeof(admin_cmd));
        admin_cmd.opcode = NVME_ADM_CMD_UBPF_UPLOAD;
        admin_cmd.addr = (uint64_t)bpf_data;
        admin_cmd.data_len = prog_size;
        admin_cmd.cdw10 = prog_size; // Inform controller of size

        printf("[2] Issuing UBPF_UPLOAD admin command to %s...\n", admin_dev_path);
        if (ioctl(admin_fd, NVME_IOCTL_ADMIN_CMD, &admin_cmd) < 0) {
            perror("ioctl UBPF_UPLOAD");
            close(admin_fd);
            free(bpf_data);
            return 1;
        }
        printf("    -> UPLOAD Success!\n");
        close(admin_fd);
        free(bpf_data);
    }

    // Open Namespace Device
    int ns_fd = open(ns_dev_path, O_RDWR);
    if (ns_fd < 0) {
        perror("open ns dev");
        return 1;
    }

    // Allocate 1 sector of data for read
    int sector_size = 512;
    void *read_data = malloc(sector_size);

    // Prepare IO Command (Read)
    struct nvme_passthru_cmd io_cmd;
    memset(&io_cmd, 0, sizeof(io_cmd));
    io_cmd.opcode = NVME_CMD_UBPF_READ;
    io_cmd.nsid = 1;
    io_cmd.addr = (uint64_t)read_data;
    io_cmd.data_len = sector_size;
    io_cmd.cdw10 = 0; // SLBA lower
    io_cmd.cdw11 = 0; // SLBA upper
    io_cmd.cdw12 = 0; // NLB = 0 (1 sector)
    io_cmd.cdw2 = depth;

    double total_time = 0;

    printf("[3] Issuing UBPF_READ io command to %s (LBA 0)... reading to depth %d\n", ns_dev_path, depth);
    for (int i = 0; i < repetitions; ++i) {
        struct timespec start, stop;
        double elapsed;

        clock_gettime(CLOCK_MONOTONIC, &start);
        if (ioctl(ns_fd, NVME_IOCTL_IO_CMD, &io_cmd) < 0) {
            perror("ioctl UBPF_READ");
            close(ns_fd);
            free(read_data);
            return 1;
        }
        clock_gettime(CLOCK_MONOTONIC, &stop);

        elapsed = (stop.tv_sec - start.tv_sec) + (double)(stop.tv_nsec - start.tv_nsec) / BILLION;
        total_time += elapsed;
        printf("Time taken: %f seconds\n", elapsed);
        sleep(2);
    }

    printf("Time taken for %d separate reads %f\n", repetitions, total_time / repetitions);    

    printf("    -> READ Success! Check QEMU output for BPF execution results.\n");

    close(ns_fd);
    free(read_data);

    return 0;
}
