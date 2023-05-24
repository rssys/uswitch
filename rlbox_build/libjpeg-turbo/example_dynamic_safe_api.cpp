/*
 * example.c
 *
 * This file illustrates how to use the IJG code as a subroutine library
 * to read or write JPEG image files.  You should look at this code in
 * conjunction with the documentation file libjpeg.txt.
 *
 * This code will not do anything useful as-is, but it may be helpful as a
 * skeleton for constructing routines that call the JPEG library.
 *
 * We present these routines in the same coding style used in the JPEG code
 * (ANSI function definitions, etc); but you are of course free to code your
 * routines in a different style if you prefer.
 */

#include <stdio.h>

/*
 * Include file for users of JPEG library.
 * You will need to have included system headers that define at least
 * the typedefs FILE and size_t before you can include jpeglib.h.
 * (stdio.h is sufficient on ANSI-conforming systems.)
 * You may also wish to include "jerror.h".
 */

#include "jpeglib.h"

/*
 * <setjmp.h> is used for the optional error recovery mechanism shown in
 * the second part of the example.
 */

#include <setjmp.h>

/* for exit() */
#include <stdlib.h>
#include <memory>

// For sandboxing
// Either USE_NACL or USE_PROCESS should be defined by the build system while compiling this file.
#ifdef USE_NACL
  #define NACL_SANDBOX_API_NO_OPTIONAL
  #include "nacl_sandbox.h"
  #undef NACL_SANDBOX_API_NO_OPTIONAL
#elif defined(USE_PROCESS)
  #include "ProcessSandbox.h"
  #include "process_sandbox_cpp.h"
#else
#error No sandbox type defined.
#endif


/*
 * ERROR HANDLING:
 *
 * The JPEG library's standard error handler (jerror.c) is divided into
 * several "methods" which you can override individually.  This lets you
 * adjust the behavior without duplicating a lot of code, which you might
 * have to update with each future release.
 *
 * Our example here shows how to override the "error_exit" method so that
 * control is returned to the library's caller when a fatal error occurs,
 * rather than calling exit() as the standard error_exit method does.
 *
 * We use C's setjmp/longjmp facility to return control.  This means that the
 * routine which calls the JPEG library must first execute a setjmp() call to
 * establish the return point.  We want the replacement error_exit to do a
 * longjmp().  But we need to make the setjmp buffer accessible to the
 * error_exit routine.  To do this, we make a private extension of the
 * standard JPEG error handler object.  (If we were using C++, we'd say we
 * were making a subclass of the regular error handler.)
 *
 * Here's the extended error handler struct:
 */

struct decoder_error_mgr {
  struct jpeg_error_mgr pub;    /* "public" fields */

  jmp_buf setjmp_buffer;        /* for return to caller */
};

typedef struct decoder_error_mgr * my_error_ptr;

#include "jpeglib_structs_for_cpp_api.h"
sandbox_nacl_load_library_api(jpeglib)

#define PRINT_FUNCTION_TIMES
#ifdef PRINT_FUNCTION_TIMES
  
  #include <inttypes.h>
  #include <chrono>
  using namespace std::chrono;

  long long timeSpentInJpeg = 0;
  long long sandboxFuncOrCallbackInvocations = 0;
  high_resolution_clock::time_point SandboxEnterTime;
  high_resolution_clock::time_point SandboxExitTime;

  long long timeSpentOutsideJpeg = 0;
  high_resolution_clock::time_point OuterEnterTime;
  high_resolution_clock::time_point OuterExitTime;

  long long programTime = 0;
  high_resolution_clock::time_point ProgramEnterTime;
  high_resolution_clock::time_point ProgramExitTime;
  
  #define START_TIMER() SandboxEnterTime = high_resolution_clock::now(); \
    sandboxFuncOrCallbackInvocations++

  #define END_TIMER()   SandboxExitTime = high_resolution_clock::now(); \
    timeSpentInJpeg+= duration_cast<nanoseconds>(SandboxExitTime - SandboxEnterTime).count()

  #define START_OUTER_TIMER() OuterEnterTime = high_resolution_clock::now()
  #define END_OUTER_TIMER()   OuterExitTime = high_resolution_clock::now(); \
    timeSpentOutsideJpeg+= duration_cast<nanoseconds>(OuterExitTime - OuterEnterTime).count()

  #define START_PROGRAM_TIMER() ProgramEnterTime = high_resolution_clock::now()
  #define END_PROGRAM_TIMER() ProgramExitTime = high_resolution_clock::now(); \
    programTime+= duration_cast<nanoseconds>(ProgramExitTime - ProgramEnterTime).count()

