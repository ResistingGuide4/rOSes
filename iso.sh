#!/bin/sh
set -e
. ./build.sh

mkdir -p isodir
mkdir -p isodir/boot
mkdir -p isodir/boot/grub

cp sysroot/boot/roses.kernel isodir/boot/roses.kernel
cat > isodir/boot/grub/grub.cfg << EOF
menuentry "roses" {
	multiboot2 /boot/roses.kernel
}
EOF
grub2-mkrescue -o roses.iso isodir
