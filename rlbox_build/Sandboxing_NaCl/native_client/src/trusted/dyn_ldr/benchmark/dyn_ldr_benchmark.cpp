#if !defined(_M_X64) && !defined(__x86_64__)
	//C++ API not supported for 32 bit mode
	int main(){
		return 0;
	}
#else
#include "native_client/src/trusted/dyn_ldr/nacl_sandbox.h"
#include "native_client/src/trusted/dyn_ldr/testing/test_dyn_lib.h"

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <time.h>
#include <chrono>
using namespace std::chrono;

#if defined(_WIN32)
	char SEPARATOR = '\\';
#else
	char SEPARATOR = '/';
#endif

NaClSandbox* sandbox;
void* simpleAddNoPrintTestPtr;
void* simpleCallbackTestPtr;

unsigned long __attribute__ ((noinline)) unsandboxedSimpleAddNoPrintTest(unsigned long a, unsigned long b)
{
	return a + b;
}

unsigned long sandboxedSimpleAddNoPrintTest(unsigned long a, unsigned long b)
{
  NaClSandbox_Thread* threadData = preFunctionCall(sandbox, sizeof(a) + sizeof(b), 0);

  PUSH_VAL_TO_STACK(threadData, unsigned long, a);
  PUSH_VAL_TO_STACK(threadData, unsigned long, b);

  invokeFunctionCall(threadData, simpleAddNoPrintTestPtr);

  return (unsigned long)functionCallReturnRawPrimitiveInt(threadData);
}

SANDBOX_CALLBACK unsigned invokeSimpleCallbackTest_callbackStub(uintptr_t sandboxPtr)
{
	int a;
	char* b;
	unsigned* c;
	int ret;

	NaClSandbox* sandbox = (NaClSandbox*) sandboxPtr;
	NaClSandbox_Thread* threadData = callbackParamsBegin(sandbox);

	a = COMPLETELY_UNTRUSTED_CALLBACK_STACK_PARAM(threadData, int);
	b = COMPLETELY_UNTRUSTED_CALLBACK_PTR_PARAM(threadData, char*);
	c = COMPLETELY_UNTRUSTED_CALLBACK_PTR_PARAM(threadData, unsigned*);

	(void)c;
	ret = a + strlen(b);
	return ret;
}

int invokeSimpleCallbackTest(NaClSandbox* sandbox, void* simpleCallbackTestPtr, unsigned a, char* b, uintptr_t callback)
{
	int ret;
	NaClSandbox_Thread* threadData;

	threadData = preFunctionCall(sandbox, sizeof(a) + sizeof(b) + sizeof(callback), 0 /* size of any arrays being pushed on the stack */);

	PUSH_VAL_TO_STACK(threadData, unsigned, a);
	PUSH_STRING_TO_STACK(threadData, b);
	PUSH_VAL_TO_STACK(threadData, uintptr_t, callback);

	invokeFunctionCall(threadData, simpleCallbackTestPtr);

	ret = (int) functionCallReturnRawPrimitiveInt(threadData);

	return ret;
}

SANDBOX_CALLBACK int unsandboxedSimpleCallbackTest_callbackStub(unsigned int a, const char* b, unsigned int c[1])
{
	int ret;
	(void)c;
	ret = a + strlen(b);
	return ret;
}