#else
  #define START_TIMER() do {} while(0)
  #define END_TIMER() do {} while(0)
  #define START_OUTER_TIMER() do {} while(0)
  #define END_OUTER_TIMER() do {} while(0)
  #define START_PROGRAM_TIMER() do {} while(0)
  #define END_PROGRAM_TIMER() do {} while(0)
#endif

/******************** JPEG COMPRESSION SAMPLE INTERFACE *******************/

/* This half of the example shows how to feed data into the JPEG compressor.
 * We present a minimal version that does not worry about refinements such
 * as error recovery (the JPEG code will just exit() if it gets an error).
 */


/*
 * IMAGE DATA FORMATS:
 *
 * The standard input image format is a rectangular array of pixels, with
 * each pixel having the same number of "component" values (color channels).
 * Each pixel row is an array of JSAMPLEs (which typically are unsigned chars).
 * If you are working with color data, then the color values for each pixel
 * must be adjacent in the row; for example, R,G,B,R,G,B,R,G,B,... for 24-bit
 * RGB color.
 *
 * For this example, we'll assume that this data structure matches the way
 * our application has stored the image in memory, so we can just pass a
 * pointer to our image buffer.  In particular, let's say that the image is
 * RGB color and is described by:
 */

#ifdef USE_NACL
  NaClSandbox* sandbox;
#elif defined(USE_PROCESS)
  JPEGProcessSandbox* sandbox;
#else
#error No sandbox type defined.
#endif

jmp_buf setjmp_buffer;

/* extern */unverified_data<JSAMPLE*> image_buffer = (JSAMPLE*) NULL;  /* Points to large array of R,G,B-order data */
/* extern */int image_height = 0;        /* Number of rows in image */
/* extern */int image_width = 0;         /* Number of columns in image */
int curr_image_row = 0;

void put_scanline_someplace(unverified_data<JSAMPROW> rowBuffer, int row_stride)
{
  int i;
  for(i = 0; i < row_stride; i++)
  {
    image_buffer[curr_image_row * row_stride + i] = rowBuffer[i];
  }

  curr_image_row++;
}

/*
 * Sample routine for JPEG compression.  We assume that the target file name
 * and a compression quality factor are passed in.
 */

