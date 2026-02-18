#include <stdbool.h>

bool qemu_ubpf_read_code(UbpfState *u_ebpf, char *path) {
    return 0;
}

bool qemu_ubpf_read_target(UbpfState *u_ebpf, char *path) {
    return 0;
}

uint64_t qemu_ubpf_run_once(UbpfState *u_ebpf, void *target, size_t target_len) {
    return 0;
}

int qemu_ubpf_prepare(UbpfState *u_ebpf, char *code_path) {
    return 0;
}