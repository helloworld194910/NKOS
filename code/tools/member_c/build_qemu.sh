#!/bin/sh
set -eu
# Use a Linux filesystem to avoid slow extraction/builds on /mnt/c.
work=${QEMU_BUILD_ROOT:-"$HOME/oslab2-member-c"}
mkdir -p "$work"
cd "$work"
if [ ! -f qemu-4.1.1.tar.xz ]; then
    curl -L --fail https://download.qemu.org/qemu-4.1.1.tar.xz -o qemu-4.1.1.tar.xz
fi
if [ ! -f qemu-4.1.1/configure ]; then
    tar -xf qemu-4.1.1.tar.xz
fi
cd qemu-4.1.1
./configure --target-list=riscv64-softmmu --enable-debug --disable-werror --disable-docs --python=python3
make -j"${JOBS:-8}"
printf 'Debug QEMU: %s/riscv64-softmmu/qemu-system-riscv64\n' "$PWD"
