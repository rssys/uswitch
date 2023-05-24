#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "native_client/src/trusted/service_runtime/nacl_config.h"
#include "native_client/src/trusted/service_runtime/include/bits/nacl_syscalls.h"
#include "native_client/src/trusted/service_runtime/sel_rt.h"
#include "native_client/src/trusted/dyn_ldr/dyn_ldr_test_structs.h"

#define EXIT_FROM_MAIN 0
#define EXIT_FROM_CALL 1

typedef int32_t (*SandboxExitType)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);

typedef int32_t (*SandboxCallbackType)(uint32_t, nacl_reg_t*, uintptr_t);

void MakeNaClSysCall_exit_sandbox(uint32_t exitLocation, 
  uint32_t register_ret_bottom, uint32_t register_ret_top, 
  uint32_t register_float_ret_bottom, uint32_t register_float_ret_top
)
{
	((SandboxExitType)NACL_SYSCALL_ADDR(NACL_sys_exit_sandbox))(exitLocation, register_ret_bottom, register_ret_top, register_float_ret_bottom, register_float_ret_top);
}

//Specifically not making this a new function as this may add a new stack frame
#define MakeNaClSysCall_callback(slotNumber, parameterRegisters, floatRetPtr) ((SandboxCallbackType)NACL_SYSCALL_ADDR(NACL_sys_callback))(slotNumber, parameterRegisters, floatRetPtr)

void exitFunctionWrapper(void)
{
	// Note : On 32 bit platforms Windows, Linux/Unix NaCl always builds ELF32 following the cdecl  (not sure if other calling conventions are supported in NaCl 86. PNaCl only supports x86) 
	//        On 64 bit platforms Windows, Linux/Unix NaCl always builds ELF64 and the System V AMD64 ABI calling convention
	// This function needs to read return values from different registers based on the calling convension of the architecture
	// In 32 bit 
	// 		If the sandboxed function being called has the cdecl calling convention (not sure if other calling conventions are supported in NaCl 86. PNaCl only supports x86)
	//			- If the return value is 32 bits or smaller and is not a float or double or struct, the return value is stored in the eax register
	//			- If the return value is > 32 bits and <= 64 bits and is not a float or double or struct, the return value is split into the eax (lower bits) and edx(higher bits) register
	//			- If the return value is a struct, it is stored in space created the caller in the callers stack frame, the pointer to the space is the first arg to the callee, the returned value is the pointer to the free space and is returned to eax
	//			- If the return value is a float or double, the returned value is in x87 ST0 register
	//		If the sandboxed function being called has the C++ ABI, since the 2 supported NaCl compilers are gcc and clang, these use basically the same ABI (independent of platform - windows/linux/unix)
	//			- This is very similar to the C ABI, with classes treated as structs
	// In 64 bit
	// 		The sandboxed function being called has to have the System V AMD64 ABI calling convention
	//			- If the return value is 64 bits or smaller and is not a float or double, the return value is stored in the rax register
	//			- If the return value is > 64 bits and <= 128 bits and is not a float or double, the return value is split into the rax (lower bits) and edx(higher bits) register
	//			- If the return value is a struct > 128 bits, it is stored in space created the caller in the callers stack frame, the pointer to the space is the first arg to the callee, the returned value is the pointer to the free space and is returned to rax
	//			- If the return value is a float or double, the returned value is in XMM0 register
	//			- If the return value is a float256 bit and float512 bit, the returned value is in YMM0 and ZMM0
	//		If the sandboxed function being called has the C++ ABI, since the 2 supported NaCl compilers are gcc and clang, these use basically the same ABI (independent of platform - windows/linux/unix)
	//			- This is very similar to the C ABI, with classes treated as structs, except for classes < 128 bits with non trivial constructors or destructors - these go on the stack

	// Building a very small part of the full functionality here. For now we will just support the most common case
	// Return eax/rax as most api's just want to return raw integer values or pointers

	#if defined(_M_IX86) || defined(__i386__)

		uint64_t return_reg;
		uint64_t float_return_reg;
		asm("movl %%eax, %0;\n"
			"fstl %1;"
			:"=r"(return_reg), "=g"(float_return_reg)        /* output */
		);


	#elif defined(_M_X64) || defined(__x86_64__)

		uint64_t return_reg;
		uint64_t float_return_reg;
		asm("movq %%rax, %0;\n"
			"movq %%xmm0, %1;"
			:"=r"(return_reg), "=r"(float_return_reg)        /* output */
		);

	#elif defined(__ARMEL__) || defined(__MIPSEL__)
		#error Unsupported platform!
	#else 
		#error Unknown platform!
	#endif

	uint32_t return_reg_bottom = return_reg;
	uint32_t return_reg_top = return_reg >> 32;
	uint32_t float_return_reg_bottom = float_return_reg;
	uint32_t float_return_reg_top = float_return_reg >> 32;

	MakeNaClSysCall_exit_sandbox(EXIT_FROM_CALL, return_reg_bottom, return_reg_top, float_return_reg_bottom, float_return_reg_top);
}

