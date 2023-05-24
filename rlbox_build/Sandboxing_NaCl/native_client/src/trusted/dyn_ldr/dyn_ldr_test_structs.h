#include <stdint.h>

struct TestStructDoubleAlign
{
	uint32_t force32Align;
	double check64Align;
	uint32_t afterDoubleField;
};

struct TestStructPointerSize
{
	uintptr_t pointerSize;
};

struct TestStructIntSize
{
	int intSize;
};

struct TestStructLongSize
{
	long longSize;
};

struct TestStructLongLongSize
{
	long long longLongSize;
};
