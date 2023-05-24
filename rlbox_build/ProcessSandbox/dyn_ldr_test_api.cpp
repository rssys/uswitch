#include <stdio.h>
#include <dlfcn.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <memory>
#include "ProcessSandbox.h"
#include "process_sandbox_cpp.h"
#include "test_dyn_lib.h"
#include "test_dyn_lib_helpers.h"

//////////////////////////////////////////////////////////////////

#define sandbox_fields_reflection_exampleId_class_testStruct(f, g) \
	f(unsigned long, fieldLong) \
	g() \
	f(char*, fieldString) \
	g() \
	f(unsigned int, fieldBool) \
	g() \
	f(char[8], fieldFixedArr) \
	g() \
	f(int (*)(unsigned, char*, unsigned[1]), fieldFnPtr) \
	g()

#define sandbox_fields_reflection_exampleId_allClasses(f) \
	f(testStruct, exampleId)

sandbox_nacl_load_library_api(exampleId)

//////////////////////////////////////////////////////////////////

char SEPARATOR = '/';
#include <pthread.h>

int invokeSimpleCallbackTest_callback(unverified_data<unsigned> a, unverified_data<char*> b, unverified_data<unsigned[1]> c)
{
	auto aCopy = a.sandbox_copyAndVerify([](unsigned val){ return val > 0 && val < 100;}, -1);
	auto bCopy = b.sandbox_copyAndVerifyString([](const char* val) { return strlen(val) < 100; }, nullptr);
	unsigned cCopy[1];
	c.sandbox_copyAndVerify(cCopy, sizeof(cCopy), [](unsigned* arr, size_t arrSize){ UNUSED(arrSize); unsigned val = *arr; return val > 0 && val < 100; });
	if(cCopy[0] + 1 != aCopy)
	{
		printf("Unexpected callback value: %d, %d\n", cCopy[0] + 1, aCopy);
		exit(1);
	}
	return aCopy + strlen(bCopy);
}

//////////////////////////////////////////////////////////////////

//Test member initialization
class TestInit
{
	unverified_data<int*> memberPtr;
	unverified_data<int> memberInt;
	TestInit() : memberPtr(nullptr), memberInt(0){}
};

//////////////////////////////////////////////////////////////////

// int fileTestPassed(PROCESS_SANDBOX_CLASSNAME* sandbox)
// {
// 	FILE* file = fopenInSandbox(sandbox, "test123.txt", "w");

// 	if(!file)
// 	{
// 		printf("File open failed\n");
// 		return 0;
// 	}

// 	sandbox_invoke(sandbox, simpleWriteToFileTest, file, sandbox_stackarr("test123"));

// 	if(fcloseInSandbox(sandbox, file))
// 	{
// 		printf("File close failed\n");
// 		return 0;
// 	}

// 	return 1;
// }

//////////////////////////////////////////////////////////////////

int invokeSimpleEchoTestPassed(PROCESS_SANDBOX_CLASSNAME* sandbox, const char* str)
{
	char* strInSandbox;
	char* retStr;
	int ret;

	//str is allocated in our heap, not the sandbox's heap
	if(!isAddressInNonSandboxMemoryOrNull(sandbox, (uintptr_t) str))
	{
		return 0;
	}

	unverified_data<char*> temp = newInSandbox<char>(sandbox, strlen(str) + 1);
	strInSandbox = temp.sandbox_onlyVerifyAddress();
	strcpy(strInSandbox, str);

	//str is allocated in sandbox heap, not our heap
	if(!isAddressInSandboxMemoryOrNull(sandbox, (uintptr_t) strInSandbox))
	{
		printf("String not in sandbox\n");
		return 0;
	}

	auto retStrRaw = sandbox_invoke(sandbox, simpleEchoTest, strInSandbox);
	*retStrRaw = 'g';
	*retStrRaw = 'H';
	retStr = retStrRaw.sandbox_copyAndVerifyString([](char* val) { return strlen(val) < 100; }, nullptr);
	printf("RetStr: %s\n", retStr);

	//retStr.field is a copy on our heap
	if(isAddressInSandboxMemoryOrNull(sandbox, (uintptr_t) retStr))
	{
		printf("String in sandbox\n");
		return 0;
	}

	ret = strcmp(str, retStr) == 0;

	freeInSandbox(sandbox, strInSandbox);

	return ret;
}