int
write_JPEG_file (unverified_data<unsigned char **> p_outbuffer, unverified_data<unsigned long *> p_outsize, int quality)
{
  /* This struct contains the JPEG compression parameters and pointers to
   * working space (which is allocated as needed by the JPEG library).
   * It is possible to have several such structures, representing multiple
   * compression/decompression processes, in existence at once.  We refer
   * to any one struct (and its associated working data) as a "JPEG object".
   */
  START_OUTER_TIMER();
  auto p_cinfo = newInSandbox<jpeg_compress_struct>(sandbox);

  /* This struct represents a JPEG error handler.  It is declared separately
   * because applications often want to supply a specialized error handler
   * (see the second half of this file for an example).  But here we just
   * take the easy way out and use the standard error handler, which will
   * print a message on stderr and call exit() if compression fails.
   * Note that this struct must live as long as the main JPEG parameter
   * struct, to avoid dangling-pointer problems.
   */
  auto p_jerr = newInSandbox<jpeg_error_mgr>(sandbox);

  /* More stuff */

  auto p_row_pointer = newInSandbox<JSAMPROW>(sandbox);
  //JSAMPROW row_pointer[1];      /* pointer to JSAMPLE row[s] */

  int row_stride;               /* physical row width in image buffer */

  /* Step 1: allocate and initialize JPEG compression object */

  /* We have to set up the error handler first, in case the initialization
   * step fails.  (Unlikely, but it could happen if you are out of memory.)
   * This routine fills in the contents of struct jerr, and returns jerr's
   * address which we place into the link field in cinfo.
   */
  p_cinfo->err = sandbox_invoke(sandbox, jpeg_std_error, p_jerr);
  /* Now we can initialize the JPEG compression object. */
  sandbox_invoke(sandbox, jpeg_CreateCompress, p_cinfo, JPEG_LIB_VERSION, (size_t) sizeof(struct jpeg_compress_struct));

  /* Step 2: specify data destination (eg, a file) */
  /* Note: steps 2 and 3 can be done in either order. */

  /* Here we use the library-supplied code to send compressed data to a
   * stdio stream.  You can also write your own code to do something else.
   * VERY IMPORTANT: use "b" option to fopen() if you are on a machine that
   * requires it in order to write binary files.
   */

  sandbox_invoke(sandbox, jpeg_mem_dest, p_cinfo, p_outbuffer, p_outsize);

  /* Step 3: set parameters for compression */

  /* First we supply a description of the input image.
   * Four fields of the cinfo struct must be filled in:
   */

  p_cinfo->image_width = image_width;      /* image width and height, in pixels */
  p_cinfo->image_height = image_height;
  p_cinfo->input_components = 3;           /* # of color components per pixel */
  p_cinfo->in_color_space = JCS_RGB;       /* colorspace of input image */
  /* Now use the library's routine to set default compression parameters.
   * (You must set at least cinfo.in_color_space before calling this,
   * since the defaults depend on the source color space.)
   */
  sandbox_invoke(sandbox, jpeg_set_defaults, p_cinfo);
  /* Now you can set any non-default parameters you wish to.
   * Here we just illustrate the use of quality (quantization table) scaling:
   */
  sandbox_invoke(sandbox, jpeg_set_quality, p_cinfo, quality, TRUE /* limit to baseline-JPEG values */);

  /* Step 4: Start compressor */

  /* TRUE ensures that we will write a complete interchange-JPEG file.
   * Pass TRUE unless you are very sure of what you're doing.
   */
  sandbox_invoke(sandbox, jpeg_start_compress, p_cinfo, TRUE);

  /* Step 5: while (scan lines remain to be written) */
  /*           jpeg_write_scanlines(...); */

  /* Here we use the library's state variable cinfo.next_scanline as the
   * loop counter, so that we don't have to keep track ourselves.
   * To keep things simple, we pass one scanline per call; you can pass
   * more if you wish, though.
   */
  row_stride = image_width * 3; /* JSAMPLEs per row in image_buffer */

  //This could cause a Denial of Service, but we do not handle that
  while (p_cinfo->next_scanline.UNSAFE_noVerify() < image_height) {
    /* jpeg_write_scanlines expects an array of pointers to scanlines.
     * Here the array is only one element long, but you could pass
     * more than one scanline at a time if that's more convenient.
     */

    // We want to pass the address of image_buffer[p_cinfo->next_scanline * row_stride]
    // to a sandbox function
    // this shouldn't need validation as we are only reading an address 
    // even if the address is outside the sandbox, the function cannot access the data
    p_row_pointer[0] = &image_buffer[p_cinfo->next_scanline.UNSAFE_noVerify() * row_stride];
    (void) sandbox_invoke(sandbox, jpeg_write_scanlines, p_cinfo, (p_row_pointer), 1);
  }

  /* Step 6: Finish compression */

  sandbox_invoke(sandbox, jpeg_finish_compress, p_cinfo);
  /* After finish_compress, we can close the output file. */

  /* Step 7: release JPEG compression object */

  /* This is an important step since it will release a good deal of memory. */
  sandbox_invoke(sandbox, jpeg_destroy_compress, p_cinfo);

  freeInSandbox(sandbox, p_cinfo);
  freeInSandbox(sandbox, p_jerr);
  freeInSandbox(sandbox, p_row_pointer);

  END_OUTER_TIMER();
  /* And we're done! */
  return 1;
}


