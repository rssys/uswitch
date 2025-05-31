/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ImageLogging.h"  // Must appear first.

#include "nsJPEGDecoder.h"

#include <cstdio>
#include <cstdint>

#include "imgFrame.h"
#include "Orientation.h"
#include "EXIF.h"

#include "nsIInputStream.h"

#include "nspr.h"
#include "nsCRT.h"
#include "gfxColor.h"

#include "jerror.h"

#include "gfxPlatform.h"
#include "mozilla/EndianUtils.h"
#include "mozilla/Telemetry.h"

extern "C" {
#include "iccjpeg.h"
}

#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

#include "sandboxmanager.h"

using namespace mozilla::image;
rlbox_load_library_api(jpeglib, TRLSandbox)
thread_local void *jpegRendererSaved = nullptr;

//static_assert(0 == sizeof(rlbox::tainted<decoder_error_mgr, TRLSandbox>::pub));

#define sandbox_invoke_custom(sandbox, ctx, fnName, ...) \
  sandbox_invoke_custom_helper_jpeg(sandbox, ctx, \
  ((JPEGSandboxResource *)sandbox->getSandbox()->custom_data)->fnName ? \
  ((JPEGSandboxResource *)sandbox->getSandbox()->custom_data)->fnName : \
  (((JPEGSandboxResource *)sandbox->getSandbox()->custom_data)->fnName = \
    (decltype(JPEGSandboxResource::fnName))sandbox->getSandbox()->get_symbol_addr(#fnName)) \
  , ##__VA_ARGS__)
#define sandbox_invoke_custom_with_ptr(sandbox, ctx, fnPtr, ...) \
  sandbox_invoke_custom_helper_jpeg(sandbox, ctx, fnPtr, ##__VA_ARGS__)

template<typename TFunc, typename... TArgs>
inline typename std::enable_if<!std::is_void<return_argument<TFunc>>::value,
tainted<return_argument<TFunc>, TRLSandbox>
>::type sandbox_invoke_custom_helper_jpeg(RLBoxSandbox<TRLSandbox>* sandbox, uswctx_t ctx, TFunc* fnPtr, TArgs&&... params)
{
  auto ret = sandbox->invokeWithFunctionPointerWithExtraData(fnPtr, ctx, params...);
  return ret;
}

template<typename TFunc, typename... TArgs>
inline typename std::enable_if<std::is_void<return_argument<TFunc>>::value,
void
>::type sandbox_invoke_custom_helper_jpeg(RLBoxSandbox<TRLSandbox>* sandbox, uswctx_t ctx, TFunc* fnPtr, TArgs&&... params)
{
  sandbox->invokeWithFunctionPointerWithExtraData(fnPtr, ctx, params...);
}

#if MOZ_BIG_ENDIAN
#define MOZ_JCS_EXT_NATIVE_ENDIAN_XRGB JCS_EXT_XRGB
#else
#define MOZ_JCS_EXT_NATIVE_ENDIAN_XRGB JCS_EXT_BGRX
#endif

static void cmyk_convert_rgb(JSAMPROW row, JDIMENSION width);

namespace mozilla {
namespace image {

static mozilla::LazyLogModule sJPEGLog("JPEGDecoder");

static mozilla::LazyLogModule sJPEGDecoderAccountingLog("JPEGDecoderAccounting");

#define ICC_MARKER  (JPEG_APP0 + 2)      /* JPEG marker code for ICC */
#define ICC_OVERHEAD_LEN  14             /* size of non-profile data in APP2 */
#define MAX_BYTES_IN_MARKER  65533       /* maximum data len of a JPEG marker */
#define MAX_DATA_BYTES_IN_MARKER  (MAX_BYTES_IN_MARKER - ICC_OVERHEAD_LEN)

static boolean marker_is_icc_cpp (RLBoxSandbox<TRLSandbox>* rlbox_sandbox, tainted<jpeg_saved_marker_ptr, TRLSandbox> marker)
{
  JOCTET data[12] = { 0 };
  JOCTET* dataCopy = marker->data.copyAndVerifyArray(rlbox_sandbox, [](JOCTET* arr){ return RLBox_Verify_Status::SAFE; }, 12, (JOCTET*) data);
  return
    marker->marker.UNSAFE_Unverified() == ICC_MARKER &&
    marker->data_length.UNSAFE_Unverified() >= ICC_OVERHEAD_LEN &&
    /* verify the identifying string */
    GETJOCTET(dataCopy[0]) == 0x49 &&
    GETJOCTET(dataCopy[1]) == 0x43 &&
    GETJOCTET(dataCopy[2]) == 0x43 &&
    GETJOCTET(dataCopy[3]) == 0x5F &&
    GETJOCTET(dataCopy[4]) == 0x50 &&
    GETJOCTET(dataCopy[5]) == 0x52 &&
    GETJOCTET(dataCopy[6]) == 0x4F &&
    GETJOCTET(dataCopy[7]) == 0x46 &&
    GETJOCTET(dataCopy[8]) == 0x49 &&
    GETJOCTET(dataCopy[9]) == 0x4C &&
    GETJOCTET(dataCopy[10]) == 0x45 &&
    GETJOCTET(dataCopy[11]) == 0x0;
}

boolean read_icc_profile_cpp (RLBoxSandbox<TRLSandbox>* rlbox_sandbox, tainted<j_decompress_ptr, TRLSandbox> cinfo, JOCTET** icc_data_ptr, unsigned int* icc_data_len)
{
  tainted<jpeg_saved_marker_ptr, TRLSandbox> marker;
  int num_markers = 0;
  int seq_no;
  JOCTET* icc_data;
  unsigned int total_length;
#define MAX_SEQ_NO  255        /* sufficient since marker numbers are bytes */
  char marker_present[MAX_SEQ_NO+1];      /* 1 if marker found */
  unsigned int data_length[MAX_SEQ_NO+1]; /* size of profile data in marker */
  unsigned int data_offset[MAX_SEQ_NO+1]; /* offset for data in marker */
  int data_seq[MAX_SEQ_NO+1]; /* shouldn't be reading the section sequence numbers twice */
  *icc_data_ptr = NULL;                   /* avoid confusion if FALSE return */
  *icc_data_len = 0;
  /* This first pass over the saved markers discovers whether there are
   * any ICC markers and verifies the consistency of the marker numbering.
   */
  for (seq_no = 1; seq_no <= MAX_SEQ_NO; seq_no++) {
    marker_present[seq_no] = 0;
  }

  unsigned int currSeqNum= 0;
  for (marker = cinfo->marker_list; marker != nullptr; marker = marker->next) {

    if (marker_is_icc_cpp(rlbox_sandbox, marker)) {
      tainted<JOCTET *, TRLSandbox> data = marker->data;
      //data13 read twice so read it once so it doesn't change after one read
      //data itself is handled carefully below so we don't need any verifications
      JOCTET data13 = (data.UNSAFE_Unverified())[13];
      if (num_markers == 0) {
        num_markers = GETJOCTET(data13);
      } else if (num_markers != GETJOCTET(data13)) {
        return FALSE;  /* inconsistent num_markers fields */
      }
      data_seq[currSeqNum] = GETJOCTET((data.UNSAFE_Unverified())[12]);
      seq_no = data_seq[currSeqNum];
      currSeqNum++;
      if (seq_no <= 0 || seq_no > num_markers) {
        return FALSE;   /* bogus sequence number */
      }
      if (marker_present[seq_no]) {
        return FALSE;   /* duplicate sequence numbers */
      }
      marker_present[seq_no] = 1;
      //data_length is used with caution below, so we don't have to verify its contents here
      data_length[seq_no] = marker->data_length.UNSAFE_Unverified() - ICC_OVERHEAD_LEN;
    }
  }

  if (num_markers == 0) {
    return FALSE;
  }

  /* Check for missing markers, count total space needed,
   * compute offset of each marker's part of the data.
   */

  total_length = 0;
  for (seq_no = 1; seq_no <= num_markers; seq_no++) {
    if (marker_present[seq_no] == 0) {
      return FALSE;  /* missing sequence number */
    }
    data_offset[seq_no] = total_length;
    total_length += data_length[seq_no];
  }

  if (total_length <= 0) {
    return FALSE;  /* found only empty markers? */
  }

  /* Allocate space for assembled data */
  icc_data = (JOCTET*) malloc(total_length * sizeof(JOCTET));
  if (icc_data == NULL) {
    return FALSE;   /* oops, out of memory */
  }

  /* and fill it in */
  currSeqNum= 0;
  for (marker = cinfo->marker_list; marker != nullptr; marker = marker->next) {
    tainted<JOCTET *, TRLSandbox> data = marker->data;

    if (marker_is_icc_cpp(rlbox_sandbox, marker)) {
      JOCTET* dst_ptr;
      unsigned int length;
      seq_no = data_seq[currSeqNum];
      currSeqNum++;
      dst_ptr = icc_data + data_offset[seq_no];
      tainted<JOCTET*, TRLSandbox> src_ptr = data.getPointerIncrement(rlbox_sandbox, ICC_OVERHEAD_LEN);
      length = data_length[seq_no];
      while (length--) {
        *dst_ptr = (*src_ptr).UNSAFE_Unverified();
        dst_ptr++;
        src_ptr = src_ptr.getPointerIncrement(rlbox_sandbox, 1);
      }
    }
  }

  *icc_data_ptr = icc_data;
  *icc_data_len = total_length;

  return TRUE;
}

static qcms_profile* GetICCProfile(RLBoxSandbox<TRLSandbox>* rlbox_sandbox, tainted<struct jpeg_decompress_struct *, TRLSandbox> info)
{
    JOCTET* profilebuf;
    uint32_t profileLength;
    qcms_profile* profile = nullptr;

    if (read_icc_profile_cpp(rlbox_sandbox, info, &profilebuf, &profileLength)) {
      profile = qcms_profile_from_memory(profilebuf, profileLength);
      free(profilebuf);
    }

    return profile;
}

METHODDEF(void) init_source (RLBoxSandbox<TRLSandbox>* sandbox, tainted<j_decompress_ptr, TRLSandbox> jd);
METHODDEF(boolean) fill_input_buffer (RLBoxSandbox<TRLSandbox>* sandbox, tainted<j_decompress_ptr, TRLSandbox> jd);
METHODDEF(void) skip_input_data (RLBoxSandbox<TRLSandbox>* sandbox, tainted<j_decompress_ptr, TRLSandbox> jd, tainted<long, TRLSandbox> num_bytes);
METHODDEF(void) term_source (RLBoxSandbox<TRLSandbox>* sandbox, tainted<j_decompress_ptr, TRLSandbox> jd);
METHODDEF(void) my_error_exit (RLBoxSandbox<TRLSandbox>* sandbox, tainted<j_common_ptr, TRLSandbox> cinfo);

// Normal JFIF markers can't have more bytes than this.
#define MAX_JPEG_MARKER_LENGTH  (((uint32_t)1 << 16) - 1)

class JPEGSandboxResource : public USwitchSandboxResource {
public:
  RLBoxSandbox<TRLSandbox>* rlbox_sandbox;
  sandbox_callback_helper<void(j_decompress_ptr jd), TRLSandbox> cpp_cb_jpeg_init_source;
  sandbox_callback_helper<boolean(j_decompress_ptr jd), TRLSandbox> cpp_cb_jpeg_fill_input_buffer;
  sandbox_callback_helper<void(j_decompress_ptr jd, long num_bytes), TRLSandbox> cpp_cb_jpeg_skip_input_data;
  sandbox_callback_helper<void(j_decompress_ptr jd), TRLSandbox> cpp_cb_jpeg_term_source;
  sandbox_callback_helper<void(j_common_ptr cinfo), TRLSandbox> cpp_cb_jpeg_my_error_exit;
  tainted<boolean(*)(j_decompress_ptr, int), TRLSandbox> cpp_resync_to_restart = nullptr;
  decltype(::jpeg_calc_output_dimensions) *jpeg_calc_output_dimensions = nullptr;
  decltype(::jpeg_consume_input) *jpeg_consume_input = nullptr;
  decltype(::jpeg_CreateDecompress) *jpeg_CreateDecompress = nullptr;
  decltype(::jpeg_destroy_decompress) *jpeg_destroy_decompress = nullptr;
  decltype(::jpeg_finish_decompress) *jpeg_finish_decompress = nullptr;
  decltype(::jpeg_finish_output) *jpeg_finish_output = nullptr;
  decltype(::jpeg_has_multiple_scans) *jpeg_has_multiple_scans = nullptr;
  decltype(::jpeg_input_complete) *jpeg_input_complete = nullptr;
  decltype(::jpeg_read_header) *jpeg_read_header = nullptr;
  decltype(::jpeg_read_scanlines) *jpeg_read_scanlines = nullptr;
  decltype(::jpeg_save_markers) *jpeg_save_markers = nullptr;
  decltype(::jpeg_start_decompress) *jpeg_start_decompress = nullptr;
  decltype(::jpeg_start_output) *jpeg_start_output = nullptr;
  decltype(::jpeg_std_error) *jpeg_std_error = nullptr;
  std::mutex callbackMutex;
  JPEGSandboxResource() {
    static const std::vector<unsigned int> AllowedSyscalls {__NR_exit, __NR_futex, __NR_sched_yield};
    const std::string &libPath = getLibraryPath("libjpeg.so");
    rlbox_sandbox = RLBoxSandbox<TRLSandbox>::createSandbox("", libPath.c_str());
    if (!rlbox_sandbox->getSandbox()->init_seccomp(AllowedSyscalls)) {
      fprintf(stderr, "failed to init seccomp for JPEG sandbox\n");
      exit(1);
    }
    rlbox_sandbox->getSandbox()->custom_data = (void *)this;
    cpp_cb_jpeg_init_source = rlbox_sandbox->createCallback(init_source);
    cpp_cb_jpeg_fill_input_buffer = rlbox_sandbox->createCallback(fill_input_buffer);
    cpp_cb_jpeg_skip_input_data = rlbox_sandbox->createCallback(skip_input_data);
    cpp_cb_jpeg_term_source = rlbox_sandbox->createCallback(term_source);
    cpp_resync_to_restart = sandbox_function(rlbox_sandbox, jpeg_resync_to_restart);
    cpp_cb_jpeg_my_error_exit = rlbox_sandbox->createCallback(my_error_exit);
  }
  virtual ~JPEGSandboxResource() {
    rlbox_sandbox->destroySandbox();
    delete rlbox_sandbox;
  }
};

SandboxManager<JPEGSandboxResource, 4, 6> sandboxManagerJPEG;

nsJPEGDecoder::nsJPEGDecoder(RasterImage* aImage,
                             Decoder::DecodeStyle aDecodeStyle,
                             RasterImage* aImageExtra)
 : Decoder(aImage)
 , mLexer(Transition::ToUnbuffered(State::FINISHED_JPEG_DATA,
                                   State::JPEG_DATA,
                                   SIZE_MAX),
          Transition::TerminateSuccess())
 , mDecodeStyle(aDecodeStyle)
{
  mhostString = getHostStringFromImage(aImage != nullptr? aImage : aImageExtra);
  rlbox_sbx_shared = sandboxManagerJPEG.createSandbox(mhostString);
  rlbox_sbx = rlbox_sbx_shared.get();
  auto rlbox_sandbox = rlbox_sbx->rlbox_sandbox;
  p_mInfo = rlbox_sandbox->mallocInSandbox<jpeg_decompress_struct>();
  p_mErr = rlbox_sandbox->mallocInSandbox<decoder_error_mgr>();
  p_mSourceMgr = rlbox_sandbox->mallocInSandbox<jpeg_source_mgr>();

  if(p_mInfo == nullptr || p_mErr == nullptr || p_mSourceMgr == nullptr)
  {
      MOZ_LOG(sJPEGDecoderAccountingLog, LogLevel::Error,
         ("nsJPEGDecoder::nsJPEGDecoder: Failed in mallocing objects in sandbox"));
  }

  auto& mInfo = *p_mInfo;
  auto& mSourceMgr = *p_mSourceMgr;

  mState = JPEG_HEADER;
  mReading = true;
  mImageData = nullptr;

  mBytesToSkip = 0;
  memset((&mInfo).UNSAFE_Unverified(), 0, sizeof(jpeg_decompress_struct));
  memset((&mSourceMgr).UNSAFE_Unverified(), 0, sizeof(mSourceMgr));
  mInfo.client_data = rlbox_sandbox->app_ptr((void*)this);

  mSegment = nullptr;
  mSegmentLen = 0;

  mBackBuffer = nullptr;
  mBackBufferLen = mBackBufferSize = mBackBufferUnreadLen = 0;

  mInProfile = nullptr;
  mTransform = nullptr;

  mCMSMode = 0;

  JpegBench.Init();

  s_mSegment = nullptr;
  s_mSegmentLen = 0;
  s_mBackBuffer = nullptr;
  s_mBackBufferLen = 0;

  MOZ_LOG(sJPEGDecoderAccountingLog, LogLevel::Debug,
         ("nsJPEGDecoder::nsJPEGDecoder: Creating JPEG decoder %p",
          this));
}

unsigned long long invJpeg = 0;

nsJPEGDecoder::~nsJPEGDecoder()
{
  // Step 8: Release JPEG decompression object
  auto& mInfo = *p_mInfo;
  mInfo.src = nullptr;
  auto rlbox_sandbox = rlbox_sbx->rlbox_sandbox;
  sandbox_invoke_custom(rlbox_sandbox, rlbox_sandbox->getSandbox()->get_context(), jpeg_destroy_decompress, &mInfo);

  free(mBackBuffer);
  mBackBuffer = nullptr;
  if (mTransform) {
    qcms_transform_release(mTransform);
  }
  if (mInProfile) {
    qcms_profile_release(mInProfile);
  }

  rlbox_sandbox->freeInSandbox(p_mSourceMgr);
  rlbox_sandbox->freeInSandbox(p_mErr);
  rlbox_sandbox->freeInSandbox(p_mInfo);

  if(s_mSegmentLen != 0) {
    rlbox_sandbox->freeInSandbox(s_mSegment);
  }
  if(s_mBackBufferLen != 0) {
    rlbox_sandbox->freeInSandbox(s_mBackBuffer);
  }

  MOZ_LOG(sJPEGDecoderAccountingLog, LogLevel::Debug,
         ("nsJPEGDecoder::~nsJPEGDecoder: Destroying JPEG decoder %p",
          this));
}

Maybe<Telemetry::HistogramID>
nsJPEGDecoder::SpeedHistogram() const
{
  return Some(Telemetry::IMAGE_DECODE_SPEED_JPEG);
}

nsresult
nsJPEGDecoder::InitInternal()
{
  mCMSMode = gfxPlatform::GetCMSMode();
  if (GetSurfaceFlags() & SurfaceFlags::NO_COLORSPACE_CONVERSION) {
    mCMSMode = eCMSMode_Off;
  }

  auto& mInfo = *p_mInfo;
  auto& mErr = *p_mErr;

  auto rlbox_sandbox = rlbox_sbx->rlbox_sandbox;
  // We set up the normal JPEG error routines, then override error_exit.
  auto ctx = rlbox_sandbox->getSandbox()->get_context();
  mInfo.err = sandbox_invoke_custom(rlbox_sandbox, ctx, jpeg_std_error, &mErr.pub);
  //   mInfo.err = jpeg_std_error(&mErr.pub);
  mErr.pub.error_exit = rlbox_sbx->cpp_cb_jpeg_my_error_exit;
  // Establish the setjmp return context for my_error_exit to use.
  if (setjmp(m_jmpBuff)) {
    // If we get here, the JPEG code has signaled an error, and initialization
    // has failed.
    return NS_ERROR_FAILURE;
  }
  m_jmpBuffValid = TRUE;

  // Step 1: allocate and initialize JPEG decompression object
  sandbox_invoke_custom(rlbox_sandbox, ctx, jpeg_CreateDecompress, &mInfo, JPEG_LIB_VERSION, (size_t) sizeof(struct jpeg_decompress_struct));
  // Set the source manager
  auto& mSourceMgr = *p_mSourceMgr;
  mInfo.src = &mSourceMgr;

  // Step 2: specify data source (eg, a file)
  // Setup callback functions.
  mSourceMgr.init_source = rlbox_sbx->cpp_cb_jpeg_init_source;
  mSourceMgr.fill_input_buffer = rlbox_sbx->cpp_cb_jpeg_fill_input_buffer;
  mSourceMgr.skip_input_data = rlbox_sbx->cpp_cb_jpeg_skip_input_data;
  mSourceMgr.resync_to_restart = rlbox_sbx->cpp_resync_to_restart;
  mSourceMgr.term_source = rlbox_sbx->cpp_cb_jpeg_term_source;

  // Record app markers for ICC data
  for (uint32_t m = 0; m < 16; m++) {
    sandbox_invoke_custom(rlbox_sandbox, ctx, jpeg_save_markers, &mInfo, JPEG_APP0 + m, 0xFFFF);
  }

  return NS_OK;
}

nsresult
nsJPEGDecoder::FinishInternal()
{
  // If we're not in any sort of error case, force our state to JPEG_DONE.
  if ((mState != JPEG_DONE && mState != JPEG_SINK_NON_JPEG_TRAILER) &&
      (mState != JPEG_ERROR) &&
      !IsMetadataDecode()) {
    mState = JPEG_DONE;
  }

  jpegRendererSaved = nullptr;

  return NS_OK;
}

LexerResult
nsJPEGDecoder::DoDecode(SourceBufferIterator& aIterator, IResumable* aOnResume)
{
  MOZ_ASSERT(!HasError(), "Shouldn't call DoDecode after error!");

  return mLexer.Lex(aIterator, aOnResume,
                    [=](State aState, const char* aData, size_t aLength) {
    switch (aState) {
      case State::JPEG_DATA:
        return ReadJPEGData(aData, aLength);
      case State::FINISHED_JPEG_DATA:
        return FinishedJPEGData();
    }
    MOZ_CRASH("Unknown State");
  });
}

J_COLOR_SPACE jpegJColorSpaceVerifier(J_COLOR_SPACE val)
{
    //J_COLOR_SPACE is an enum with JCS_RGB565 as the last value and JCS_UNKNOWN as the error value
    //Firefox already gracefully handles JCS_UNKNOWN
    return val <= JCS_RGB565? val : JCS_UNKNOWN;
}

LexerTransition<nsJPEGDecoder::State>
nsJPEGDecoder::ReadJPEGData(const char* aData, size_t aLength)
{
  jpegRendererSaved = this;

  auto rlbox_sandbox = rlbox_sbx->rlbox_sandbox;
  auto ctx = rlbox_sandbox->getSandbox()->get_context();
  mSegment = reinterpret_cast<const JOCTET*>(aData);
  mSegmentLen = aLength;

  // Return here if there is a fatal error within libjpeg.
  nsresult error_code;
  // This cast to nsresult makes sense because setjmp() returns whatever we
  // passed to longjmp(), which was actually an nsresult.
  if ((error_code = static_cast<nsresult>(setjmp(m_jmpBuff))) != NS_OK) {
    if (error_code == NS_ERROR_FAILURE) {
      // Error due to corrupt data. Make sure that we don't feed any more data
      // to libjpeg-turbo.
      mState = JPEG_SINK_NON_JPEG_TRAILER;
      MOZ_LOG(sJPEGDecoderAccountingLog, LogLevel::Debug,
             ("} (setjmp returned NS_ERROR_FAILURE)"));
    } else {
      // Error for another reason. (Possibly OOM.)
      mState = JPEG_ERROR;
      MOZ_LOG(sJPEGDecoderAccountingLog, LogLevel::Debug,
             ("} (setjmp returned an error)"));
    }

    return Transition::TerminateFailure();
  }
  m_jmpBuffValid = TRUE;

  MOZ_LOG(sJPEGLog, LogLevel::Debug,
         ("[this=%p] nsJPEGDecoder::Write -- processing JPEG data\n", this));

  auto& mInfo = *p_mInfo;
  
  switch (mState) {
    case JPEG_HEADER: {
      LOG_SCOPE((mozilla::LogModule*)sJPEGLog, "nsJPEGDecoder::Write -- entering JPEG_HEADER"
                " case");

      // Step 3: read file parameters with jpeg_read_header()
      auto read_header_ret = sandbox_invoke_custom(rlbox_sandbox, ctx, jpeg_read_header, &mInfo, TRUE)
        .copyAndVerify([](int val){
        if(val == JPEG_SUSPENDED || val == JPEG_HEADER_OK || val == JPEG_HEADER_TABLES_ONLY) {
          return val;
        }
        printf("nsJPEGDecoder jpeg_read_header bad return value\n");
        abort();
      });
      if (read_header_ret == JPEG_SUSPENDED) {
        MOZ_LOG(sJPEGDecoderAccountingLog, LogLevel::Debug,
               ("} (JPEG_SUSPENDED)"));
        return Transition::ContinueUnbuffered(State::JPEG_DATA); // I/O suspension
      }

      // Post our size to the superclass
      auto image_width = mInfo.image_width.copyAndVerify([this](JDIMENSION val) {
        if(HasSize()) {
          auto s = Size();
          if(s.width < 0 || ((unsigned) s.width) != val) {
            printf("nsJPEGDecoder::nsJPEGDecoder: unexpected image width\n");
            abort();
          }
        }
        return val; 
      });
      mImageWidth = image_width;

      auto image_height = mInfo.image_height.copyAndVerify([this](JDIMENSION val) {
        if(HasSize()) {
          auto s = Size();
          if(s.height < 0 || ((unsigned) s.height) != val) {
            printf("nsJPEGDecoder::nsJPEGDecoder: unexpected image height\n");
            abort();
          }
        }
        return val; 
      });

      PostSize(image_width, image_height,
               ReadOrientationFromEXIF());
      if (HasError()) {
        // Setting the size led to an error.
        mState = JPEG_ERROR;
        return Transition::TerminateFailure();
      }

      // If we're doing a metadata decode, we're done.
      if (IsMetadataDecode()) {
        return Transition::TerminateSuccess();
      }

      JpegBench.Start();

      auto jpeg_color_space = mInfo.jpeg_color_space.copyAndVerify(jpegJColorSpaceVerifier);

      // We're doing a full decode.
      if (mCMSMode != eCMSMode_Off &&
          (mInProfile = GetICCProfile(rlbox_sandbox, p_mInfo)) != nullptr) {
        uint32_t profileSpace = qcms_profile_get_color_space(mInProfile);
        bool mismatch = false;

#ifdef DEBUG_tor
      fprintf(stderr, "JPEG profileSpace: 0x%08X\n", profileSpace);
#endif

      J_COLOR_SPACE out_color_space_shadow = mInfo.out_color_space.copyAndVerify(jpegJColorSpaceVerifier);

      switch (jpeg_color_space) {
        case JCS_GRAYSCALE:
          if (profileSpace == icSigRgbData) {
            out_color_space_shadow = JCS_RGB;
          } else if (profileSpace != icSigGrayData) {
            mismatch = true;
          }
          break;
        case JCS_RGB:
          if (profileSpace != icSigRgbData) {
            mismatch =  true;
          }
          break;
        case JCS_YCbCr:
          if (profileSpace == icSigRgbData) {
            mInfo.out_color_space = JCS_RGB;
            out_color_space_shadow = JCS_RGB;
          } else {
            // qcms doesn't support ycbcr
            mismatch = true;
          }
          break;
        case JCS_CMYK:
        case JCS_YCCK:
            // qcms doesn't support cmyk
            mismatch = true;
          break;
        default:
          mState = JPEG_ERROR;
          MOZ_LOG(sJPEGDecoderAccountingLog, LogLevel::Debug,
                 ("} (unknown colorpsace (1))"));
          return Transition::TerminateFailure();
      }

      if (!mismatch) {
        qcms_data_type type;
        switch(out_color_space_shadow) {
          case JCS_GRAYSCALE:
            type = QCMS_DATA_GRAY_8;
            break;
          case JCS_RGB:
            type = QCMS_DATA_RGB_8;
            break;
          default:
            mState = JPEG_ERROR;
            MOZ_LOG(sJPEGDecoderAccountingLog, LogLevel::Debug,
                   ("} (unknown colorpsace (2))"));
            return Transition::TerminateFailure();
        }
#if 0
        // We don't currently support CMYK profiles. The following
        // code dealt with lcms types. Add something like this
        // back when we gain support for CMYK.

        // Adobe Photoshop writes YCCK/CMYK files with inverted data
        if (mInfo.out_color_space == JCS_CMYK) {
          type |= FLAVOR_SH(mInfo.saw_Adobe_marker ? 1 : 0);
        }
#endif

        if (gfxPlatform::GetCMSOutputProfile()) {

          // Calculate rendering intent.
          int intent = gfxPlatform::GetRenderingIntent();
          if (intent == -1) {
            intent = qcms_profile_get_rendering_intent(mInProfile);
          }

          // Create the color management transform.
          mTransform = qcms_transform_create(mInProfile,
                                          type,
                                          gfxPlatform::GetCMSOutputProfile(),
                                          QCMS_DATA_RGB_8,
                                          (qcms_intent)intent);
        }
      } else {
#ifdef DEBUG_tor
        fprintf(stderr, "ICM profile colorspace mismatch\n");
#endif
      }
    }

    if (!mTransform) {
      switch (jpeg_color_space) {
        case JCS_GRAYSCALE:
        case JCS_RGB:
        case JCS_YCbCr:
          // if we're not color managing we can decode directly to
          // MOZ_JCS_EXT_NATIVE_ENDIAN_XRGB
          if (mCMSMode != eCMSMode_All) {
              mInfo.out_color_space = MOZ_JCS_EXT_NATIVE_ENDIAN_XRGB;
              mInfo.out_color_components = 4;
          } else {
              mInfo.out_color_space = JCS_RGB;
          }
          break;
        case JCS_CMYK:
        case JCS_YCCK:
          // libjpeg can convert from YCCK to CMYK, but not to RGB
          mInfo.out_color_space = JCS_CMYK;
          break;
        default:
          mState = JPEG_ERROR;
          MOZ_LOG(sJPEGDecoderAccountingLog, LogLevel::Debug,
                 ("} (unknown colorpsace (3))"));
          return Transition::TerminateFailure();
      }
    }

    // Don't allocate a giant and superfluous memory buffer
    // when not doing a progressive decode.
    m_out_color_space = mInfo.out_color_space.copyAndVerify(jpegJColorSpaceVerifier);
    mInfo.buffered_image = mDecodeStyle == PROGRESSIVE && sandbox_invoke_custom(rlbox_sandbox, ctx, jpeg_has_multiple_scans, &mInfo).UNSAFE_Unverified();
    mInfo.buffered_image.freeze();

    /* Used to set up image size so arrays can be allocated */
    sandbox_invoke_custom(rlbox_sandbox, ctx, jpeg_calc_output_dimensions, &mInfo);

    MOZ_ASSERT(!mImageData, "Already have a buffer allocated?");
    nsresult rv = AllocateFrame(/* aFrameNum = */ 0, OutputSize(),
                                FullOutputFrame(), SurfaceFormat::B8G8R8X8);
    if (NS_FAILED(rv)) {
      mState = JPEG_ERROR;
      MOZ_LOG(sJPEGDecoderAccountingLog, LogLevel::Debug,
             ("} (could not initialize image frame)"));
      return Transition::TerminateFailure();
    }

    MOZ_ASSERT(mImageData, "Should have a buffer now");

    if (mDownscaler) {
      nsresult rv = mDownscaler->BeginFrame(Size(), Nothing(),
                                            mImageData,
                                            /* aHasAlpha = */ false);
      if (NS_FAILED(rv)) {
        mState = JPEG_ERROR;
        return Transition::TerminateFailure();
      }
    }

    MOZ_LOG(sJPEGDecoderAccountingLog, LogLevel::Debug,
           ("        JPEGDecoderAccounting: nsJPEGDecoder::"
            "Write -- created image frame with %ux%u pixels",
            image_width, image_height));

    mState = JPEG_START_DECOMPRESS;
    MOZ_FALLTHROUGH; // to start decompressing.
  }

  case JPEG_START_DECOMPRESS: {
    LOG_SCOPE((mozilla::LogModule*)sJPEGLog, "nsJPEGDecoder::Write -- entering"
                            " JPEG_START_DECOMPRESS case");
    // Step 4: set parameters for decompression

    // FIXME -- Should reset dct_method and dither mode
    // for final pass of progressive JPEG
    JpegBench.StartIfNeeded();
    mInfo.dct_method =  JDCT_ISLOW;
    mInfo.dither_mode = JDITHER_FS;
    mInfo.do_fancy_upsampling = TRUE;
    mInfo.enable_2pass_quant = FALSE;
    mInfo.do_block_smoothing = TRUE;

    // Step 5: Start decompressor
    if (sandbox_invoke_custom(rlbox_sandbox, ctx, jpeg_start_decompress, &mInfo).UNSAFE_Unverified() == FALSE) {
      JpegBench.Stop();
      MOZ_LOG(sJPEGDecoderAccountingLog, LogLevel::Debug,
            ("} (I/O suspension after jpeg_start_decompress())"));
      return Transition::ContinueUnbuffered(State::JPEG_DATA); // I/O suspension
    }

    // If this is a progressive JPEG ...
    mState = mInfo.buffered_image.UNSAFE_Unverified()? JPEG_DECOMPRESS_PROGRESSIVE : JPEG_DECOMPRESS_SEQUENTIAL;
    mInfo.buffered_image.unfreeze();
    MOZ_FALLTHROUGH; // to decompress sequential JPEG.
  }

  case JPEG_DECOMPRESS_SEQUENTIAL: {
    if (mState == JPEG_DECOMPRESS_SEQUENTIAL) {
      LOG_SCOPE((mozilla::LogModule*)sJPEGLog, "nsJPEGDecoder::Write -- "
                              "JPEG_DECOMPRESS_SEQUENTIAL case");

      m_output_height_shadow = mInfo.output_height.UNSAFE_Unverified();
      bool suspend;
      JpegBench.StartIfNeeded();
      OutputScanlines(&suspend);

      if (suspend) {
        JpegBench.Stop();
        MOZ_LOG(sJPEGDecoderAccountingLog, LogLevel::Debug,
               ("} (I/O suspension after OutputScanlines() - SEQUENTIAL)"));
        return Transition::ContinueUnbuffered(State::JPEG_DATA); // I/O suspension
      }

      // If we've completed image output ...
      NS_ASSERTION(mInfo.output_scanline.UNSAFE_Unverified() == mInfo.output_height.UNSAFE_Unverified(),
                     "We didn't process all of the data!");
      mState = JPEG_DONE;
    }
    MOZ_FALLTHROUGH; // to decompress progressive JPEG.
  }

  case JPEG_DECOMPRESS_PROGRESSIVE: {
    if (mState == JPEG_DECOMPRESS_PROGRESSIVE) {
      LOG_SCOPE((mozilla::LogModule*)sJPEGLog,
                "nsJPEGDecoder::Write -- JPEG_DECOMPRESS_PROGRESSIVE case");

      JpegBench.StartIfNeeded();
      int status;
      do {
        status = sandbox_invoke_custom(rlbox_sandbox, ctx, jpeg_consume_input, &mInfo).copyAndVerify([](int val) {
          if(val == JPEG_SUSPENDED || val == JPEG_REACHED_SOS || val == JPEG_REACHED_EOI || val == JPEG_ROW_COMPLETED || val == JPEG_SCAN_COMPLETED) {
            return val;
          }

          printf("nsJPEGDecoder::nsJPEGDecoder got an unknown status from jpeg_consume_input\n");
          abort();
        });
      } while ((status != JPEG_SUSPENDED) &&
               (status != JPEG_REACHED_EOI));

      auto output_scanline_verifier = [this](JDIMENSION val) {
        if(val > m_output_height_shadow) {
           // Just set it back inbounds. A corrupted image is better than taking down the site due to a bad parse
          val = m_output_height_shadow;
        }
        return val;
      };

      for (;;) {
        int scan = mInfo.input_scan_number.UNSAFE_Unverified();
        int output_scan_number = mInfo.output_scan_number.copyAndVerify([&scan](int val ){
          if(val > scan) {
            printf("nsJPEGDecoder::nsJPEGDecoder: output_scan_number > input_scan_number\n");
            abort();
          }
          return val;
        });
        //invariant output_scanline <= output_height
        m_output_height_shadow = mInfo.output_height.UNSAFE_Unverified();

        JDIMENSION output_scanline = mInfo.output_scanline.copyAndVerify(output_scanline_verifier);

        if (output_scanline == 0) {
          // if we haven't displayed anything yet (output_scan_number==0)
          // and we have enough data for a complete scan, force output
          // of the last full scan
          if ((output_scan_number == 0) &&
              (scan > 1) &&
              (status != JPEG_REACHED_EOI))
            scan--;

          if (!sandbox_invoke_custom(rlbox_sandbox, ctx, jpeg_start_output, &mInfo, scan).UNSAFE_Unverified()) {
            JpegBench.Stop();
            MOZ_LOG(sJPEGDecoderAccountingLog, LogLevel::Debug,
                   ("} (I/O suspension after jpeg_start_output() -"
                    " PROGRESSIVE)"));
            return Transition::ContinueUnbuffered(State::JPEG_DATA); // I/O suspension
          }
        }

        output_scanline = mInfo.output_scanline.copyAndVerify(output_scanline_verifier);

        if (output_scanline == 0xffffff) {
          mInfo.output_scanline = 0;
        }

        bool suspend;
        OutputScanlines(&suspend);

        if (suspend) {
          if (mInfo.output_scanline.UNSAFE_Unverified() == 0) {
            // didn't manage to read any lines - flag so we don't call
            // jpeg_start_output() multiple times for the same scan
            mInfo.output_scanline = 0xffffff;
          }
          JpegBench.Stop();
          MOZ_LOG(sJPEGDecoderAccountingLog, LogLevel::Debug,
                 ("} (I/O suspension after OutputScanlines() - PROGRESSIVE)"));
          return Transition::ContinueUnbuffered(State::JPEG_DATA); // I/O suspension
        }

        output_scanline = mInfo.output_scanline.copyAndVerify(output_scanline_verifier);

        if (output_scanline == m_output_height_shadow) {
          if (!sandbox_invoke_custom(rlbox_sandbox, ctx, jpeg_finish_output, &mInfo).UNSAFE_Unverified()) {
            JpegBench.Stop();
            MOZ_LOG(sJPEGDecoderAccountingLog, LogLevel::Debug,
                   ("} (I/O suspension after jpeg_finish_output() -"
                    " PROGRESSIVE)"));
            return Transition::ContinueUnbuffered(State::JPEG_DATA); // I/O suspension
          }

          int input_scan_number = mInfo.input_scan_number.UNSAFE_Unverified();
          output_scan_number = mInfo.output_scan_number.copyAndVerify([&input_scan_number](int val) {
            if(val > input_scan_number) {
              printf("nsJPEGDecoder::nsJPEGDecoder: output_scan_number > input_scan_number\n");
              abort();
            }
            return val;
          });
          //bool return - no verification
          if (sandbox_invoke_custom(rlbox_sandbox, ctx, jpeg_input_complete, &mInfo).UNSAFE_Unverified() &&
              (input_scan_number == output_scan_number))
            break;

          mInfo.output_scanline = 0;
          if (mDownscaler) {
            mDownscaler->ResetForNextProgressivePass();
          }
        }
      }

      mState = JPEG_DONE;
    }
    MOZ_FALLTHROUGH; // to finish decompressing.
  }

  case JPEG_DONE: {
    LOG_SCOPE((mozilla::LogModule*)sJPEGLog, "nsJPEGDecoder::ProcessData -- entering"
                            " JPEG_DONE case");

    JpegBench.StartIfNeeded();
    // Step 7: Finish decompression
    if (sandbox_invoke_custom(rlbox_sandbox, ctx, jpeg_finish_decompress, &mInfo).UNSAFE_Unverified() == FALSE) {
      JpegBench.Stop();
      MOZ_LOG(sJPEGDecoderAccountingLog, LogLevel::Debug,
             ("} (I/O suspension after jpeg_finish_decompress() - DONE)"));
      return Transition::ContinueUnbuffered(State::JPEG_DATA); // I/O suspension
    }

    // Make sure we don't feed any more data to libjpeg-turbo.
    mState = JPEG_SINK_NON_JPEG_TRAILER;

    // We're done.
    return Transition::TerminateSuccess();
  }
  case JPEG_SINK_NON_JPEG_TRAILER:
    MOZ_LOG(sJPEGLog, LogLevel::Debug,
           ("[this=%p] nsJPEGDecoder::ProcessData -- entering"
            " JPEG_SINK_NON_JPEG_TRAILER case\n", this));

    MOZ_ASSERT_UNREACHABLE("Should stop getting data after entering state "
                           "JPEG_SINK_NON_JPEG_TRAILER");

    return Transition::TerminateSuccess();

  case JPEG_ERROR:
    MOZ_ASSERT_UNREACHABLE("Should stop getting data after entering state "
                           "JPEG_ERROR");

    return Transition::TerminateFailure();
  }

  MOZ_ASSERT_UNREACHABLE("Escaped the JPEG decoder state machine");
  return Transition::TerminateFailure();
}

LexerTransition<nsJPEGDecoder::State>
nsJPEGDecoder::FinishedJPEGData()
{
  // Since we set up an unbuffered read for SIZE_MAX bytes, if we actually read
  // all that data something is really wrong.
  MOZ_ASSERT_UNREACHABLE("Read the entire address space?");
  return Transition::TerminateFailure();
}

Orientation
nsJPEGDecoder::ReadOrientationFromEXIF()
{
  tainted<jpeg_saved_marker_ptr, TRLSandbox> marker;
  auto& mInfo = *p_mInfo;

  // Locate the APP1 marker, where EXIF data is stored, in the marker list.
  for (marker = mInfo.marker_list ; marker != nullptr ; marker = marker->next) {
    if (marker->marker.UNSAFE_Unverified() == JPEG_APP0 + 1) {
      break;
    }
  }

  // If we're at the end of the list, there's no EXIF data.
  if (!marker) {
    return Orientation();
  }

  JOCTET* dataConv = marker->data.UNSAFE_Unverified();
  auto data_length = marker->data_length.UNSAFE_Unverified();

  // Extract the orientation information.
  EXIFData exif = EXIFParser::Parse(dataConv,
                                    static_cast<uint32_t>(data_length));
  return exif.orientation;
}

void
nsJPEGDecoder::NotifyDone()
{
  PostFrameStop(Opacity::FULLY_OPAQUE);
  PostDecodeDone();
}

void
nsJPEGDecoder::OutputScanlines(bool* suspend)
{
  *suspend = false;
  auto& mInfo = *p_mInfo;
  auto rlbox_sandbox = rlbox_sbx->rlbox_sandbox;
  auto ctx = rlbox_sandbox->getSandbox()->get_context();
  auto output_scanline_verifier = [this](JDIMENSION val) {
    if(val > m_output_height_shadow) {
      // Just set it back inbounds. A corrupted image is better than taking down the site due to a bad parse
      val = m_output_height_shadow;
    }
    return val;
  };

  const uint32_t top = mInfo.output_scanline.copyAndVerify(output_scanline_verifier);

  auto output_width = mInfo.output_width.UNSAFE_Unverified();
  auto output_components = mInfo.output_components.copyAndVerify([](int val) {
    if(val < 1) {
      printf("nsJPEGDecoder::nsJPEGDecoder: output_components < 1. Unexpected value\n");
      abort();
    }
    return val;
  });

  unsigned int row_stride = output_width * output_components;
  tainted<JSAMPARRAY, TRLSandbox> pBufferSys;
  {
    //Unfortunately, the buffer for image data used by firefox is very complex
    //So figuring out how to allocate that buffer in the sandbox needs tracking down of too many internals of firefox
    //use a simpler approach for now
    //allocate a new buffer on the target sandbox and then copy it
    //We will most likely loose perf because of this
    //Jpeg provides an api to allocate a buffer for a particular image, that will be destroyed automatically
    //we use this instead of malloc
    tainted<j_common_ptr, TRLSandbox> common_ptr = sandbox_reinterpret_cast<j_common_ptr>(&mInfo);
    tainted<jpeg_memory_mgr *, TRLSandbox> mem_mgr = mInfo.mem;
    auto p_alloc_sarray = mem_mgr->alloc_sarray.UNSAFE_Unverified();
    pBufferSys = sandbox_invoke_custom_with_ptr(rlbox_sandbox, ctx, p_alloc_sarray, common_ptr, JPOOL_IMAGE, row_stride, 1);
  }

  auto output_height_non_rounded = mInfo.output_height.copyAndVerify([this](JDIMENSION val) {
    if(m_output_height_shadow == val || m_output_height_shadow == (val + 1)) {
      return val;
    }

    printf("Could not validate image height: %u %u\n", m_output_height_shadow, val);
    abort();
  });

  JDIMENSION output_scanline_forLoop;
  while (true) {
      output_scanline_forLoop = mInfo.output_scanline.copyAndVerify(output_scanline_verifier);
      if(output_scanline_forLoop >= output_height_non_rounded) {
        break;
      }
      uint32_t* imageRow = nullptr;
      if (mDownscaler) {
        imageRow = reinterpret_cast<uint32_t*>(mDownscaler->RowBuffer());
      } else {
        imageRow = reinterpret_cast<uint32_t*>(mImageData) +
                   (output_scanline_forLoop * output_width);
      }

      MOZ_ASSERT(imageRow, "Should have a row buffer here");

      auto readScanLinesVerif = [](JDIMENSION val) {
        //max lines asked for is 1, so only valid values are 0 and 1
        if (val != 0 && val != 1) {
          printf("nsJPEGDecoder::nsJPEGDecoder: readScanLinesRet unexpected value - %d\n", val);
          abort();
        }
        return val;
      };

      J_COLOR_SPACE out_color_space = mInfo.out_color_space.copyAndVerify([this](J_COLOR_SPACE val){
        if (val != m_out_color_space) {
          printf("nsJPEGDecoder::nsJPEGDecoder: out_color_space changed\n");
          abort();
        }
        return val;
      });

      if (out_color_space == MOZ_JCS_EXT_NATIVE_ENDIAN_XRGB) {
        // Special case: scanline will be directly converted into packed ARGB
        // if (jpeg_read_scanlines(&mInfo, (JSAMPARRAY)&imageRow, 1) != 1) {

        JDIMENSION readScanLinesRet;
        //jpegStartTimerCore();
        readScanLinesRet = sandbox_invoke_custom(rlbox_sandbox, ctx, jpeg_read_scanlines, &mInfo, pBufferSys, 1).copyAndVerify(readScanLinesVerif);
        //jpegEndTimerCore();
        void* pBufferSysMemCpyTarget = (*pBufferSys).UNSAFE_Unverified();
        memcpy((void *)imageRow, pBufferSysMemCpyTarget, row_stride);

        if (readScanLinesRet != 1) {
          *suspend = true; // suspend
          break;
        }
        if (mDownscaler) {
          mDownscaler->CommitRow();
        }
        continue; // all done for this row!
      }

      JSAMPROW sampleRow = (JSAMPROW)imageRow;
      if (output_components == 3) {
        // Put the pixels at end of row to enable in-place expansion
        sampleRow += output_width;
      }

      // Request one scanline.  Returns 0 or 1 scanlines.
      //jpegStartTimerCore();
      JDIMENSION readScanLinesRet2 = sandbox_invoke_custom(rlbox_sandbox, ctx, jpeg_read_scanlines, &mInfo, pBufferSys, 1).copyAndVerify(readScanLinesVerif);
      //jpegEndTimerCore();
      void* pBufferSysMemCpyTarget2 = (*pBufferSys).UNSAFE_Unverified();
      memcpy((void *)sampleRow, pBufferSysMemCpyTarget2, row_stride);
      
      if (readScanLinesRet2 != 1) {
        *suspend = true; // suspend
        break;
      }

      if (mTransform) {
        JSAMPROW source = sampleRow;
        if (out_color_space == JCS_GRAYSCALE) {
          // Convert from the 1byte grey pixels at begin of row
          // to the 3byte RGB byte pixels at 'end' of row
          sampleRow += output_width;
        }
        qcms_transform_data(mTransform, source, sampleRow, output_width);
        // Move 3byte RGB data to end of row
        if (out_color_space == JCS_CMYK) {
          memmove(sampleRow + output_width,
                  sampleRow,
                  3 * output_width);
          sampleRow += output_width;
        }
      } else {
        if (out_color_space == JCS_CMYK) {
          // Convert from CMYK to RGB
          // We cannot convert directly to Cairo, as the CMSRGBTransform
          // may wants to do a RGB transform...
          // Would be better to have platform CMSenabled transformation
          // from CMYK to (A)RGB...
          cmyk_convert_rgb((JSAMPROW)imageRow, output_width);
          sampleRow += output_width;
        }
        if (mCMSMode == eCMSMode_All) {
          // No embedded ICC profile - treat as sRGB
          qcms_transform* transform = gfxPlatform::GetCMSRGBTransform();
          if (transform) {
            qcms_transform_data(transform, sampleRow, sampleRow,
                                output_width);
          }
        }
      }

      // counter for while() loops below
      uint32_t idx = output_width;

      // copy as bytes until source pointer is 32-bit-aligned
      for (; (NS_PTR_TO_UINT32(sampleRow) & 0x3) && idx; --idx) {
        *imageRow++ = gfxPackedPixel(0xFF, sampleRow[0], sampleRow[1],
                                     sampleRow[2]);
        sampleRow += 3;
      }

      // copy pixels in blocks of 4
      while (idx >= 4) {
        GFX_BLOCK_RGB_TO_FRGB(sampleRow, imageRow);
        idx       -=  4;
        sampleRow += 12;
        imageRow  +=  4;
      }

      // copy remaining pixel(s)
      while (idx--) {
        // 32-bit read of final pixel will exceed buffer, so read bytes
        *imageRow++ = gfxPackedPixel(0xFF, sampleRow[0], sampleRow[1],
                                     sampleRow[2]);
        sampleRow += 3;
      }

      if (mDownscaler) {
        mDownscaler->CommitRow();
      }
  }

  output_scanline_forLoop = mInfo.output_scanline.copyAndVerify(output_scanline_verifier);

  if (mDownscaler && mDownscaler->HasInvalidation()) {
    DownscalerInvalidRect invalidRect = mDownscaler->TakeInvalidRect();
    PostInvalidation(invalidRect.mOriginalSizeRect,
                     Some(invalidRect.mTargetSizeRect));
    MOZ_ASSERT(!mDownscaler->HasInvalidation());
  } else if (!mDownscaler && top != output_scanline_forLoop) {
    PostInvalidation(nsIntRect(0, top,
                               output_width,
                               output_scanline_forLoop - top));
  }
}

// Override the standard error method in the IJG JPEG decoder code.
METHODDEF(void)
my_error_exit (RLBoxSandbox<TRLSandbox>* sandbox, tainted<j_common_ptr, TRLSandbox> cinfo)
{
  tainted<decoder_error_mgr *, TRLSandbox> err = sandbox_reinterpret_cast<decoder_error_mgr *>(cinfo->err);
  // Convert error to a browser error code
  nsresult error_code = err->pub.msg_code.UNSAFE_Unverified() == JERR_OUT_OF_MEMORY
                    ? NS_ERROR_OUT_OF_MEMORY
                    : NS_ERROR_FAILURE;

  nsJPEGDecoder* decoder = (nsJPEGDecoder *)jpegRendererSaved;
  if (!decoder) {
    printf("Invalid jpeg pointer for my_error_exit\n");
    abort();
  }


#ifdef DEBUG
  auto rlbox_sandbox = decoder->rlbox_sbx->rlbox_sandbox;
  auto buffer = rlbox_sandbox->mallocInSandbox<char>(sizeof(char[JMSG_LENGTH_MAX]));
  auto formatMessagePtr = err->pub.format_message.UNSAFE_Unverified();
  sandbox_invoke_custom_with_ptr(rlbox_sandbox, rlbox_sandbox->getSandbox()->get_context(), formatMessagePtr, cinfo, buffer);

  auto verifBuffer = buffer.copyAndVerifyString(rlbox_sandbox, [](char* val){ 
    return val != nullptr? RLBox_Verify_Status::SAFE : RLBox_Verify_Status::UNSAFE;
  }, (char*) "String Verification of ErrMsg Failed: Unknown Error occurred in jpeg lib");

  fprintf(stderr, "JPEG decoding error:\n%s\n", verifBuffer);
  delete[] verifBuffer;
  rlbox_sandbox->freeInSandbox(buffer);
#endif

  // Return control to the setjmp point.  We pass an nsresult masquerading as
  // an int, which works because the setjmp() caller casts it back.
  if(!(decoder->m_jmpBuffValid)) {
    printf("Trying to jump to an invalid jump buffer\n");
    abort();
  }
  else {
    decoder->m_jmpBuffValid = FALSE;
    longjmp(decoder->m_jmpBuff, static_cast<int>(error_code));
  }
}

/*******************************************************************************
 * This is the callback routine from the IJG JPEG library used to supply new
 * data to the decompressor when its input buffer is exhausted.  It juggles
 * multiple buffers in an attempt to avoid unnecessary copying of input data.
 *
 * (A simpler scheme is possible: It's much easier to use only a single
 * buffer; when fill_input_buffer() is called, move any unconsumed data
 * (beyond the current pointer/count) down to the beginning of this buffer and
 * then load new data into the remaining buffer space.  This approach requires
 * a little more data copying but is far easier to get right.)
 *
 * At any one time, the JPEG decompressor is either reading from the necko
 * input buffer, which is volatile across top-level calls to the IJG library,
 * or the "backtrack" buffer.  The backtrack buffer contains the remaining
 * unconsumed data from the necko buffer after parsing was suspended due
 * to insufficient data in some previous call to the IJG library.
 *
 * When suspending, the decompressor will back up to a convenient restart
 * point (typically the start of the current MCU). The variables
 * next_input_byte & bytes_in_buffer indicate where the restart point will be
 * if the current call returns FALSE.  Data beyond this point must be
 * rescanned after resumption, so it must be preserved in case the decompressor
 * decides to backtrack.
 *
 * Returns:
 *  TRUE if additional data is available, FALSE if no data present and
 *   the JPEG library should therefore suspend processing of input stream
 ******************************************************************************/

/******************************************************************************/
/* data source manager method                                                 */
/******************************************************************************/

/******************************************************************************/
/* data source manager method
        Initialize source.  This is called by jpeg_read_header() before any
        data is actually read.  May leave
        bytes_in_buffer set to 0 (in which case a fill_input_buffer() call
        will occur immediately).
*/
METHODDEF(void)
init_source (RLBoxSandbox<TRLSandbox>* sandbox, tainted<j_decompress_ptr, TRLSandbox> jd) {}

/******************************************************************************/
/* data source manager method
        Skip num_bytes worth of data.  The buffer pointer and count should
        be advanced over num_bytes input bytes, refilling the buffer as
        needed.  This is used to skip over a potentially large amount of
        uninteresting data (such as an APPn marker).  In some applications
        it may be possible to optimize away the reading of the skipped data,
        but it's not clear that being smart is worth much trouble; large
        skips are uncommon.  bytes_in_buffer may be zero on return.
        A zero or negative skip count should be treated as a no-op.
*/
METHODDEF(void)
skip_input_data (RLBoxSandbox<TRLSandbox>* sandbox, tainted<j_decompress_ptr, TRLSandbox> jd, tainted<long, TRLSandbox> unv_num_bytes)
{
  tainted<jpeg_source_mgr*, TRLSandbox> src = jd->src;
  long num_bytes = unv_num_bytes.copyAndVerify([](long val) -> long {
    //Firefox assumes this will be positive... no guarantee provided by the type though, so check it
    if (val < 0) { return 0; }
    return val;
  });

  nsJPEGDecoder* decoder = (nsJPEGDecoder *)jpegRendererSaved;
  if (!decoder) {
    printf("Invalid jpeg pointer for skip_input_data\n");
    abort();
  }
  auto rlbox_sandbox = decoder->rlbox_sbx->rlbox_sandbox;

  //bytes_in buffer is only used to set fields on an unverified data structure only
  //hence no verification is necessary
  long bytes_in_buffer = (long) src->bytes_in_buffer.UNSAFE_Unverified();
  //next_input_byte is only used to set fields on an unverified data structure only
  //hence no verification is necessary
  tainted<const JOCTET*, TRLSandbox> next_input_byte = src->next_input_byte;
  
  if (num_bytes > bytes_in_buffer) {
    // Can't skip it all right now until we get more data from
    // network stream. Set things up so that fill_input_buffer
    // will skip remaining amount.
    decoder->mBytesToSkip = (size_t)num_bytes - bytes_in_buffer;
    src->next_input_byte = next_input_byte.getPointerIncrement(rlbox_sandbox, bytes_in_buffer);
    src->bytes_in_buffer = 0;

  } else {
    // Simple case. Just advance buffer pointer
    src->bytes_in_buffer = bytes_in_buffer - (size_t)num_bytes;
    src->next_input_byte = next_input_byte.getPointerIncrement(rlbox_sandbox, num_bytes);
  }
}

/******************************************************************************/
/* data source manager method
        This is called whenever bytes_in_buffer has reached zero and more
        data is wanted.  In typical applications, it should read fresh data
        into the buffer (ignoring the current state of next_input_byte and
        bytes_in_buffer), reset the pointer & count to the start of the
        buffer, and return TRUE indicating that the buffer has been reloaded.
        It is not necessary to fill the buffer entirely, only to obtain at
        least one more byte.  bytes_in_buffer MUST be set to a positive value
        if TRUE is returned.  A FALSE return should only be used when I/O
        suspension is desired.
*/
void ensureBufferLength(RLBoxSandbox<TRLSandbox>* rlbox_sandbox, tainted<JOCTET*, TRLSandbox>& currBuff, uint32_t& currLen, uint32_t newLen) {
  if(currLen != 0 && newLen > currLen) {
    rlbox_sandbox->freeInSandbox(currBuff);
    currLen = 0;
  }

  if(currLen == 0) {
    currBuff = rlbox_sandbox->mallocInSandbox<JOCTET>(newLen);
    currLen = newLen;
    if(currBuff == nullptr) {
      printf("Reallocation of buffer has failed: %u\n", newLen);
      abort();
    }
  }
}

template <typename T>
const char *type_name() {
  return __PRETTY_FUNCTION__;
}

METHODDEF(boolean) fill_input_buffer (RLBoxSandbox<TRLSandbox>* sandbox, tainted<j_decompress_ptr, TRLSandbox> jd)
{
  tainted<jpeg_source_mgr*, TRLSandbox> src(jd->src);
  nsJPEGDecoder* decoder = (nsJPEGDecoder *)jpegRendererSaved;
  if (!decoder) {
    printf("Invalid jpeg pointer for fill_input_buffer\n");
    abort();
  }
  auto rlbox_sandbox = decoder->rlbox_sbx->rlbox_sandbox;

  if (decoder->mReading) {
    const JOCTET* new_buffer = decoder->mSegment;
    uint32_t new_buflen = decoder->mSegmentLen;

    if (!new_buffer || new_buflen == 0) {
      return false; // suspend
    }

    decoder->mSegmentLen = 0;

    if (decoder->mBytesToSkip) {
      if (decoder->mBytesToSkip < new_buflen) {
        // All done skipping bytes; Return what's left.
        new_buffer += decoder->mBytesToSkip;
        new_buflen -= decoder->mBytesToSkip;
        decoder->mBytesToSkip = 0;
      } else {
        // Still need to skip some more data in the future
        decoder->mBytesToSkip -= (size_t)new_buflen;
        return false; // suspend
      }
    }

    decoder->mBackBufferUnreadLen = src->bytes_in_buffer.copyAndVerify([](size_t val){
      return val;
    });

    ensureBufferLength(rlbox_sandbox, decoder->s_mSegment, decoder->s_mSegmentLen, new_buflen);
    memcpy(decoder->s_mSegment.UNSAFE_Unverified(), new_buffer, new_buflen);
    src->next_input_byte = decoder->s_mSegment;
    src->bytes_in_buffer = (size_t)new_buflen;
    decoder->mReading = false;

    return true;
  }

  tainted<const JOCTET *, TRLSandbox> next_input_byte = src->next_input_byte;
  //we expand buffers below to make sure this number is valid
  size_t bytes_in_buffer = src->bytes_in_buffer.UNSAFE_Unverified();

  if (next_input_byte.UNSAFE_Unverified() != decoder->s_mSegment.UNSAFE_Unverified()) {
    // Backtrack data has been permanently consumed.
    decoder->mBackBufferUnreadLen = 0;
    decoder->mBackBufferLen = 0;
  }

  // Save remainder of netlib buffer in backtrack buffer
  const uint32_t new_backtrack_buflen = bytes_in_buffer + decoder->mBackBufferLen;
  
  // Make sure backtrack buffer is big enough to hold new data.
  if (decoder->mBackBufferSize < new_backtrack_buflen) {
    // Check for malformed MARKER segment lengths, before allocating space
    // for it
    if (new_backtrack_buflen > MAX_JPEG_MARKER_LENGTH) {
      my_error_exit(rlbox_sandbox, sandbox_reinterpret_cast<j_common_ptr>(decoder->p_mInfo));
    }

    // Round up to multiple of 256 bytes.
    const size_t roundup_buflen = ((new_backtrack_buflen + 255) >> 8) << 8;
    JOCTET* buf = (JOCTET*) realloc(decoder->mBackBuffer, roundup_buflen);
    // Check for OOM
    if (!buf) {
      struct jpeg_error_mgr* err = decoder->p_mInfo->err.UNSAFE_Unverified();
      err->msg_code = JERR_OUT_OF_MEMORY;
      //CPP_TODO - Again we need a better cast
      //Don't need any verifications as my_error_exit accepts an unverified as a param
      my_error_exit(rlbox_sandbox, sandbox_reinterpret_cast<j_common_ptr>(decoder->p_mInfo));
    }
    decoder->mBackBuffer = buf;
    decoder->mBackBufferSize = roundup_buflen;
  }

  // Copy remainder of netlib segment into backtrack buffer.
  memmove(decoder->mBackBuffer + decoder->mBackBufferLen,
          next_input_byte.UNSAFE_Unverified(),
          bytes_in_buffer);

  // Point to start of data to be rescanned.
  bytes_in_buffer += decoder->mBackBufferUnreadLen;
  src->bytes_in_buffer = bytes_in_buffer;

  JOCTET* new_input_byte_val = decoder->mBackBuffer + decoder->mBackBufferLen -
                       decoder->mBackBufferUnreadLen;
  ensureBufferLength(rlbox_sandbox, decoder->s_mBackBuffer, decoder->s_mBackBufferLen, bytes_in_buffer);
  auto new_backBuffer = decoder->s_mBackBuffer;
  memcpy(new_backBuffer.UNSAFE_Unverified(), new_input_byte_val, bytes_in_buffer);

  src->next_input_byte = new_backBuffer;

  decoder->mBackBufferLen = (size_t)new_backtrack_buflen;
  decoder->mReading = true;
  return false;
}

/******************************************************************************/
/* data source manager method */
/*
 * Terminate source --- called by jpeg_finish_decompress() after all
 * data has been read to clean up JPEG source manager. NOT called by
 * jpeg_abort() or jpeg_destroy().
 */
METHODDEF(void) term_source (RLBoxSandbox<TRLSandbox>* sandbox, tainted<j_decompress_ptr, TRLSandbox> jd)
{
  nsJPEGDecoder* decoder = (nsJPEGDecoder *)jpegRendererSaved;
  if (!decoder) {
    printf("Invalid jpeg pointer for term_source\n");
    abort();
  }

  auto diff = decoder->JpegBench.StopAndFinish();
  //std::string tag = "JPEG_destroy(" + decoder->mImageString + ")";
  //printf("Capture_Time:%s,%llu,%llu|\n", tag.c_str(), invJpeg, diff);
  invJpeg++;

  // This function shouldn't be called if we ran into an error we didn't
  // recover from.
  MOZ_ASSERT(decoder->mState != JPEG_ERROR,
             "Calling term_source on a JPEG with mState == JPEG_ERROR!");

  // Notify using a helper method to get around protectedness issues.
  decoder->NotifyDone();
}


} // namespace image
} // namespace mozilla

///*************** Inverted CMYK -> RGB conversion *************************
/// Input is (Inverted) CMYK stored as 4 bytes per pixel.
/// Output is RGB stored as 3 bytes per pixel.
/// @param row Points to row buffer containing the CMYK bytes for each pixel
/// in the row.
/// @param width Number of pixels in the row.
static void cmyk_convert_rgb(JSAMPROW row, JDIMENSION width)
{
  // Work from end to front to shrink from 4 bytes per pixel to 3
  JSAMPROW in = row + width*4;
  JSAMPROW out = in;

  for (uint32_t i = width; i > 0; i--) {
    in -= 4;
    out -= 3;

    // Source is 'Inverted CMYK', output is RGB.
    // See: http://www.easyrgb.com/math.php?MATH=M12#text12
    // Or:  http://www.ilkeratalay.com/colorspacesfaq.php#rgb

    // From CMYK to CMY
    // C = ( C * ( 1 - K ) + K )
    // M = ( M * ( 1 - K ) + K )
    // Y = ( Y * ( 1 - K ) + K )

    // From Inverted CMYK to CMY is thus:
    // C = ( (1-iC) * (1 - (1-iK)) + (1-iK) ) => 1 - iC*iK
    // Same for M and Y

    // Convert from CMY (0..1) to RGB (0..1)
    // R = 1 - C => 1 - (1 - iC*iK) => iC*iK
    // G = 1 - M => 1 - (1 - iM*iK) => iM*iK
    // B = 1 - Y => 1 - (1 - iY*iK) => iY*iK

    // Convert from Inverted CMYK (0..255) to RGB (0..255)
    const uint32_t iC = in[0];
    const uint32_t iM = in[1];
    const uint32_t iY = in[2];
    const uint32_t iK = in[3];
    out[0] = iC*iK/255;   // Red
    out[1] = iM*iK/255;   // Green
    out[2] = iY*iK/255;   // Blue
  }
}

#undef sandbox_invoke_custom
#undef sandbox_invoke_custom_with_ptr