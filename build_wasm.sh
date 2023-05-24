#!/bin/bash
cd wasm2c_sandbox_compiler
rm -rf build
mkdir build
cd build
cmake .. -DBUILD_TESTS=OFF
make -j