/*
 * SOME FINE POINTS:
 *
 * In the above loop, we ignored the return value of jpeg_write_scanlines,
 * which is the number of scanlines actually written.  We could get away
 * with this because we were only relying on the value of cinfo.next_scanline,
 * which will be incremented correctly.  If you maintain additional loop
 * variables then you should be careful to increment them properly.
 * Actually, for output to a stdio stream you needn't worry, because
 * then jpeg_write_scanlines will write all the lines passed (or else exit
 * with a fatal error).  Partial writes can only occur if you use a data
 * destination module that can demand suspension of the compressor.
 * (If you don't know what that's for, you don't need it.)
 *
 * If the compressor requires full-image buffers (for entropy-coding
 * optimization or a multi-scan JPEG file), it will create temporary
 * files for anything that doesn't fit within the maximum-memory setting.
 * (Note that temp files are NOT needed if you use the default parameters.)
 * On some systems you may need to set up a signal handler to ensure that
 * temporary files are deleted if the program is interrupted.  See libjpeg.txt.
 *
 * Scanlines MUST be supplied in top-to-bottom order if you want your JPEG
 * files to be compatible with everyone else's.  If you cannot readily read
 * your data in that order, you'll need an intermediate array to hold the
 * image.  See rdtarga.c or rdbmp.c for examples of handling bottom-to-top
 * source data using the JPEG code's internal virtual-array mechanisms.
 */



/******************** JPEG DECOMPRESSION SAMPLE INTERFACE *******************/

/* This half of the example shows how to read data from the JPEG decompressor.
 * It's a bit more refined than the above, in that we show:
 *   (a) how to modify the JPEG library's standard error-reporting behavior;
 *   (b) how to allocate workspace using the library's memory manager.
 *
 * Just to make this example a little different from the first one, we'll
 * assume that we do not intend to put the whole image into an in-memory
 * buffer, but to send it line-by-line someplace else.  We need a one-
 * scanline-high JSAMPLE array as a work buffer, and we will let the JPEG
 * memory manager allocate it for us.  This approach is actually quite useful
 * because we don't need to remember to deallocate the buffer separately: it
 * will go away automatically when the JPEG object is cleaned up.
 */

/*
 * Here's the routine that will replace the standard error_exit method:
 */

METHODDEF(void)
my_error_exit (unverified_data<j_common_ptr> cinfo)
{
  /* Always display the message. */
  /* We could postpone this until after returning, if we chose. */
  //(*cinfo->err->output_message) (cinfo);

  /* Return control to the setjmp point */
  longjmp(setjmp_buffer, 1);
}

/*
 * Sample routine for JPEG decompression.  We assume that the source file name
 * is passed in.  We want to return 1 on success, 0 on error.
 */


