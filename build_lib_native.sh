#!/bin/bash
cd libraries_native
cd libevent
make distclean
CFLAGS="-Wl,-z,relro,-z,now -fno-stack-protector" \
./configure --disable-openssl --disable-samples --disable-libevent-install
make -j
gcc -shared .libs/*.o -lpthread -o libevent.so
cd ../libvpx
make distclean
CFLAGS=-Wl,-z,relro,-z,now \
./configure --enable-shared --enable-pic --disable-examples --disable-tools --disable-docs \
    --enable-multi-res-encoding --enable-postproc --enable-vp9-postproc --enable-optimizations \
    --disable-static --extra-cflags='-fno-stack-protector -fno-builtin-printf' --disable-unit-tests
make -j
cd ../libpng
make clean && make -j
cd ../libjpeg
make clean && make -j
cd ../zlib
make clean && make -j
cd ../..
