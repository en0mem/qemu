#ifndef QEMU_UBPF_H
#define QEMU_UBPF_H

#include <ubpf.h>
#include <math.h>
#include <elf.h>

#define MAX_LEN (1024*1024)

typedef struct UbpfState {
    char *code_path;
    void *code;
    size_t code_len;
    
    char *target_path;
    void *target;
    size_t target_len;

    struct ubpf_vm *vm;
    
    int type;
    char *func;
} UbpfState;

bool qemu_ubpf_read_code(UbpfState *u_ebpf, char *path);
bool qemu_ubpf_read_target(UbpfState *u_ebpf, char *path);

uint64_t qemu_ubpf_run_once(UbpfState *u_ebpf, void *target, size_t target_len);

int qemu_ubpf_prepare(UbpfState *u_ebpf, char *code_path);

#endif