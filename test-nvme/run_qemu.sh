#!/bin/bash

# Path to the compiled QEMU binary
QEMU="../build/qemu-system-x86_64"

# Boot from the ISO and attach the NVMe disk
# transfer.dmg is a FAT image containing crc32_bpf.o (mount at /mnt in guest)
$QEMU \
    -m 1G \
    -smp 2 \
    -boot d \
    -cdrom alpine-virt-3.21.3-x86_64.iso \
    -drive file=nvme.img,if=none,id=nvme-disk,format=raw \
    -device nvme,serial=deadbeef,drive=nvme-disk \
    -drive file=transfer.dmg,format=raw,if=virtio,readonly=on \
    -nographic
