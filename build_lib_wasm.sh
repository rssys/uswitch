#!/bin/bash
FIREFOX_DIR=`pwd`/ffbuilds/firefox_64bit_optdebug_wasm/dist/bin/
cd libraries_wasm
cd libvpx
make distclean
rm -f libvpx.wasm libvpx.so
CXX=../../wasi-sdk-14.0/bin/clang++ CC=../../wasi-sdk-14.0/bin/clang AR=../../wasi-sdk-14.0/bin/ar \
./configure --enable-static --enable-debug \
    --disable-examples --disable-tools --disable-docs --disable-shared --disable-unit-tests --disable-multithread \
    --target=generic-gnu
make -j
../../wasi-sdk-14.0/bin/clang -Wl,--whole-archive libvpx.a -Wl,--no-whole-archive main.c -Wl,--export-all -Wl,--no-entry -Wl,--growable-table -o libvpx.wasm
../../wasm2c_sandbox_compiler/bin/wasm2c libvpx.wasm -o libvpx.c -n libvpx_
gcc libvpx.c ../../wasm2c_sandbox_compiler/wasm2c/wasm-rt-wasi.c \
    ../../wasm2c_sandbox_compiler/wasm2c/wasm-rt-impl.c \
    ../../wasm2c_sandbox_compiler/wasm2c/wasm-rt-os-unix.c \
    -I ../../wasm2c_sandbox_compiler/wasm2c -g -O2 -fPIC -shared -o libvpx.so
#cp libvpx.so $FIREFOX_DIR/libvpx.so
cd ../libpng
make clean && make -j #&& cp libpng.so $FIREFOX_DIR/libpng.so
cd ../libjpeg
make clean && make -j #&& cp libjpeg.so $FIREFOX_DIR/libjpeg.so
cd ../zlib
make clean && make -j #&& cp libz.so $FIREFOX_DIR/libz.so
cd ../..
