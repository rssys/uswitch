#!/bin/bash
cd libuswitch && make clean && cd ..
cd rlbox_uswitch && make clean && cd ..
cd rlbox_wasm && make clean && cd ..
cd libraries_uswitch
cd libevent && make distclean && rm -f libevent.so && cd ..
cd libjpeg && make clean && cd ..
cd libpng && make clean && cd ..
cd libvpx && make distclean; cd ..
cd zlib && make clean && cd ..
cd ../libraries_wasm
cd libjpeg && make clean && cd ..
cd libpng && make clean && cd ..
cd libvpx && make distclean; cd ..
cd zlib && make clean && cd ..
cd ../libraries_native
cd libevent && make distclean && rm -f libevent.so && cd ..
cd libjpeg && make clean && cd ..
cd libpng && make clean && cd ..
cd zlib && make clean && cd ..
cd musl-1.2.2 && make distclean; cd ..
cd benchmark && make clean; cd ..
cd rlbox_build
rm -rf libjpeg-turbo/builds/x64 libpng_nacl/builds/x64 libvorbis/builds/x64 libvpx/builds/x64 libtheora/builds/x64 zlib_nacl/builds/x64
cd ProcessSandbox && make clean