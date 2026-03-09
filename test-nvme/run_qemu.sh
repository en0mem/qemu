#!/bin/bash

# Path to the compiled QEMU binary
QEMU="../build/qemu-system-x86_64"

# Boot from the ISO and attach the NVMe disk
$QEMU \
    -m 1G \
    -smp 2 \
    -boot d \
    -cdrom alpine-virt-3.21.3-x86_64.iso \
    -drive file=nvme.img,if=none,id=nvme-disk,format=raw \
    -device nvme,serial=deadbeef,drive=nvme-disk \
    -nographic
