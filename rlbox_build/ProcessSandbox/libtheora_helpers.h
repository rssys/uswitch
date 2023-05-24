#pragma once

#include "generic_helpers.h"
#include "theora/theoradec.h"

// Note: Currently we only have helpers for theora/theoradec.h and ogg/ogg.h.
// Later we may need to add helpers for theora/theoraenc.h, etc.
// We also currently don't include the function `ogg_set_mem_functions()`.

#define LIB Theora
namespace LIB {

// Must define callback types by these aliases
typedef th_stripe_decoded_func CB_TYPE_0;

// unused, but we require these typedef'd to some function-pointer type (for now)
typedef void (*CB_UNUSED)();
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
typedef th_dec_ctx* TH_DEC_CTX_STAR;
typedef th_info* TH_INFO_STAR;
typedef th_comment* TH_COMMENT_STAR;
typedef th_setup_info* TH_SETUP_INFO_STAR;
typedef th_setup_info** TH_SETUP_INFO_STARSTAR;
typedef ogg_packet* OGG_PACKET_STAR;
typedef const ogg_packet* CONST_OGG_PACKET_STAR;
typedef ogg_int64_t* OGG_INT64_T_STAR;
typedef th_img_plane* TH_YCBCR_BUFFER;  // theora/codec.h has `typedef th_img_plane th_ycbcr_buffer[3];`
typedef oggpack_buffer* OGGPACK_BUFFER_STAR;
typedef ogg_stream_state* OGG_STREAM_STATE_STAR;
typedef ogg_sync_state* OGG_SYNC_STATE_STAR;
typedef ogg_page* OGG_PAGE_STAR;
typedef ogg_packet* OGG_PACKET_STAR;
typedef ogg_iovec_t* OGG_IOVEC_T_STAR;
typedef void* VOIDSTAR;
typedef char* CHARSTAR;
typedef unsigned char* UCHARSTAR;

// Like FOR_EACH_0ARG_LIBRARY_FUNCTION, but only void functions
#define FOR_EACH_0ARG_VOID_LIBRARY_FUNCTION(macro) \

// Like FOR_EACH_0ARG_LIBRARY_FUNCTION, but only nonvoid functions
#define FOR_EACH_0ARG_NONVOID_LIBRARY_FUNCTION(macro) \
  macro(th_version_string, const char *) \
  macro(th_version_number, ogg_uint32_t ) \

// Like FOR_EACH_1ARG_LIBRARY_FUNCTION, but only void functions
#define FOR_EACH_1ARG_VOID_LIBRARY_FUNCTION(macro) \
  macro(th_setup_free, void, TH_SETUP_INFO_STAR) \
  macro(th_decode_free, void, TH_DEC_CTX_STAR) \
  macro(oggpack_writeinit, void, OGGPACK_BUFFER_STAR) \
  macro(oggpackB_writeinit, void, OGGPACK_BUFFER_STAR) \
  macro(oggpack_writealign, void, OGGPACK_BUFFER_STAR) \
  macro(oggpackB_writealign, void, OGGPACK_BUFFER_STAR) \
  macro(oggpack_reset, void, OGGPACK_BUFFER_STAR) \
  macro(oggpackB_reset, void, OGGPACK_BUFFER_STAR) \
  macro(oggpack_writeclear, void, OGGPACK_BUFFER_STAR) \
  macro(oggpackB_writeclear, void, OGGPACK_BUFFER_STAR) \
  macro(oggpack_adv1, void, OGGPACK_BUFFER_STAR) \
  macro(oggpackB_adv1, void, OGGPACK_BUFFER_STAR) \
  macro(ogg_page_checksum_set, void, OGG_PAGE_STAR) \
  macro(ogg_packet_clear, void, OGG_PACKET_STAR) \
  macro(th_info_init, void, th_info *) \
  macro(th_info_clear, void, th_info *) \
  macro(th_comment_init, void, th_comment *) \
  macro(th_comment_clear, void, th_comment *) \

// Like FOR_EACH_1ARG_LIBRARY_FUNCTION, but only nonvoid functions
#define FOR_EACH_1ARG_NONVOID_LIBRARY_FUNCTION(macro) \
  macro(oggpack_writecheck, int, OGGPACK_BUFFER_STAR) \
  macro(oggpackB_writecheck, int, OGGPACK_BUFFER_STAR) \
  macro(oggpack_look1, long, OGGPACK_BUFFER_STAR) \
  macro(oggpackB_look1, long, OGGPACK_BUFFER_STAR) \
  macro(oggpack_read1, long, OGGPACK_BUFFER_STAR) \
  macro(oggpackB_read1, long, OGGPACK_BUFFER_STAR) \
  macro(oggpack_bytes, long, OGGPACK_BUFFER_STAR) \
  macro(oggpackB_bytes, long, OGGPACK_BUFFER_STAR) \
  macro(oggpack_bits, long, OGGPACK_BUFFER_STAR) \
  macro(oggpackB_bits, long, OGGPACK_BUFFER_STAR) \
  macro(oggpack_get_buffer, UCHARSTAR, OGGPACK_BUFFER_STAR) \
  macro(oggpackB_get_buffer, UCHARSTAR, OGGPACK_BUFFER_STAR) \
  macro(ogg_sync_init, int, OGG_SYNC_STATE_STAR) \
  macro(ogg_sync_clear, int, OGG_SYNC_STATE_STAR) \
  macro(ogg_sync_reset, int, OGG_SYNC_STATE_STAR) \
  macro(ogg_sync_destroy, int, OGG_SYNC_STATE_STAR) \
  macro(ogg_sync_check, int, OGG_SYNC_STATE_STAR) \
  macro(ogg_stream_clear, int, OGG_STREAM_STATE_STAR) \
  macro(ogg_stream_reset, int, OGG_STREAM_STATE_STAR) \
  macro(ogg_stream_destroy, int, OGG_STREAM_STATE_STAR) \
  macro(ogg_stream_check, int, OGG_STREAM_STATE_STAR) \
  macro(ogg_stream_eos, int, OGG_STREAM_STATE_STAR) \
  macro(ogg_page_version, int, OGG_PAGE_STAR) \
  macro(ogg_page_continued, int, OGG_PAGE_STAR) \
  macro(ogg_page_bos, int, OGG_PAGE_STAR) \
  macro(ogg_page_eos, int, OGG_PAGE_STAR) \
  macro(ogg_page_granulepos, ogg_int64_t, OGG_PAGE_STAR) \
  macro(ogg_page_serialno, int, OGG_PAGE_STAR) \
  macro(ogg_page_pageno, long, OGG_PAGE_STAR) \
  macro(ogg_page_packets, int, OGG_PAGE_STAR) \
  macro(th_packet_isheader, int, ogg_packet *) \
  macro(th_packet_iskeyframe, int, ogg_packet *) \

// Like FOR_EACH_2ARG_LIBRARY_FUNCTION, but only void functions
#define FOR_EACH_2ARG_VOID_LIBRARY_FUNCTION(macro) \
  macro(oggpack_writetrunc, void, OGGPACK_BUFFER_STAR, long) \
  macro(oggpackB_writetrunc, void, OGGPACK_BUFFER_STAR, long) \
  macro(oggpack_adv, void, OGGPACK_BUFFER_STAR, int) \
  macro(oggpackB_adv, void, OGGPACK_BUFFER_STAR, int) \
  macro(th_comment_add, void, th_comment *, char *) \

// Like FOR_EACH_2ARG_LIBRARY_FUNCTION, but only nonvoid functions
#define FOR_EACH_2ARG_NONVOID_LIBRARY_FUNCTION(macro) \
  macro(th_decode_alloc, TH_DEC_CTX_STAR, TH_INFO_STAR, TH_SETUP_INFO_STAR) \
  macro(th_decode_ycbcr_out, int, TH_DEC_CTX_STAR, TH_YCBCR_BUFFER) \
  macro(oggpack_look, long, OGGPACK_BUFFER_STAR, int) \
  macro(oggpackB_look, long, OGGPACK_BUFFER_STAR, int) \
  macro(oggpack_read, long, OGGPACK_BUFFER_STAR, int) \
  macro(oggpackB_read, long, OGGPACK_BUFFER_STAR, int) \
  macro(ogg_stream_packetin, int, OGG_STREAM_STATE_STAR, OGG_PACKET_STAR) \
  macro(ogg_stream_pageout, int, OGG_STREAM_STATE_STAR, OGG_PAGE_STAR) \
  macro(ogg_stream_flush, int, OGG_STREAM_STATE_STAR, OGG_PAGE_STAR) \
  macro(ogg_sync_buffer, CHARSTAR, OGG_SYNC_STATE_STAR, long) \
  macro(ogg_sync_wrote, int, OGG_SYNC_STATE_STAR, long) \
  macro(ogg_sync_pageseek, long, OGG_SYNC_STATE_STAR, OGG_PAGE_STAR) \
  macro(ogg_sync_pageout, int, OGG_SYNC_STATE_STAR, OGG_PAGE_STAR) \
  macro(ogg_stream_pagein, int, OGG_STREAM_STATE_STAR, OGG_PAGE_STAR) \
  macro(ogg_stream_packetout, int, OGG_STREAM_STATE_STAR, OGG_PACKET_STAR) \
  macro(ogg_stream_packetpeek, int, OGG_STREAM_STATE_STAR, OGG_PACKET_STAR) \
  macro(ogg_stream_init, int, OGG_STREAM_STATE_STAR, int) \
  macro(ogg_stream_reset_serialno, int, OGG_STREAM_STATE_STAR, int) \
  macro(th_granule_frame, ogg_int64_t, void *,ogg_int64_t) \
  macro(th_granule_time, double, void *,ogg_int64_t) \
  macro(th_comment_query_count, int, th_comment *,char *) \


// Like FOR_EACH_3ARG_LIBRARY_FUNCTION, but only void functions
#define FOR_EACH_3ARG_VOID_LIBRARY_FUNCTION(macro) \
  macro(oggpack_writecopy, void, OGGPACK_BUFFER_STAR, VOIDSTAR, long) \
  macro(oggpackB_writecopy, void, OGGPACK_BUFFER_STAR, VOIDSTAR, long) \
  macro(oggpack_readinit, void, OGGPACK_BUFFER_STAR, UCHARSTAR, int) \
  macro(oggpackB_readinit, void, OGGPACK_BUFFER_STAR, UCHARSTAR, int) \
  macro(oggpack_write, void, OGGPACK_BUFFER_STAR, unsigned long, int) \
  macro(oggpackB_write, void, OGGPACK_BUFFER_STAR, unsigned long, int) \
  macro(th_comment_add_tag, void, th_comment *, char *, char *) \

// Like FOR_EACH_3ARG_LIBRARY_FUNCTION, but only nonvoid functions
#define FOR_EACH_3ARG_NONVOID_LIBRARY_FUNCTION(macro) \
  macro(th_decode_packetin, int, TH_DEC_CTX_STAR, CONST_OGG_PACKET_STAR, OGG_INT64_T_STAR) \
  macro(ogg_stream_pageout_fill, int, OGG_STREAM_STATE_STAR, OGG_PAGE_STAR, int) \
  macro(ogg_stream_flush_fill, int, OGG_STREAM_STATE_STAR, OGG_PAGE_STAR, int) \
  macro(th_comment_query, char *, th_comment *, char *, int) \

// Like FOR_EACH_4ARG_LIBRARY_FUNCTION, but only void functions
#define FOR_EACH_4ARG_VOID_LIBRARY_FUNCTION(macro) \

// Like FOR_EACH_4ARG_LIBRARY_FUNCTION, but only nonvoid functions
#define FOR_EACH_4ARG_NONVOID_LIBRARY_FUNCTION(macro) \
  macro(th_decode_headerin, int, TH_INFO_STAR, TH_COMMENT_STAR, TH_SETUP_INFO_STARSTAR, OGG_PACKET_STAR) \
  macro(th_decode_ctl, int, TH_DEC_CTX_STAR, int, VOIDSTAR, size_t) \

// Like FOR_EACH_5ARG_LIBRARY_FUNCTION, but only void functions
#define FOR_EACH_5ARG_VOID_LIBRARY_FUNCTION(macro) \

// Like FOR_EACH_5ARG_LIBRARY_FUNCTION, but only nonvoid functions
#define FOR_EACH_5ARG_NONVOID_LIBRARY_FUNCTION(macro) \
  macro(ogg_stream_iovecin, int, OGG_STREAM_STATE_STAR, OGG_IOVEC_T_STAR, int, long, ogg_int64_t) \

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