GLOBAL(int)
read_JPEG_file (unverified_data<unsigned char *> fileBuff, unsigned long fsize)
{
  /* This struct contains the JPEG decompression parameters and pointers to
   * working space (which is allocated as needed by the JPEG library).
   */
  START_OUTER_TIMER();
  auto p_cinfo = newInSandbox<jpeg_decompress_struct>(sandbox);

  /* We use our private extension JPEG error handler.
   * Note that this struct must live as long as the main JPEG parameter
   * struct, to avoid dangling-pointer problems.
   */
  auto p_jerr = newInSandbox<decoder_error_mgr>(sandbox);

  /* More stuff */
  auto p_buffer = newInSandbox<JSAMPARRAY>(sandbox);            /* Output row buffer */

  int row_stride;               /* physical row width in output buffer */

  /* In this example we want to open the input file before doing anything else,
   * so that the setjmp() error recovery below can assume the file is open.
   * VERY IMPORTANT: use "b" option to fopen() if you are on a machine that
   * requires it in order to read binary files.
   */

  /* Step 1: allocate and initialize JPEG decompression object */

  /* We set up the normal JPEG error routines, then override error_exit. */
  p_cinfo->err = sandbox_invoke(sandbox, jpeg_std_error, &(p_jerr->pub));

  #ifdef USE_NACL
    std::unique_ptr<sandbox_callback_helper<void(unverified_data<j_common_ptr>)>> callback(sandbox_callback(sandbox, my_error_exit));
  #else
    std::unique_ptr<sandbox_callback_helper<JPEGProcessSandbox, void(unverified_data<j_common_ptr>)>> callback(sandbox_callback(sandbox, my_error_exit));
  #endif

  p_jerr->pub.error_exit = callback.get();

  /* Establish the setjmp return context for my_error_exit to use. */
  if (setjmp(setjmp_buffer)) {
    /* If we get here, the JPEG code has signaled an error.
     * We need to clean up the JPEG object, close the input file, and return.
     */
    sandbox_invoke(sandbox, jpeg_destroy_decompress, p_cinfo);
    return 0;
  }

  /* Now we can initialize the JPEG decompression object. */
  sandbox_invoke(sandbox, jpeg_CreateDecompress, p_cinfo, JPEG_LIB_VERSION, (size_t) sizeof(struct jpeg_decompress_struct));

  /* Step 2: specify data source (eg, a file) */
  sandbox_invoke(sandbox, jpeg_mem_src, p_cinfo, fileBuff, fsize);

  /* Step 3: read file parameters with jpeg_read_header() */

  (void) sandbox_invoke(sandbox, jpeg_read_header, p_cinfo, TRUE);
  /* We can ignore the return value from jpeg_read_header since
   *   (a) suspension is not possible with the stdio data source, and
   *   (b) we passed TRUE to reject a tables-only JPEG file as an error.
   * See libjpeg.txt for more info.
   */

  /* Step 4: set parameters for decompression */

  /* In this example, we don't need to change any of the defaults set by
   * jpeg_read_header(), so we do nothing here.
   */

  /* Step 5: Start decompressor */

  (void) sandbox_invoke(sandbox, jpeg_start_decompress, p_cinfo);
  /* We can ignore the return value since suspension is not possible
   * with the stdio data source.
   */

  /* We may need to do some setup of our own at this point before reading
   * the data.  After jpeg_start_decompress() we have the correct scaled
   * output image dimensions available, as well as the output colormap
   * if we asked for color quantization.
   * In this example, we need to make an output work buffer of the right size.
   */
  /* JSAMPLEs per row in output buffer */
  //Save data
  //we accept the values of image_width height, and components from jpeg
  //we don't verify these, but rather make sure we can accomodate any values returned by jpeg here
  image_height = p_cinfo->output_height.UNSAFE_noVerify();
  image_width = p_cinfo->output_width.UNSAFE_noVerify();
  row_stride = image_width * p_cinfo->output_components.UNSAFE_noVerify();

  /* Make a one-row-high sample array that will go away when done with image */
  //this is a function pointer we execute, nothing to verify
  auto p_alloc_sarray = p_cinfo->mem->alloc_sarray.sandbox_onlyVerifyAddress();

  #ifdef USE_NACL
    unverified_data<JSAMPARRAY> pBufferVal = sandbox_invoke_with_ptr(sandbox, p_alloc_sarray, (j_common_ptr) p_cinfo.sandbox_onlyVerifyAddress() /* hack - we need to temporarily remove wrapper to cast*/, JPOOL_IMAGE, row_stride, 1);
  #elif defined(USE_PROCESS)
    //process sandbox currently uses a workaround for invoking function pointers.
    unverified_data<JSAMPARRAY> pBufferVal = sandbox_invoke(sandbox, alloc_sarray_ps, (j_common_ptr) p_cinfo.sandbox_onlyVerifyAddress() /* hack - we need to temporarily remove wrapper to cast*/, JPOOL_IMAGE, row_stride, 1);
  #endif

  *p_buffer = pBufferVal;

  image_buffer = newInSandbox<JSAMPLE>(sandbox, image_width * image_height * 3 * sizeof(JSAMPLE));

  if(!image_buffer)
  {
    printf("Memory alloc failure\n");
    return 1; 
  }

  /* Step 6: while (scan lines remain to be read) */
  /*           jpeg_read_scanlines(...); */

  /* Here we use the library's state variable cinfo.output_scanline as the
   * loop counter, so that we don't have to keep track ourselves.
   */
  //This could cause a Denial of Service, but we do not handle that
  while (p_cinfo->output_scanline.UNSAFE_noVerify() < image_height) {
    /* jpeg_read_scanlines expects an array of pointers to scanlines.
     * Here the array is only one element long, but you could ask for
     * more than one scanline at a time if that's more convenient.
     */

    unverified_data<JSAMPARRAY> pBufferSys = *p_buffer;
    (void) sandbox_invoke(sandbox, jpeg_read_scanlines, p_cinfo, pBufferSys, 1);

    /* Assume put_scanline_someplace wants a pointer and sample count. */
    put_scanline_someplace(pBufferSys[0], row_stride);
  }

  /* Step 7: Finish decompression */

  (void) sandbox_invoke(sandbox, jpeg_finish_decompress, p_cinfo);
  /* We can ignore the return value since suspension is not possible
   * with the stdio data source.
   */

  /* Step 8: Release JPEG decompression object */

  /* This is an important step since it will release a good deal of memory. */
  sandbox_invoke(sandbox, jpeg_destroy_decompress, p_cinfo);

  /* After finish_decompress, we can close the input file.
   * Here we postpone it until after no more JPEG errors are possible,
   * so as to simplify the setjmp error logic above.  (Actually, I don't
   * think that jpeg_destroy can do an error exit, but why assume anything...)
   */

  freeInSandbox(sandbox, p_cinfo);
  freeInSandbox(sandbox, p_jerr);
  freeInSandbox(sandbox, p_buffer);

  /* At this point you may want to check to see whether any corrupt-data
   * warnings occurred (test whether jerr.pub.num_warnings is nonzero).
   */

  END_OUTER_TIMER();
  /* And we're done! */
  return 1;
}


