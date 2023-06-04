# uSwitch
This repository stores the code for the paper [uSwitch: Fast Kernel Context Isolation with Implicit Context Switches](https://www.cs.purdue.edu/homes/pfonseca/papers/sp23-uswitch.pdf).

uSwitch is a sandboxing technique that achieves fast isolation of memory and kernel resources.

uSwitch is based on Intel MPK and **implicit context switch**, which is a technique that allows
kernel context selection from user space and only adds 1 nanosecond overhead to the
Intel MPK domain switch latency. It uses a protected shared memory region that can be accessed by both
the kernel and the user space to achieve that.

uSwitch achieves 129 ns for the latency of a single sandboxed function call, which is
7x and 55x faster than lwC and process isolation respectively.

Please read our paper for more information. The paper is published at IEEE S&P 2021 as:

> **uSwitch: Fast Kernel Context Isolation with Implicit Context Switches**\
Dinglan Peng, Congyu Liu, Tapti Palit, Pedro Fonseca, Anjo Vahldiek-Oberwagner, and Mona Vij.

The BibTex citation of the work is
```
@inproceedings{uswitch,
  author = {Peng, Dinglan and Liu, Congyu and Palit, Tapti and Fonseca, Pedro and Vahldiek-Oberwagner, Anjo and Vij, Mona},
  booktitle = {2023 IEEE Symposium on Security and Privacy (S&P)},
  title = {uSWITCH: Fast Kernel Context Isolation with Implicit Context Switches},
  location = {San Francisco, CA},
  year = {2023},
  pages = {1506-1523},
  doi = {10.1109/SP46215.2023.00086},
  url = {https://doi.ieeecomputersociety.org/10.1109/SP46215.2023.00086},
  publisher = {IEEE Computer Society},
  address = {Los Alamitos, CA, USA},
  month = may
}
```

# License
The code of uSwitch in the the directory `kernel` is released with the GPLv2 license.
Other code of the project (in the directory `libuswitch`, `rlbox_uswitch`, `rlbox_wasm`, and `benchmark`) is released with the GPLv3.
The modification to other projects including Musl and RLBox is released
to their license.

# Build kernel
```shell
cd linux
cp config-uswitch .config
make -j`nproc`
sudo make install
```

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
Run the following command to build the programs for the media decoding and decompression benchmarks
and the HTTP benchmarks.

```shell
cd benchmark
make -j
```
