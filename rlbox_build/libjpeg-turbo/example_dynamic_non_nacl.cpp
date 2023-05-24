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

//For dynamic lib loading
#include <dlfcn.h>

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


typedef struct jpeg_error_mgr * (*t_jpeg_std_error) (struct jpeg_error_mgr * err);
typedef void (*t_jpeg_CreateCompress) (j_compress_ptr cinfo, int version, size_t structsize);
typedef void (*t_jpeg_mem_dest) (j_compress_ptr cinfo, unsigned char ** outbuffer, unsigned long * outsize);
typedef void (*t_jpeg_set_defaults) (j_compress_ptr cinfo);
typedef void (*t_jpeg_set_quality) (j_compress_ptr cinfo, int quality, boolean force_baseline);
typedef void (*t_jpeg_start_compress) (j_compress_ptr cinfo, boolean write_all_tables);
typedef JDIMENSION (*t_jpeg_write_scanlines) (j_compress_ptr cinfo, JSAMPARRAY scanlines, JDIMENSION num_lines);
typedef void (*t_jpeg_finish_compress) (j_compress_ptr cinfo);
typedef void (*t_jpeg_destroy_compress) (j_compress_ptr cinfo);
typedef void (*t_jpeg_CreateDecompress) (j_decompress_ptr cinfo, int version, size_t structsize);
typedef void (*t_jpeg_mem_src)(j_decompress_ptr cinfo, unsigned char * inbuffer, unsigned long insize);
typedef int (*t_jpeg_read_header) (j_decompress_ptr cinfo, boolean require_image);
typedef boolean (*t_jpeg_start_decompress) (j_decompress_ptr cinfo);
typedef JDIMENSION (*t_jpeg_read_scanlines) (j_decompress_ptr cinfo, JSAMPARRAY scanlines, JDIMENSION max_lines);
typedef boolean (*t_jpeg_finish_decompress) (j_decompress_ptr cinfo);
typedef void (*t_jpeg_destroy_decompress) (j_decompress_ptr cinfo);

t_jpeg_std_error ptr_jpeg_std_error;
t_jpeg_CreateCompress ptr_jpeg_CreateCompress;
t_jpeg_mem_dest ptr_jpeg_mem_dest;
t_jpeg_set_defaults ptr_jpeg_set_defaults;
t_jpeg_set_quality ptr_jpeg_set_quality;
t_jpeg_start_compress ptr_jpeg_start_compress;
t_jpeg_write_scanlines ptr_jpeg_write_scanlines;
t_jpeg_finish_compress ptr_jpeg_finish_compress;
t_jpeg_destroy_compress ptr_jpeg_destroy_compress;
t_jpeg_CreateDecompress ptr_jpeg_CreateDecompress;
t_jpeg_mem_src ptr_jpeg_mem_src;
t_jpeg_read_header ptr_jpeg_read_header;
t_jpeg_start_decompress ptr_jpeg_start_decompress;
t_jpeg_read_scanlines ptr_jpeg_read_scanlines;
t_jpeg_finish_decompress ptr_jpeg_finish_decompress;
t_jpeg_destroy_decompress ptr_jpeg_destroy_decompress;


