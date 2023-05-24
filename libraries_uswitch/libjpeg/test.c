#include <unistd.h>
#include <time.h>
#include <stdio.h>

int test_func1(int x, int y) {
    return x + y;
}

int test_func2(int x, int y) {
    struct timespec ts;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts);
    return x + y;
}

void test_func3() {
    printf("test");
}