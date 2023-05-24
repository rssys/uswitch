#pragma once

#include <stdio.h>  // FILE*
#include "generic_helpers.h"
#include "png.h"

#define LIB PNG
namespace LIB {

// Must define callback types by these aliases
typedef png_error_ptr CB_TYPE_0;  // void (*)(png_structp, png_const_charp)
typedef png_progressive_info_ptr CB_TYPE_1;  // void (*)(png_structp, png_infop)
typedef png_progressive_row_ptr CB_TYPE_2;  // void (*)(png_structp, png_bytep, png_uint_32, int)
typedef png_progressive_end_ptr CB_TYPE_3;  // void (*)(png_structp, png_infop)
typedef png_progressive_frame_ptr CB_TYPE_4;  // void (*)(png_structp, png_uint_32)
typedef png_longjmp_ptr CB_TYPE_5;

// unused, but we require these typedef'd to some function-pointer type (for now)
typedef void (*CB_UNUSED)();
typedef CB_UNUSED CB_TYPE_6;
typedef CB_UNUSED CB_TYPE_7;

};  // namespace LIB

// All types used by library functions must be a single preprocessor token, so that
//   they play nice with macros
// This means that we can't have two-word types like "struct x", nor types that contain *
typedef int* INTSTAR;
typedef double* DOUBLESTAR;
typedef jmp_buf* JMP_BUF_STAR;
typedef png_bytep* PNG_BYTEP_STAR;
typedef png_color_16p* PNG_COLOR_16P_STAR;
typedef png_uint_32* PNG_UINT_32_STAR;

// Like FOR_EACH_0ARG_LIBRARY_FUNCTION, but only void functions
#define FOR_EACH_0ARG_VOID_LIBRARY_FUNCTION(macro) \

// Like FOR_EACH_0ARG_LIBRARY_FUNCTION, but only nonvoid functions
#define FOR_EACH_0ARG_NONVOID_LIBRARY_FUNCTION(macro) \

// Like FOR_EACH_1ARG_LIBRARY_FUNCTION, but only void functions
#define FOR_EACH_1ARG_VOID_LIBRARY_FUNCTION(macro) \
  macro(png_set_expand, void, png_structrp) \
  macro(png_set_gray_to_rgb, void, png_structrp) \
  macro(png_set_scale_16, void, png_structrp) \

// Like FOR_EACH_1ARG_LIBRARY_FUNCTION, but only nonvoid functions
#define FOR_EACH_1ARG_NONVOID_LIBRARY_FUNCTION(macro) \
  macro(png_create_info_struct, png_infop, png_const_structrp) \
  macro(png_set_interlace_handling, int, png_structrp) \
  macro(png_get_progressive_ptr, png_voidp, png_const_structrp) \

// Like FOR_EACH_2ARG_LIBRARY_FUNCTION, but only void functions
#define FOR_EACH_2ARG_VOID_LIBRARY_FUNCTION(macro) \
  macro(png_set_chunk_malloc_max, void, png_structrp, png_alloc_size_t) \
  macro(png_set_check_for_invalid_index, void, png_structrp, int) \
  macro(png_read_update_info, void, png_structrp, png_inforp) \
  macro(png_error, void, png_const_structrp, png_const_charp) \
  macro(png_longjmp, void, png_const_structrp, int) \

// Like FOR_EACH_2ARG_LIBRARY_FUNCTION, but only nonvoid functions
#define FOR_EACH_2ARG_NONVOID_LIBRARY_FUNCTION(macro) \
  macro(png_get_next_frame_delay_num, png_uint_16, png_structp, png_infop) \
  macro(png_get_next_frame_delay_den, png_uint_16, png_structp, png_infop) \
  macro(png_get_next_frame_dispose_op, png_byte, png_structp, png_infop) \
  macro(png_get_next_frame_blend_op, png_byte, png_structp, png_infop) \
  macro(png_get_channels, png_byte, png_const_structrp, png_const_inforp) \
  macro(png_get_first_frame_is_hidden, png_byte, png_structp, png_infop) \
  macro(png_process_data_pause, png_size_t, png_structrp, int) \
  macro(png_get_num_plays, png_uint_32, png_structp, png_infop) \
  macro(png_get_next_frame_x_offset, png_uint_32, png_structp, png_infop) \
  macro(png_get_next_frame_y_offset, png_uint_32, png_structp, png_infop) \
  macro(png_get_next_frame_width, png_uint_32, png_structp, png_infop) \
  macro(png_get_next_frame_height, png_uint_32, png_structp, png_infop) \

