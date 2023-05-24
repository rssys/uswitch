#!/bin/bash
cd rlbox_build
cd NASM_NaCl && ./configure && make -j && cd ..
make -C zlib_nacl/builds/ x64/non_nacl_build
make -C libjpeg-turbo/builds x64/non_nacl_build_simd
make -C libjpeg-turbo/builds x64/non_nacl_build
make -C libpng_nacl/builds x64/non_nacl_build
make -C libvorbis/builds x64/non_nacl_build
make -C libtheora/builds x64/non_nacl_build
make -C libvpx/builds x64/non_nacl_build
make -C ProcessSandbox/ -j