//////////////////////////////////////////////////////////////////

/**************** Main function ****************/

char* getExecFolder(char* executablePath);
char* concatenateAndFixSlash(const char* string1, const char* string2);

struct runTestParams
{
	PROCESS_SANDBOX_CLASSNAME* sandbox;
	int testResult;
	std::shared_ptr<sandbox_callback_helper<PROCESS_SANDBOX_CLASSNAME, int(unverified_data<unsigned>, unverified_data<char*>, unverified_data<unsigned[1]>)>> registeredCallback;

	//for multi threaded test only
	pthread_t newThread;
};


void* runTests(void* runTestParamsPtr)
{
	struct runTestParams* testParams = (struct runTestParams*) runTestParamsPtr;
	PROCESS_SANDBOX_CLASSNAME* sandbox = testParams->sandbox;

	int* testResult = &(testParams->testResult);
	*testResult = -1;


	auto result1 = sandbox_invoke(sandbox, simpleAddTest, 2, 3)
		.sandbox_copyAndVerify([](int val){ return val > 0 && val < 10;}, -1);
	if(result1 != 5)
	{
		printf("Dyn loader Test 1: Failed\n");
		*testResult = 0;
		return NULL;
	}

	auto result2 = sandbox_invoke(sandbox, simpleStrLenTest, sandbox_stackarr((char *)"Hello"))
		.sandbox_copyAndVerify([](size_t val) -> size_t { return (val <= 0 || val >= 10)? -1 : val; });
	if(result2 != 5)
	{
		printf("Dyn loader Test 2: Failed %d \n", (int)result2);
		*testResult = 0;
		return NULL;
	}

	{
		auto sharedHeapPtr = std::shared_ptr<sandbox_heaparr_helper<char>>(sandbox_heaparr(sandbox, (char *)"Hello"));

		auto result3 = sandbox_invoke(sandbox, simpleStrLenTest, sharedHeapPtr.get())
			.sandbox_copyAndVerify([](size_t val) -> size_t { return (val <= 0 || val >= 10)? -1 : val; });
		if(result3 != 5)
		{
			printf("Dyn loader Test 3: Failed\n");
			*testResult = 0;
			return NULL;
		}
	}

	auto result4 = sandbox_invoke(sandbox, simpleCallbackTest, (unsigned) 4, sandbox_stackarr((char*)"Hello"), testParams->registeredCallback.get())
		.sandbox_copyAndVerify([](int val){ return val > 0 && val < 100;}, -1);
	if(result4 != 10)
	{
		printf("Dyn loader Test 4: Failed\n");
		*testResult = 0;
		return NULL;
	}

	// if(!fileTestPassed(sandbox))
	// {
	// 	printf("Dyn loader Test 5: Failed\n");
	// 	*testResult = 0;
	// 	return NULL;
	// }

	if(!invokeSimpleEchoTestPassed(sandbox, "Hello"))
	{
		printf("Dyn loader Test 6: Failed\n");
		*testResult = 0;
		return NULL;
	}

	auto result7 = sandbox_invoke(sandbox, simpleDoubleAddTest, 1.0, 2.0)
		.sandbox_copyAndVerify([](double val){ return val > 0 && val < 100;}, -1.0);
	if(result7 != 3.0)
	{
		printf("Dyn loader Test 7: Failed\n");
		*testResult = 0;
		return NULL;
	}

	auto result8 = sandbox_invoke(sandbox, simpleLongAddTest, ULONG_MAX - 1ul, 1ul)
		.sandbox_copyAndVerify([](unsigned long val){ return val > 100;}, 0);
	if(result8 != ULONG_MAX)
	{
		printf("Dyn loader Test 8: Failed\n");
		*testResult = 0;
		return NULL;
	}

	// //////////////////////////////////////////////////////////////////

	auto result9T = sandbox_invoke(sandbox, simpleTestStructValOnePar, 0);
	auto result9 = result9T
		.sandbox_copyAndVerify([](unverified_data<testStruct>& val){ 
			testStruct ret;
			ret.fieldLong = val.fieldLong.sandbox_copyAndVerify([](unsigned long val) { return val; });
			ret.fieldString = val.fieldString.sandbox_copyAndVerifyString([](const char* val) { return strlen(val) < 100; }, nullptr);
			ret.fieldBool = val.fieldBool.sandbox_copyAndVerify([](unsigned int val) { return val; });
			val.fieldFixedArr.sandbox_copyAndVerify(ret.fieldFixedArr, sizeof(ret.fieldFixedArr), [](char* arr, size_t size){ UNUSED(arr); UNUSED(size); return true; });
			return ret; 
		});
	if(result9.fieldLong != 7 || strcmp(result9.fieldString, "Hello") != 0 || result9.fieldBool != 1 || strcmp(result9.fieldFixedArr, "Bye"))
	{
		printf("Dyn loader Test 9: Failed\n");
		*testResult = 0;
		return NULL;
	}

	//writes should still go through
	result9T.fieldLong = 17;
	long val = result9T.fieldLong.sandbox_copyAndVerify([](unsigned long val) { return val; });
	if(val != 17)
	{
		printf("Dyn loader Test 9.1: Failed\n");
		*testResult = 0;
		return NULL;
	}

	//////////////////////////////////////////////////////////////////
	auto result10T = sandbox_invoke(sandbox, simpleTestStructPtrOnePar, 0);
	auto result10 = result10T
		.sandbox_copyAndVerify([](sandbox_unverified_data<testStruct>* val) { 
			testStruct ret;
			ret.fieldLong = val->fieldLong.sandbox_copyAndVerify([](unsigned long val) { return val; });
			ret.fieldString = val->fieldString.sandbox_copyAndVerifyString([](const char* val) { return strlen(val) < 100; }, nullptr);
			ret.fieldBool = val->fieldBool.sandbox_copyAndVerify([](unsigned int val) { return val; });
			val->fieldFixedArr.sandbox_copyAndVerify(ret.fieldFixedArr, sizeof(ret.fieldFixedArr), [](char* arr, size_t size){ UNUSED(arr); UNUSED(size); return true; });
			return ret; 
		});
	if(result10.fieldLong != 7 || strcmp(result10.fieldString, "Hello") != 0 || result10.fieldBool != 1|| strcmp(result10.fieldFixedArr, "Bye"))
	{
		printf("Dyn loader Test 10: Failed\n");
		*testResult = 0;
		return NULL;
	}

	result10T->fieldFnPtr = nullptr;
	//writes should still go through
	result10T->fieldLong = 17;
	//writes of callback functions should check parameters
	result10T->fieldFnPtr = testParams->registeredCallback.get();

	long val2 = result10T->fieldLong.sandbox_copyAndVerify([](unsigned long val) { return val; });
	if(val2 != 17)
	{
		printf("Dyn loader Test 10.1: Failed\n");
		*testResult = 0;
		return NULL;
	}

	//test & and * operators
	unsigned long result11 = (*&result10T->fieldLong).sandbox_copyAndVerify([](unsigned long val) { return val; });

	if(result11 != 17)
	{
		printf("Dyn loader Test 11: Failed\n");
		*testResult = 0;
		return NULL;
	}


	// //////////////////////////////////////////////////////////////////
	int* ptr = (int *)(uintptr_t) 0x1234567812345678;
	int* result12 = sandbox_invoke_ret_unsandboxed_ptr(sandbox, echoPointer, sandbox_unsandboxed_ptr(ptr));

	if(result12 != ptr)
	{
		printf("Dyn loader Test 12: Failed\n");
		*testResult = 0;
		return NULL;
	}

	// //////////////////////////////////////////////////////////////////
	auto tempValPtr = newInSandbox<int>(sandbox);
	*tempValPtr = 3;
	auto result13T = sandbox_invoke(sandbox, echoPointer, tempValPtr);
	//make sure null checks go through

	if(result13T == nullptr)
	{
		printf("Dyn loader Test 13: Failed\n");
		*testResult = 0;
		return NULL;
	}

	auto result13 = result13T.sandbox_copyAndVerify([](int* val) -> int { return *val; });

	if(result13 != 3)
	{
		printf("Dyn loader Test 13.1: Failed\n");
		*testResult = 0;
		return NULL;
	}

	//capture something to test stateful lambdas
	int result14 = tempValPtr->sandbox_copyAndVerify([&tempValPtr](int val) { return val; });

	if(result14 != 3)
	{
		printf("Dyn loader Test 14: Failed\n");
		*testResult = 0;
		return NULL;
	}

	*testResult = 1;
	return NULL;
}