struct jpeg_error_mgr * d_jpeg_std_error(struct jpeg_error_mgr * err)
{
  START_TIMER();
  auto ret = ptr_jpeg_std_error(err);
  END_TIMER();
  return ret;
}
void d_jpeg_CreateCompress(j_compress_ptr cinfo, int version, size_t structsize)
{
  START_TIMER();
  ptr_jpeg_CreateCompress(cinfo, version, structsize);
  END_TIMER();
}
void d_jpeg_mem_dest(j_compress_ptr cinfo, unsigned char ** outbuffer, unsigned long * outsize)
{
  START_TIMER();
  ptr_jpeg_mem_dest(cinfo, outbuffer, outsize);
  END_TIMER();
}
void d_jpeg_set_defaults(j_compress_ptr cinfo)
{
  START_TIMER();
  ptr_jpeg_set_defaults(cinfo);
  END_TIMER();
}
void d_jpeg_set_quality(j_compress_ptr cinfo, int quality, boolean force_baseline)
{
  START_TIMER();
  ptr_jpeg_set_quality(cinfo, quality, force_baseline);
  END_TIMER();
}
void d_jpeg_start_compress(j_compress_ptr cinfo, boolean write_all_tables)
{
  START_TIMER();
  ptr_jpeg_start_compress(cinfo, write_all_tables);
  END_TIMER();
}
JDIMENSION d_jpeg_write_scanlines(j_compress_ptr cinfo, JSAMPARRAY scanlines, JDIMENSION num_lines)
{
  START_TIMER();
  auto ret = ptr_jpeg_write_scanlines(cinfo, scanlines, num_lines);
  END_TIMER();
  return ret;
}
void d_jpeg_finish_compress(j_compress_ptr cinfo)
{
  START_TIMER();
  ptr_jpeg_finish_compress(cinfo);
  END_TIMER();
}
void d_jpeg_destroy_compress(j_compress_ptr cinfo)
{
  START_TIMER();
  ptr_jpeg_destroy_compress(cinfo);
  END_TIMER();
}
void d_jpeg_CreateDecompress(j_decompress_ptr cinfo, int version, size_t structsize)
{
  START_TIMER();
  ptr_jpeg_CreateDecompress(cinfo, version, structsize);
  END_TIMER();
}
void d_jpeg_mem_src(j_decompress_ptr cinfo, unsigned char * inbuffer, unsigned long insize)
{
  START_TIMER();
  ptr_jpeg_mem_src(cinfo, inbuffer, insize);
  END_TIMER();
}
int d_jpeg_read_header(j_decompress_ptr cinfo, boolean require_image)
{
  START_TIMER();
  auto ret = ptr_jpeg_read_header(cinfo, require_image);
  END_TIMER();
  return ret;
}
boolean d_jpeg_start_decompress(j_decompress_ptr cinfo)
{
  START_TIMER();
  auto ret = ptr_jpeg_start_decompress(cinfo);
  END_TIMER();
  return ret;
}
JDIMENSION d_jpeg_read_scanlines(j_decompress_ptr cinfo, JSAMPARRAY scanlines, JDIMENSION max_lines)
{
  START_TIMER();
  auto ret = ptr_jpeg_read_scanlines(cinfo, scanlines, max_lines);
  END_TIMER();
  return ret;
}
boolean d_jpeg_finish_decompress(j_decompress_ptr cinfo)
{
  START_TIMER();
  auto ret = ptr_jpeg_finish_decompress(cinfo);
  END_TIMER();
  return ret;
}
void d_jpeg_destroy_decompress(j_decompress_ptr cinfo)
{
  START_TIMER();
  ptr_jpeg_destroy_decompress(cinfo);
  END_TIMER();
}

/* extern */JSAMPLE * image_buffer = NULL;  /* Points to large array of R,G,B-order data */
/* extern */int image_height = 0;        /* Number of rows in image */
/* extern */int image_width = 0;         /* Number of columns in image */
int curr_image_row = 0;
void* dlPtr;

