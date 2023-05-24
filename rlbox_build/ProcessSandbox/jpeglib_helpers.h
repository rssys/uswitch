#pragma once

#include <stdio.h>  // FILE*
#include "generic_helpers.h"
#include "jpeglib.h"

#define LIB JPEG
namespace LIB {

// Callback signatures
typedef void (*t_my_error_exit) (j_common_ptr cinfo);
typedef void (*t_init_source) (j_decompress_ptr jd);
typedef void (*t_skip_input_data) (j_decompress_ptr jd, long num_bytes);
typedef boolean (*t_fill_input_buffer) (j_decompress_ptr jd);
typedef void (*t_term_source) (j_decompress_ptr jd);
typedef boolean (*t_jpeg_resync_to_restart) (j_decompress_ptr cinfo, int desired);

// Must define callback types by these aliases
typedef t_my_error_exit CB_TYPE_0;
typedef t_init_source CB_TYPE_1;
typedef t_skip_input_data CB_TYPE_2;
typedef t_fill_input_buffer CB_TYPE_3;
typedef t_term_source CB_TYPE_4;
typedef t_jpeg_resync_to_restart CB_TYPE_5;

// unused, but we require these typedef'd to some function-pointer type (for now)
typedef void (*CB_UNUSED)();
typedef CB_UNUSED CB_TYPE_6;
typedef CB_UNUSED CB_TYPE_7;

};  // namespace LIB

// All types used by library functions must be a single preprocessor token, so that
//   they play nice with macros
// This means that we can't have two-word types like "struct x", nor types that contain *
typedef struct jpeg_error_mgr* JPEG_ERROR_MGR;
typedef FILE* FILESTAR;
typedef char* CHARSTAR;
typedef void* VOIDSTAR;
typedef unsigned char* UCHARSTAR;
typedef unsigned char** UCHARSTARSTAR;
typedef unsigned long ULONG;
typedef unsigned long* ULONGSTAR;

// Like FOR_EACH_0ARG_LIBRARY_FUNCTION, but only void functions
#define FOR_EACH_0ARG_VOID_LIBRARY_FUNCTION(macro) \

// Like FOR_EACH_0ARG_LIBRARY_FUNCTION, but only nonvoid functions
#define FOR_EACH_0ARG_NONVOID_LIBRARY_FUNCTION(macro) \

// Like FOR_EACH_1ARG_LIBRARY_FUNCTION, but only void functions
#define FOR_EACH_1ARG_VOID_LIBRARY_FUNCTION(macro) \
  macro(jpeg_set_defaults, void, j_compress_ptr) \
  macro(jpeg_finish_compress, void, j_compress_ptr) \
  macro(jpeg_destroy_compress, void, j_compress_ptr) \
  macro(jpeg_destroy_decompress, void, j_decompress_ptr) \
  macro(jpeg_calc_output_dimensions, void, j_decompress_ptr)

// Like FOR_EACH_1ARG_LIBRARY_FUNCTION, but only nonvoid functions
#define FOR_EACH_1ARG_NONVOID_LIBRARY_FUNCTION(macro) \
  macro(jpeg_std_error, JPEG_ERROR_MGR, JPEG_ERROR_MGR) \
  macro(jpeg_start_decompress, boolean, j_decompress_ptr) \
  macro(jpeg_finish_decompress, boolean, j_decompress_ptr) \
  macro(jpeg_has_multiple_scans, boolean, j_decompress_ptr) \
  macro(jpeg_finish_output, boolean, j_decompress_ptr) \
  macro(jpeg_input_complete, boolean, j_decompress_ptr) \
  macro(jpeg_consume_input, int, j_decompress_ptr) \
  /*macro(my_error_exit, t_my_error_exit, j_common_ptr)*/ \
  /*macro(init_source, t_init_source, j_decompress_ptr)*/ \
  /*macro(fill_input_buffer, t_fill_input_buffer, j_decompress_ptr)*/ \
  /*macro(term_source, t_term_source, j_decompress_ptr)*/

// Like FOR_EACH_2ARG_LIBRARY_FUNCTION, but only void functions
#define FOR_EACH_2ARG_VOID_LIBRARY_FUNCTION(macro) \
  macro(jpeg_stdio_dest, void, j_compress_ptr, FILESTAR) \
  macro(jpeg_start_compress, void, j_compress_ptr, boolean) \
  macro(jpeg_stdio_src, void, j_decompress_ptr, FILESTAR) \
  /*macro(format_message, void, j_common_ptr, CHARSTAR)*/ \

// Like FOR_EACH_2ARG_LIBRARY_FUNCTION, but only nonvoid functions
#define FOR_EACH_2ARG_NONVOID_LIBRARY_FUNCTION(macro) \
  macro(jpeg_read_header, int, j_decompress_ptr, boolean) \
  macro(jpeg_start_output, boolean, j_decompress_ptr, int) \
  /*macro(skip_input_data, t_skip_input_data, j_decompress_ptr, long)*/ \
  /*macro(jpeg_resync_to_restart, t_jpeg_resync_to_restart, j_decompress_ptr, int)*/

