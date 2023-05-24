#include <iostream>
#include <stdlib.h>
#include <string>
#include <inttypes.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <chrono>
#include <memory>

#include <setjmp.h>
#include "jpeglib.h"

using namespace std::chrono;

jmp_buf setjmp_buffer;

struct decoder_error_mgr {
  struct jpeg_error_mgr pub;    /* "public" fields */

  jmp_buf setjmp_buffer;        /* for return to caller */
};

typedef struct decoder_error_mgr * my_error_ptr;

#ifdef USE_NACL
	#include "nacl_sandbox.h"
	NaClSandbox* sandbox;
	#include "jpeglib_structs_for_cpp_api.h"
	sandbox_nacl_load_library_api(jpeglib)
#elif defined(USE_PROCESS)
  #include "ProcessSandbox.h"
  #include "process_sandbox_cpp.h"
	JPEGProcessSandbox* sandbox;
	#include "jpeglib_structs_for_cpp_api.h"
	sandbox_nacl_load_library_api(jpeglib)
#elif defined(USE_NONE)
#else
	#error No sandbox type defined.
#endif



const char* images[] = {
	"../../scaling_jpgs/5.jpg",
	"../../scaling_jpgs/10.jpg",
	"../../scaling_jpgs/15.jpg",
	"../../scaling_jpgs/20.jpg",
	"../../scaling_jpgs/25.jpg",
	"../../scaling_jpgs/30.jpg",
	"../../scaling_jpgs/35.jpg",
	"../../scaling_jpgs/40.jpg",
	"../../scaling_jpgs/45.jpg",
	"../../scaling_jpgs/50.jpg",
	"../../scaling_jpgs/55.jpg",
	"../../scaling_jpgs/60.jpg",
	"../../scaling_jpgs/65.jpg",
	"../../scaling_jpgs/70.jpg",
	"../../scaling_jpgs/75.jpg",
	"../../scaling_jpgs/80.jpg",
	"../../scaling_jpgs/85.jpg",
	"../../scaling_jpgs/90.jpg",
	"../../scaling_jpgs/95.jpg",
	"../../scaling_jpgs/100.jpg"
};

#define TestCount sizeof(images)/sizeof(char*)

uint64_t results[TestCount];

#ifdef USE_NONE
METHODDEF(void)
my_error_exit (j_common_ptr cinfo)
{
  /* Return control to the setjmp point */
  longjmp(setjmp_buffer, 1);
}

void put_scanline_someplace(JSAMPROW rowBuffer, int row_stride)
{
}
#else
METHODDEF(void)
my_error_exit (unverified_data<j_common_ptr> cinfo)
{
  /* Return control to the setjmp point */
  longjmp(setjmp_buffer, 1);
}

void put_scanline_someplace(unverified_data<JSAMPROW> rowBuffer, int row_stride)
{
}
#endif

#if defined(USE_NACL) || defined(USE_PROCESS)
GLOBAL(int)
read_JPEG_file (unverified_data<unsigned char *> fileBuff, unsigned long fsize)
{
  /* This struct contains the JPEG decompression parameters and pointers to
   * working space (which is allocated as needed by the JPEG library).
   */
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
  auto image_height = p_cinfo->output_height.UNSAFE_noVerify();
  auto image_width = p_cinfo->output_width.UNSAFE_noVerify();
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

  /* And we're done! */
  return 1;
}
#endif

