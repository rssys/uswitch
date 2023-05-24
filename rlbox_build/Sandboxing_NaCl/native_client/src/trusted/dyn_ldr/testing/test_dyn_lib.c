#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "native_client/src/trusted/dyn_ldr/testing/test_dyn_lib.h"

unsigned long simpleAddNoPrintTest(unsigned long a, unsigned long b)
{
	return a + b;
}

int simpleAddTest(int a, int b)
{
	printf("simpleAddTest\n");
	fflush(stdout);
	return a + b;
}

size_t simpleStrLenTest(const char* str)
{
	printf("simpleStrLenTest\n");
	fflush(stdout);
	return strlen(str);
}

int simpleCallbackNoPrintTest(unsigned a, const char* b, CallbackType callback)
{
	int ret;
	ret = callback(a + 1, b, &a);
	return ret;
}


int simpleCallbackTest(unsigned a, const char* b, CallbackType callback)
{
	int ret;
	
	printf("simpleCallbackTest\n");
	fflush(stdout);

	ret = callback(a + 1, b, &a);
	return ret;
}

int simpleWriteToFileTest(FILE* file, const char* str)
{
	printf("simpleWriteToFileTest\n");
	fflush(stdout);
	return fputs(str, file);
}

char* simpleEchoTest(char * str)
{
	printf("simpleEchoTest\n");
	fflush(stdout);
	return str;
}

double simpleDoubleAddTest(const double a, const double b)
{
	printf("simpleDoubleAddTest\n");
	return a + b;
}

unsigned long simpleLongAddTest(unsigned long a, unsigned long b)
{
	printf("simpleLongAddTest\n");
	fflush(stdout);
	return a + b;
}

struct testStruct simpleTestStructVal()
{
	struct testStruct ret;
	ret.fieldLong = 7;
	ret.fieldString = "Hello";
	//explicitly mess up the top bits of the pointer. The sandbox checks outside the sandbox should catch this
	ret.fieldString = (char *)((((uintptr_t) ret.fieldString) & 0xFFFFFFFF) | 0x1234567800000000);
	ret.fieldBool = 1;
	strcpy(ret.fieldFixedArr, "Bye");
	return ret;
}

struct testStruct* simpleTestStructPtr()
{
	struct testStruct* ret = (struct testStruct*) malloc(sizeof(struct testStruct));
	ret->fieldLong = 7;
	ret->fieldString = "Hello";
	//explicitly mess up the top bits of the pointer. The sandbox checks outside the sandbox should catch this
	ret->fieldString = (char *)((((uintptr_t) ret->fieldString) & 0xFFFFFFFF) | 0x1234567800000000);
	ret->fieldBool = 1;
	strcpy(ret->fieldFixedArr, "Bye");
	return ret;
}

int* echoPointer(int* pointer)
{
	return pointer;
}
