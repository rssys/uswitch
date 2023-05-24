#pragma once

#include <stdio.h>  // FILE*
#include "generic_helpers.h"
#include "zlib.h"

#define LIB Z
namespace LIB {

// Must define callback types by these aliases
typedef alloc_func CB_TYPE_0;  // voidpf (*)(voidpf, uInt, uInt)
typedef free_func CB_TYPE_1;  // void (*)(voidpf, voidpf)
typedef in_func CB_TYPE_2;  // unsigned (*)(void FAR *, z_const unsigned char FAR * FAR *)
typedef out_func CB_TYPE_3;  // int (*)(void FAR *, unsigned char FAR *, unsigned)

// unused, but we require these typedef'd to some function-pointer type (for now)
typedef void (*CB_UNUSED)();
typedef CB_UNUSED CB_TYPE_4;
typedef CB_UNUSED CB_TYPE_5;
typedef CB_UNUSED CB_TYPE_6;
typedef CB_UNUSED CB_TYPE_7;

// TODO: The above should be CB_TYPE<int>

};  // namespace LIB

// All types used by library functions must be a single preprocessor token, so that
//   they play nice with macros
// This means that we can't have two-word types like "struct x", nor types that contain *
typedef char* CHARSTAR;
typedef const char* CONSTCHARSTAR;
typedef Bytef* BYTEFSTAR;
typedef const Bytef* CONSTBYTEFSTAR;
typedef unsigned char FAR * UCHARFSTAR;
typedef uInt* UINTSTAR;
typedef unsigned* UNSIGNEDSTAR;
typedef int* INTSTAR;
typedef void FAR * VOIDFARSTAR;
typedef uLong* ULONGSTAR;
typedef uLongf* ULONGFSTAR;

// Like FOR_EACH_0ARG_LIBRARY_FUNCTION, but only void functions
#define FOR_EACH_0ARG_VOID_LIBRARY_FUNCTION(macro) \

// Like FOR_EACH_0ARG_LIBRARY_FUNCTION, but only nonvoid functions
#define FOR_EACH_0ARG_NONVOID_LIBRARY_FUNCTION(macro) \
  macro(zlibVersion, CONSTCHARSTAR) \
  macro(zlibCompileFlags, uLong) \

// Like FOR_EACH_1ARG_LIBRARY_FUNCTION, but only void functions
#define FOR_EACH_1ARG_VOID_LIBRARY_FUNCTION(macro) \
  macro(gzclearerr, void, gzFile) \

// Like FOR_EACH_1ARG_LIBRARY_FUNCTION, but only nonvoid functions
#define FOR_EACH_1ARG_NONVOID_LIBRARY_FUNCTION(macro) \
  macro(deflateEnd, int, z_streamp) \
  macro(inflateEnd, int, z_streamp) \
  macro(deflateReset, int, z_streamp) \
  macro(inflateSync, int, z_streamp) \
  macro(inflateReset, int, z_streamp) \
  macro(inflateMark, long, z_streamp) \
  macro(inflateBackEnd, int, z_streamp) \
  macro(compressBound, uLong, uLong) \
  macro(gzgetc, int, gzFile) \
  macro(gzrewind, int, gzFile) \
  macro(gzeof, int, gzFile) \
  macro(gzdirect, int, gzFile) \
  macro(gzclose, int, gzFile) \
  macro(gzclose_r, int, gzFile) \
  macro(gzclose_w, int, gzFile) \

// Like FOR_EACH_2ARG_LIBRARY_FUNCTION, but only void functions
#define FOR_EACH_2ARG_VOID_LIBRARY_FUNCTION(macro) \

// Like FOR_EACH_2ARG_LIBRARY_FUNCTION, but only nonvoid functions
#define FOR_EACH_2ARG_NONVOID_LIBRARY_FUNCTION(macro) \
  macro(deflate, int, z_streamp, int) \
  macro(inflate, int, z_streamp, int) \
  macro(deflateCopy, int, z_streamp, z_streamp) \
  macro(deflateBound, uLong, z_streamp, uLong) \
  macro(deflateSetHeader, int, z_streamp, gz_headerp) \
  macro(inflateCopy, int, z_streamp, z_streamp) \
  macro(inflateReset2, int, z_streamp, int) \
  macro(inflateGetHeader, int, z_streamp, gz_headerp) \
  macro(gzdopen, gzFile, int, CONSTCHARSTAR) \
  macro(gzbuffer, int, gzFile, unsigned) \
  macro(gzputs, int, gzFile, CONSTCHARSTAR) \
  macro(gzputc, int, gzFile, int) \
  macro(gzungetc, int, int, gzFile) \
  macro(gzflush, int, gzFile, int) \
  macro(gzerror, CONSTCHARSTAR, gzFile, INTSTAR) \

