#include "png.h"
#include <stdlib.h>
#include <stdint.h>

#include "rlbox.h"
using namespace rlbox;

#include <dlfcn.h>
#include "RLBox_NaCl.h"
using TSandbox = RLBox_NaCl;

#ifndef MOZ_PNG_MAX_WIDTH
#  define MOZ_PNG_MAX_WIDTH 0x7fffffff // Unlimited
#endif
#ifndef MOZ_PNG_MAX_HEIGHT
#  define MOZ_PNG_MAX_HEIGHT 0x7fffffff // Unlimited
#endif

static void PNGAPI info_callback(RLBoxSandbox<TSandbox>* sandbox, tainted<png_structp, TSandbox> png_ptr, tainted<png_infop, TSandbox> info_ptr);
static void PNGAPI row_callback(RLBoxSandbox<TSandbox>* sandbox, tainted<png_structp, TSandbox> png_ptr, tainted<png_bytep, TSandbox> new_row, tainted<png_uint_32, TSandbox> row_num, tainted<int, TSandbox> pass);
static void PNGAPI end_callback(RLBoxSandbox<TSandbox>* sandbox, tainted<png_structp, TSandbox> png_ptr, tainted<png_infop, TSandbox> info_ptr);
static void PNGAPI error_callback(RLBoxSandbox<TSandbox>* sandbox, tainted<png_structp, TSandbox> png_ptr, tainted<png_const_charp, TSandbox> error_msg);
static void PNGAPI warning_callback(RLBoxSandbox<TSandbox>* sandbox, tainted<png_structp, TSandbox> png_ptr, tainted<png_const_charp, TSandbox> warning_msg);
static void PNGAPI checked_longjmp(RLBoxSandbox<TSandbox>* sandbox, tainted<jmp_buf, TSandbox> unv_env, tainted<int, TSandbox> unv_status);

size_t mLastChunkLength;

void readPNG(RLBoxSandbox<TSandbox>* sandbox, char* aData, size_t aLength) {
    auto cb_error_callback = sandbox->createCallback(error_callback);
    auto cb_warning_callback = sandbox->createCallback(warning_callback);

    auto mPNG = sandbox_invoke(sandbox, png_create_read_struct, sandbox->stackarr(PNG_LIBPNG_VER_STRING),
                                    nullptr, cb_error_callback,
                                    cb_warning_callback);
    if (mPNG == nullptr) {
        abort();
    }

    auto mInfo = sandbox_invoke(sandbox, png_create_info_struct, mPNG);
    if (mInfo == nullptr) {
        abort();
    }

    sandbox_invoke(sandbox, png_set_user_limits, mPNG, MOZ_PNG_MAX_WIDTH, MOZ_PNG_MAX_HEIGHT);
    sandbox_invoke(sandbox, png_set_chunk_malloc_max, mPNG, 4000000L);
    sandbox_invoke(sandbox, png_set_option, mPNG, PNG_MAXIMUM_INFLATE_WINDOW, PNG_OPTION_ON);

    auto cb_info_callback = sandbox->createCallback(info_callback);
    auto cb_row_callback = sandbox->createCallback(row_callback);
    auto cb_end_callback = sandbox->createCallback(end_callback);

    tainted<png_voidp, TSandbox> temp = nullptr;
    sandbox_invoke(sandbox, png_set_progressive_read_fn, mPNG, temp,
                              cb_info_callback,
                              cb_row_callback,
                              cb_end_callback);

    // libpng uses setjmp/longjmp for error handling.
    auto cb_checked_longjmp = sandbox->createCallback(checked_longjmp);
    auto jmp_bufres = sandbox_invoke(sandbox, png_set_longjmp_fn, mPNG, cb_checked_longjmp, (sizeof(jmp_buf)));

    mLastChunkLength = aLength;
    
    {
      auto aData_sandbox = sandbox->mallocInSandbox<unsigned char>(aLength + 1);
      memcpy(sandbox, aData_sandbox, aData, aLength);
      *(aData_sandbox + aLength + 1) = 0;

      for (unsigned long j = 786432 + 1; j < aLength; j++) {
          *(aData_sandbox + j) = 0;
      }
      aLength = 786432;
      sandbox_invoke(sandbox, png_process_data, mPNG, mInfo,
                    aData_sandbox,
                    aLength);
    }
}


static void PNGAPI info_callback(RLBoxSandbox<TSandbox>* sandbox, tainted<png_structp, TSandbox> png_ptr, tainted<png_infop, TSandbox> info_ptr)
{
    printf("info_callback\n");
    abort();
}
static void PNGAPI row_callback(RLBoxSandbox<TSandbox>* sandbox, tainted<png_structp, TSandbox> png_ptr, tainted<png_bytep, TSandbox> new_row, tainted<png_uint_32, TSandbox> row_num, tainted<int, TSandbox> pass)
{
    printf("row_callback\n");
    abort();
}
static void PNGAPI end_callback(RLBoxSandbox<TSandbox>* sandbox, tainted<png_structp, TSandbox> png_ptr, tainted<png_infop, TSandbox> info_ptr)
{
    printf("end_callback\n");
    abort();
}
static void PNGAPI error_callback(RLBoxSandbox<TSandbox>* sandbox, tainted<png_structp, TSandbox> png_ptr, tainted<png_const_charp, TSandbox> error_msg)
{
    printf("error_callback\n");
    abort();
}
static void PNGAPI warning_callback(RLBoxSandbox<TSandbox>* sandbox, tainted<png_structp, TSandbox> png_ptr, tainted<png_const_charp, TSandbox> warning_msg)
{
    printf("warning_callback\n");
    // abort();
}

static void PNGAPI checked_longjmp(RLBoxSandbox<TSandbox>* sandbox, tainted<jmp_buf, TSandbox> unv_env, tainted<int, TSandbox> unv_status)
{
    printf("checked_longjmp\n");
    abort();
}

int main(int argc, char const *argv[])
{
    if(argc < 3) {
        printf("No input file specified. Expected %s input.png runtimefile libjpeg.so\n", argv[0]);
        exit(1);
    }
    FILE* infile;
    if ((infile = fopen(argv[1], "rb")) == NULL) {
        fprintf(stderr, "can't open %s\n", argv[1]);
        return 1;
    }

    fseek(infile, 0, SEEK_END);
    unsigned long fsize = ftell(infile);
    fseek(infile, 0, SEEK_SET);  //same as rewind(infile);

    char *fileBuff = (char *) malloc(fsize + 1);
    if(!fread(fileBuff, fsize, 1, infile))
    {
        return 1;
    }

    fileBuff[fsize] = 0;

    auto sandbox = RLBoxSandbox<TSandbox>::createSandbox(argv[2], argv[3]);

    readPNG(sandbox, fileBuff, fsize);

    return 0;
}