#define ThreadsToTest 4

void runSingleThreadedTest(struct runTestParams testParams)
{
	runTests((void *) &testParams);
	if(testParams.testResult != 1)
	{
		printf("Main thread tests failed\n");
		exit(1);
	}

	printf("Main thread tests successful\n");
}

void runMultiThreadedTest(struct runTestParams * testParams, unsigned threadCount)
{
	for(unsigned i = 0; i < threadCount; i++)
	{
		if(pthread_create(&(testParams[i].newThread), NULL /* use default thread attributes */, runTests, (void *) &(testParams[i]) /* parameter */))
		{
			printf("Error creating thread %d\n", i);
			exit(1);
		}
	}
}

void checkMultiThreadedTest(struct runTestParams * testParams, unsigned threadCount)
{
	for(unsigned i = 0; i < threadCount; i++)
	{
		if(pthread_join(testParams[i].newThread, NULL))
		{
			printf("Error joining thread %d\n", i);
			exit(1);
		}
	}

	for(unsigned i = 0; i < threadCount; i++)
	{
		if(testParams[i].testResult != 1)
		{
			printf("Secondary thread tests %d failed\n", i);
			exit(1);
		}

		printf("Secondary thread tests %d successful\n", i);
	}
}

int main(int argc, char** argv)
{
	/**************** Some calculations of relative paths ****************/
	char* execFolder;
	char* libraryPath;
	struct runTestParams sandboxParams[2];

	if(argc < 1)
	{
		printf("Argv not filled correctly");
		return 1;
	}

	//exec folder is something like: "ProcessSandbox/"
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

	printf("Starting Dyn loader Test.\n");

	if(!initializeDlSandboxCreator(2 /* Should enable detailed logging */))
	{
		printf("Dyn loader Test: initializeDlSandboxCreator returned null\n");
		return 1;
	}

	for(int i = 0; i < 2; i++)
	{
		sandboxParams[i].sandbox = createDlSandbox<PROCESS_SANDBOX_CLASSNAME>(libraryPath);
		initCPPApi(sandboxParams[i].sandbox);

		if(sandboxParams[i].sandbox == NULL)
		{
			printf("Dyn loader Test: createDlSandbox returned null: %d\n", i);
			return 1;
		}

		printf("Sandbox created: %d\n", i);

		/**************** Invoking functions in sandbox ****************/

		//Note will return NULL if given a slot number greater than getTotalNumberOfCallbackSlots(), a valid ptr if it succeeds
		sandboxParams[i].registeredCallback = std::shared_ptr<sandbox_callback_helper<PROCESS_SANDBOX_CLASSNAME, int (unverified_data<unsigned>, unverified_data<char*>, unverified_data<unsigned[1]>)>>
		(
			sandbox_callback(sandboxParams[i].sandbox, invokeSimpleCallbackTest_callback)
		);
	}

	for(int i = 0; i < 2; i++)
	{
		runSingleThreadedTest(sandboxParams[i]);
	}

	for(int i = 0; i < 2; i++)
	{
		struct runTestParams threadParams[ThreadsToTest];

		for(int j = 0; j < ThreadsToTest; j++)
		{
			threadParams[j] = sandboxParams[i];
		}

		runMultiThreadedTest(threadParams, ThreadsToTest);
		checkMultiThreadedTest(threadParams, ThreadsToTest);
	}

	//interleaved sandbox thread
	{
		struct runTestParams threadParams1[ThreadsToTest];
		struct runTestParams threadParams2[ThreadsToTest];

		for(int j = 0; j < ThreadsToTest; j++)
		{
			threadParams1[j] = sandboxParams[0];
			threadParams2[j] = sandboxParams[1];
		}

		runMultiThreadedTest(threadParams1, ThreadsToTest);
		runMultiThreadedTest(threadParams2, ThreadsToTest);
		checkMultiThreadedTest(threadParams1, ThreadsToTest);
		checkMultiThreadedTest(threadParams2, ThreadsToTest);
	}

	printf("Dyn loader Test Succeeded\n");

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

char* getExecFolder(char* executablePath)
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
