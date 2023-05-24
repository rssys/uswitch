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

void __attribute__ ((noinline)) put_scanline_someplace(tainted<JSAMPROW, TSandbox> rowBuffer, tainted<unsigned int, TSandbox> row_stride)
{

}

void my_error_exit (RLBoxSandbox<TSandbox>* sandbox, tainted<j_common_ptr, TSandbox> cinfo)
{
  fflush(stdout);
  abort();
}

struct runTestParams {
  char const * const runtimePath;
  char const * const libraryPath;
  int const decodeCount;
  unsigned char * const fileBuff;
  unsigned long const fsize;
  std::atomic<long long> timeSpentInJpeg;
  std::atomic<unsigned int> count;
};

void* read_JPEG_file (void* testParams)
{
  runTestParams* testParamsC = (runTestParams*) testParams;
  bool infinite = testParamsC->decodeCount == -1;
  auto sandbox = RLBoxSandbox<TSandbox>::createSandbox(testParamsC->runtimePath, testParamsC->libraryPath);
  testParamsC->count++;
  printf("Created sandbox %d\n", testParamsC->count.load());
  auto cb = sandbox->createCallback(my_error_exit);

  auto fileBuffCopy = sandbox->mallocInSandbox<unsigned char>(testParamsC->fsize);
  memcpy(sandbox, fileBuffCopy, testParamsC->fileBuff, testParamsC->fsize);

  auto decodeCount = testParamsC->decodeCount;

  high_resolution_clock::time_point EnterTime = high_resolution_clock::now();

  while(infinite || decodeCount > 0) {
    if(!infinite) { decodeCount--; }

    auto p_cinfo = sandbox->mallocInSandbox<jpeg_decompress_struct>();
    auto& cinfo = *p_cinfo;

    auto p_jerr_pub= sandbox->mallocInSandbox<jpeg_error_mgr>();
    auto& jerr_pub = *p_jerr_pub;

    cinfo.err = sandbox_invoke(sandbox, jpeg_std_error, &jerr_pub);
    jerr_pub.error_exit = cb;

    sandbox_invoke(sandbox, jpeg_CreateDecompress, &cinfo, JPEG_LIB_VERSION, (size_t) sizeof(struct jpeg_decompress_struct));
    sandbox_invoke(sandbox, jpeg_mem_src, &cinfo, fileBuffCopy, testParamsC->fsize);
    sandbox_invoke(sandbox, jpeg_read_header, &cinfo, TRUE);
    sandbox_invoke(sandbox, jpeg_start_decompress, &cinfo);

    tainted<int, TSandbox> output_components = cinfo.output_components;
    auto row_stride = cinfo.output_width * output_components;

    auto buffer = sandbox_invoke_with_fnptr(sandbox, cinfo.mem->alloc_sarray.UNSAFE_Unverified(), 
      sandbox_reinterpret_cast<j_common_ptr>(&cinfo), JPOOL_IMAGE, row_stride, 1);

    while (cinfo.output_scanline.UNSAFE_Unverified()  < cinfo.output_height.UNSAFE_Unverified()) {
      sandbox_invoke(sandbox, jpeg_read_scanlines, &cinfo, buffer, 1);
      put_scanline_someplace(*buffer, row_stride);
    }

    sandbox_invoke(sandbox, jpeg_finish_decompress, &cinfo);
    sandbox_invoke(sandbox, jpeg_destroy_decompress, &cinfo);

    sandbox->freeInSandbox(p_jerr_pub);
    sandbox->freeInSandbox(p_cinfo);
  }

  testParamsC->timeSpentInJpeg += duration_cast<nanoseconds>(high_resolution_clock::now() - EnterTime).count();

  sandbox->freeInSandbox(fileBuffCopy);
  cb.unregister();
  sandbox->destroySandbox();
  return 0;
}

int main(int argc, char** argv)
{
  if(argc < 6)
  {
    printf("No io files specified. Expected arg \n"
      "%s input.jpeg runtimefile libjpeg.so sandboxesCount decodeCount\n\n"
      "decodeCount is the number of times the image is decoded\n"
      "if decodeCount is -1 it is considered infinite\n"
      "-----------------------------------------\n"
      ,argv[0]);
    return 1;
  }

  printf("Starting\n");

  const char* inputImage = argv[1];
  const char* runtimePath = argv[2];
  const char* libraryPath = argv[3];
  unsigned int sandboxCount;
  int decodeCount;
  sscanf(argv[4], "%u", &sandboxCount);
  sscanf(argv[5], "%d", &decodeCount);

  printf("Creating test for "
    #if defined(USE_DYN)
      "DynLib"
    #elif defined(USE_NACL)
      "NaCl"
    #else
      #error "No sandbox num choice"
    #endif
    " , sandbox count: %u, decode count: %d\n",
    sandboxCount,
    decodeCount
  );

  FILE* infile;
  if ((infile = fopen(inputImage, "rb")) == NULL) {
    fprintf(stderr, "can't open %s\n", inputImage);
    return 1;
  }

  fseek(infile, 0, SEEK_END);
  unsigned long fsize = ftell(infile);
  fseek(infile, 0, SEEK_SET);  //same as rewind(infile);

  unsigned char *fileBuff = (unsigned char *) malloc(fsize + 1);
  if(!fread(fileBuff, fsize, 1, infile))
  {
    return 1;
  }

  fclose(infile);

  fileBuff[fsize] = 0;

  struct runTestParams testParams {
    runtimePath,
    libraryPath,
    decodeCount,
    fileBuff,
    fsize,
    {0}, // timespent
    {0} // count
  };

  if (sandboxCount == 1)
  {
    read_JPEG_file(&testParams);
  }
  else
  {
    std::unique_ptr<pthread_t[]> threads(new pthread_t[sandboxCount]);

    for(unsigned i = 0; i < sandboxCount; i++)
    {
      if(pthread_create(&(threads[i]), NULL /* use default thread attributes */, read_JPEG_file, &testParams /* parameter */))
      {
        printf("Error creating thread %d\n", i);
        exit(1);
      }

      char name[100];
      sprintf(name, "Sbox %u", i);
      pthread_setname_np(threads[i], name);
    }

    for(unsigned i = 0; i < sandboxCount; i++)
    {
      if(pthread_join(threads[i], NULL))
      {
        printf("Error joining thread %d\n", i);
        exit(1);
      }
    }
  }

  printf("Read JPEG total time = %lld ns\n", testParams.timeSpentInJpeg.load());

  free(fileBuff);
  printf("Success\n");
  fflush(stdout);
  return 0;
}