#ifdef USE_NONE
GLOBAL(int)
read_JPEG_file (unsigned char * fileBuff, unsigned long fsize)
{
  /* This struct contains the JPEG decompression parameters and pointers to
   * working space (which is allocated as needed by the JPEG library).
   */
  auto p_cinfo = new jpeg_decompress_struct();

  /* We use our private extension JPEG error handler.
   * Note that this struct must live as long as the main JPEG parameter
   * struct, to avoid dangling-pointer problems.
   */
  auto p_jerr = new decoder_error_mgr();

  /* More stuff */
  auto p_buffer = new JSAMPARRAY();            /* Output row buffer */

  int row_stride;               /* physical row width in output buffer */

  /* In this example we want to open the input file before doing anything else,
   * so that the setjmp() error recovery below can assume the file is open.
   * VERY IMPORTANT: use "b" option to fopen() if you are on a machine that
   * requires it in order to read binary files.
   */

  /* Step 1: allocate and initialize JPEG decompression object */

  /* We set up the normal JPEG error routines, then override error_exit. */
  p_cinfo->err = jpeg_std_error(&(p_jerr->pub));

  p_jerr->pub.error_exit = my_error_exit;

  /* Establish the setjmp return context for my_error_exit to use. */
  if (setjmp(setjmp_buffer)) {
    /* If we get here, the JPEG code has signaled an error.
     * We need to clean up the JPEG object, close the input file, and return.
     */
    jpeg_destroy_decompress(p_cinfo);
    return 0;
  }

  /* Now we can initialize the JPEG decompression object. */
  jpeg_CreateDecompress(p_cinfo, JPEG_LIB_VERSION, (size_t) sizeof(struct jpeg_decompress_struct));

  /* Step 2: specify data source (eg, a file) */
  jpeg_mem_src(p_cinfo, fileBuff, fsize);

  /* Step 3: read file parameters with jpeg_read_header() */

  (void) jpeg_read_header(p_cinfo, TRUE);
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

  (void) jpeg_start_decompress(p_cinfo);
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
  auto image_height = p_cinfo->output_height;
  auto image_width = p_cinfo->output_width;
  row_stride = image_width * p_cinfo->output_components;

  /* Make a one-row-high sample array that will go away when done with image */
  //this is a function pointer we execute, nothing to verify
  auto p_alloc_sarray = p_cinfo->mem->alloc_sarray;

  JSAMPARRAY pBufferVal = p_alloc_sarray((j_common_ptr) p_cinfo, JPOOL_IMAGE, row_stride, 1);

  *p_buffer = pBufferVal;

  /* Step 6: while (scan lines remain to be read) */
  /*           jpeg_read_scanlines(...); */

  /* Here we use the library's state variable cinfo.output_scanline as the
   * loop counter, so that we don't have to keep track ourselves.
   */
  //This could cause a Denial of Service, but we do not handle that
  while (p_cinfo->output_scanline < image_height) {
    /* jpeg_read_scanlines expects an array of pointers to scanlines.
     * Here the array is only one element long, but you could ask for
     * more than one scanline at a time if that's more convenient.
     */

    JSAMPARRAY pBufferSys = *p_buffer;
    (void) jpeg_read_scanlines(p_cinfo, pBufferSys, 1);

    /* Assume put_scanline_someplace wants a pointer and sample count. */
    put_scanline_someplace(pBufferSys[0], row_stride);
  }

  /* Step 7: Finish decompression */

  (void) jpeg_finish_decompress(p_cinfo);
  /* We can ignore the return value since suspension is not possible
   * with the stdio data source.
   */

  /* Step 8: Release JPEG decompression object */

  /* This is an important step since it will release a good deal of memory. */
  jpeg_destroy_decompress(p_cinfo);

  /* After finish_decompress, we can close the input file.
   * Here we postpone it until after no more JPEG errors are possible,
   * so as to simplify the setjmp error logic above.  (Actually, I don't
   * think that jpeg_destroy can do an error exit, but why assume anything...)
   */

  free(p_cinfo);
  free(p_jerr);
  free(p_buffer);

  /* At this point you may want to check to see whether any corrupt-data
   * warnings occurred (test whether jerr.pub.num_warnings is nonzero).
   */

  /* And we're done! */
  return 1;
}
#endif

