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
    if (argc != 3) {
        std::cerr << "Usage: ./test5-ps <times> <twice>\n";
        return 1;
    }
    int n = atoi(argv[1]);
    bool twice = atoi(argv[2]);
    DummyProcessSandbox sandbox("./ProcessSandbox/ProcessSandbox_otherside_dummy64", 9999, 0);
    uint64_t t1 = time_nanosec();
    if (twice) {
        for (int i = 0; i < n; ++i) {
            int res = sandbox.inv_APIFuncSeven(i, i);
            struct timespec ts;
            clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts);
        }
    } else {
        for (int i = 0; i < n; ++i) {
            int res = sandbox.inv_APIFuncSeven(i, i);
        }
    }
    uint64_t t2 = time_nanosec();
    printf("%f\n", (double)(t2 - t1) / n);
    std::cout << (double)(t2 - t1) / n << std::endl;
    return 0;
}