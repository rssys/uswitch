#include <stdint.h>
#include <stdbool.h>

// Interface of our sample dummy library
int APIFuncZero(int a, int b);
float APIFuncOne(unsigned char c);
void* APIFuncTwo(intptr_t a, intptr_t b);
bool APIFuncThree(int* arr);
int APIFuncFour(int i, int (*cb)(int));
void APIFuncFive(char c, void (*cb)(char, char));
void* APIFuncSix(void* (*cb)(int*));
int APIFuncSeven(int a, int b);