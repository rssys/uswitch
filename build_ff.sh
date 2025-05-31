#!/bin/bash
cd mozilla_firefox/ipc/chromium/src/third_party/libevent
cmake . && make -j
cd ../../../../..
make