#if defined(_M_IX86) || defined(__i386__)
	//for 32 bit the parameters are on the stack
	#define generateCallbackFunc(num) void callbackFunctionWrapper##num(void) { MakeNaClSysCall_callback(num, 0, 0); }
	#define generateCallbackFuncFloat(num)                                               \
	float callbackFunctionWrapper##num(void) {                                           \
		float ret = 0;                                                                   \
		MakeNaClSysCall_callback(num, 0, (uintptr_t) &ret);                              \
		return ret;                                                                      \
	}
#elif defined(_M_X64) || defined(__x86_64__)
	//for 64 bit the parameters are in registers, which will get overwritten, so we need to save it
	//nacl does not allow 64 bit parameters to trusted code calls, so we just save the values in an array and pass it out as a 64 bit pointer
	#define generateCallbackFunc(num) \
	void callbackFunctionWrapper##num(unsigned long p0, unsigned long p1, unsigned long p2, unsigned long p3, unsigned long p4, unsigned long p5) \
	{ \
			nacl_reg_t parameterRegisters[6];\
			parameterRegisters[0] = p0;\
			parameterRegisters[1] = p1;\
			parameterRegisters[2] = p2;\
			parameterRegisters[3] = p3;\
			parameterRegisters[4] = p4;\
			parameterRegisters[5] = p5;\
			MakeNaClSysCall_callback(num, parameterRegisters, 0);\
	}
	#define generateCallbackFuncFloat(num)                                                                                                              \
	float callbackFunctionWrapper##num(unsigned long p0, unsigned long p1, unsigned long p2, unsigned long p3, unsigned long p4, unsigned long p5)      \
	{                                                                                                                                                   \
			float ret = 0;                                                                                                                              \
			nacl_reg_t parameterRegisters[6];                                                                                                           \
			parameterRegisters[0] = p0;                                                                                                                 \
			parameterRegisters[1] = p1;                                                                                                                 \
			parameterRegisters[2] = p2;                                                                                                                 \
			parameterRegisters[3] = p3;                                                                                                                 \
			parameterRegisters[4] = p4;                                                                                                                 \
			parameterRegisters[5] = p5;                                                                                                                 \
			MakeNaClSysCall_callback(num, parameterRegisters, &ret);                                                                                    \
			return ret;                                                                                                                                 \
	}
#elif defined(__ARMEL__) || defined(__MIPSEL__)
	#error Unsupported platform!
#else
	#error Unknown platform!
#endif

generateCallbackFunc(0)
generateCallbackFunc(1) 
generateCallbackFunc(2) 
generateCallbackFunc(3) 
generateCallbackFunc(4) 
generateCallbackFunc(5) 
generateCallbackFunc(6) 
generateCallbackFuncFloat(7)

unsigned test_localMath(unsigned a, unsigned  b, unsigned c)
{
	unsigned ret;
	ret = (a*100) + (b * 10) + (c);
	return ret;
}

size_t test_localString(char* test)
{
	return strlen(test);
}

int test_checkStructSizes
(
	int size_DoubleAlign,
	int size_PointerSize,
	int size_IntSize,
	int size_LongSize,
	int size_LongLongSize
)
{
	int isEqual = 
		size_DoubleAlign == sizeof(struct TestStructDoubleAlign) &&
		size_PointerSize == sizeof(struct TestStructPointerSize) &&
		size_IntSize == sizeof(struct TestStructIntSize) &&
		size_LongSize == sizeof(struct TestStructLongSize) &&
		size_LongLongSize == sizeof(struct TestStructLongLongSize);
	return isEqual;
}

typedef void (*IdentifyCallbackHelperType) (uint32_t, uint32_t, uint32_t,
		                                    uint32_t, uint32_t, uint32_t,
		                                    uint32_t, uint32_t, uint32_t, uint32_t);
void identifyCallbackOffsetHelper(IdentifyCallbackHelperType callback)
{
	callback(1, 2, 3, 4, 5, 6, 7, 8, 9, 10);
}

int threadMain(void)
{
	MakeNaClSysCall_exit_sandbox(EXIT_FROM_MAIN, 
		0, 0, 0, 0 /* return values not used here */);
	return 0;
}

//fopen and fclose aren't used in this file
//so they are not included in the symbol table
//Small hack to ensure fopen and fclose remain in the symbol table
void* fopenCopy = (void *) fopen;
void* fcloseCopy = (void *) fclose;

int main(int argc, char** argv)
{
	MakeNaClSysCall_exit_sandbox(EXIT_FROM_MAIN, 
		0, 0, 0, 0 /* return values not used here */);
	return 0;
}
