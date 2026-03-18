#include "qemu/osdep.h"
#include "qemu/error-report.h"
#include "ubpf.h"
#include "ebpf/ubpf.h"

#include <elf.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void *qemu_ubpf_read(const char *path, size_t maxlen, size_t *len)
{
    FILE *file;
    size_t offset = 0, rv;
    void *data;

    if (!strcmp(path, "-")) {
        file = fdopen(STDIN_FILENO, "r");
    } else {
        file = fopen(path, "r");
    }

    if (file == NULL) {
        error_report("Failed to open file %s", path);
        return NULL;
    }

    data = g_malloc0(maxlen);
    while ((rv = fread(data + offset, 1, maxlen - offset, file)) > 0) {
        offset += rv;
    }

    if (ferror(file)) {
        error_report("Failed to read %s: %s", path, strerror(errno));
        goto err;
    }

    if (!feof(file)) {
        error_report("Failed to read %s because it is too large"
                     " (max %u bytes)", path, (unsigned)maxlen);
        goto err;
    }

    fclose(file);
    if (len) {
        *len = offset;
    }
    return data;

err:
    fclose(file);
    free(data);
    return false;
}

bool qemu_ubpf_read_code(UbpfState *u_ebpf, char *path)
{
    if (!path) {
        return false;
    }
    u_ebpf->code_path = path;

    u_ebpf->code = qemu_ubpf_read(u_ebpf->code_path, MAX_LEN,
                                  &u_ebpf->code_len);
    if (u_ebpf->code) {
        return true;
    } else {
        return false;
    }
}

bool qemu_ubpf_read_target(UbpfState *u_ebpf, char *path)
{
    if (!path) {
        return false;
    }
    u_ebpf->target_path = path;

    u_ebpf->target = qemu_ubpf_read(u_ebpf->target_path, MAX_LEN,
                                    &u_ebpf->target_len);
    if (u_ebpf->target) {
        return true;
    } else {
        return false;
    }
}

uint64_t qemu_ubpf_run_once(UbpfState *u_ebpf, void *target, size_t target_len) {
    uint64_t result;

    if (ubpf_exec(u_ebpf->vm, target, target_len, &result) < 0) {
        result = UINT64_MAX;
    }

    return result;
}

static uint64_t get_random_number(void)
{
    static uint64_t rng_state = 0xdeadbeefcafebabe;
    uint64_t x = rng_state;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    rng_state = x;
    return x;
}

static void register_functions(struct ubpf_vm *vm) {
    ubpf_register(vm, 1, "get_random_number", as_external_function_t(get_random_number));
}

int qemu_ubpf_load_bytecode(UbpfState *u_ebpf, const void *code, size_t code_len) {
    bool is_elf;
    char *errmsg;
    int ret;

    u_ebpf->code_len = code_len;
    u_ebpf->code = g_malloc(code_len);
    memcpy(u_ebpf->code, code, code_len);

    u_ebpf->vm = ubpf_create();
    if (!u_ebpf->vm) {
        error_report("Failed to create UBpf VM!");
        return -1;
    }

    register_functions(u_ebpf->vm);

    is_elf = u_ebpf->code_len >= SELFMAG && !memcmp(u_ebpf->code, ELFMAG, SELFMAG);

    if (is_elf) {
        ret = ubpf_load_elf(u_ebpf->vm, u_ebpf->code, u_ebpf->code_len, &errmsg);
    } else {
        ret = ubpf_load(u_ebpf->vm, u_ebpf->code, u_ebpf->code_len, &errmsg);
    }

    if (ret < 0) {
        error_report("Failed to load ubpf code: %s ", errmsg);
        free(errmsg);
        ubpf_destroy(u_ebpf->vm);
        return -1;
    }

    return 0;
}

int qemu_ubpf_prepare(UbpfState *u_ebpf, char *code_path) {
    bool is_elf;
    char *errmsg;
    int ret;

    if (!qemu_ubpf_read_code(u_ebpf, code_path)) {
        error_report("Ubpf failed to read code");
        return -1;
    }

    u_ebpf->vm = ubpf_create();
    if (!u_ebpf->vm) {
        error_report("Failed to create UBpf VM!");
        return -1;
    }

    register_functions(u_ebpf->vm);

    is_elf = u_ebpf->code_len >= SELFMAG && !memcmp(u_ebpf->code, ELFMAG, SELFMAG);

    if (is_elf) {
#if defined(UBPF_HAS_ELF_H)
        ret = ubpf_load_elf(u_ebpf->vm, u_ebpf->code, u_ebpf->code_len, &errmsg);
#else
        error_report("uBPF ELF loading is not supported in this build.");
        ubpf_destroy(u_ebpf->vm);
        return -1;
#endif
    } else {
        ret = ubpf_load(u_ebpf->vm, u_ebpf->code, u_ebpf->code_len, &errmsg);
    }
    
    if (ret < 0) {
        error_report("Failed to load ubpf code: %s ", errmsg);
        free(errmsg);
        ubpf_destroy(u_ebpf->vm);
        return -1;
    }

    return 0;
}
