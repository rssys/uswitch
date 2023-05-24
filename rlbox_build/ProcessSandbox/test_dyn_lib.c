#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "test_dyn_lib.h"

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

size_t simpleStrLenTest(char* str)
{
	printf("simpleStrLenTest\n");
	fflush(stdout);
	return strlen(str);
}


int simpleCallbackTest(unsigned a, char* b, CallbackType callback)
{
	int ret;
	
	printf("simpleCallbackTest\n");
	fflush(stdout);

	ret = callback(a + 1, b, &a);
	return ret;
}

int simpleWriteToFileTest(FILE* file, char* str)
{
	printf("simpleWriteToFileTest\n");
	fflush(stdout);
	return fputs(str, file);
}

char* simpleEchoTest(char * str)
{
	printf("simpleEchoTest\n");
	fflush(stdout);
	//explicitly mess up the top bits of the pointer. The sandbox checks outside the sandbox should catch this
	char* ret = (char *)((((uintptr_t) str) & 0xFFFFFFFF) | 0x1234567800000000);
	return ret;
}

double simpleDoubleAddTest(double a, double b)
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
	ret.fieldString = (char *) malloc(10);
	strcpy((char*) ret.fieldString, "Hello");
	char* tempOld = ret.fieldString;
	//explicitly mess up the top bits of the pointer. The sandbox checks outside the sandbox should catch this
	ret.fieldString = (char *)((((uintptr_t) ret.fieldString) & 0xFFFFFFFF) | 0x1234567800000000);
	char* tempNew = ret.fieldString;
	printf("String old: %p, new: %p\n", tempOld, tempNew);
	ret.fieldBool = 1;
	strcpy(ret.fieldFixedArr, "Bye");
	printf("String field: %s\n", ret.fieldFixedArr);
	return ret;
}

struct testStruct* simpleTestStructPtr()
{
	struct testStruct* ret = (struct testStruct*) malloc(sizeof(struct testStruct));
	ret->fieldLong = 7;
	ret->fieldString = (char *) malloc(10);
	strcpy((char*) ret->fieldString, "Hello");
	//explicitly mess up the top bits of the pointer. The sandbox checks outside the sandbox should catch this
	ret->fieldString = (char *)((((uintptr_t) ret->fieldString) & 0xFFFFFFFF) | 0x1234567800000000);
	ret->fieldBool = 1;
	strcpy(ret->fieldFixedArr, "Bye");
	return ret;
}

struct testStruct simpleTestStructValOnePar(int unused)
{
	return simpleTestStructVal();
}
struct testStruct* simpleTestStructPtrOnePar(int unused)
{
	return simpleTestStructPtr();
}

int* echoPointer(int* pointer)
{
	return pointer;
}
