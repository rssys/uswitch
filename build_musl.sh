#!/bin/sh
MUSL_PATH=`pwd`/musl
cd musl-1.2.2
CFLAGS="-O2" ./configure --enable-static --prefix=$MUSL_PATH --with-malloc=mallocuswitch \
    --disable-shared --enable-debug --enable-optimize=yes
make -j && make install
