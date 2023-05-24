#pragma once
#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

typedef int (*CallbackType)(unsigned, char*, unsigned[1]);

struct testStruct
{
	unsigned long fieldLong;
	char* fieldString;
	unsigned int fieldBool; 
	char fieldFixedArr[8];
	int (*fieldFnPtr)(unsigned, char*, unsigned[1]);
};

unsigned long simpleAddNoPrintTest(unsigned long a, unsigned long b);
int simpleAddTest(int a, int b);
size_t simpleStrLenTest(char* str);
int simpleCallbackTest(unsigned a, char* b, CallbackType callback);
int simpleWriteToFileTest(FILE* file, char* str);
char* simpleEchoTest(char * str);
double simpleDoubleAddTest(double a, double b);
unsigned long simpleLongAddTest(unsigned long a, unsigned long b);
struct testStruct simpleTestStructVal();
struct testStruct* simpleTestStructPtr();
struct testStruct simpleTestStructValOnePar(int unused);
struct testStruct* simpleTestStructPtrOnePar(int unused);
int* echoPointer(int* pointer);

#ifdef __cplusplus
}
#endif