void setup(const char* isolationFramework, const char* libraryPath, const char* maincore_as_str, const char* sbcore_as_str, int sbpriority)
{
#ifdef USE_NACL
  printf("Creating NaCl Sandbox for %s\n", libraryPath);
  initializeDlSandboxCreator(0 /* Should enable detailed logging */);
  sandbox = createDlSandbox(isolationFramework, libraryPath);
  initCPPApi(sandbox);
#elif defined(USE_PROCESS)
  printf("Creating process sandbox\n");
  unsigned maincore, sbcore;
  if(!sscanf(maincore_as_str, "%u", &maincore)) {
    printf("Bad maincore argument\n");
    exit(1);
  }
  if(!sscanf(sbcore_as_str, "%u", &sbcore)) {
    printf("Bad sandboxcore argument\n");
    exit(1);
  }
  sandbox = new JPEGProcessSandbox(libraryPath, maincore, sbcore);
  #if defined(USE_PROCESS_SPIN)
    sandbox->makeActiveSandbox();
  #endif
  initCPPApi(sandbox);
#else
#endif
}

void runTest(uint64_t* resultsArr)
{
	for(int i = 0; i < 20; i++)
	{
		results[i] = 0;
	}
	for (int i = 0; i < TestCount; ++i)
	{
		std::string test = images[i];

		FILE* infile;
		if ((infile = fopen(test.c_str(), "rb")) == NULL) 
		{
			std::cout << "can't open " << test << "\n";
			exit(1);
		}

		fseek(infile, 0, SEEK_END);
		unsigned long fsize = ftell(infile);
		fseek(infile, 0, SEEK_SET);  //same as rewind(infile);

		#ifdef USE_NONE
		unsigned char * fileBuff = new unsigned char[fsize + 1];
		//only reading from a buffer, no verification necessary
		if(!fread(fileBuff, fsize, 1, infile))
		#else
		unverified_data<unsigned char *> fileBuff = newInSandbox<unsigned char>(sandbox, fsize + 1);
		//only reading from a buffer, no verification necessary
		if(!fread(fileBuff.sandbox_onlyVerifyAddress(), fsize, 1, infile))
		#endif
		{
			std::cout << "can't read " << test << "\n";
			exit(1);
		}

		fileBuff[fsize] = 0;
		fclose(infile);


		high_resolution_clock::time_point enterTime = high_resolution_clock::now();
		read_JPEG_file(fileBuff, fsize);
		high_resolution_clock::time_point exitTime = high_resolution_clock::now();
		#ifdef USE_NONE
		free(fileBuff);
		#else
		freeInSandbox(sandbox, fileBuff);
		#endif
		resultsArr[i] = duration_cast<nanoseconds>(exitTime - enterTime).count();
	}
}

void switchToCurrentFolder(std::string executablePath);

int main(int argc, char const *argv[])
{
	#ifdef USE_NACL
		if(argc < 3)
		{
			printf("Expected arg libjpeg.nexe naclIrtPath\n");
			exit(1);
		}
		setup(argv[2], argv[1], nullptr, nullptr, 0);
	#elif defined(USE_PROCESS)
		if(argc < 5)
		{
			printf("Error: ProcessSandbox_otherside main_process_core sandbox_process_core\n");
			exit(1);
		}
		int defaultlibprio;
	    if(!sscanf(argv[4], "%i", &defaultlibprio)) { printf("Bad argument 4\n"); exit(1);}
		setup(nullptr, argv[1], argv[2], argv[3], defaultlibprio);
	#endif

	switchToCurrentFolder(argv[0]);

	std::cout << "Running warmup rounds\n";
	for(int i = 0; i < 3; i++)
	{
		runTest(results);
	}

	runTest(results);
	for (int i = 0; i < TestCount; ++i)
	{
		std::cout << images[i] << " "  << results[i] << "\n";
	}

	return 0;
}

void switchToCurrentFolder(std::string executablePath)
{
	auto index = executablePath.find_last_of("/");

	if(index == std::string::npos || index == 1)
	{
		//already in current folder
		return;
	}
	else
	{
		std::string targetDir = executablePath.substr(0, index);
		auto failed = chdir(targetDir.c_str());
		if(failed)
		{
			std::cout << "Couldn't change working dir: "  << executablePath  << " | " << targetDir << "\n";
			exit(1);
		}
	}
}
