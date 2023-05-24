#pragma once

#include "generic_helpers.h"
#include "../mozilla-release/image/decoders/nsPNGDecoder_clib.h"

#define LIB PNGDEC
namesapce LIB {

// Must define callback types by these aliases

// unused, but we require these typedef'd to some function-pointer type (for now)
typedef void (*CB_UNUSED)();
typedef CB_UNUSED CB_TYPE_0;
typedef CB_UNUSED CB_TYPE_1;
typedef CB_UNUSED CB_TYPE_2;
typedef CB_UNUSED CB_TYPE_3;
typedef CB_UNUSED CB_TYPE_4;
typedef CB_UNUSED CB_TYPE_5;
typedef CB_UNUSED CB_TYPE_6;
typedef CB_UNUSED CB_TYPE_7;

};  // namespace LIB

// All types used by library functions must be a single preprocessor token, so that
//   they play nice with macros
// This means that we can't have two-word types like "struct x", nor types that contain *
typedef nsPNGDecoder_clib* NSPNGDECODER_CLIB_STAR;
typedef const nsPNGDecoder_clib* CONST_NSPNGDECODER_CLIB_STAR;
typedef void* VOIDSTAR;
typedef bool* BOOLSTAR;
typedef uint8_t* UINT8_T_STAR;
typedef uint32_t* UINT32_T_STAR;
typedef const uint8_t* CONST_UINT8_T_STAR;
typedef void** VOIDSTARSTAR;
typedef uint8_t** UINT8_T_STAR_STAR;
typedef const uint8_t** CONST_UINT8_T_STAR_STAR;

// Like FOR_EACH_0ARG_LIBRARY_FUNCTION, but only void functions
#define FOR_EACH_0ARG_VOID_LIBRARY_FUNCTION(macro) \

// Like FOR_EACH_0ARG_LIBRARY_FUNCTION, but only nonvoid functions
#define FOR_EACH_0ARG_NONVOID_LIBRARY_FUNCTION(macro) \

// Like FOR_EACH_1ARG_LIBRARY_FUNCTION, but only void functions
#define FOR_EACH_1ARG_VOID_LIBRARY_FUNCTION(macro) \
  macro(nsPNGDecoder_clib_destructor, void, NSPNGDECODER_CLIB_STAR) \

// Like FOR_EACH_1ARG_LIBRARY_FUNCTION, but only nonvoid functions
#define FOR_EACH_1ARG_NONVOID_LIBRARY_FUNCTION(macro) \
  macro(nsPNGDecoder_clib_constructor, NSPNGDECODER_CLIB_STAR, VOIDSTAR) \
  macro(IsValidICOResource, bool, CONST_NSPNGDECODER_CLIB_STAR) \
  macro(getPtrToMPNG, VOIDSTAR, CONST_NSPNGDECODER_CLIB_STAR) \
  macro(getPtrToMInfo, VOIDSTAR, CONST_NSPNGDECODER_CLIB_STAR) \
  macro(getPtrToMFrameRect, VOIDSTAR, CONST_NSPNGDECODER_CLIB_STAR) \
  macro(getPtrToMCMSLine, UINT8_T_STAR_STAR, CONST_NSPNGDECODER_CLIB_STAR) \
  macro(getPtrToInterlaceBuf, UINT8_T_STAR_STAR, CONST_NSPNGDECODER_CLIB_STAR) \
  macro(getPtrToMInProfile, VOIDSTARSTAR, CONST_NSPNGDECODER_CLIB_STAR) \
  macro(getPtrToMTransform, VOIDSTARSTAR, CONST_NSPNGDECODER_CLIB_STAR) \
  macro(getPtrToMFormat, VOIDSTAR, CONST_NSPNGDECODER_CLIB_STAR) \
  macro(getPtrToMCMSMode, UINT32_T_STAR, CONST_NSPNGDECODER_CLIB_STAR) \
  macro(getPtrToMChannels, UINT8_T_STAR, CONST_NSPNGDECODER_CLIB_STAR) \
  macro(getPtrToMPass, UINT8_T_STAR, CONST_NSPNGDECODER_CLIB_STAR) \
  macro(getPtrToMFrameIsHidden, BOOLSTAR, CONST_NSPNGDECODER_CLIB_STAR) \
  macro(getPtrToMDisablePremultipliedAlpha, BOOLSTAR, CONST_NSPNGDECODER_CLIB_STAR) \
  macro(getPtrToMAnimInfo, VOIDSTAR, CONST_NSPNGDECODER_CLIB_STAR) \
  macro(getPtrToMPipe, VOIDSTAR, CONST_NSPNGDECODER_CLIB_STAR) \
  macro(getPtrToMNumFrames, UINT32_T_STAR, CONST_NSPNGDECODER_CLIB_STAR) \

// Like FOR_EACH_2ARG_LIBRARY_FUNCTION, but only void functions
#define FOR_EACH_2ARG_VOID_LIBRARY_FUNCTION(macro) \

// Like FOR_EACH_2ARG_LIBRARY_FUNCTION, but only nonvoid functions
#define FOR_EACH_2ARG_NONVOID_LIBRARY_FUNCTION(macro) \

// Like FOR_EACH_3ARG_LIBRARY_FUNCTION, but only void functions
#define FOR_EACH_3ARG_VOID_LIBRARY_FUNCTION(macro) \

// Like FOR_EACH_3ARG_LIBRARY_FUNCTION, but only nonvoid functions
#define FOR_EACH_3ARG_NONVOID_LIBRARY_FUNCTION(macro) \
  macro(DoDecode, VOIDSTAR, NSPNGDECODER_CLIB_STAR, VOIDSTAR, VOIDSTAR) \

// Like FOR_EACH_4ARG_LIBRARY_FUNCTION, but only void functions
#define FOR_EACH_4ARG_VOID_LIBRARY_FUNCTION(macro) \

// Like FOR_EACH_4ARG_LIBRARY_FUNCTION, but only nonvoid functions
#define FOR_EACH_4ARG_NONVOID_LIBRARY_FUNCTION(macro) \

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

