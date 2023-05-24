#include "library.h"
#include <math.h>
#include <time.h>

__attribute__((noinline)) int APIFuncZero(int a, int b) {
  return a+b;
}

__attribute__((noinline)) float APIFuncOne(unsigned char c) {
  return sqrt((float)c*1000.0);
}

__attribute__((noinline)) void* APIFuncTwo(intptr_t a, intptr_t b) {
  return (void*)(a+2*b);
}

__attribute__((noinline)) bool APIFuncThree(int* arr) {
  arr[2] = arr[0];
  return arr[0] == arr[1];
}

__attribute__((noinline)) int APIFuncFour(int i, int (*cb)(int)) {
  return cb(i) + 23;
}

__attribute__((noinline)) void APIFuncFive(char c, void (*cb)(char, char)) {
  cb(c, c+2);
}

__attribute__((noinline)) void* APIFuncSix(void* (*cb)(int*)) {
  return cb((int*)321);
}

__attribute__((noinline)) int APIFuncSeven(int a, int b) {
  struct timespec ts;
  clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts);
  return a+b;
}