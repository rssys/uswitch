#!/bin/bash
FIREFOX_DIR=`pwd`/ffbuilds/firefox_64bit_optdebug_uswitch_mpk/dist/bin/
mkdir -p libraries_uswitch
cd libraries_uswitch
cd libevent
make distclean
CC=../../musl/bin/musl-gcc CFLAGS="-Wl,-z,relro,-z,now -fno-stack-protector" \
./configure --disable-openssl --disable-samples --disable-libevent-install --host='x86_64-linux-musl'
make -j
../../musl/bin/musl-gcc -shared .libs/*.o ../fast_memcpy/libfast_memcpy.a -o libevent.so
cd ../libvpx
make distclean
CXX=../../musl/bin/musl-gcc CC=../../musl/bin/musl-gcc CFLAGS=-Wl,-z,relro,-z,now \
./configure --enable-shared --enable-pic --disable-examples --disable-tools --disable-docs \
    --enable-multi-res-encoding --enable-postproc --enable-vp9-postproc --enable-optimizations \
    --disable-static --extra-cflags='-fno-stack-protector -fno-builtin-printf' --disable-unit-tests
make -j
#cp libvpx.so.4.1.0 $FIREFOX_DIR/libvpx.so
cd ../libpng
make clean && make -j #&& cp libpng.so $FIREFOX_DIR/libpng.so
cd ../libjpeg
make clean && make -j #&& cp libjpeg.so $FIREFOX_DIR/libjpeg.so
cd ../zlib
make clean && make -j #&& cp libz.so $FIREFOX_DIR/libz.so
cd ../..