/*
 * SOME FINE POINTS:
 *
 * In the above code, we ignored the return value of jpeg_read_scanlines,
 * which is the number of scanlines actually read.  We could get away with
 * this because we asked for only one line at a time and we weren't using
 * a suspending data source.  See libjpeg.txt for more info.
 *
 * We cheated a bit by calling alloc_sarray() after jpeg_start_decompress();
 * we should have done it beforehand to ensure that the space would be
 * counted against the JPEG max_memory setting.  In some systems the above
 * code would risk an out-of-memory error.  However, in general we don't
 * know the output image dimensions before jpeg_start_decompress(), unless we
 * call jpeg_calc_output_dimensions().  See libjpeg.txt for more about this.
 *
 * Scanlines are returned in the same order as they appear in the JPEG file,
 * which is standardly top-to-bottom.  If you must emit data bottom-to-top,
 * you can use one of the virtual arrays provided by the JPEG memory manager
 * to invert the data.  See wrbmp.c for an example.
 *
 * As with compression, some operating modes may require temporary files.
 * On some systems you may need to set up a signal handler to ensure that
 * temporary files are deleted if the program is interrupted.  See libjpeg.txt.
 */

// For NACL we ignore maincore_as_str and sbcore_as_str.
int dynamicLoad(char* path, char* libraryPath, char* maincore_as_str, char* sbcore_as_str)
{
  END_PROGRAM_TIMER();
#ifdef USE_NACL
  printf("Creating NaCl Sandbox for %s\n", path);
  initializeDlSandboxCreator(0 /* Should enable detailed logging */);
  sandbox = createDlSandbox(libraryPath, path);
#elif defined(USE_PROCESS)
  printf("Creating process sandbox\n");
  unsigned maincore, sbcore;
  if(!sscanf(maincore_as_str, "%u", &maincore)) {
    printf("Bad maincore argument\n");
    return 0;
  }
  if(!sscanf(sbcore_as_str, "%u", &sbcore)) {
    printf("Bad sandboxcore argument\n");
    return 0;
  }
  sandbox = new JPEGProcessSandbox(libraryPath, maincore, sbcore);
#else
#error No sandbox type defined.
#endif

  if(!sandbox)
  {
    printf("Error creating sandbox");
    return 0;
  }
  initCPPApi(sandbox);
  START_PROGRAM_TIMER();

  return 1;
}