// Like FOR_EACH_3ARG_LIBRARY_FUNCTION, but only void functions
#define FOR_EACH_3ARG_VOID_LIBRARY_FUNCTION(macro) \
  macro(jpeg_CreateCompress, void, j_compress_ptr, int, size_t) \
  macro(jpeg_set_quality, void, j_compress_ptr, int, boolean) \
  macro(jpeg_CreateDecompress, void, j_decompress_ptr, int, size_t) \
  macro(jpeg_save_markers, void, j_decompress_ptr, int, unsigned) \
  macro(jpeg_mem_dest, void, j_compress_ptr, UCHARSTARSTAR, ULONGSTAR) \
  macro(jpeg_mem_src, void, j_decompress_ptr, UCHARSTAR, ULONG)

// Like FOR_EACH_3ARG_LIBRARY_FUNCTION, but only nonvoid functions
#define FOR_EACH_3ARG_NONVOID_LIBRARY_FUNCTION(macro) \
  macro(jpeg_write_scanlines, JDIMENSION, j_compress_ptr, JSAMPARRAY, JDIMENSION) \
  macro(jpeg_read_scanlines, JDIMENSION, j_decompress_ptr, JSAMPARRAY, JDIMENSION)

// Like FOR_EACH_4ARG_LIBRARY_FUNCTION, but only void functions
#define FOR_EACH_4ARG_VOID_LIBRARY_FUNCTION(macro) \

// Like FOR_EACH_4ARG_LIBRARY_FUNCTION, but only nonvoid functions
#define FOR_EACH_4ARG_NONVOID_LIBRARY_FUNCTION(macro) \
	macro(alloc_sarray_ps, JSAMPARRAY, j_common_ptr, int, JDIMENSION, JDIMENSION)

// Like FOR_EACH_5ARG_LIBRARY_FUNCTION, but only void functions
#define FOR_EACH_5ARG_VOID_LIBRARY_FUNCTION(macro) \

// Like FOR_EACH_5ARG_LIBRARY_FUNCTION, but only nonvoid functions
#define FOR_EACH_5ARG_NONVOID_LIBRARY_FUNCTION(macro) \

// Like FOR_EACH_6ARG_LIBRARY_FUNCTION, but only void functions
#define FOR_EACH_6ARG_VOID_LIBRARY_FUNCTION(macro) \

// Like FOR_EACH_6ARG_LIBRARY_FUNCTION, but only nonvoid functions
#define FOR_EACH_6ARG_NONVOID_LIBRARY_FUNCTION(macro) \

// Like FOR_EACH_7ARG_LIBRARY_FUNCTION, but only void functions
#define FOR_EACH_7ARG_VOID_LIBRARY_FUNCTION(macro) \

// Like FOR_EACH_7ARG_LIBRARY_FUNCTION, but only nonvoid functions
#define FOR_EACH_7ARG_NONVOID_LIBRARY_FUNCTION(macro) \

// Like FOR_EACH_8ARG_LIBRARY_FUNCTION, but only void functions
#define FOR_EACH_8ARG_VOID_LIBRARY_FUNCTION(macro) \

// Like FOR_EACH_8ARG_LIBRARY_FUNCTION, but only nonvoid functions
#define FOR_EACH_8ARG_NONVOID_LIBRARY_FUNCTION(macro) \

// Like FOR_EACH_9ARG_LIBRARY_FUNCTION, but only void functions
#define FOR_EACH_9ARG_VOID_LIBRARY_FUNCTION(macro) \

// Like FOR_EACH_9ARG_LIBRARY_FUNCTION, but only nonvoid functions
#define FOR_EACH_9ARG_NONVOID_LIBRARY_FUNCTION(macro) \

// Like FOR_EACH_10ARG_LIBRARY_FUNCTION, but only void functions
#define FOR_EACH_10ARG_VOID_LIBRARY_FUNCTION(macro) \

// Like FOR_EACH_10ARG_LIBRARY_FUNCTION, but only nonvoid functions
#define FOR_EACH_10ARG_NONVOID_LIBRARY_FUNCTION(macro) \

// Like FOR_EACH_11ARG_LIBRARY_FUNCTION, but only void functions
#define FOR_EACH_11ARG_VOID_LIBRARY_FUNCTION(macro) \

// Like FOR_EACH_11ARG_LIBRARY_FUNCTION, but only nonvoid functions
#define FOR_EACH_11ARG_NONVOID_LIBRARY_FUNCTION(macro) \

// alloc_sarray is not a normal function in libjpeg
//   but we want to call it like one
//   so we declare this wrapper at global scope
inline JSAMPARRAY alloc_sarray_ps(j_common_ptr cinfo, int pool_id, JDIMENSION samplesperrow, JDIMENSION numrows) {
	return cinfo->mem->alloc_sarray(cinfo, pool_id, samplesperrow, numrows);
}
