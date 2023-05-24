#include <iostream>
#include <fstream>
#include <ctime>
#include <cstdlib>
#include <cinttypes>
#include <cstring>
#include "uswitchsandbox.h"
#include "uswitch.hpp"

static uint64_t time_nanosec() {
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return t.tv_sec * 1000000000ull + t.tv_nsec;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        std::cerr << "Usage: ./test4-uswitch <times>\n";
        return 1;
    }
    int n = atoi(argv[1]);
    USwitchSandbox sandbox("../libraries_uswitch/libjpeg/libjpeg.so", 1024l << 20, 2l << 20);
    sandbox.init();
    uswctx_t ctx = sandbox.get_context();
    int (*test_func1)(int, int) = (int (*)(int, int))sandbox.get_symbol_addr("test_func1");
    uint64_t t1 = time_nanosec();
    for (int i = 0; i < n; ++i) {
        int res;
        uswitch_call_dynamic(ctx, test_func1, res, i, i);
    }
    uint64_t t2 = time_nanosec();
    std::cout << (double)(t2 - t1) / n << std::endl;
    return 0;
}