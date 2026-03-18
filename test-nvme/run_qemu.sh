#!/bin/bash
set -e

docker run --rm --platform linux/amd64 -v $(pwd):/src -w /src alpine:latest sh -c "apk add --no-cache gcc musl-dev linux-headers && cc -O2 -static -o test-ubpf test_ubpf.c && cc -O2 -static -o test-regular-read test-regular-read.c"
clang -O2 -target bpf -Wall -Wextra -c ubpf-read-b-tree.c -o ubpf-read-b-tree.o
llvm-objcopy -O binary -j .text ubpf-read-b-tree.o ubpf-read-b-tree.bin
mkdir -p transfer_tmp && cp test-regular-read transfer_tmp/ && cp test-ubpf transfer_tmp/ && cp ubpf-read-b-tree.bin transfer_tmp/ && hdiutil create -ov -srcfolder transfer_tmp -format UDRW -fs MS-DOS -volname TRANSFER transfer.dmg && rm -rf transfer_tmp

# Path to the compiled QEMU binary
QEMU="../build/qemu-system-x86_64-unsigned"

# Boot from the ISO and attach the NVMe disk
# transfer.dmg is a FAT image containing crc32_bpf.o (mount at /mnt in guest)
$QEMU \
    -m 1G \
    -boot d \
    -cdrom alpine-virt-3.21.3-x86_64.iso \
    -drive file=nvme.img,if=none,id=nvme-disk,format=raw \
    -device nvme,serial=deadbeef,drive=nvme-disk \
    -drive file=transfer.dmg,format=raw,if=virtio,readonly=on \
    -nographic \
    -d trace:pci_nvme_dma_read,trace:dma_blk_io,trace:dma_complete \
    -D /tmp/nvme-dma.log