// Like FOR_EACH_3ARG_LIBRARY_FUNCTION, but only void functions
#define FOR_EACH_3ARG_VOID_LIBRARY_FUNCTION(macro) \
  macro(png_destroy_read_struct, void, png_structpp, png_infopp, png_infopp) \
  macro(png_set_user_limits, void, png_structrp, png_uint_32, png_uint_32) \
  macro(png_set_gAMA, void, png_const_structrp, png_inforp, double) \
  macro(png_set_gamma, void, png_structrp, double, double) \
  macro(png_set_progressive_frame_fn, void, png_structp, png_progressive_frame_ptr, png_progressive_frame_ptr) \
  macro(png_progressive_combine_row, void, png_const_structrp, png_bytep, png_const_bytep) \

// Like FOR_EACH_3ARG_LIBRARY_FUNCTION, but only nonvoid functions
#define FOR_EACH_3ARG_NONVOID_LIBRARY_FUNCTION(macro) \
  macro(png_set_option, int, png_structrp, int, int) \
  macro(png_get_gAMA, png_uint_32, png_const_structrp, png_const_inforp, DOUBLESTAR) \
  macro(png_get_sRGB, png_uint_32, png_const_structrp, png_const_inforp, INTSTAR) \
  macro(png_get_valid, png_uint_32, png_const_structrp, png_const_inforp, png_uint_32) \
  macro(png_set_longjmp_fn, JMP_BUF_STAR, png_structrp, png_longjmp_ptr, size_t) \

// Like FOR_EACH_4ARG_LIBRARY_FUNCTION, but only void functions
#define FOR_EACH_4ARG_VOID_LIBRARY_FUNCTION(macro) \
  macro(png_set_keep_unknown_chunks, void, png_structrp, int, png_const_bytep, int) \
  macro(png_free_data, void, png_const_structrp, png_inforp, png_uint_32, int) \
  macro(png_process_data, void, png_structrp, png_inforp, png_bytep, png_size_t) \

// Like FOR_EACH_4ARG_LIBRARY_FUNCTION, but only nonvoid functions
#define FOR_EACH_4ARG_NONVOID_LIBRARY_FUNCTION(macro) \
  macro(png_create_read_struct, png_structp, png_const_charp, png_voidp, png_error_ptr, png_error_ptr) \

// Like FOR_EACH_5ARG_LIBRARY_FUNCTION, but only void functions
#define FOR_EACH_5ARG_VOID_LIBRARY_FUNCTION(macro) \
  macro(png_set_progressive_read_fn, void, png_structrp, png_voidp, png_progressive_info_ptr, png_progressive_row_ptr, png_progressive_end_ptr) \

// Like FOR_EACH_5ARG_LIBRARY_FUNCTION, but only nonvoid functions
#define FOR_EACH_5ARG_NONVOID_LIBRARY_FUNCTION(macro) \
  macro(png_get_tRNS, png_uint_32, png_const_structrp, png_inforp, PNG_BYTEP_STAR, INTSTAR, PNG_COLOR_16P_STAR) \

// Like FOR_EACH_6ARG_LIBRARY_FUNCTION, but only void functions
#define FOR_EACH_6ARG_VOID_LIBRARY_FUNCTION(macro) \

// Like FOR_EACH_6ARG_LIBRARY_FUNCTION, but only nonvoid functions
#define FOR_EACH_6ARG_NONVOID_LIBRARY_FUNCTION(macro) \
  macro(png_get_iCCP, png_uint_32, png_const_structrp, png_inforp, png_charpp, INTSTAR, png_bytepp, PNG_UINT_32_STAR) \

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
  macro(png_get_IHDR, png_uint_32, png_const_structrp, png_const_inforp, PNG_UINT_32_STAR, PNG_UINT_32_STAR, INTSTAR, INTSTAR, INTSTAR, INTSTAR, INTSTAR) \

// Like FOR_EACH_10ARG_LIBRARY_FUNCTION, but only void functions
#define FOR_EACH_10ARG_VOID_LIBRARY_FUNCTION(macro) \

// Like FOR_EACH_10ARG_LIBRARY_FUNCTION, but only nonvoid functions
#define FOR_EACH_10ARG_NONVOID_LIBRARY_FUNCTION(macro) \
  macro(png_get_cHRM, png_uint_32, png_const_structrp, png_const_inforp, DOUBLESTAR, DOUBLESTAR, DOUBLESTAR, DOUBLESTAR, DOUBLESTAR, DOUBLESTAR, DOUBLESTAR, DOUBLESTAR) \

// Like FOR_EACH_11ARG_LIBRARY_FUNCTION, but only void functions
#define FOR_EACH_11ARG_VOID_LIBRARY_FUNCTION(macro) \

// Like FOR_EACH_11ARG_LIBRARY_FUNCTION, but only nonvoid functions
#define FOR_EACH_11ARG_NONVOID_LIBRARY_FUNCTION(macro) \