int main(int argc, char** argv)
{
  START_PROGRAM_TIMER();
#ifdef USE_NACL
  if(argc < 5) {
    printf("No io files specified. Expected arg example input.jpeg output.jpeg libjpeg.nexe naclLibraryPath\n");
#elif defined(USE_PROCESS)
  if(argc < 7) {
    printf("Error: expected 7 arguments\n  (1) input.jpeg\n  (2) output.jpeg\n  (3) libjpeg.so\n  (4) ProcessSandbox_otherside\n  (5) main process core\n  (6) sandbox process core\n");
#else
#error No sandbox type defined.
#endif
    return 1;
  }

  printf("Starting\n");

#ifdef USE_NACL
  if(!dynamicLoad(argv[3], argv[4], NULL, NULL)) {
#elif defined(USE_PROCESS)
  if(!dynamicLoad(argv[3], argv[4], argv[5], argv[6])) {
#else
#error No sandbox type defined.
#endif
    printf("Dynamic load failed\n");
    return 1;
  }

  FILE* infile;
  if ((infile = fopen(argv[1], "rb")) == NULL) {
    fprintf(stderr, "can't open %s\n", argv[1]);
    return 1;
  }

  fseek(infile, 0, SEEK_END);
  unsigned long fsize = ftell(infile);
  fseek(infile, 0, SEEK_SET);  //same as rewind(infile);

  unverified_data<unsigned char *> fileBuff = newInSandbox<unsigned char>(sandbox, fsize + 1);
  //only reading from a buffer, no verification necessary
  if(!fread(fileBuff.sandbox_onlyVerifyAddress(), fsize, 1, infile))
  {
    return 1;
  }

  fileBuff[fsize] = 0;

  FILE * outfile;
  if ((outfile = fopen(argv[2], "wb")) == NULL) {
    fprintf(stderr, "can't open %s\n", argv[2]);
    return 0;
  }

  unverified_data<unsigned char **> p_outbuffer = newInSandbox<unsigned char *>(sandbox);
  unverified_data<unsigned long *> p_outsize = newInSandbox<unsigned long>(sandbox);
  *p_outbuffer = (unsigned char *) 0;
  *p_outsize = 0;

  if(!read_JPEG_file(fileBuff, fsize))
  {
    if(image_buffer)
    {
      freeInSandbox(sandbox, image_buffer);
    }

    printf("Reading file %s failed\n", argv[1]);
    fflush(stdout);
    return 1;
  }

  #ifdef PRINT_FUNCTION_TIMES
    #if defined(_WIN32)
      printf("Read JPEG invocations = %I64d, time = %I64d ns\n", sandboxFuncOrCallbackInvocations, timeSpentInJpeg);
      printf("Read JPEG total time = %I64d ns\n", timeSpentOutsideJpeg);
    #else
      printf("Read JPEG invocations = %lld, time = %lld ns\n", sandboxFuncOrCallbackInvocations, timeSpentInJpeg);
      printf("Read JPEG total time = %lld ns\n", timeSpentOutsideJpeg);
    #endif
  #endif

  printf("Width: %d, Height: %d\n", image_width, image_height);

  #ifdef PRINT_FUNCTION_TIMES
    timeSpentInJpeg = 0;
    sandboxFuncOrCallbackInvocations = 0;
    timeSpentOutsideJpeg = 0;
  #endif

  if(!write_JPEG_file(p_outbuffer, p_outsize, 30))
  {
    printf("Writing to file %s failed\n", argv[2]);
    fflush(stdout);
    return 1; 
  }

  #ifdef PRINT_FUNCTION_TIMES
    #if defined(_WIN32)
      printf("Write JPEG invocations = %I64d, time = %I64d ns\n", sandboxFuncOrCallbackInvocations, timeSpentInJpeg);
      printf("Write JPEG total time = %I64d ns\n", timeSpentOutsideJpeg);
    #else
      printf("Write JPEG invocations = %lld, time = %lld ns\n", sandboxFuncOrCallbackInvocations, timeSpentInJpeg);
      printf("Write JPEG total time = %lld ns\n", timeSpentOutsideJpeg);
    #endif
  #endif

  freeInSandbox(sandbox, image_buffer);

  //to validate: outbuffer to outbuffer[outsize] should be in the sandbox
  unsigned long validatedOutSize = (*p_outsize).UNSAFE_noVerify();
  unsigned char * outbuffer_contents = (*p_outbuffer).sandbox_onlyVerifyAddressRange(validatedOutSize);

  fwrite(outbuffer_contents, validatedOutSize, 1, outfile);
  fclose(outfile);
  fclose(infile);
  freeInSandbox(sandbox, fileBuff);
  freeInSandbox(sandbox, p_outbuffer);
  freeInSandbox(sandbox, p_outsize);
  END_PROGRAM_TIMER();

  #ifdef PRINT_FUNCTION_TIMES
    #if defined(_WIN32)
      printf("JPEG program time = %I64d ns\n", programTime);
    #else
      printf("JPEG program time = %lld ns\n", programTime);
    #endif
  #endif

  printf("Success\n");

  #if defined(USE_PROCESS)
  sandbox->destroySandbox();
  #endif
  fflush(stdout);
  return 0;
}
