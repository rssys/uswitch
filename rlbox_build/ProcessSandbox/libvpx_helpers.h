#pragma once

#include "generic_helpers.h"
#include "vpx/vp8.h"
#include "vpx/vp8dx.h"
#include "vpx/vpx_codec.h"
#include "vpx/vpx_decoder.h"

// Note: Currently we only have helpers for:
//   - vp8dx.h, vpx_codec.h, and vpx_decoder.h, as those seem to be the headers used in FF's VPXDecoder.h
//   - vp8.h, vpx_frame_buffer.h, vpx_image.h, vpx_integer.h, and vpx_image.h, as those are included by the headers in the group above

#define LIB VPX
namespace LIB {

// Must define callback types by these aliases
typedef vpx_decrypt_cb CB_TYPE_0;
typedef vpx_codec_put_frame_cb_fn_t CB_TYPE_1;
typedef vpx_codec_put_slice_cb_fn_t CB_TYPE_2;
typedef vpx_get_frame_buffer_cb_fn_t CB_TYPE_3;
typedef vpx_release_frame_buffer_cb_fn_t CB_TYPE_4;

// unused, but we require these typedef'd to some function-pointer type (for now)
typedef void (*CB_UNUSED)();
typedef CB_UNUSED CB_TYPE_5;
typedef CB_UNUSED CB_TYPE_6;
typedef CB_UNUSED CB_TYPE_7;

};  // namespace LIB

// All types used by library functions must be a single preprocessor token, so that
//   they play nice with macros
// This means that we can't have two-word types like "struct x", nor types that contain *
typedef void* VOIDSTAR;
typedef char* CHARSTAR;
typedef unsigned char* UCHARSTAR;
typedef const char* CONST_CHAR_STAR;
typedef const uint8_t* CONST_UINT8_T_STAR;
typedef vpx_codec_iface_t* VPX_CODEC_IFACE_T_STAR;
typedef vpx_codec_ctx_t* VPX_CODEC_CTX_T_STAR;
typedef const vpx_codec_dec_cfg_t* CONST_VPX_CODEC_DEC_CFG_T_STAR;
typedef vpx_codec_stream_info_t* VPX_CODEC_STREAM_INFO_T_STAR;
typedef vpx_codec_iter_t* VPX_CODEC_ITER_T_STAR;
typedef vpx_image_t* VPX_IMAGE_T_STAR;

// Like FOR_EACH_0ARG_LIBRARY_FUNCTION, but only void functions
#define FOR_EACH_0ARG_VOID_LIBRARY_FUNCTION(macro) \

// Like FOR_EACH_0ARG_LIBRARY_FUNCTION, but only nonvoid functions
#define FOR_EACH_0ARG_NONVOID_LIBRARY_FUNCTION(macro) \
  macro(vpx_codec_vp9_dx, VPX_CODEC_IFACE_T_STAR) \
  macro(vpx_codec_version, int) \
  macro(vpx_codec_version_str, CONST_CHAR_STAR) \
  macro(vpx_codec_version_extra_str, CONST_CHAR_STAR) \
  macro(vpx_codec_build_config, CONST_CHAR_STAR) \
  macro(vpx_codec_vp8_dx, vpx_codec_iface_t *)

// Like FOR_EACH_1ARG_LIBRARY_FUNCTION, but only void functions
#define FOR_EACH_1ARG_VOID_LIBRARY_FUNCTION(macro) \
  macro(vpx_img_flip, void, VPX_IMAGE_T_STAR) \
  macro(vpx_img_free, void, VPX_IMAGE_T_STAR) \

// Like FOR_EACH_1ARG_LIBRARY_FUNCTION, but only nonvoid functions
#define FOR_EACH_1ARG_NONVOID_LIBRARY_FUNCTION(macro) \
  macro(vpx_codec_iface_name, CONST_CHAR_STAR, VPX_CODEC_IFACE_T_STAR) \
  macro(vpx_codec_err_to_string, CONST_CHAR_STAR, vpx_codec_err_t) \
  macro(vpx_codec_error, CONST_CHAR_STAR, VPX_CODEC_CTX_T_STAR) \
  macro(vpx_codec_error_detail, CONST_CHAR_STAR, VPX_CODEC_CTX_T_STAR) \
  macro(vpx_codec_destroy, vpx_codec_err_t, VPX_CODEC_CTX_T_STAR) \
  macro(vpx_codec_get_caps, vpx_codec_caps_t, VPX_CODEC_IFACE_T_STAR) \