// Like FOR_EACH_3ARG_LIBRARY_FUNCTION, but only void functions
#define FOR_EACH_3ARG_VOID_LIBRARY_FUNCTION(macro) \

// Like FOR_EACH_3ARG_LIBRARY_FUNCTION, but only nonvoid functions
#define FOR_EACH_3ARG_NONVOID_LIBRARY_FUNCTION(macro) \
  macro(deflateSetDictionary, int, z_streamp, CONSTBYTEFSTAR, uInt) \
  macro(deflateGetDictionary, int, z_streamp, BYTEFSTAR, UINTSTAR) \
  macro(deflateParams, int, z_streamp, int, int) \
  macro(deflatePending, int, z_streamp, UNSIGNEDSTAR, INTSTAR) \
  macro(deflatePrime, int, z_streamp, int, int) \
  macro(inflateSetDictionary, int, z_streamp, CONSTBYTEFSTAR, uInt) \
  macro(inflateGetDictionary, int, z_streamp, BYTEFSTAR, UINTSTAR) \
  macro(inflatePrime, int, z_streamp, int, int) \
  macro(gzsetparams, int, gzFile, int, int) \
  macro(gzread, int, gzFile, voidp, unsigned) \
  macro(gzwrite, int, gzFile, voidpc, unsigned) \
  macro(gzgets, CHARSTAR, gzFile, CHARSTAR, int) \
  macro(adler32, uLong, uLong, CONSTBYTEFSTAR, uInt) \
  macro(adler32_z, uLong, uLong, CONSTBYTEFSTAR, z_size_t) \
  macro(crc32, uLong, uLong, CONSTBYTEFSTAR, uInt) \
  macro(crc32_z, uLong, uLong, CONSTBYTEFSTAR, z_size_t) \
  macro(inflateInit_, int, z_streamp, CONSTCHARSTAR, int) \

// Like FOR_EACH_4ARG_LIBRARY_FUNCTION, but only void functions
#define FOR_EACH_4ARG_VOID_LIBRARY_FUNCTION(macro) \

// Like FOR_EACH_4ARG_LIBRARY_FUNCTION, but only nonvoid functions
#define FOR_EACH_4ARG_NONVOID_LIBRARY_FUNCTION(macro) \
  macro(compress, int, BYTEFSTAR, ULONGFSTAR, CONSTBYTEFSTAR, uLong) \
  macro(uncompress, int, BYTEFSTAR, ULONGFSTAR, CONSTBYTEFSTAR, uLong) \
  macro(uncompress2, int, BYTEFSTAR, ULONGFSTAR, CONSTBYTEFSTAR, ULONGSTAR) \
  macro(gzfread, z_size_t, voidp, z_size_t, z_size_t, gzFile) \
  macro(gzfwrite, z_size_t, voidpc, z_size_t, z_size_t, gzFile) \
  macro(deflateInit_, int, z_streamp, int, CONSTCHARSTAR, int) \
  macro(inflateInit2_, int, z_streamp, int, CONSTCHARSTAR, int) \

// Like FOR_EACH_5ARG_LIBRARY_FUNCTION, but only void functions
#define FOR_EACH_5ARG_VOID_LIBRARY_FUNCTION(macro) \

// Like FOR_EACH_5ARG_LIBRARY_FUNCTION, but only nonvoid functions
#define FOR_EACH_5ARG_NONVOID_LIBRARY_FUNCTION(macro) \
  macro(deflateTune, int, z_streamp, int, int, int, int) \
  macro(inflateBack, int, z_streamp, in_func, VOIDFARSTAR, out_func, VOIDFARSTAR) \
  macro(compress2, int, BYTEFSTAR, ULONGFSTAR, CONSTBYTEFSTAR, uLong, int) \
  macro(inflateBackInit_, int, z_streamp, int, UCHARFSTAR, CONSTCHARSTAR, int) \

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
  macro(deflateInit2_, int, z_streamp, int, int, int, int, int, CONSTCHARSTAR, int) \

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

