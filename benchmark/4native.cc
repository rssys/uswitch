#include <iostream>
#include <fstream>
#include <ctime>
#include <cstdlib>
#include <cinttypes>
#include <cstring>

extern "C" int test_func1(int x, int y);

static uint64_t time_nanosec() {
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return t.tv_sec * 1000000000ull + t.tv_nsec;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        std::cerr << "Usage: ./test4-native <times>\n";
        return 1;
    }
    int n = atoi(argv[1]);
    uint64_t t1 = time_nanosec();
    for (int i = 0; i < n; ++i) {
        int res = test_func1(i, i);
    }
    uint64_t t2 = time_nanosec();
    std::cout << (double)(t2 - t1) / n << std::endl;
    return 0;
}