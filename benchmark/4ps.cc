#include <iostream>
#include <fstream>
#include <ctime>
#include <cstdlib>
#include <cinttypes>
#include <cstring>
#define USE_DUMMY_LIB
#include "ProcessSandbox.h"

static uint64_t time_nanosec() {
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return t.tv_sec * 1000000000ull + t.tv_nsec;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        std::cerr << "Usage: ./test4-ps <times>\n";
        return 1;
    }
    int n = atoi(argv[1]);
    DummyProcessSandbox sandbox("./ProcessSandbox/ProcessSandbox_otherside_dummy64", 9999, 0);
    uint64_t t1 = time_nanosec();
    for (int i = 0; i < n; ++i) {
        int res = sandbox.inv_APIFuncZero(i, i);
    }
    uint64_t t2 = time_nanosec();
    std::cout << (double)(t2 - t1) / n << std::endl;
    sandbox.destroySandbox();
    return 0;
}