/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_image_decoders_nsJPEGDecoder_h
#define mozilla_image_decoders_nsJPEGDecoder_h

#include "RasterImage.h"
// On Windows systems, RasterImage.h brings in 'windows.h', which defines INT32.
// But the jpeg decoder has its own definition of INT32. To avoid build issues,
// we need to undefine the version from 'windows.h'.
#undef INT32

#include "Decoder.h"

#include "nsIInputStream.h"
#include "nsIPipe.h"
#include "qcms.h"

extern "C" {
#include "jpeglib.h"
}

#include <setjmp.h>
#include <chrono>
#include <atomic>
#include <string>

#include <map>
#include <memory>
#include <mutex>
#include <vector>

#include "uswitchsandbox.h"
#include "uswitch.hpp"
#include "RLBox_USwitch.h"
#include "rlbox.h"
#include "jpeglib_structs_for_cpp_api_new.h"

using TRLSandbox = RLBox_USwitch;
using namespace rlbox;

using namespace std::chrono;

namespace mozilla {
namespace image {

typedef struct {
    struct jpeg_error_mgr pub;  // "public" fields for IJG library
    jmp_buf setjmp_buffer;      // For handling catastropic errors
} decoder_error_mgr;

typedef enum {
    JPEG_HEADER,                          // Reading JFIF headers
    JPEG_START_DECOMPRESS,
    JPEG_DECOMPRESS_PROGRESSIVE,          // Output progressive pixels
    JPEG_DECOMPRESS_SEQUENTIAL,           // Output sequential pixels
    JPEG_DONE,
    JPEG_SINK_NON_JPEG_TRAILER,          // Some image files have a
                                         // non-JPEG trailer
    JPEG_ERROR
} jstate;

class RasterImage;
struct Orientation;

class JPEGSandboxResource;

class nsJPEGDecoder : public Decoder
{
public:
  virtual ~nsJPEGDecoder();

  void NotifyDone();

protected:
  nsresult InitInternal() override;
  LexerResult DoDecode(SourceBufferIterator& aIterator,
                       IResumable* aOnResume) override;
  nsresult FinishInternal() override;

  Maybe<Telemetry::HistogramID> SpeedHistogram() const override;

protected:
  Orientation ReadOrientationFromEXIF();
  void OutputScanlines(bool* suspend);

private:
  friend class DecoderFactory;

  // Decoders should only be instantiated via DecoderFactory.
  nsJPEGDecoder(RasterImage* aImage, Decoder::DecodeStyle aDecodeStyle, RasterImage* aImageExtra);

  enum class State
  {
    JPEG_DATA,
    FINISHED_JPEG_DATA
  };

  LexerTransition<State> ReadJPEGData(const char* aData, size_t aLength);
  LexerTransition<State> FinishedJPEGData();

  StreamingLexer<State> mLexer;

public:

  std::string mImageString;
  std::string mhostString;
  // std::once_flag rlbox_jpeg_init;
  // RLBoxSandbox<TRLSandbox>* rlbox_jpeg;
  // void init_rlbox();
  // sandbox_callback_helper<void(j_decompress_ptr jd), TRLSandbox> cpp_cb_jpeg_init_source;
  // sandbox_callback_helper<boolean(j_decompress_ptr jd), TRLSandbox> cpp_cb_jpeg_fill_input_buffer;
  // sandbox_callback_helper<void(j_decompress_ptr jd, long num_bytes), TRLSandbox> cpp_cb_jpeg_skip_input_data;
  // sandbox_callback_helper<void(j_decompress_ptr jd), TRLSandbox> cpp_cb_jpeg_term_source;
  // sandbox_callback_helper<void(j_common_ptr cinfo), TRLSandbox> cpp_cb_jpeg_my_error_exit;
  // tainted<boolean(*)(j_decompress_ptr, int), TRLSandbox> cpp_resync_to_restart;
  std::shared_ptr<JPEGSandboxResource> rlbox_sbx_shared;
  JPEGSandboxResource* rlbox_sbx;
  tainted<struct jpeg_decompress_struct*, TRLSandbox> p_mInfo;
  tainted<struct jpeg_source_mgr*, TRLSandbox> p_mSourceMgr;
  tainted<decoder_error_mgr*, TRLSandbox> p_mErr;
  J_COLOR_SPACE m_out_color_space;
  jmp_buf m_jmpBuff;
  bool m_jmpBuffValid = FALSE; 
  unsigned int mImageWidth = 0;
  JDIMENSION m_output_height_shadow;
  jstate mState;

  uint32_t mBytesToSkip;

  const JOCTET* mSegment;   // The current segment we are decoding from
  uint32_t mSegmentLen;     // amount of data in mSegment

  tainted<JOCTET*, TRLSandbox> s_mSegment;
  tainted<JOCTET*, TRLSandbox> s_mBackBuffer;
  uint32_t s_mSegmentLen;
  uint32_t s_mBackBufferLen;

  JOCTET* mBackBuffer;
  uint32_t mBackBufferLen; // Offset of end of active backtrack data
  uint32_t mBackBufferSize; // size in bytes what mBackBuffer was created with
  uint32_t mBackBufferUnreadLen; // amount of data currently in mBackBuffer

  JOCTET * mProfile;
  uint32_t mProfileLength;

  qcms_profile* mInProfile;
  qcms_transform* mTransform;

  bool mReading;

  const Decoder::DecodeStyle mDecodeStyle;

  uint32_t mCMSMode;
  RLBench JpegBench;
  bool activatePerImage;
};

} // namespace image
} // namespace mozilla

#endif // mozilla_image_decoders_nsJPEGDecoder_h
