#include "dyn_ldr_lib.h"
#include <stdio.h>
#include <dlfcn.h>


NaClSandbox* sandbox;

char* strdupInSandboxAndGetSandboxPtr(const char* src)
{
  char* strCopy = (char *) mallocInSandbox(sandbox, strlen(src) + 1);
  strcpy(strCopy, src);
  return (char*) getSandboxedAddress(sandbox, (uintptr_t) strCopy);
}

void freeSandboxedStringSandboxPtr(char *str)
{
  char* strU = (char *) getUnsandboxedAddress(sandbox, (uintptr_t) str);
  freeInSandbox(sandbox, strU);
}

int invokeDlMain(char* inputJpeg, char* outputJpeg, char* libJpegSo, void* mainFuncPtr)
{
  char** argvCopy = (char ** ) mallocInSandbox(sandbox, 4 * sizeof(char *));
  argvCopy[0] = strdupInSandboxAndGetSandboxPtr("example");
  argvCopy[1] = strdupInSandboxAndGetSandboxPtr(inputJpeg);
  argvCopy[2] = strdupInSandboxAndGetSandboxPtr(outputJpeg);
  argvCopy[3] = strdupInSandboxAndGetSandboxPtr(libJpegSo); 

  NaClSandbox_Thread* threadData = preFunctionCall(sandbox, sizeof(int) + sizeof(argvCopy), 0);
  PUSH_VAL_TO_STACK(threadData, int, 4);
  PUSH_PTR_TO_STACK(threadData, char**, argvCopy);
  invokeFunctionCall(threadData, mainFuncPtr);
  int ret = functionCallReturnRawPrimitiveInt(threadData);	

  freeSandboxedStringSandboxPtr(argvCopy[0]);
  freeSandboxedStringSandboxPtr(argvCopy[1]);
  freeSandboxedStringSandboxPtr(argvCopy[2]);
  freeSandboxedStringSandboxPtr(argvCopy[3]);
  freeInSandbox(sandbox, argvCopy);

  return ret;
}

int main(int argc, char** argv)
{
  if(argc < 7)
  {
    printf("No io files specified. Expected arg example input.jpeg output.jpeg libjpeg.so lib_example_dynamic_non_nacl.so naclLibraryPath sandboxInitApp\n");
    return 1;
  }

  initializeDlSandboxCreator(0 /* Should enable detailed logging */);
  sandbox = createDlSandbox(argv[5], argv[6]);

  if(!sandbox)
  {
    printf("Error creating sandbox");
    return 0;
  }

  void* dlPtr = dlopenInSandbox(sandbox, argv[4], RTLD_LAZY);

  if(!dlPtr)
  {
    printf("Loading of dynamic library %s has failed: %s\n", argv[4], dlerrorInSandbox(sandbox));
    return 0;
  }

  void* mainFuncPtr = dlsymInSandbox(sandbox, dlPtr, "main");
  if(mainFuncPtr == NULL) { printf("Symbol resolution failed for main\n"); return 1; }

  int retFail = invokeDlMain(argv[1], argv[2], argv[3], mainFuncPtr);  
  fflush(stdout);
  
  if(retFail)
  {
  	printf("Inner main failed\n");
  }
  else
  {
  	printf("Done\n");
  }
  
  fflush(stdout);

  return retFail;
}