// Like FOR_EACH_2ARG_LIBRARY_FUNCTION, but only void functions
#define FOR_EACH_2ARG_VOID_LIBRARY_FUNCTION(macro) \

// Like FOR_EACH_2ARG_LIBRARY_FUNCTION, but only nonvoid functions
#define FOR_EACH_2ARG_NONVOID_LIBRARY_FUNCTION(macro) \
  macro(vpx_codec_get_stream_info, vpx_codec_err_t, VPX_CODEC_CTX_T_STAR, VPX_CODEC_STREAM_INFO_T_STAR) \
  macro(vpx_codec_get_frame, VPX_IMAGE_T_STAR, VPX_CODEC_CTX_T_STAR, VPX_CODEC_ITER_T_STAR) \

// Like FOR_EACH_3ARG_LIBRARY_FUNCTION, but only void functions
#define FOR_EACH_3ARG_VOID_LIBRARY_FUNCTION(macro) \

// Like FOR_EACH_3ARG_LIBRARY_FUNCTION, but only nonvoid functions
#define FOR_EACH_3ARG_NONVOID_LIBRARY_FUNCTION(macro) \
  macro(vpx_codec_register_put_frame_cb, vpx_codec_err_t, VPX_CODEC_CTX_T_STAR, vpx_codec_put_frame_cb_fn_t, VOIDSTAR) \
  macro(vpx_codec_register_put_slice_cb, vpx_codec_err_t, VPX_CODEC_CTX_T_STAR, vpx_codec_put_slice_cb_fn_t, VOIDSTAR) \

// Like FOR_EACH_4ARG_LIBRARY_FUNCTION, but only void functions
#define FOR_EACH_4ARG_VOID_LIBRARY_FUNCTION(macro) \

// Like FOR_EACH_4ARG_LIBRARY_FUNCTION, but only nonvoid functions
#define FOR_EACH_4ARG_NONVOID_LIBRARY_FUNCTION(macro) \
  macro(vpx_codec_peek_stream_info, vpx_codec_err_t, VPX_CODEC_IFACE_T_STAR, CONST_UINT8_T_STAR, unsigned, VPX_CODEC_STREAM_INFO_T_STAR) \
  macro(vpx_codec_set_frame_buffer_functions, vpx_codec_err_t, VPX_CODEC_CTX_T_STAR, vpx_get_frame_buffer_cb_fn_t, vpx_release_frame_buffer_cb_fn_t, VOIDSTAR) \

// Like FOR_EACH_5ARG_LIBRARY_FUNCTION, but only void functions
#define FOR_EACH_5ARG_VOID_LIBRARY_FUNCTION(macro) \

// Like FOR_EACH_5ARG_LIBRARY_FUNCTION, but only nonvoid functions
#define FOR_EACH_5ARG_NONVOID_LIBRARY_FUNCTION(macro) \
  macro(vpx_codec_dec_init_ver, vpx_codec_err_t, VPX_CODEC_CTX_T_STAR, VPX_CODEC_IFACE_T_STAR, CONST_VPX_CODEC_DEC_CFG_T_STAR, vpx_codec_flags_t, int) \
  macro(vpx_codec_decode, vpx_codec_err_t, VPX_CODEC_CTX_T_STAR, CONST_UINT8_T_STAR, unsigned, VOIDSTAR, long) \
  macro(vpx_img_alloc, VPX_IMAGE_T_STAR, VPX_IMAGE_T_STAR, vpx_img_fmt_t, unsigned, unsigned, unsigned) \
  macro(vpx_img_set_rect, int, VPX_IMAGE_T_STAR, unsigned, unsigned, unsigned, unsigned) \

// Like FOR_EACH_6ARG_LIBRARY_FUNCTION, but only void functions
#define FOR_EACH_6ARG_VOID_LIBRARY_FUNCTION(macro) \

// Like FOR_EACH_6ARG_LIBRARY_FUNCTION, but only nonvoid functions
#define FOR_EACH_6ARG_NONVOID_LIBRARY_FUNCTION(macro) \
  macro(vpx_img_wrap, VPX_IMAGE_T_STAR, VPX_IMAGE_T_STAR, vpx_img_fmt_t, unsigned, unsigned, unsigned, UCHARSTAR) \

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

