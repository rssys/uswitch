#include <iostream>
#include <fstream>
#include <ctime>
#include <cstdlib>
#include <cinttypes>
#include <cstring>
#include "RLBox_NaCl.h"
#include "jpeglib.h"

static uint64_t time_nanosec() {
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return t.tv_sec * 1000000000ull + t.tv_nsec;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        std::cerr << "Usage: ./test4-nacl <times>\n";
        return 1;
    }
    int n = atoi(argv[1]);
    RLBox_NaCl sandbox;
    sandbox.impl_CreateSandbox("Sandboxing_NaCl/native_client/scons-out/nacl_irt-x86-64/staging/irt_core.nexe", "../rlbox_build/libjpeg-turbo/builds/x64/nacl_build_simd/mainCombine/libjpeg.nexe");
    uint64_t t1 = time_nanosec();
    int (*test_func1)(int, int) = (int (*)(int, int))sandbox.impl_LookupSymbol("test_func1", false);
    for (int i = 0; i < n; ++i) {
        sandbox.impl_InvokeFunction(test_func1, i, i);
    }
    uint64_t t2 = time_nanosec();
    std::cout << (double)(t2 - t1) / n << std::endl;
    return 0;
}