int unsandboxedSimpleCallbackNoPrintTest(unsigned a, const char* b, CallbackType callback)
{
	int ret;
	ret = callback(a + 1, b, &a);
	return ret;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//https://stackoverflow.com/questions/1558402/memory-usage-of-current-process-in-c

void printMemoryStatus()
{
  const char* statm_path = "/proc/self/statm";

  FILE *f = fopen(statm_path,"r");
  if(!f){
    perror(statm_path);
    abort();
  }

  unsigned long size,resident,share,text,lib,data,dt;

  if(7 != fscanf(f,"%lu %lu %lu %lu %lu %lu %lu",
    &size,&resident,&share,&text,&lib,&data,&dt))
  {
    perror(statm_path);
    abort();
  }
  fclose(f);

  printf("size:%lu\nresident:%lu\nshared pages:%lu\ntext:%lu\nlib:%lu\ndata+stack:%lu\ndirty pages:%lu\n", 
	size, resident, share, text, lib, data, dt
  );
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

char* getExecFolder(const char* executablePath);
char* concatenateAndFixSlash(const char* string1, const char* string2);

int main(int argc, char** argv)
{
	/**************** Some calculations of relative paths ****************/
	char* execFolder;
	char* libraryPath;
	char* libraryToLoad;
	uintptr_t registeredCallback;

	if(argc < 1)
	{
		printf("Argv not filled correctly");
		return 1;
	}
	
	printf("Warmup on timer. \n");
	for(int count = 0; count < 10; count++)
	{
		high_resolution_clock::time_point enterTime = high_resolution_clock::now();
		high_resolution_clock::time_point exitTime = high_resolution_clock::now();
		printf("Warm up for = %10" PRId64 " ns\n", duration_cast<nanoseconds>(exitTime - enterTime).count());
		printf("------------------------------\n");
	}
	printf("\n");

	//exec folder is something like: "native_client/src/trusted/dyn_ldr/benchmark/"
	execFolder = getExecFolder(argv[0]);

	#if defined(_M_IX86) || defined(__i386__)
		//libraryPath is something like: "/home/shr/Code/nacl2/native_client/scons-out/nacl_irt-x86-32/staging/irt_core.nexe"
		libraryPath = concatenateAndFixSlash(execFolder, "../../../scons-out/nacl_irt-x86-32/staging/irt_core.nexe");
		//libraryToLoad is something like: "/home/shr/Code/nacl2/native_client/scons-out/nacl-x86-32/staging/test_dyn_lib.nexe"
		libraryToLoad = concatenateAndFixSlash(execFolder, "../../../scons-out/nacl-x86-32/staging/test_dyn_lib.nexe");
	#elif defined(_M_X64) || defined(__x86_64__)
		//libraryPath is something like: "/home/shr/Code/nacl2/native_client/scons-out/nacl_irt-x86-64/staging/irt_core.nexe"
		libraryPath = concatenateAndFixSlash(execFolder, "../../../scons-out/nacl_irt-x86-64/staging/irt_core.nexe");
		//libraryToLoad is something like: "/home/shr/Code/nacl2/native_client/scons-out/nacl-x86-64/staging/test_dyn_lib.nexe"
		libraryToLoad = concatenateAndFixSlash(execFolder, "../../../scons-out/nacl-x86-64/staging/test_dyn_lib.nexe");
	#else
		#error Unknown platform!
	#endif

	printf("libraryPath: %s\n", libraryPath);
	printf("libraryToLoad: %s\n", libraryToLoad);

	/**************** Actual sandbox with dynamic lib test ****************/

	printf("Starting Dyn loader Benchmark\n");
	printf("------------------------------\n");

	printf("Memory Before Sandbox Creation\n");
	printf("------------------------------\n");
	printMemoryStatus();
	printf("------------------------------\n");

	
	{
		high_resolution_clock::time_point enterTime = high_resolution_clock::now();
		if(!initializeDlSandboxCreator(0 /* Disable logging */))
		{
			printf("Dyn loader Benchmark: initializeDlSandboxCreator returned null\n");
			return 1;
		}

		sandbox = createDlSandbox(libraryPath, libraryToLoad);
		high_resolution_clock::time_point exitTime = high_resolution_clock::now();
		uint64_t timeSpentInSandboxCpp = duration_cast<nanoseconds>(exitTime  - enterTime).count();
		printf("Sandbox create time  = %10" PRId64 " ns\n", timeSpentInSandboxCpp);
		printf("------------------------------\n");

	}	
	printf("Memory After Sandbox Creation\n");
	printf("------------------------------\n");
	printMemoryStatus();
	printf("------------------------------\n");

	if(sandbox == NULL)
	{
		printf("Dyn loader Benchmark: createDlSandbox returned null\n");
		return 1;
	}

	initCPPApi(sandbox);

	simpleAddNoPrintTestPtr = symbolTableLookupInSandbox(sandbox, "simpleAddNoPrintTest");
	simpleCallbackTestPtr = symbolTableLookupInSandbox(sandbox, "simpleCallbackNoPrintTest");
	registeredCallback = registerSandboxCallback(sandbox, 0, (uintptr_t) invokeSimpleCallbackTest_callbackStub);

	printf("Sandbox created\n");
	printf("------------------------------\n");


	unsigned long ret1;
	uint64_t timeSpentInFunc;

	unsigned long ret2;
	uint64_t timeSpentInSandbox;

	unsigned long ret3;
	uint64_t timeSpentInSandboxCppNoSymRes;

	unsigned long ret4;
	uint64_t timeSpentInSandboxCpp;

	unsigned long ret5;
	uint64_t timeSpentInCb;

	unsigned long ret6;
	uint64_t timeSpentInSandboxCb;

	srand(time(NULL));
	unsigned long val1_1;
	unsigned long val1_2;

	val1_1 = rand();
	val1_2 = rand();

	ret1 = 0;
	timeSpentInFunc = 0;
	ret2 = 0;
	timeSpentInSandbox = 0;
	ret3 = 0;
	timeSpentInSandboxCppNoSymRes = 0;
	ret4 = 0;
	timeSpentInSandboxCpp = 0;

	{
		//some warm up rounds
		high_resolution_clock::time_point enterTime = high_resolution_clock::now();
		ret1 += unsandboxedSimpleAddNoPrintTest(val1_1, val1_2);
		ret2 += sandboxedSimpleAddNoPrintTest(val1_1, val1_2);
		ret3 += sandbox_invoke_with_ptr(sandbox, (decltype(simpleAddNoPrintTest)*)simpleAddNoPrintTestPtr, val1_1, val1_2).UNSAFE_noVerify();
		ret4 += sandbox_invoke(sandbox, simpleAddNoPrintTest, val1_1, val1_2).UNSAFE_noVerify();
		ret1 += unsandboxedSimpleAddNoPrintTest(val1_1, val1_2);
		ret2 += sandboxedSimpleAddNoPrintTest(val1_1, val1_2);
		ret3 += sandbox_invoke_with_ptr(sandbox, (decltype(simpleAddNoPrintTest)*)simpleAddNoPrintTestPtr, val1_1, val1_2).UNSAFE_noVerify();
		ret4 += sandbox_invoke(sandbox, simpleAddNoPrintTest, val1_1, val1_2).UNSAFE_noVerify();
		ret1 += unsandboxedSimpleAddNoPrintTest(val1_1, val1_2);
		ret2 += sandboxedSimpleAddNoPrintTest(val1_1, val1_2);
		ret3 += sandbox_invoke_with_ptr(sandbox, (decltype(simpleAddNoPrintTest)*)simpleAddNoPrintTestPtr, val1_1, val1_2).UNSAFE_noVerify();
		ret4 += sandbox_invoke(sandbox, simpleAddNoPrintTest, val1_1, val1_2).UNSAFE_noVerify();
		high_resolution_clock::time_point exitTime = high_resolution_clock::now();
		printf("Warm up for = %10" PRId64 " ns\n", duration_cast<nanoseconds>(exitTime - enterTime).count());
		printf("------------------------------\n");
	}

	///////////////////////////////////////////////////////////////////////////////////////////////////////

	for (int i = 0; i < 10; ++i)
	{
		val1_1 = rand();
		val1_2 = rand();

		ret1 = 0;
		timeSpentInFunc = 0;
		ret2 = 0;
		timeSpentInSandbox = 0;
		ret3 = 0;
		timeSpentInSandboxCppNoSymRes = 0;
		ret4 = 0;
		timeSpentInSandboxCpp = 0;
		ret5 = 0;
		timeSpentInCb = 0;
		ret6 = 0;
		timeSpentInSandboxCb = 0;

		{
			high_resolution_clock::time_point enterTime = high_resolution_clock::now();
			ret1 += unsandboxedSimpleAddNoPrintTest(val1_1, val1_2);
			high_resolution_clock::time_point exitTime = high_resolution_clock::now();
			timeSpentInFunc += duration_cast<nanoseconds>(exitTime - enterTime).count();
		}

		{
			high_resolution_clock::time_point enterTime = high_resolution_clock::now();
			ret2 += sandboxedSimpleAddNoPrintTest(val1_1, val1_2);
			high_resolution_clock::time_point exitTime = high_resolution_clock::now();
			timeSpentInSandbox += duration_cast<nanoseconds>(exitTime  - enterTime).count();
		}

		{
			high_resolution_clock::time_point enterTime = high_resolution_clock::now();
			ret3 += sandbox_invoke_with_ptr(sandbox, (decltype(simpleAddNoPrintTest)*)simpleAddNoPrintTestPtr, val1_1, val1_2).UNSAFE_noVerify();
			high_resolution_clock::time_point exitTime = high_resolution_clock::now();
			timeSpentInSandboxCppNoSymRes += duration_cast<nanoseconds>(exitTime  - enterTime).count();
		}

		{
			high_resolution_clock::time_point enterTime = high_resolution_clock::now();
			ret4 += sandbox_invoke(sandbox, simpleAddNoPrintTest, val1_1, val1_2).UNSAFE_noVerify();
			high_resolution_clock::time_point exitTime = high_resolution_clock::now();
			timeSpentInSandboxCpp += duration_cast<nanoseconds>(exitTime  - enterTime).count();
		}

		{
			high_resolution_clock::time_point enterTime = high_resolution_clock::now();
			ret5 += unsandboxedSimpleCallbackNoPrintTest(4, "Hello", unsandboxedSimpleCallbackTest_callbackStub);
			high_resolution_clock::time_point exitTime = high_resolution_clock::now();
			timeSpentInCb += duration_cast<nanoseconds>(exitTime  - enterTime).count();
		}

		{
			high_resolution_clock::time_point enterTime = high_resolution_clock::now();
			ret6 += invokeSimpleCallbackTest(sandbox, simpleCallbackTestPtr, 4, "Hello", registeredCallback);
			high_resolution_clock::time_point exitTime = high_resolution_clock::now();
			timeSpentInSandboxCb += duration_cast<nanoseconds>(exitTime  - enterTime).count();
		}

		if(ret1 != ret2 || ret2 != ret3 || ret3 != ret4 || ret5 != ret6)
		{
			printf("Return values don't agree\n");
			return 1;
		}

		printf("Func Call = %10" PRId64
			", Sandbox Func Call = %10" PRId64
			// ", Sandbox Func Call(C++, no symbol res) = %10" PRId64
			// ", Sandbox Func Call(C++) = %10" PRId64  " ns"
			", Callback = %10" PRId64 " ns"
			", Sandbox Callback = %10" PRId64  " ns\n"
			,
			timeSpentInFunc,
			timeSpentInSandbox,
			// timeSpentInSandboxCppNoSymRes,
			// timeSpentInSandboxCpp,
			timeSpentInCb,
			timeSpentInSandboxCb
		);
	}


	/**************** Cleanup ****************/

	free(execFolder);
	free(libraryPath);
	free(libraryToLoad);

	return 0;
}

/**************** Path Helpers ****************/

int lastIndexOf(const char * s, char target)
{
	 int ret = -1;
	 int curIdx = 0;
	 while(s[curIdx] != '\0')
	 {
	    if (s[curIdx] == target) { ret = curIdx; }
	    curIdx++;
	 }
	 return ret;
}

void replaceChar(char* str, char toReplace, char replaceWith)
{
	if(toReplace == replaceWith)
	{
		return;
	}

	while(*str != '\0')
	{
		if(*str == toReplace) { *str = replaceWith; }
		str++;
	}
}

char* getExecFolder(const char* executablePath)
{
	int index;
	char* execFolder;

	index = lastIndexOf(executablePath, SEPARATOR);

	if(index < 0)
	{
		execFolder = (char*)malloc(4);
		execFolder[0] = '.';
		execFolder[1] = SEPARATOR;
		execFolder[2] = '\0';
	}
	else
	{
		size_t len = strlen(executablePath);
		execFolder = (char*)malloc(len + 2);
		strcpy(execFolder, executablePath);

		if((size_t)index < len)
		{
			execFolder[index + 1] = '\0';
		}
	}

	return execFolder;
}

char* concatenateAndFixSlash(const char* string1, const char* string2)
{
	char* ret;

	ret = (char*)malloc(strlen(string1) + strlen(string2) + 2);
	strcpy(ret, string1);
	strcat(ret, string2);

	replaceChar(ret, '/', SEPARATOR);
	return ret;
}
#endif