void put_scanline_someplace(JSAMPROW rowBuffer, int row_stride)
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
write_JPEG_file (unsigned char ** p_outbuffer, unsigned long * p_outsize, int quality)
{
  /* This struct contains the JPEG compression parameters and pointers to
   * working space (which is allocated as needed by the JPEG library).
   * It is possible to have several such structures, representing multiple
   * compression/decompression processes, in existence at once.  We refer
   * to any one struct (and its associated working data) as a "JPEG object".
   */
  START_OUTER_TIMER();
  struct jpeg_compress_struct cinfo;
  /* This struct represents a JPEG error handler.  It is declared separately
   * because applications often want to supply a specialized error handler
   * (see the second half of this file for an example).  But here we just
   * take the easy way out and use the standard error handler, which will
   * print a message on stderr and call exit() if compression fails.
   * Note that this struct must live as long as the main JPEG parameter
   * struct, to avoid dangling-pointer problems.
   */
  struct jpeg_error_mgr jerr;
  /* More stuff */
 
  JSAMPROW row_pointer[1];      /* pointer to JSAMPLE row[s] */
  int row_stride;               /* physical row width in image buffer */

  /* Step 1: allocate and initialize JPEG compression object */

  /* We have to set up the error handler first, in case the initialization
   * step fails.  (Unlikely, but it could happen if you are out of memory.)
   * This routine fills in the contents of struct jerr, and returns jerr's
   * address which we place into the link field in cinfo.
   */
  cinfo.err = d_jpeg_std_error(&jerr);
  /* Now we can initialize the JPEG compression object. */
  d_jpeg_CreateCompress(&cinfo, JPEG_LIB_VERSION, (size_t) sizeof(struct jpeg_compress_struct));

  /* Step 2: specify data destination (eg, a file) */
  /* Note: steps 2 and 3 can be done in either order. */

  /* Here we use the library-supplied code to send compressed data to a
   * stdio stream.  You can also write your own code to do something else.
   * VERY IMPORTANT: use "b" option to fopen() if you are on a machine that
   * requires it in order to write binary files.
   */

  d_jpeg_mem_dest(&cinfo, p_outbuffer, p_outsize);


  /* Step 3: set parameters for compression */

  /* First we supply a description of the input image.
   * Four fields of the cinfo struct must be filled in:
   */
  cinfo.image_width = image_width;      /* image width and height, in pixels */
  cinfo.image_height = image_height;
  cinfo.input_components = 3;           /* # of color components per pixel */
  cinfo.in_color_space = JCS_RGB;       /* colorspace of input image */
  /* Now use the library's routine to set default compression parameters.
   * (You must set at least cinfo.in_color_space before calling this,
   * since the defaults depend on the source color space.)
   */
  d_jpeg_set_defaults(&cinfo);
  /* Now you can set any non-default parameters you wish to.
   * Here we just illustrate the use of quality (quantization table) scaling:
   */
  d_jpeg_set_quality(&cinfo, quality, TRUE /* limit to baseline-JPEG values */);

  /* Step 4: Start compressor */

  /* TRUE ensures that we will write a complete interchange-JPEG file.
   * Pass TRUE unless you are very sure of what you're doing.
   */
  d_jpeg_start_compress(&cinfo, TRUE);

  /* Step 5: while (scan lines remain to be written) */
  /*           jpeg_write_scanlines(...); */

  /* Here we use the library's state variable cinfo.next_scanline as the
   * loop counter, so that we don't have to keep track ourselves.
   * To keep things simple, we pass one scanline per call; you can pass
   * more if you wish, though.
   */
  row_stride = image_width * 3; /* JSAMPLEs per row in image_buffer */

  while (cinfo.next_scanline < cinfo.image_height) {
    /* jpeg_write_scanlines expects an array of pointers to scanlines.
     * Here the array is only one element long, but you could pass
     * more than one scanline at a time if that's more convenient.
     */
    row_pointer[0] = & image_buffer[cinfo.next_scanline * row_stride];
    (void) d_jpeg_write_scanlines(&cinfo, row_pointer, 1);
  }

  /* Step 6: Finish compression */

  d_jpeg_finish_compress(&cinfo);
  /* After finish_compress, we can close the output file. */

  /* Step 7: release JPEG compression object */

  /* This is an important step since it will release a good deal of memory. */
  d_jpeg_destroy_compress(&cinfo);

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

struct my_error_mgr {
  struct jpeg_error_mgr pub;    /* "public" fields */

  jmp_buf setjmp_buffer;        /* for return to caller */
};

typedef struct my_error_mgr * my_error_ptr;

/*
 * Here's the routine that will replace the standard error_exit method:
 */

METHODDEF(void)
my_error_exit (j_common_ptr cinfo)
{
  /* cinfo->err really points to a my_error_mgr struct, so coerce pointer */
  my_error_ptr myerr = (my_error_ptr) cinfo->err;

  /* Always display the message. */
  /* We could postpone this until after returning, if we chose. */
  (*cinfo->err->output_message) (cinfo);

  /* Return control to the setjmp point */
  longjmp(myerr->setjmp_buffer, 1);
}


/*
 * Sample routine for JPEG decompression.  We assume that the source file name
 * is passed in.  We want to return 1 on success, 0 on error.
 */


GLOBAL(int)
read_JPEG_file (unsigned char *fileBuff, unsigned long fsize)
{
  /* This struct contains the JPEG decompression parameters and pointers to
   * working space (which is allocated as needed by the JPEG library).
   */
  START_OUTER_TIMER();
  struct jpeg_decompress_struct cinfo;
  /* We use our private extension JPEG error handler.
   * Note that this struct must live as long as the main JPEG parameter
   * struct, to avoid dangling-pointer problems.
   */
  struct my_error_mgr jerr;
  /* More stuff */
  JSAMPARRAY buffer;            /* Output row buffer */
  int row_stride;               /* physical row width in output buffer */

  /* In this example we want to open the input file before doing anything else,
   * so that the setjmp() error recovery below can assume the file is open.
   * VERY IMPORTANT: use "b" option to fopen() if you are on a machine that
   * requires it in order to read binary files.
   */

  /* Step 1: allocate and initialize JPEG decompression object */

  /* We set up the normal JPEG error routines, then override error_exit. */
  cinfo.err = d_jpeg_std_error(&jerr.pub);
  jerr.pub.error_exit = my_error_exit;
  /* Establish the setjmp return context for my_error_exit to use. */
  if (setjmp(jerr.setjmp_buffer)) {
    /* If we get here, the JPEG code has signaled an error.
     * We need to clean up the JPEG object, close the input file, and return.
     */
    d_jpeg_destroy_decompress(&cinfo);
    return 0;
  }
  /* Now we can initialize the JPEG decompression object. */
  d_jpeg_CreateDecompress(&cinfo, JPEG_LIB_VERSION, (size_t) sizeof(struct jpeg_decompress_struct));

  /* Step 2: specify data source (eg, a file) */

  d_jpeg_mem_src(&cinfo, fileBuff, fsize);

  /* Step 3: read file parameters with jpeg_read_header() */

  (void) d_jpeg_read_header(&cinfo, TRUE);
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

  (void) d_jpeg_start_decompress(&cinfo);
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
  row_stride = cinfo.output_width * cinfo.output_components;
  /* Make a one-row-high sample array that will go away when done with image */
  buffer = (*cinfo.mem->alloc_sarray)
                ((j_common_ptr) &cinfo, JPOOL_IMAGE, row_stride, 1);


  //Save data
  image_height = cinfo.output_height;
  image_width = cinfo.output_width;
  image_buffer = (JSAMPLE *) malloc(image_width * image_height * 3 * sizeof(JSAMPLE));

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
  while (cinfo.output_scanline < cinfo.output_height) {
    /* jpeg_read_scanlines expects an array of pointers to scanlines.
     * Here the array is only one element long, but you could ask for
     * more than one scanline at a time if that's more convenient.
     */
    (void) d_jpeg_read_scanlines(&cinfo, buffer, 1);
    /* Assume put_scanline_someplace wants a pointer and sample count. */
    put_scanline_someplace(buffer[0], row_stride);
  }

  /* Step 7: Finish decompression */

  (void) d_jpeg_finish_decompress(&cinfo);
  /* We can ignore the return value since suspension is not possible
   * with the stdio data source.
   */

  /* Step 8: Release JPEG decompression object */

  /* This is an important step since it will release a good deal of memory. */
  d_jpeg_destroy_decompress(&cinfo);

  /* After finish_decompress, we can close the input file.
   * Here we postpone it until after no more JPEG errors are possible,
   * so as to simplify the setjmp error logic above.  (Actually, I don't
   * think that jpeg_destroy can do an error exit, but why assume anything...)
   */

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

int dynamicLoad(char* path)
{
  printf("Loading dynamic library %s\n", path);

  dlPtr = dlopen(path, RTLD_LAZY);

  if(!dlPtr)
  {
    printf("Loading of dynamic library %s has failed\n", path);
    return 0;
  }

  printf("Loading symbols\n");

  void* p_jpeg_std_error = dlsym(dlPtr, "jpeg_std_error");
  void* p_jpeg_CreateCompress = dlsym(dlPtr, "jpeg_CreateCompress");
  void* p_jpeg_mem_dest = dlsym(dlPtr, "jpeg_mem_dest");
  void* p_jpeg_set_defaults = dlsym(dlPtr, "jpeg_set_defaults");
  void* p_jpeg_set_quality = dlsym(dlPtr, "jpeg_set_quality");
  void* p_jpeg_start_compress = dlsym(dlPtr, "jpeg_start_compress");
  void* p_jpeg_write_scanlines = dlsym(dlPtr, "jpeg_write_scanlines");
  void* p_jpeg_finish_compress = dlsym(dlPtr, "jpeg_finish_compress");
  void* p_jpeg_destroy_compress = dlsym(dlPtr, "jpeg_destroy_compress");
  void* p_jpeg_CreateDecompress = dlsym(dlPtr, "jpeg_CreateDecompress");
  void* p_jpeg_mem_src = dlsym(dlPtr, "jpeg_mem_src");
  void* p_jpeg_read_header = dlsym(dlPtr, "jpeg_read_header");
  void* p_jpeg_start_decompress = dlsym(dlPtr, "jpeg_start_decompress");
  void* p_jpeg_read_scanlines = dlsym(dlPtr, "jpeg_read_scanlines");
  void* p_jpeg_finish_decompress = dlsym(dlPtr, "jpeg_finish_decompress");
  void* p_jpeg_destroy_decompress = dlsym(dlPtr, "jpeg_destroy_decompress");

  int failed = 0;
  if(p_jpeg_std_error == NULL) { printf("Symbol resolution failed for jpeg_std_error\n"); failed = 1; }
  if(p_jpeg_CreateCompress == NULL) { printf("Symbol resolution failed for jpeg_CreateCompress\n"); failed = 1; }
  if(p_jpeg_mem_dest == NULL) { printf("Symbol resolution failed for jpeg_mem_dest\n"); failed = 1; }
  if(p_jpeg_set_defaults == NULL) { printf("Symbol resolution failed for jpeg_set_defaults\n"); failed = 1; }
  if(p_jpeg_set_quality == NULL) { printf("Symbol resolution failed for jpeg_set_quality\n"); failed = 1; }
  if(p_jpeg_start_compress == NULL) { printf("Symbol resolution failed for jpeg_start_compress\n"); failed = 1; }
  if(p_jpeg_write_scanlines == NULL) { printf("Symbol resolution failed for jpeg_write_scanlines\n"); failed = 1; }
  if(p_jpeg_finish_compress == NULL) { printf("Symbol resolution failed for jpeg_finish_compress\n"); failed = 1; }
  if(p_jpeg_destroy_compress == NULL) { printf("Symbol resolution failed for jpeg_destroy_compress\n"); failed = 1; }
  if(p_jpeg_CreateDecompress == NULL) { printf("Symbol resolution failed for jpeg_CreateDecompress\n"); failed = 1; }
  if(p_jpeg_mem_src == NULL) { printf("Symbol resolution failed for jpeg_mem_src\n"); failed = 1; }
  if(p_jpeg_read_header == NULL) { printf("Symbol resolution failed for jpeg_read_header\n"); failed = 1; }
  if(p_jpeg_start_decompress == NULL) { printf("Symbol resolution failed for jpeg_start_decompress\n"); failed = 1; }
  if(p_jpeg_read_scanlines == NULL) { printf("Symbol resolution failed for jpeg_read_scanlines\n"); failed = 1; }
  if(p_jpeg_finish_decompress == NULL) { printf("Symbol resolution failed for jpeg_finish_decompress\n"); failed = 1; }
  if(p_jpeg_destroy_decompress == NULL) { printf("Symbol resolution failed for jpeg_destroy_decompress\n"); failed = 1; }

  if(failed) { return 0; }

  *((void **) &ptr_jpeg_std_error) = p_jpeg_std_error;
  *((void **) &ptr_jpeg_CreateCompress) = p_jpeg_CreateCompress;
  *((void **) &ptr_jpeg_mem_dest) = p_jpeg_mem_dest;
  *((void **) &ptr_jpeg_set_defaults) = p_jpeg_set_defaults;
  *((void **) &ptr_jpeg_set_quality) = p_jpeg_set_quality;
  *((void **) &ptr_jpeg_start_compress) = p_jpeg_start_compress;
  *((void **) &ptr_jpeg_write_scanlines) = p_jpeg_write_scanlines;
  *((void **) &ptr_jpeg_finish_compress) = p_jpeg_finish_compress;
  *((void **) &ptr_jpeg_destroy_compress) = p_jpeg_destroy_compress;
  *((void **) &ptr_jpeg_CreateDecompress) = p_jpeg_CreateDecompress;
  *((void **) &ptr_jpeg_mem_src) = p_jpeg_mem_src;
  *((void **) &ptr_jpeg_read_header) = p_jpeg_read_header;
  *((void **) &ptr_jpeg_start_decompress) = p_jpeg_start_decompress;
  *((void **) &ptr_jpeg_read_scanlines) = p_jpeg_read_scanlines;
  *((void **) &ptr_jpeg_finish_decompress) = p_jpeg_finish_decompress;
  *((void **) &ptr_jpeg_destroy_decompress) = p_jpeg_destroy_decompress;

  return 1;
}

int main(int argc, char** argv)
{
  START_PROGRAM_TIMER();
  if(argc < 4)
  {
    printf("No io files specified. Expected arg example input.jpeg output.jpeg libjpeg.so\n");
    return 1;
  }

  printf("Starting\n");

  if(!dynamicLoad(argv[3]))
  {
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

  unsigned char *fileBuff = (unsigned char *) malloc(fsize + 1);
  if(!fread(fileBuff, fsize, 1, infile))
  {
    return 1;
  }

  fileBuff[fsize] = 0;

  FILE * outfile;
  if ((outfile = fopen(argv[2], "wb")) == NULL) {
    fprintf(stderr, "can't open %s\n", argv[2]);
    return 0;
  }

  unsigned char ** p_outbuffer = (unsigned char **) malloc(sizeof(unsigned char *));
  unsigned long * p_outsize = (unsigned long *) malloc(sizeof(unsigned long));
  *p_outbuffer = 0;
  *p_outsize = 0;

  if(!read_JPEG_file(fileBuff, fsize))
  {
    if(image_buffer)
    {
      free(image_buffer);
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
    printf("Writing to fe %s failed\n", argv[2]);
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

  free(image_buffer);
  dlclose(dlPtr);

  fwrite(*p_outbuffer, *p_outsize, 1, outfile);
  fclose(outfile);
  fclose(infile);
  free(fileBuff);
  free(p_outbuffer);
  free(p_outsize);
  END_PROGRAM_TIMER();

  #ifdef PRINT_FUNCTION_TIMES
    #if defined(_WIN32)
      printf("JPEG program time = %I64d ns\n", programTime);
    #else
      printf("JPEG program time = %lld ns\n", programTime);
    #endif
  #endif

  printf("Success\n");
  fflush(stdout);
  return 0;
}