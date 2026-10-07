#!/bin/sh
set -e
mkdir -p /mnt/d/Opensweet/build/debian_test
cd /mnt/d/Opensweet/build/debian_test

cat << 'EOF' > test.c
#include <unistd.h>
int main(void) {
    write(1, "[Debian] Hello from dynamic glibc ELF!\n", 39);
    return 0;
}
EOF

gcc -O2 test.c -o debian_hello

# Copy glibc libraries and ld-linux
mkdir -p lib64 lib/x86_64-linux-gnu
cp /lib/x86_64-linux-gnu/ld-2.31.so lib64/ld-linux-x86-64.so.2
cp /lib/x86_64-linux-gnu/libc-2.31.so lib/x86_64-linux-gnu/libc.so.6

ls -lh debian_hello lib64/ld-linux-x86-64.so.2 lib/x86_64-linux-gnu/libc.so.6
