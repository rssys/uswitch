#pragma once

#include "generic_helpers.h"
#include "vorbis/codec.h"

#define LIB Vorbis
namespace LIB {

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

// Like FOR_EACH_0ARG_LIBRARY_FUNCTION, but only void functions
#define FOR_EACH_0ARG_VOID_LIBRARY_FUNCTION(macro) \

// Like FOR_EACH_0ARG_LIBRARY_FUNCTION, but only nonvoid functions
#define FOR_EACH_0ARG_NONVOID_LIBRARY_FUNCTION(macro) \
  macro(vorbis_version_string, const char*) \

// Like FOR_EACH_1ARG_LIBRARY_FUNCTION, but only void functions
#define FOR_EACH_1ARG_VOID_LIBRARY_FUNCTION(macro) \
  macro(vorbis_info_init, void, vorbis_info*) \
  macro(vorbis_info_clear, void, vorbis_info*) \
  macro(vorbis_comment_init, void, vorbis_comment*) \
  macro(vorbis_comment_clear, void, vorbis_comment*) \
  macro(vorbis_dsp_clear, void, vorbis_dsp_state*) \

// Like FOR_EACH_1ARG_LIBRARY_FUNCTION, but only nonvoid functions
#define FOR_EACH_1ARG_NONVOID_LIBRARY_FUNCTION(macro) \
  macro(vorbis_block_clear, int, vorbis_block*) \
  macro(vorbis_bitrate_addblock, int, vorbis_block*) \
  macro(vorbis_synthesis_idheader, int, ogg_packet*) \
  macro(vorbis_synthesis_restart, int, vorbis_dsp_state*) \
  macro(vorbis_synthesis_halfrate_p, int, vorbis_info*) \

// Like FOR_EACH_2ARG_LIBRARY_FUNCTION, but only void functions
#define FOR_EACH_2ARG_VOID_LIBRARY_FUNCTION(macro) \
  macro(vorbis_comment_add, void, vorbis_comment*, const char*) \

// Like FOR_EACH_2ARG_LIBRARY_FUNCTION, but only nonvoid functions
#define FOR_EACH_2ARG_NONVOID_LIBRARY_FUNCTION(macro) \
  macro(vorbis_info_blocksize, int, vorbis_info*, int) \
  macro(vorbis_comment_query_count, int, vorbis_comment*, const char*) \
  macro(vorbis_block_init, int, vorbis_dsp_state*, vorbis_block*) \
  macro(vorbis_granule_time, double, vorbis_dsp_state*, ogg_int64_t) \
  macro(vorbis_analysis_init, int, vorbis_dsp_state*, vorbis_info*) \
  macro(vorbis_commentheader_out, int, vorbis_comment*, ogg_packet*) \
  macro(vorbis_analysis_buffer, float**, vorbis_dsp_state*, int) \
  macro(vorbis_analysis_wrote, int, vorbis_dsp_state*, int) \
  macro(vorbis_analysis_blockout, int, vorbis_dsp_state*, vorbis_block*) \
  macro(vorbis_analysis, int, vorbis_block*, ogg_packet*) \
  macro(vorbis_bitrate_flushpacket, int, vorbis_dsp_state*, ogg_packet*) \
  macro(vorbis_synthesis_init, int, vorbis_dsp_state*, vorbis_info*) \
  macro(vorbis_synthesis, int, vorbis_block*, ogg_packet*) \
  macro(vorbis_synthesis_trackonly, int, vorbis_block*, ogg_packet*) \
  macro(vorbis_synthesis_blockin, int, vorbis_dsp_state*, vorbis_block*) \
  macro(vorbis_synthesis_pcmout, int, vorbis_dsp_state*, float***) \
  macro(vorbis_synthesis_lapout, int, vorbis_dsp_state*, float***) \
  macro(vorbis_synthesis_read, int, vorbis_dsp_state*, int) \
  macro(vorbis_packet_blocksize, long, vorbis_info*, ogg_packet*) \
  macro(vorbis_synthesis_halfrate, int, vorbis_info*, int) \

// Like FOR_EACH_3ARG_LIBRARY_FUNCTION, but only void functions
#define FOR_EACH_3ARG_VOID_LIBRARY_FUNCTION(macro) \
  macro(vorbis_comment_add_tag, void, vorbis_comment*, const char*, const char*) \

// Like FOR_EACH_3ARG_LIBRARY_FUNCTION, but only nonvoid functions
#define FOR_EACH_3ARG_NONVOID_LIBRARY_FUNCTION(macro) \
  macro(vorbis_comment_query, char*, vorbis_comment*, const char*, int) \
  macro(vorbis_synthesis_headerin, int, vorbis_info*, vorbis_comment*, ogg_packet*) \

// Like FOR_EACH_4ARG_LIBRARY_FUNCTION, but only void functions
#define FOR_EACH_4ARG_VOID_LIBRARY_FUNCTION(macro) \

// Like FOR_EACH_4ARG_LIBRARY_FUNCTION, but only nonvoid functions
#define FOR_EACH_4ARG_NONVOID_LIBRARY_FUNCTION(macro) \

// Like FOR_EACH_5ARG_LIBRARY_FUNCTION, but only void functions
#define FOR_EACH_5ARG_VOID_LIBRARY_FUNCTION(macro) \

// Like FOR_EACH_5ARG_LIBRARY_FUNCTION, but only nonvoid functions
#define FOR_EACH_5ARG_NONVOID_LIBRARY_FUNCTION(macro) \
  macro(vorbis_analysis_headerout, int, vorbis_dsp_state*, vorbis_comment*, ogg_packet*, ogg_packet*, ogg_packet*) \

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

