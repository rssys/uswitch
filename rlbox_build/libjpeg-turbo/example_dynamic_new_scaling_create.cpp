#include <stdio.h>
#include <setjmp.h>
#include <stdlib.h>
#include <memory>
#include <inttypes.h>
#include <chrono>
#include <atomic>
using namespace std::chrono;

#include "jpeglib.h"

#include "rlbox.h"
#include "jpeglib_structs_for_cpp_api_new.h"
using namespace rlbox;

#if defined(USE_DYN)
  #include <dlfcn.h>
  #include "RLBox_DynLib.h"
  using TSandbox = RLBox_DynLib;
#elif defined(USE_NACL)
  #include "dyn_ldr_lib.h"
  #include "RLBox_NaCl.h"
  using TSandbox = RLBox_NaCl;
#else
  #error "No sandbox num choice"
#endif

rlbox_load_library_api(jpeglib, TSandbox)



int main(int argc, char** argv)
{
  if(argc < 3)
  {
    printf("No io files specified. Expected arg \n"
      "%s runtimefile libjpeg.so\n"
      ,argv[0]);
    return 1;
  }

  printf("Starting\n");

  const char* runtimePath = argv[1];
  const char* libraryPath = argv[2];

  printf("Creating test for "
    #if defined(USE_DYN)
      "DynLib"
    #elif defined(USE_NACL)
      "NaCl"
    #else
      #error "No sandbox num choice"
    #endif
  );

  for(unsigned i = 0; i < 10000; i++)
  {
    auto sandbox = RLBoxSandbox<TSandbox>::createSandbox(runtimePath, libraryPath);
    auto p_jerr_pub= sandbox->mallocInSandbox<jpeg_error_mgr>();
    sandbox_invoke(sandbox, jpeg_std_error, p_jerr_pub);
    printf("Created sandbox %d\n", i);
  }

  printf("Success\n");
  fflush(stdout);
  return 0;
}