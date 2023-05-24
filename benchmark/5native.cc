#include <iostream>
#include <fstream>
#include <ctime>
#include <cstdlib>
#include <cinttypes>
#include <cstring>

extern "C" int test_func2(int x, int y);

static uint64_t time_nanosec() {
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return t.tv_sec * 1000000000ull + t.tv_nsec;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        std::cerr << "Usage: ./test5-native <times> <twice>\n";
        return 1;
    }
    int n = atoi(argv[1]);
    bool twice = atoi(argv[2]);
    uint64_t t1 = time_nanosec();
    if (twice) {
        for (int i = 0; i < n; ++i) {
            int res = test_func2(i, i);
            struct timespec ts;
            clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts);
        }
    } else {
        for (int i = 0; i < n; ++i) {
            int res = test_func2(i, i);
        }
    }
    uint64_t t2 = time_nanosec();
    std::cout << (double)(t2 - t1) / n << std::endl;
    return 0;
}