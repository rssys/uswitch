#include "ProcessSandbox.h"
#include "process_sandbox_cpp.h"
#include "test_dyn_lib.h"
#include "test_dyn_lib_helpers.h"

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

PROCESS_SANDBOX_CLASSNAME* sandbox;

unsigned long __attribute__ ((noinline)) unsandboxedSimpleAddNoPrintTest(unsigned long a, unsigned long b)
{
	return a + b;
}

inline unsigned long sandboxedSimpleAddNoPrintTest(unsigned long a, unsigned long b)
{
	return sandbox->inv_simpleAddNoPrintTest(a, b);
}

char* getExecFolder(const char* executablePath);
char* concatenateAndFixSlash(const char* string1, const char* string2);

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

int main(int argc, char** argv)
{
	/**************** Some calculations of relative paths ****************/
	char* execFolder;
	char* libraryPath;

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
		libraryPath = concatenateAndFixSlash(execFolder, "ProcessSandbox_otherside_test32");
	#elif defined(_M_X64) || defined(__x86_64__)
		libraryPath = concatenateAndFixSlash(execFolder, "ProcessSandbox_otherside_test64");
	#else
		#error Unknown platform!
	#endif

	printf("libraryPath: %s\n", libraryPath);

	/**************** Actual sandbox with dynamic lib test ****************/

	printf("Starting Dyn loader Benchmark\n");
	printf("------------------------------\n");

	printf("Memory Before Sandbox Creation\n");
	printf("------------------------------\n");
	printMemoryStatus();
	printf("------------------------------\n");

	if(!initializeDlSandboxCreator(0 /* Disable logging */))
	{
		printf("Dyn loader Benchmark: initializeDlSandboxCreator returned null\n");
		return 1;
	}

	{
		high_resolution_clock::time_point enterTime = high_resolution_clock::now();
		sandbox = createDlSandbox<PROCESS_SANDBOX_CLASSNAME>(libraryPath);
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

	printf("Sandbox created\n");
	printf("------------------------------\n");


	unsigned long ret1;
	uint64_t timeSpentInFunc;

	unsigned long ret2;
	uint64_t timeSpentInSandbox;

	unsigned long ret3;
	uint64_t timeSpentInSandboxCpp;

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
	timeSpentInSandboxCpp = 0;

	{
		//some warm up rounds
		high_resolution_clock::time_point enterTime = high_resolution_clock::now();
		ret1 += unsandboxedSimpleAddNoPrintTest(val1_1, val1_2);
		ret2 += sandboxedSimpleAddNoPrintTest(val1_1, val1_2);
		ret3 += sandbox_invoke(sandbox, simpleAddNoPrintTest, val1_1, val1_2).UNSAFE_noVerify();
		ret1 += unsandboxedSimpleAddNoPrintTest(val1_1, val1_2);
		ret2 += sandboxedSimpleAddNoPrintTest(val1_1, val1_2);
		ret3 += sandbox_invoke(sandbox, simpleAddNoPrintTest, val1_1, val1_2).UNSAFE_noVerify();
		ret1 += unsandboxedSimpleAddNoPrintTest(val1_1, val1_2);
		ret2 += sandboxedSimpleAddNoPrintTest(val1_1, val1_2);
		ret3 += sandbox_invoke(sandbox, simpleAddNoPrintTest, val1_1, val1_2).UNSAFE_noVerify();
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
		timeSpentInSandboxCpp = 0;

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
			ret3 += sandbox_invoke(sandbox, simpleAddNoPrintTest, val1_1, val1_2).UNSAFE_noVerify();
			high_resolution_clock::time_point exitTime = high_resolution_clock::now();
			timeSpentInSandboxCpp += duration_cast<nanoseconds>(exitTime  - enterTime).count();
		}

		if(ret1 != ret2 || ret2 != ret3)
		{
			printf("Return values don't agree\n");
			return 1;
		}

		printf("1 Function Call = %10" PRId64 
			", Sandbox Time = %10" PRId64 
			", Sandbox Time(C++) = %10" PRId64  " ns\n",
			timeSpentInFunc,
			timeSpentInSandbox,
			timeSpentInSandboxCpp);
	}


	/**************** Cleanup ****************/

	free(execFolder);
	free(libraryPath);

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