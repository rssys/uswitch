# Build dependencies
```shell
sudo apt install build-essential yasm nasm cmake
./build_musl.sh
./build_wasm.sh
./build_rlbox.sh
```

# Build libraries
```shell
./build_lib_uswitch.sh
./build_lib_wasm.sh
./build_lib_native.sh
```

# Build benchmarks (WIP)
```shell
cd benchmark
make -j
```