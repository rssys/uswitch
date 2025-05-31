/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ImageLogging.h" // Must appear first
#include "nsPNGDecoder.h"

#include <algorithm>
#include <cstdint>
#include <cmath>
#include <limits>
#include <type_traits>
#include <sys/syscall.h>
#include "gfxColor.h"
#include "gfxPlatform.h"
#include "imgFrame.h"
#include "nsColor.h"
#include "nsIInputStream.h"
#include "nsMemory.h"
#include "nsRect.h"
#include "nspr.h"
#include "RasterImage.h"
#include "SurfaceCache.h"
#include "SurfacePipeFactory.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/Telemetry.h"

#include "sandboxmanager.h"

using namespace mozilla::gfx;

using std::min;

//helper structs so that we don't have to malloc multiple times
struct png_get_iCCP_params {
  png_uint_32 profileLen;
  png_bytep profileData;
  png_charp profileName;
  int compression;
};

struct png_get_cHRM_And_gAMA_params {
  qcms_CIE_xyYTRIPLE primaries;
  qcms_CIE_xyY whitePoint;
  double gammaOfFile;
};

struct png_get_IHDR_params {
  png_uint_32 width;
  png_uint_32 height;
  int bit_depth;
  int color_type;
  int interlace_type;
  int compression_type;
  int filter_type;
};

struct png_get_tRNS_params {
  png_bytep trans;
  png_color_16p trans_values;
  int num_trans;
};

#include "pngstruct.h"
#include "pnginfo.h"
#include <set>
#include <map>
#include <mutex>
static std::mutex pngMapMutex;
thread_local void* pngRendererSaved = nullptr;
thread_local unsigned long jumpBufferFilledIn = 0;
std::map<unsigned long, jmp_buf> pngJmpBuffers;
typedef struct png_unknown_chunk_t
{
  png_byte name[5]; /* Textual chunk name with '\0' terminator */
  png_byte *data;   /* Data, should not be modified on read! */
  png_size_t size;
  png_byte location; /* mode of operation at read time */
} png_unknown_chunk;
typedef struct png_text_struct
{
  int  compression;       /* compression value:
                            -1: tEXt, none
                            0: zTXt, deflate
                            1: iTXt, none
                            2: iTXt, deflate  */
  png_charp key;          /* keyword, 1-79 character description of "text" */
  png_charp text;         /* comment, may be an empty string (ie "")
                            or a NULL pointer */
  png_size_t text_length; /* length of the text string */
  png_size_t itxt_length; /* length of the itxt string */
  png_charp lang;         /* language code, 0-79 characters
                            or a NULL pointer */
  png_charp lang_key;     /* keyword translated UTF-8 string, 0 or more
                            chars or a NULL pointer */
} png_text;
typedef struct png_control
{
  png_structp png_ptr;
  png_infop   info_ptr;
  png_voidp   error_buf;           /* Always a jmp_buf at present. */

  png_const_bytep memory;          /* Memory buffer. */
  png_size_t      size;            /* Size of the memory buffer. */

  unsigned int for_write       ; /* Otherwise it is a read structure */
  unsigned int owned_file      ; /* We own the file in io_ptr */
} png_control;

rlbox_load_library_api(pnglib, TRLSandboxP)

namespace mozilla {
namespace image {

static LazyLogModule sPNGLog("PNGDecoder");
static LazyLogModule sPNGDecoderAccountingLog("PNGDecoderAccounting");

// limit image dimensions (bug #251381, #591822, #967656, and #1283961)
#ifndef MOZ_PNG_MAX_WIDTH
#  define MOZ_PNG_MAX_WIDTH 0x7fffffff // Unlimited
#endif
#ifndef MOZ_PNG_MAX_HEIGHT
#  define MOZ_PNG_MAX_HEIGHT 0x7fffffff // Unlimited
#endif

#define sandbox_invoke_custom(sandbox, ctx, fnName, ...) \
  sandbox_invoke_custom_helper_png(sandbox, ctx, \
  ((PNGSandboxResource *)sandbox->getSandbox()->custom_data)->fnName ? \
  ((PNGSandboxResource *)sandbox->getSandbox()->custom_data)->fnName : \
  (((PNGSandboxResource *)sandbox->getSandbox()->custom_data)->fnName = \
    (decltype(PNGSandboxResource::fnName))sandbox->getSandbox()->get_symbol_addr(#fnName)) \
  , ##__VA_ARGS__)

template<typename TFunc, typename... TArgs>
inline typename std::enable_if<!std::is_void<return_argument<TFunc>>::value,
tainted<return_argument<TFunc>, TRLSandboxP>
>::type sandbox_invoke_custom_helper_png(RLBoxSandbox<TRLSandboxP>* sandbox, uswctx_t ctx, TFunc* fnPtr, TArgs&&... params)
{
  // //pngStartTimer();
  auto ret = sandbox->invokeWithFunctionPointerWithExtraData(fnPtr, ctx, params...);
  // //pngEndTimer();
  return ret;
}

template<typename TFunc, typename... TArgs>
inline typename std::enable_if<std::is_void<return_argument<TFunc>>::value,
void
>::type sandbox_invoke_custom_helper_png(RLBoxSandbox<TRLSandboxP>* sandbox, uswctx_t ctx, TFunc* fnPtr, TArgs&&... params)
{
  // //pngStartTimer();
  sandbox->invokeWithFunctionPointerWithExtraData(fnPtr, ctx, params...);
  // //pngEndTimer();
}

nsPNGDecoder::AnimFrameInfo::AnimFrameInfo()
 : mDispose(DisposalMethod::KEEP)
 , mBlend(BlendMethod::OVER)
 , mTimeout(0)
{ }

class PNGSandboxResource : public USwitchSandboxResource {
public:
  RLBoxSandbox<TRLSandboxP>* rlbox_sandbox;
  sandbox_callback_helper<void(png_structp,png_const_charp), TRLSandboxP> cpp_cb_png_error_fn;
  sandbox_callback_helper<void(png_structp,png_const_charp), TRLSandboxP> cpp_cb_png_warn_fn;
  sandbox_callback_helper<void(png_structp,png_infop), TRLSandboxP> cpp_cb_png_progressive_info_fn;
  sandbox_callback_helper<void(png_structp,png_bytep,png_uint_32,int), TRLSandboxP> cpp_cb_png_progressive_row_fn;
  sandbox_callback_helper<void(png_structp,png_infop), TRLSandboxP> cpp_cb_png_progressive_end_fn;
  sandbox_callback_helper<void(png_structp,png_uint_32), TRLSandboxP> cpp_cb_png_progressive_frame_info_fn;
  sandbox_callback_helper<void(jmp_buf, int), TRLSandboxP> cpp_cb_longjmp_fn;
  decltype(::png_create_info_struct) *png_create_info_struct = nullptr;
  decltype(::png_create_read_struct) *png_create_read_struct = nullptr;
  decltype(::png_destroy_read_struct) *png_destroy_read_struct = nullptr;
  decltype(::png_error) *png_error = nullptr;
  decltype(::png_free_data) *png_free_data = nullptr;
  decltype(::png_get_channels) *png_get_channels = nullptr;
  decltype(::png_get_cHRM) *png_get_cHRM = nullptr;
  decltype(::png_get_first_frame_is_hidden) *png_get_first_frame_is_hidden = nullptr;
  decltype(::png_get_gAMA) *png_get_gAMA = nullptr;
  decltype(::png_get_iCCP) *png_get_iCCP = nullptr;
  decltype(::png_get_IHDR) *png_get_IHDR = nullptr;
  decltype(::png_get_next_frame_blend_op) *png_get_next_frame_blend_op = nullptr;
  decltype(::png_get_next_frame_delay_den) *png_get_next_frame_delay_den = nullptr;
  decltype(::png_get_next_frame_delay_num) *png_get_next_frame_delay_num = nullptr;
  decltype(::png_get_next_frame_dispose_op) *png_get_next_frame_dispose_op = nullptr;
  decltype(::png_get_next_frame_height) *png_get_next_frame_height = nullptr;
  decltype(::png_get_next_frame_width) *png_get_next_frame_width = nullptr;
  decltype(::png_get_next_frame_x_offset) *png_get_next_frame_x_offset = nullptr;
  decltype(::png_get_next_frame_y_offset) *png_get_next_frame_y_offset = nullptr;
  decltype(::png_get_num_plays) *png_get_num_plays = nullptr;
  decltype(::png_get_sRGB) *png_get_sRGB = nullptr;
  decltype(::png_get_tRNS) *png_get_tRNS = nullptr;
  decltype(::png_get_valid) *png_get_valid = nullptr;
  decltype(::png_longjmp) *png_longjmp = nullptr;
  decltype(::png_process_data) *png_process_data = nullptr;
  decltype(::png_process_data_pause) *png_process_data_pause = nullptr;
  decltype(::png_progressive_combine_row) *png_progressive_combine_row = nullptr;
  decltype(::png_read_update_info) *png_read_update_info = nullptr;
#ifdef PNG_READ_CHECK_FOR_INVALID_INDEX_SUPPORTED
  decltype(::png_set_check_for_invalid_index) *png_set_check_for_invalid_index = nullptr;
#endif
  decltype(::png_set_chunk_malloc_max) *png_set_chunk_malloc_max = nullptr;
  decltype(::png_set_expand) *png_set_expand = nullptr;
  decltype(::png_set_gAMA) *png_set_gAMA = nullptr;
  decltype(::png_set_gamma) *png_set_gamma = nullptr;
  decltype(::png_set_gray_to_rgb) *png_set_gray_to_rgb = nullptr;
  decltype(::png_set_interlace_handling) *png_set_interlace_handling = nullptr;
#ifdef PNG_HANDLE_AS_UNKNOWN_SUPPORTED
  decltype(::png_set_keep_unknown_chunks) *png_set_keep_unknown_chunks = nullptr;
#endif
  decltype(::png_set_longjmp_fn) *png_set_longjmp_fn = nullptr;
  decltype(::png_set_option) *png_set_option = nullptr;
  decltype(::png_set_progressive_frame_fn) *png_set_progressive_frame_fn = nullptr;
  decltype(::png_set_progressive_read_fn) *png_set_progressive_read_fn = nullptr;
  decltype(::png_set_scale_16) *png_set_scale_16 = nullptr;
  decltype(::png_set_user_limits) *png_set_user_limits = nullptr;

  PNGSandboxResource() {
    static const std::vector<unsigned int> AllowedSyscalls {__NR_exit, __NR_futex, __NR_sched_yield};
    const std::string &libPath = getLibraryPath("libpng.so");
    rlbox_sandbox = RLBoxSandbox<TRLSandboxP>::createSandbox("", libPath.c_str());
    if (!rlbox_sandbox->getSandbox()->init_seccomp(AllowedSyscalls)) {
      fprintf(stderr, "failed to init seccomp for PNG sandbox\n");
      exit(1);
    }
    rlbox_sandbox->getSandbox()->custom_data = (void *)this;

    cpp_cb_png_error_fn = rlbox_sandbox->createCallback(nsPNGDecoder::error_callback);
    cpp_cb_png_warn_fn = rlbox_sandbox->createCallback(nsPNGDecoder::warning_callback);
    cpp_cb_png_progressive_info_fn = rlbox_sandbox->createCallback(nsPNGDecoder::info_callback);
    cpp_cb_png_progressive_row_fn = rlbox_sandbox->createCallback(nsPNGDecoder::row_callback);
    cpp_cb_png_progressive_end_fn = rlbox_sandbox->createCallback(nsPNGDecoder::end_callback);
    #ifdef PNG_APNG_SUPPORTED
      cpp_cb_png_progressive_frame_info_fn = rlbox_sandbox->createCallback(nsPNGDecoder::frame_info_callback);
    #endif
    cpp_cb_longjmp_fn = rlbox_sandbox->createCallback(nsPNGDecoder::checked_longjmp);
  }

  virtual ~PNGSandboxResource() {
    rlbox_sandbox->destroySandbox();
    delete rlbox_sandbox;
  }
};

static SandboxManager<PNGSandboxResource, 4, 6> sandboxManagerPNG;

#ifdef PNG_APNG_SUPPORTED

int32_t GetNextFrameDelay(RLBoxSandbox<TRLSandboxP>* rlbox_sandbox, tainted<png_structp, TRLSandboxP> aPNG, tainted<png_infop, TRLSandboxP> aInfo)
{
  auto ctx = rlbox_sandbox->getSandbox()->get_context();
  // Delay, in seconds, is delayNum / delayDen.
  png_uint_16 delayNum = sandbox_invoke_custom(rlbox_sandbox, ctx, png_get_next_frame_delay_num, aPNG, aInfo)
    .copyAndVerify([](png_uint_16 val){ return val; });
  png_uint_16 delayDen = sandbox_invoke_custom(rlbox_sandbox, ctx, png_get_next_frame_delay_den, aPNG, aInfo)
    .copyAndVerify([](png_uint_16 val){ return val; });

  if (delayNum == 0) {
    return 0; // SetFrameTimeout() will set to a minimum.
  }

  if (delayDen == 0) {
    delayDen = 100; // So says the APNG spec.
  }

  // Need to cast delay_num to float to have a proper division and
  // the result to int to avoid a compiler warning.
  return static_cast<int32_t>(static_cast<double>(delayNum) * 1000 / delayDen);
}

nsPNGDecoder::AnimFrameInfo::AnimFrameInfo(RLBoxSandbox<TRLSandboxP>* rlbox_sandbox, tainted<png_structp, TRLSandboxP> aPNG, tainted<png_infop, TRLSandboxP> aInfo)
 : mDispose(DisposalMethod::KEEP)
 , mBlend(BlendMethod::OVER)
 , mTimeout(0)
{
  auto ctx = rlbox_sandbox->getSandbox()->get_context();
  png_byte dispose_op = sandbox_invoke_custom(rlbox_sandbox, ctx, png_get_next_frame_dispose_op, aPNG, aInfo)
    .copyAndVerify([](png_byte val){ return val; });
  png_byte blend_op = sandbox_invoke_custom(rlbox_sandbox, ctx, png_get_next_frame_blend_op, aPNG, aInfo)
    .copyAndVerify([](png_byte val){ return val; });

  if (dispose_op == PNG_DISPOSE_OP_PREVIOUS) {
    mDispose = DisposalMethod::RESTORE_PREVIOUS;
  } else if (dispose_op == PNG_DISPOSE_OP_BACKGROUND) {
    mDispose = DisposalMethod::CLEAR;
  } else {
    mDispose = DisposalMethod::KEEP;
  }

  if (blend_op == PNG_BLEND_OP_SOURCE) {
    mBlend = BlendMethod::SOURCE;
  } else {
    mBlend = BlendMethod::OVER;
  }

  mTimeout = GetNextFrameDelay(rlbox_sandbox, aPNG, aInfo);
}
#endif

static unsigned long freshMapId()
{
  unsigned long startingPoint = rand();
  std::lock_guard<std::mutex> guard(pngMapMutex);
  //keep looking around for a free location, unsigned addition wraps
  for(unsigned long current = startingPoint + 1; current != startingPoint; current++)
  {
    if(pngJmpBuffers.find(current) == pngJmpBuffers.end() ) {
      //ensure the location is cerated and initialized
      auto &jmpBuffLoc = pngJmpBuffers[current];
      //not using the location yet
      (void)(jmpBuffLoc);
      return current;
    }
  }

  printf("nsPNGDecoder::nsPNGDecoder Could not find any more jump buffer locations. All locations are used?\n");
  abort();
}

void nsPNGDecoder::checked_longjmp(RLBoxSandbox<TRLSandboxP>* sandbox, tainted<jmp_buf, TRLSandboxP> unv_env, tainted<int, TRLSandboxP> unv_status)
{
  if(!jumpBufferFilledIn)
  {
    printf("nsPNGDecoder::nsPNGDecoder: Critical Error in finding the right jmp_buf\n");
    abort();
  }
  //clear the jmpBufferIndex on the way out as it is unsafe to call setjmp after this
  int savedJumpBufferFilledIn = jumpBufferFilledIn;
  jumpBufferFilledIn = 0;

  int status = unv_status.copyAndVerify([](int val){ return val; });
  longjmp(pngJmpBuffers[savedJumpBufferFilledIn], status);
}

// First 8 bytes of a PNG file
const uint8_t
nsPNGDecoder::pngSignatureBytes[] = { 137, 80, 78, 71, 13, 10, 26, 10 };

nsPNGDecoder::nsPNGDecoder(RasterImage* aImage, RasterImage* aImageExtra)
 : Decoder(aImage)
 , mLexer(Transition::ToUnbuffered(State::FINISHED_PNG_DATA,
                                   State::PNG_DATA,
                                   SIZE_MAX),
          Transition::TerminateSuccess())
 , mNextTransition(Transition::ContinueUnbuffered(State::PNG_DATA))
 , mLastChunkLength(0)
 , mPNG(nullptr)
 , mInfo(nullptr)
 , mCMSLine(nullptr)
 , interlacebuf(nullptr)
 , mInProfile(nullptr)
 , mTransform(nullptr)
 , mFormat(SurfaceFormat::UNKNOWN)
 , mCMSMode(0)
 , mChannels(0)
 , mPass(0)
 , mFrameIsHidden(false)
 , mDisablePremultipliedAlpha(false)
 , mNumFrames(0)
 , PngMaybeTooSmall(true)
{
    mImageString = getImageURIString(aImage != nullptr? aImage : aImageExtra);
    std::string hostString = getHostStringFromImage(aImage != nullptr? aImage : aImageExtra);
    rlbox_sbx_shared = sandboxManagerPNG.createSandbox(hostString);
    rlbox_sbx = rlbox_sbx_shared.get();
}

unsigned long long invPng = 0;

nsPNGDecoder::~nsPNGDecoder()
{
  auto rlbox_sandbox = rlbox_sbx->rlbox_sandbox;
  auto ctx = rlbox_sandbox->getSandbox()->get_context();
  if (mPNG != nullptr) {
    auto new_mPNG_Loc = rlbox_sandbox->mallocInSandbox<png_structp>();
    *new_mPNG_Loc = mPNG;

    auto new_mInfo_Loc = rlbox_sandbox->mallocInSandbox<png_infop>();
    *new_mInfo_Loc = mInfo;

    sandbox_invoke_custom(rlbox_sandbox, ctx, png_destroy_read_struct, new_mPNG_Loc, mInfo != nullptr? new_mInfo_Loc : nullptr, nullptr);
    rlbox_sandbox->freeInSandbox(new_mPNG_Loc);
    rlbox_sandbox->freeInSandbox(new_mInfo_Loc);
  }
  if (mCMSLine) {
    free(mCMSLine);
  }
  if (interlacebuf) {
    tainted<uint8_t*, TRLSandboxP> interlacebufTainted;
    interlacebufTainted.assignPointerInSandbox(rlbox_sandbox, interlacebuf);
    rlbox_sandbox->freeInSandbox(interlacebufTainted);
  }

  //delete rlbox_sbx;
  //uswitch_sandbox_semaphore.signal();
}

nsPNGDecoder::TransparencyType
nsPNGDecoder::GetTransparencyType(const IntRect& aFrameRect)
{
  // Check if the image has a transparent color in its palette.
  if (HasAlphaChannel()) {
    return TransparencyType::eAlpha;
  }
  if (!aFrameRect.IsEqualEdges(FullFrame())) {
    MOZ_ASSERT(HasAnimation());
    return TransparencyType::eFrameRect;
  }

  return TransparencyType::eNone;
}

void
nsPNGDecoder::PostHasTransparencyIfNeeded(TransparencyType aTransparencyType)
{
  switch (aTransparencyType) {
    case TransparencyType::eNone:
      return;

    case TransparencyType::eAlpha:
      PostHasTransparency();
      return;

    case TransparencyType::eFrameRect:
      // If the first frame of animated image doesn't draw into the whole image,
      // then record that it is transparent. For subsequent frames, this doesn't
      // affect transparency, because they're composited on top of all previous
      // frames.
      if (mNumFrames == 0) {
        PostHasTransparency();
      }
      return;
  }
}

// CreateFrame() is used for both simple and animated images.
nsresult
nsPNGDecoder::CreateFrame(const FrameInfo& aFrameInfo)
{
  MOZ_ASSERT(HasSize());
  MOZ_ASSERT(!IsMetadataDecode());

  // Check if we have transparency, and send notifications if needed.
  auto transparency = GetTransparencyType(aFrameInfo.mFrameRect);
  PostHasTransparencyIfNeeded(transparency);
  mFormat = transparency == TransparencyType::eNone
          ? SurfaceFormat::B8G8R8X8
          : SurfaceFormat::B8G8R8A8;

  // Make sure there's no animation or padding if we're downscaling.
  MOZ_ASSERT_IF(Size() != OutputSize(), mNumFrames == 0);
  MOZ_ASSERT_IF(Size() != OutputSize(), !GetImageMetadata().HasAnimation());
  MOZ_ASSERT_IF(Size() != OutputSize(),
                transparency != TransparencyType::eFrameRect);

  // If this image is interlaced, we can display better quality intermediate
  // results to the user by post processing them with ADAM7InterpolatingFilter.
  SurfacePipeFlags pipeFlags = aFrameInfo.mIsInterlaced
                             ? SurfacePipeFlags::ADAM7_INTERPOLATE
                             : SurfacePipeFlags();

  if (mNumFrames == 0) {
    // The first frame may be displayed progressively.
    pipeFlags |= SurfacePipeFlags::PROGRESSIVE_DISPLAY;
  }

  Maybe<SurfacePipe> pipe =
    SurfacePipeFactory::CreateSurfacePipe(this, mNumFrames, Size(),
                                          OutputSize(), aFrameInfo.mFrameRect,
                                          mFormat, pipeFlags);

  if (!pipe) {
    mPipe = SurfacePipe();
    return NS_ERROR_FAILURE;
  }

  mPipe = Move(*pipe);

  mFrameRect = aFrameInfo.mFrameRect;
  mPass = 0;

  MOZ_LOG(sPNGDecoderAccountingLog, LogLevel::Debug,
         ("PNGDecoderAccounting: nsPNGDecoder::CreateFrame -- created "
          "image frame with %dx%d pixels for decoder %p",
          mFrameRect.Width(), mFrameRect.Height(), this));

#ifdef PNG_APNG_SUPPORTED
  auto rlbox_sandbox = rlbox_sbx->rlbox_sandbox;
  auto ctx = rlbox_sandbox->getSandbox()->get_context();
  if (
    sandbox_invoke_custom(rlbox_sandbox, ctx, png_get_valid, mPNG, mInfo, PNG_INFO_acTL)
      .copyAndVerify([](png_uint_32 val){ return val; })
  ) {
    mAnimInfo = AnimFrameInfo(rlbox_sandbox, mPNG, mInfo);

    if (mAnimInfo.mDispose == DisposalMethod::CLEAR) {
      // We may have to display the background under this image during
      // animation playback, so we regard it as transparent.
      PostHasTransparency();
    }
  }
#endif

  return NS_OK;
}

// set timeout and frame disposal method for the current frame
void
nsPNGDecoder::EndImageFrame()
{
  if (mFrameIsHidden) {
    return;
  }

  mNumFrames++;

  Opacity opacity = mFormat == SurfaceFormat::B8G8R8X8
                  ? Opacity::FULLY_OPAQUE
                  : Opacity::SOME_TRANSPARENCY;

  PostFrameStop(opacity, mAnimInfo.mDispose,
                FrameTimeout::FromRawMilliseconds(mAnimInfo.mTimeout),
                mAnimInfo.mBlend, Some(mFrameRect));
}

#ifdef PNG_HANDLE_AS_UNKNOWN_SUPPORTED 
  #if(USE_SANDBOXING_BUFFERS != 0)
    png_byte* color_chunks_replace = 0;
    png_byte* unused_chunks_replace = 0;
    bool chunks_generated_started = 0;
    bool chunks_generated_finished = 0;
  #endif
#endif

nsresult
nsPNGDecoder::InitInternal()
{
  PngBench.Init();
  mCMSMode = gfxPlatform::GetCMSMode();
  if (GetSurfaceFlags() & SurfaceFlags::NO_COLORSPACE_CONVERSION) {
    mCMSMode = eCMSMode_Off;
  }
  mDisablePremultipliedAlpha =
    bool(GetSurfaceFlags() & SurfaceFlags::NO_PREMULTIPLY_ALPHA);

  auto rlbox_sandbox = rlbox_sbx->rlbox_sandbox;
  auto ctx = rlbox_sandbox->getSandbox()->get_context();

#ifdef PNG_HANDLE_AS_UNKNOWN_SUPPORTED
  #if(USE_SANDBOXING_BUFFERS != 0)
    static png_byte color_chunks_orig[]=
         { 99,  72,  82,  77, '\0',   // cHRM
          105,  67,  67,  80, '\0'};  // iCCP
    static png_byte unused_chunks_orig[]=
         { 98,  75,  71,  68, '\0',   // bKGD
          104,  73,  83,  84, '\0',   // hIST
          105,  84,  88, 116, '\0',   // iTXt
          111,  70,  70, 115, '\0',   // oFFs
          112,  67,  65,  76, '\0',   // pCAL
          115,  67,  65,  76, '\0',   // sCAL
          112,  72,  89, 115, '\0',   // pHYs
          115,  66,  73,  84, '\0',   // sBIT
          115,  80,  76,  84, '\0',   // sPLT
          116,  69,  88, 116, '\0',   // tEXt
          116,  73,  77,  69, '\0',   // tIME
          122,  84,  88, 116, '\0'};  // zTXt

    if(chunks_generated_started)
    {
      while(!chunks_generated_finished){}
    }
    else
    {
      chunks_generated_started = 1;
      color_chunks_replace = rlbox_sandbox->mallocInSandbox<png_bytep>(sizeof(color_chunks_orig)).UNSAFE_Unverified();
      unused_chunks_replace = rlbox_sandbox->mallocInSandbox<png_bytep>(sizeof(unused_chunks_orig)).UNSAFE_Unverified();
      memcpy(color_chunks_replace, color_chunks_orig, sizeof(color_chunks_orig));
      memcpy(unused_chunks_replace, unused_chunks_orig, sizeof(unused_chunks_orig));
      chunks_generated_finished = 1;
    }
  #elif
    static png_byte color_chunks_started[]=
         { 99,  72,  82,  77, '\0',   // cHRM
          105,  67,  67,  80, '\0'};  // iCCP
    static png_byte unused_chunks_started[]=
         { 98,  75,  71,  68, '\0',   // bKGD
          104,  73,  83,  84, '\0',   // hIST
          105,  84,  88, 116, '\0',   // iTXt
          111,  70,  70, 115, '\0',   // oFFs
          112,  67,  65,  76, '\0',   // pCAL
          115,  67,  65,  76, '\0',   // sCAL
          112,  72,  89, 115, '\0',   // pHYs
          115,  66,  73,  84, '\0',   // sBIT
          115,  80,  76,  84, '\0',   // sPLT
          116,  69,  88, 116, '\0',   // tEXt
          116,  73,  77,  69, '\0',   // tIME
          122,  84,  88, 116, '\0'};  // zTXt
  #endif
#endif

  // Initialize the container's source image header
  // Always decode to 24 bit pixdepth

  mPNG = sandbox_invoke_custom(rlbox_sandbox, ctx, png_create_read_struct, rlbox_sandbox->stackarr(PNG_LIBPNG_VER_STRING),
                               nullptr, rlbox_sbx->cpp_cb_png_error_fn,
                               rlbox_sbx->cpp_cb_png_warn_fn);

  if (mPNG == nullptr) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  mInfo = sandbox_invoke_custom(rlbox_sandbox, ctx, png_create_info_struct, mPNG);

  if (mInfo == nullptr) {
    auto new_mPNG_Loc = rlbox_sandbox->mallocInSandbox<png_structp>();
    *new_mPNG_Loc = mPNG;
    sandbox_invoke_custom(rlbox_sandbox, ctx, png_destroy_read_struct, new_mPNG_Loc, nullptr, nullptr);
    rlbox_sandbox->freeInSandbox(new_mPNG_Loc);
    return NS_ERROR_OUT_OF_MEMORY;
  }

#ifdef PNG_HANDLE_AS_UNKNOWN_SUPPORTED
  // Ignore unused chunks
  if (mCMSMode == eCMSMode_Off || IsMetadataDecode()) {
    sandbox_invoke_custom(rlbox_sandbox, ctx, png_set_keep_unknown_chunks, mPNG, 1, color_chunks_replace, 2);
  }

  sandbox_invoke_custom(rlbox_sandbox, ctx, png_set_keep_unknown_chunks, mPNG, 1, unused_chunks_replace,
                            (int)sizeof(unused_chunks_orig)/5);
#endif

#ifdef PNG_SET_USER_LIMITS_SUPPORTED
  sandbox_invoke_custom(rlbox_sandbox, ctx, png_set_user_limits, mPNG, MOZ_PNG_MAX_WIDTH, MOZ_PNG_MAX_HEIGHT);
  if (mCMSMode != eCMSMode_Off) {
    sandbox_invoke_custom(rlbox_sandbox, ctx, png_set_chunk_malloc_max, mPNG, 4000000L);
  }
#endif

#ifdef PNG_READ_CHECK_FOR_INVALID_INDEX_SUPPORTED
  // Disallow palette-index checking, for speed; we would ignore the warning
  // anyhow.  This feature was added at libpng version 1.5.10 and is disabled
  // in the embedded libpng but enabled by default in the system libpng.  This
  // call also disables it in the system libpng, for decoding speed.
  // Bug #745202.
  sandbox_invoke_custom(rlbox_sandbox, ctx, png_set_check_for_invalid_index, mPNG, 0);
#endif

#ifdef PNG_SET_OPTION_SUPPORTED
#if defined(PNG_sRGB_PROFILE_CHECKS) && PNG_sRGB_PROFILE_CHECKS >= 0
  // Skip checking of sRGB ICC profiles
  sandbox_invoke_custom(rlbox_sandbox, ctx, png_set_option, mPNG, PNG_SKIP_sRGB_CHECK_PROFILE, PNG_OPTION_ON);
#endif

#ifdef PNG_MAXIMUM_INFLATE_WINDOW
  // Force a larger zlib inflate window as some images in the wild have
  // incorrectly set metadata (specifically CMF bits) which prevent us from
  // decoding them otherwise.
  sandbox_invoke_custom(rlbox_sandbox, ctx, png_set_option, mPNG, PNG_MAXIMUM_INFLATE_WINDOW, PNG_OPTION_ON);
#endif
#endif

  // use this as libpng "progressive pointer" (retrieve in callbacks)
  sandbox_invoke_custom(rlbox_sandbox, ctx, png_set_progressive_read_fn, mPNG, rlbox_sandbox->app_ptr(static_cast<png_voidp>(this)),
                            rlbox_sbx->cpp_cb_png_progressive_info_fn,
                            rlbox_sbx->cpp_cb_png_progressive_row_fn,
                            rlbox_sbx->cpp_cb_png_progressive_end_fn);

  return NS_OK;
}

LexerResult
nsPNGDecoder::DoDecode(SourceBufferIterator& aIterator, IResumable* aOnResume)
{
  MOZ_ASSERT(!HasError(), "Shouldn't call DoDecode after error!");

  return mLexer.Lex(aIterator, aOnResume,
                    [=](State aState, const char* aData, size_t aLength) {
    switch (aState) {
      case State::PNG_DATA:
        return ReadPNGData(aData, aLength);
      case State::FINISHED_PNG_DATA:
        return FinishedPNGData();
    }
    MOZ_CRASH("Unknown State");
  });
}

LexerTransition<nsPNGDecoder::State>
nsPNGDecoder::ReadPNGData(const char* aData, size_t aLength)
{
  pngRendererSaved = this;
  auto rlbox_sandbox = rlbox_sbx->rlbox_sandbox;
  auto ctx = rlbox_sandbox->getSandbox()->get_context();
  // If we were waiting until after returning from a yield to call
  // CreateFrame(), call it now.
  if (mNextFrameInfo) {
    if (NS_FAILED(CreateFrame(*mNextFrameInfo))) {
      return Transition::TerminateFailure();
    }

    MOZ_ASSERT(mImageData, "Should have a buffer now");
    mNextFrameInfo = Nothing();
  }

  // libpng uses setjmp/longjmp for error handling.
  if(jumpBufferFilledIn)
  {
    printf("nsPNGDecoder::nsPNGDecoder - jmp_buf jump buffer already in use1\n");
    abort();
  }

  sandbox_invoke_custom(rlbox_sandbox, ctx, png_set_longjmp_fn, mPNG, rlbox_sbx->cpp_cb_longjmp_fn, sizeof(jmp_buf));
  jumpBufferFilledIn = freshMapId();
  if (setjmp(pngJmpBuffers[jumpBufferFilledIn])) {
    return Transition::TerminateFailure();
  }

  auto aData_sandbox = rlbox_sandbox->mallocInSandbox<char>(aLength);
  memcpy(aData_sandbox.UNSAFE_Unverified(), aData, aLength);

  // Pass the data off to libpng.
  mLastChunkLength = aLength;
  mNextTransition = Transition::ContinueUnbuffered(State::PNG_DATA);
  PngBench.Start();
  sandbox_invoke_custom(rlbox_sandbox, ctx, png_process_data, mPNG, mInfo,
                  sandbox_reinterpret_cast<unsigned char*>(aData_sandbox),
                  aLength);
  rlbox_sandbox->freeInSandbox(aData_sandbox);
  PngBench.Stop();

  // Make sure that we've reached a terminal state if decoding is done.
  MOZ_ASSERT_IF(GetDecodeDone(), mNextTransition.NextStateIsTerminal());
  MOZ_ASSERT_IF(HasError(), mNextTransition.NextStateIsTerminal());

  //clear the jmpBufferIndex on the way out as it is unsafe to call setjmp after this
  jumpBufferFilledIn = 0;

  // Continue with whatever transition the callback code requested. We
  // initialized this to Transition::ContinueUnbuffered(State::PNG_DATA) above,
  // so by default we just continue the unbuffered read.
  return mNextTransition;
}

LexerTransition<nsPNGDecoder::State>
nsPNGDecoder::FinishedPNGData()
{
  // Since we set up an unbuffered read for SIZE_MAX bytes, if we actually read
  // all that data something is really wrong.
  MOZ_ASSERT_UNREACHABLE("Read the entire address space?");
  return Transition::TerminateFailure();
}

// Sets up gamma pre-correction in libpng before our callback gets called.
// We need to do this if we don't end up with a CMS profile.
static void PNGDoGammaCorrection(RLBoxSandbox<TRLSandboxP>* rlbox_sandbox, tainted<png_structp, TRLSandboxP> png_ptr, tainted<png_infop, TRLSandboxP> info_ptr)
{
  auto p_aGamma = rlbox_sandbox->mallocInSandbox<double>();
  auto ctx = rlbox_sandbox->getSandbox()->get_context();
  auto pngGetGammaRet = sandbox_invoke_custom(rlbox_sandbox, ctx, png_get_gAMA, png_ptr, info_ptr, p_aGamma)
    .copyAndVerify([](png_uint_32 val){ return val; });

  if (pngGetGammaRet) {
    double aGamma = p_aGamma.copyAndVerify([&png_ptr](double* val){ 
      if(val == nullptr)
      {
        printf("Sbox - got a null gamma\n"); abort();
      }

      return *val; 
    });

    if ((aGamma <= 0.0) || (aGamma > 21474.83)) {
      aGamma = 0.45455;
      sandbox_invoke_custom(rlbox_sandbox, ctx, png_set_gAMA, png_ptr, info_ptr, aGamma);
    }
    sandbox_invoke_custom(rlbox_sandbox, ctx, png_set_gamma, png_ptr, 2.2, aGamma);
  } else {
    sandbox_invoke_custom(rlbox_sandbox, ctx, png_set_gamma, png_ptr, 2.2, 0.45455);
  }
  rlbox_sandbox->freeInSandbox(p_aGamma);
}

// Adapted from http://www.littlecms.com/pngchrm.c example code
static qcms_profile* PNGGetColorProfile(RLBoxSandbox<TRLSandboxP>* rlbox_sandbox, tainted<png_structp, TRLSandboxP> png_ptr, tainted<png_infop, TRLSandboxP> info_ptr, int color_type, qcms_data_type* inType, uint32_t* intent)
{
  qcms_profile* profile = nullptr;
  *intent = QCMS_INTENT_PERCEPTUAL; // Our default
  auto ctx = rlbox_sandbox->getSandbox()->get_context();

  // First try to see if iCCP chunk is present
  if (
    sandbox_invoke_custom(rlbox_sandbox, ctx, png_get_valid, png_ptr, info_ptr, PNG_INFO_iCCP)
      .copyAndVerify([](png_uint_32 val){ return val; })
  ) {

    auto p_params = rlbox_sandbox->mallocInSandbox<png_get_iCCP_params>();

    sandbox_invoke_custom(rlbox_sandbox, ctx, png_get_iCCP, png_ptr, info_ptr, &(p_params->profileName), &(p_params->compression),
                  &(p_params->profileData), &(p_params->profileLen));

    png_uint_32 profileLen = p_params->profileLen.copyAndVerify([](png_uint_32 pVal){
      return pVal;
    });

    png_bytep profileData = p_params->profileData.copyAndVerifyArray(rlbox_sandbox, [](png_bytep val){
        return RLBox_Verify_Status::SAFE;
      }, profileLen, nullptr);

    if(profileData == nullptr)
    {
      printf("Sbox - profileData array invalid value\n"); abort();
    }

    rlbox_sandbox->freeInSandbox(p_params);

    profile = qcms_profile_from_memory((char*)profileData, profileLen);
    if (profile) {
      uint32_t profileSpace = qcms_profile_get_color_space(profile);

      bool mismatch = false;
      if (color_type & PNG_COLOR_MASK_COLOR) {
        if (profileSpace != icSigRgbData) {
          mismatch = true;
        }
      } else {
        if (profileSpace == icSigRgbData) {
          sandbox_invoke_custom(rlbox_sandbox, ctx, png_set_gray_to_rgb, png_ptr);
        } else if (profileSpace != icSigGrayData) {
          mismatch = true;
        }
      }

      if (mismatch) {
        qcms_profile_release(profile);
        profile = nullptr;
      } else {
        *intent = qcms_profile_get_rendering_intent(profile);
      }
    }
  }

  // Check sRGB chunk
  if (!profile &&
    sandbox_invoke_custom(rlbox_sandbox, ctx, png_get_valid, png_ptr, info_ptr, PNG_INFO_sRGB)
      .copyAndVerify([](png_uint_32 val){ return val; })
  ) {
    profile = qcms_profile_sRGB();

    if (profile) {
      auto p_fileIntent = rlbox_sandbox->mallocInSandbox<int>();

      sandbox_invoke_custom(rlbox_sandbox, ctx, png_set_gray_to_rgb, png_ptr);
      sandbox_invoke_custom(rlbox_sandbox, ctx, png_get_sRGB, png_ptr, info_ptr, p_fileIntent);

      int fileIntent = p_fileIntent.copyAndVerify([&png_ptr](int *val){
        if(val != nullptr && *val >= 0 && *val < 4) { return *val; }
        printf("Sbox - fileIntent value out of range\n");
        return 0;
      });

      rlbox_sandbox->freeInSandbox(p_fileIntent);
      uint32_t map[] = { QCMS_INTENT_PERCEPTUAL,
                         QCMS_INTENT_RELATIVE_COLORIMETRIC,
                         QCMS_INTENT_SATURATION,
                         QCMS_INTENT_ABSOLUTE_COLORIMETRIC };
      *intent = map[fileIntent];
    }
  }

  // Check gAMA/cHRM chunks
  if (!profile &&
    sandbox_invoke_custom(rlbox_sandbox, ctx, png_get_valid, png_ptr, info_ptr, PNG_INFO_gAMA)
      .copyAndVerify([](png_uint_32 val){ return val; })
    &&
    sandbox_invoke_custom(rlbox_sandbox, ctx, png_get_valid, png_ptr, info_ptr, PNG_INFO_cHRM)
      .copyAndVerify([](png_uint_32 val){ return val; })
  )
  {
    auto p_params = rlbox_sandbox->mallocInSandbox<png_get_cHRM_And_gAMA_params>();

    sandbox_invoke_custom(rlbox_sandbox, ctx, png_get_cHRM, png_ptr, info_ptr,
                  &(p_params->whitePoint.x), &(p_params->whitePoint.y),
                  &(p_params->primaries.red.x),   &(p_params->primaries.red.y),
                  &(p_params->primaries.green.x), &(p_params->primaries.green.y),
                  &(p_params->primaries.blue.x),  &(p_params->primaries.blue.y));

    auto qcms_CIE_xyY_Verifier = [](tainted<qcms_CIE_xyY, TRLSandboxP>* val) { 
      qcms_CIE_xyY ret;
      ret.x = val->x.copyAndVerify([](double val) { return val; });
      ret.y = val->y.copyAndVerify([](double val) { return val; });
      ret.Y = val->Y.copyAndVerify([](double val) { return val; });
      return ret; 
    };

    qcms_CIE_xyY whitePoint = (&(p_params->whitePoint)).copyAndVerify(qcms_CIE_xyY_Verifier);

    qcms_CIE_xyYTRIPLE primaries = (&(p_params->primaries)).copyAndVerify([&qcms_CIE_xyY_Verifier](tainted<qcms_CIE_xyYTRIPLE, TRLSandboxP>* val) { 
      qcms_CIE_xyYTRIPLE ret;
      ret.red   = qcms_CIE_xyY_Verifier(&val->red);
      ret.green = qcms_CIE_xyY_Verifier(&val->green);
      ret.blue  = qcms_CIE_xyY_Verifier(&val->blue);
      return ret;
    });

    whitePoint.Y =
      primaries.red.Y = primaries.green.Y = primaries.blue.Y = 1.0;

    sandbox_invoke_custom(rlbox_sandbox, ctx, png_get_gAMA, png_ptr, info_ptr, &(p_params->gammaOfFile));

    double gammaOfFile = p_params->gammaOfFile.copyAndVerify([&png_ptr](double val) { 
      if(std::isfinite(val))
      {
        return val;
      }

      printf("Sbox - gamma value out of range\n"); abort();
      return double(0);
    });

    rlbox_sandbox->freeInSandbox(p_params);

    profile = qcms_profile_create_rgb_with_gamma(whitePoint, primaries,
                                                 1.0/gammaOfFile);

    if (profile) {
      sandbox_invoke_custom(rlbox_sandbox, ctx, png_set_gray_to_rgb, png_ptr);
    }
  }

  if (profile) {
    uint32_t profileSpace = qcms_profile_get_color_space(profile);
    if (profileSpace == icSigGrayData) {
      if (color_type & PNG_COLOR_MASK_ALPHA) {
        *inType = QCMS_DATA_GRAYA_8;
      } else {
        *inType = QCMS_DATA_GRAY_8;
      }
    } else {
      if (color_type & PNG_COLOR_MASK_ALPHA ||
          sandbox_invoke_custom(rlbox_sandbox, ctx, png_get_valid, png_ptr, info_ptr, PNG_INFO_tRNS)
            .copyAndVerify([](png_uint_32 val){ return val; })
      ) {
        *inType = QCMS_DATA_RGBA_8;
      } else {
        *inType = QCMS_DATA_RGB_8;
      }
    }
  }

  return profile;
}

void nsPNGDecoder::info_callback(RLBoxSandbox<TRLSandboxP>* sandbox, tainted<png_structp, TRLSandboxP> png_ptr, tainted<png_infop, TRLSandboxP> info_ptr)
{
  nsPNGDecoder* decoder = static_cast<nsPNGDecoder*>(pngRendererSaved);
  auto rlbox_sandbox = decoder->rlbox_sbx->rlbox_sandbox;
  auto ctx = rlbox_sandbox->getSandbox()->get_context();

  auto p_params = rlbox_sandbox->mallocInSandbox<png_get_IHDR_params>();

  unsigned int channels;

  // Always decode to 24-bit RGB or 32-bit RGBA
  sandbox_invoke_custom(rlbox_sandbox, ctx, png_get_IHDR, png_ptr, info_ptr, &(p_params->width), &(p_params->height), &(p_params->bit_depth), &(p_params->color_type),
                &(p_params->interlace_type), &(p_params->compression_type), &(p_params->filter_type));

  png_uint_32 width  = (&(p_params->width)). copyAndVerify([](png_uint_32* val) { return *val; });
  png_uint_32 height = (&(p_params->height)).copyAndVerify([](png_uint_32* val) { return *val; });

  //from here http://www.libpng.org/pub/png/spec/1.2/PNG-Chunks.html
  int color_type = (&(p_params->color_type)).copyAndVerify([&png_ptr](int* val) {
    if(val != nullptr && (*val == 0 || *val == 2 || *val == 3 || *val == 4 || *val == 6))
    {
      return *val;
    }

    printf("Sbox - color_type value out of range\n"); abort();
    return 0;
  });

  int bit_depth = (&(p_params->bit_depth)).copyAndVerify([&png_ptr, &color_type](int* val) {
    if(val != nullptr)
    {
      if(color_type == 0 && (*val == 1 || *val == 2 || *val == 4 || *val == 8 || *val == 16))
      {
        return *val;
      }
      else if(color_type == 2 && (*val == 8 || *val == 16))
      {
        return *val;
      }
      else if(color_type == 3 && (*val == 1 || *val == 2 || *val == 4 || *val == 8))
      {
        return *val;
      }
      else if(color_type == 4 && (*val == 8 || *val == 16))
      {
        return *val;
      }
      else if(color_type == 6 && (*val == 8 || *val == 16))
      {
        return *val;
      }
    }

    printf("Sbox - bit_depth value out of range\n"); abort();
    return 0;
  });

  int interlace_type = (&(p_params->interlace_type)).copyAndVerify([&png_ptr](int* val) {
    if(val != nullptr && (*val == 0 || *val == 1))
    {
      return *val;
    }

    printf("Sbox - interlace_type value out of range\n"); abort();
    return 0;
  });

  rlbox_sandbox->freeInSandbox(p_params);

  // Exclude very small images as the actual impact is very small
  if(width < 100) {
    decoder->PngMaybeTooSmall = true;
  } else {
    decoder->PngMaybeTooSmall = false;
  }

  const IntRect frameRect(0, 0, width, height);

  // Post our size to the superclass
  decoder->PostSize(frameRect.Width(), frameRect.Height());

  if (width >
    SurfaceCache::MaximumCapacity()/(bit_depth > 8 ? 16:8)) {
    // libpng needs space to allocate two row buffers
    sandbox_invoke_custom(rlbox_sandbox, ctx, png_error, decoder->mPNG, rlbox_sandbox->stackarr("Image is too wide"));
  }

  if (decoder->HasError()) {
    // Setting the size led to an error.
    sandbox_invoke_custom(rlbox_sandbox, ctx, png_error, decoder->mPNG, rlbox_sandbox->stackarr("Sizing error"));
  }

  if (color_type == PNG_COLOR_TYPE_PALETTE) {
    sandbox_invoke_custom(rlbox_sandbox, ctx, png_set_expand, png_ptr);
  }

  if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) {
    sandbox_invoke_custom(rlbox_sandbox, ctx, png_set_expand, png_ptr);
  }

  int num_trans = 0;

  if (
    sandbox_invoke_custom(rlbox_sandbox, ctx, png_get_valid, png_ptr, info_ptr, PNG_INFO_tRNS)
      .copyAndVerify([](png_uint_32 val){ return val; })
  ) {
    auto p_params = rlbox_sandbox->mallocInSandbox<png_get_tRNS_params>();
    sandbox_invoke_custom(rlbox_sandbox, ctx, png_get_tRNS, png_ptr, info_ptr, &(p_params->trans), &(p_params->num_trans), &(p_params->trans_values));
    tainted<png_color_16p, TRLSandboxP> trans_values = p_params->trans_values;

    if(trans_values == nullptr)
    {
      printf("Sbox - *p_trans_values is null\n"); abort();
    }

    png_uint_16 trans_values_red   = trans_values->red  .copyAndVerify([](png_uint_16 val) { return val; });
    png_uint_16 trans_values_green = trans_values->green.copyAndVerify([](png_uint_16 val) { return val; });
    png_uint_16 trans_values_blue  = trans_values->blue .copyAndVerify([](png_uint_16 val) { return val; });
    png_uint_16 trans_values_gray  = trans_values->gray .copyAndVerify([](png_uint_16 val) { return val; });
    num_trans = (&(p_params->num_trans)).copyAndVerify([decoder, rlbox_sandbox, ctx](int* val){ 
      if(val == nullptr) 
      {
        sandbox_invoke_custom(rlbox_sandbox, ctx, png_error, decoder->mPNG, rlbox_sandbox->stackarr("Sbox - png_color_type value null"));
      }
      return *val; 
    });

    rlbox_sandbox->freeInSandbox(p_params);

    // libpng doesn't reject a tRNS chunk with out-of-range samples
    // so we check it here to avoid setting up a useless opacity
    // channel or producing unexpected transparent pixels (bug #428045)
    if (bit_depth < 16) {
      png_uint_16 sample_max = (1 << bit_depth) - 1;

      if ((color_type == PNG_COLOR_TYPE_GRAY &&
        trans_values_gray > sample_max) ||
        (color_type == PNG_COLOR_TYPE_RGB &&
        (trans_values_red > sample_max ||
        trans_values_green > sample_max ||
        trans_values_blue > sample_max))) {
        sandbox_invoke_custom(rlbox_sandbox, ctx, png_free_data, png_ptr, info_ptr, PNG_FREE_TRNS, 0);
        num_trans = 0;
      }
    }
    if (num_trans != 0) {
      sandbox_invoke_custom(rlbox_sandbox, ctx, png_set_expand, png_ptr);
    }
  }

  if (bit_depth == 16) {
    sandbox_invoke_custom(rlbox_sandbox, ctx, png_set_scale_16, png_ptr);
  }

  qcms_data_type inType = QCMS_DATA_RGBA_8;
  uint32_t intent = -1;
  uint32_t pIntent;
  if (decoder->mCMSMode != eCMSMode_Off) {
    intent = gfxPlatform::GetRenderingIntent();
    decoder->mInProfile = PNGGetColorProfile(rlbox_sandbox, png_ptr, info_ptr,
                                             color_type, &inType, &pIntent);
    // If we're not mandating an intent, use the one from the image.
    if (intent == uint32_t(-1)) {
      intent = pIntent;
    }
  }

  if (decoder->mInProfile && gfxPlatform::GetCMSOutputProfile()) {
    qcms_data_type outType;

    if (color_type & PNG_COLOR_MASK_ALPHA || num_trans) {
      outType = QCMS_DATA_RGBA_8;
    } else {
      outType = QCMS_DATA_RGB_8;
    }

    decoder->mTransform = qcms_transform_create(decoder->mInProfile,
                                           inType,
                                           gfxPlatform::GetCMSOutputProfile(),
                                           outType,
                                           (qcms_intent)intent);
  } else {
    sandbox_invoke_custom(rlbox_sandbox, ctx, png_set_gray_to_rgb, png_ptr);

    // only do gamma correction if CMS isn't entirely disabled
    if (decoder->mCMSMode != eCMSMode_Off) {
      PNGDoGammaCorrection(rlbox_sandbox, png_ptr, info_ptr);
    }

    if (decoder->mCMSMode == eCMSMode_All) {
      if (color_type & PNG_COLOR_MASK_ALPHA || num_trans) {
        decoder->mTransform = gfxPlatform::GetCMSRGBATransform();
      } else {
        decoder->mTransform = gfxPlatform::GetCMSRGBTransform();
      }
    }
  }

  // Let libpng expand interlaced images.
  const bool isInterlaced = interlace_type == PNG_INTERLACE_ADAM7;
  if (isInterlaced) {
    sandbox_invoke_custom(rlbox_sandbox, ctx, png_set_interlace_handling, png_ptr);
  }

  // now all of those things we set above are used to update various struct
  // members and whatnot, after which we can get channels, rowbytes, etc.
  sandbox_invoke_custom(rlbox_sandbox, ctx, png_read_update_info, png_ptr, info_ptr);
  decoder->mChannels = channels = sandbox_invoke_custom(rlbox_sandbox, ctx, png_get_channels, png_ptr, info_ptr)
    .copyAndVerify([&png_ptr, &color_type](int val) {
      if(val >= 1 && val <= 4)
      {
        return val;
      }

      printf("Sbox - channels value out of range\n"); abort();
      return 0;
    });

  //---------------------------------------------------------------//
  // copy PNG info into imagelib structs (formerly png_set_dims()) //
  //---------------------------------------------------------------//

  if (channels < 1 || channels > 4) {
    sandbox_invoke_custom(rlbox_sandbox, ctx, png_error, decoder->mPNG, rlbox_sandbox->stackarr("Invalid number of channels"));
  }

#ifdef PNG_APNG_SUPPORTED
  bool isAnimated = 
  sandbox_invoke_custom(rlbox_sandbox, ctx, png_get_valid, png_ptr, info_ptr, PNG_INFO_acTL)
    .copyAndVerify([](png_uint_32 val){ return val; });

  if (isAnimated) {
    int32_t rawTimeout = GetNextFrameDelay(rlbox_sandbox, png_ptr, info_ptr);
    decoder->PostIsAnimated(FrameTimeout::FromRawMilliseconds(rawTimeout));

    if (decoder->Size() != decoder->OutputSize() &&
        !decoder->IsFirstFrameDecode()) {
      MOZ_ASSERT_UNREACHABLE("Doing downscale-during-decode "
                             "for an animated image?");

      sandbox_invoke_custom(rlbox_sandbox, ctx, png_error, decoder->mPNG, rlbox_sandbox->stackarr("Invalid downscale attempt"));
    }
  }
#endif

  if (decoder->IsMetadataDecode()) {
    // If we are animated then the first frame rect is either:
    // 1) the whole image if the IDAT chunk is part of the animation
    // 2) the frame rect of the first fDAT chunk otherwise.
    // If we are not animated then we want to make sure to call
    // PostHasTransparency in the metadata decode if we need to. So it's
    // okay to pass IntRect(0, 0, width, height) here for animated images;
    // they will call with the proper first frame rect in the full decode.
    auto transparency = decoder->GetTransparencyType(frameRect);
    decoder->PostHasTransparencyIfNeeded(transparency);

    // We have the metadata we're looking for, so stop here, before we allocate
    // buffers below.
    return decoder->DoTerminate(png_ptr, TerminalState::SUCCESS);
  }

#ifdef PNG_APNG_SUPPORTED
  if (isAnimated) {
    sandbox_invoke_custom(rlbox_sandbox, ctx, png_set_progressive_frame_fn, png_ptr, decoder->rlbox_sbx->cpp_cb_png_progressive_frame_info_fn,
                          nullptr);
  }

  if (
    sandbox_invoke_custom(rlbox_sandbox, ctx, png_get_first_frame_is_hidden, png_ptr, info_ptr)
      .copyAndVerify([](png_byte val){ return val; })
  ) {
    decoder->mFrameIsHidden = true;
  } else {
#endif
    nsresult rv = decoder->CreateFrame(FrameInfo{ frameRect,
                                                  isInterlaced });
    if (NS_FAILED(rv)) {
      sandbox_invoke_custom(rlbox_sandbox, ctx, png_error, decoder->mPNG, rlbox_sandbox->stackarr("CreateFrame failed"));
    }
    MOZ_ASSERT(decoder->mImageData, "Should have a buffer now");
#ifdef PNG_APNG_SUPPORTED
  }
#endif

  if (decoder->mTransform && (channels <= 2 || isInterlaced)) {
    uint32_t bpp[] = { 0, 3, 4, 3, 4 };
    decoder->mCMSLine =
      static_cast<uint8_t*>(malloc(bpp[channels] * frameRect.Width()));
    if (!decoder->mCMSLine) {
      sandbox_invoke_custom(rlbox_sandbox, ctx, png_error, decoder->mPNG, rlbox_sandbox->stackarr("malloc of mCMSLine failed"));
    }
  }

  if (interlace_type == PNG_INTERLACE_ADAM7) {
    if (frameRect.Height() < INT32_MAX / (frameRect.Width() * int32_t(channels))) {
      const size_t bufferSize = channels * frameRect.Width() * frameRect.Height();

      if (bufferSize > SurfaceCache::MaximumCapacity()) {
        sandbox_invoke_custom(rlbox_sandbox, ctx, png_error, decoder->mPNG, rlbox_sandbox->stackarr("Insufficient memory to deinterlace image"));

      }

      decoder->interlacebuf = rlbox_sandbox->mallocInSandbox<uint8_t>(bufferSize).UNSAFE_Unverified();
    }
    if (!decoder->interlacebuf) {
      sandbox_invoke_custom(rlbox_sandbox, ctx, png_error, decoder->mPNG, rlbox_sandbox->stackarr("malloc of interlacebuf failed"));
    }
  }
}

void
nsPNGDecoder::PostInvalidationIfNeeded()
{
  Maybe<SurfaceInvalidRect> invalidRect = mPipe.TakeInvalidRect();
  if (!invalidRect) {
    return;
  }

  PostInvalidation(invalidRect->mInputSpaceRect,
                   Some(invalidRect->mOutputSpaceRect));
}

static NextPixel<uint32_t>
PackRGBPixelAndAdvance(uint8_t*& aRawPixelInOut)
{
  const uint32_t pixel =
    gfxPackedPixel(0xFF, aRawPixelInOut[0], aRawPixelInOut[1],
                   aRawPixelInOut[2]);
  aRawPixelInOut += 3;
  return AsVariant(pixel);
}

static NextPixel<uint32_t>
PackRGBAPixelAndAdvance(uint8_t*& aRawPixelInOut)
{
  const uint32_t pixel =
    gfxPackedPixel(aRawPixelInOut[3], aRawPixelInOut[0],
                   aRawPixelInOut[1], aRawPixelInOut[2]);
  aRawPixelInOut += 4;
  return AsVariant(pixel);
}

static NextPixel<uint32_t>
PackUnpremultipliedRGBAPixelAndAdvance(uint8_t*& aRawPixelInOut)
{
  const uint32_t pixel =
    gfxPackedPixelNoPreMultiply(aRawPixelInOut[3], aRawPixelInOut[0],
                                aRawPixelInOut[1], aRawPixelInOut[2]);
  aRawPixelInOut += 4;
  return AsVariant(pixel);
}

void nsPNGDecoder::row_callback(RLBoxSandbox<TRLSandboxP>* sandbox, tainted<png_structp, TRLSandboxP> png_ptr, tainted<png_bytep, TRLSandboxP> new_row, tainted<png_uint_32, TRLSandboxP> row_num_unv, tainted<int, TRLSandboxP> pass_unv)
{
  /* libpng comments:
   *
   * This function is called for every row in the image.  If the
   * image is interlacing, and you turned on the interlace handler,
   * this function will be called for every row in every pass.
   * Some of these rows will not be changed from the previous pass.
   * When the row is not changed, the new_row variable will be
   * nullptr. The rows and passes are called in order, so you don't
   * really need the row_num and pass, but I'm supplying them
   * because it may make your life easier.
   *
   * For the non-nullptr rows of interlaced images, you must call
   * png_progressive_combine_row() passing in the row and the
   * old row.  You can call this function for nullptr rows (it will
   * just return) and for non-interlaced images (it just does the
   * memcpy for you) if it will make the code easier.  Thus, you
   * can just do this for all cases:
   *
   *    png_progressive_combine_row(png_ptr, old_row, new_row);
   *
   * where old_row is what was displayed for previous rows.  Note
   * that the first pass (pass == 0 really) will completely cover
   * the old row, so the rows do not have to be initialized.  After
   * the first pass (and only for interlaced images), you will have
   * to pass the current row, and the function will combine the
   * old row and the new row.
   */

  nsPNGDecoder* decoder = static_cast<nsPNGDecoder*>(pngRendererSaved);
  auto rlbox_sandbox = decoder->rlbox_sbx->rlbox_sandbox;
  auto ctx = rlbox_sandbox->getSandbox()->get_context();

  if (decoder->mFrameIsHidden) {
    return;  // Skip this frame.
  }

  MOZ_ASSERT_IF(decoder->IsFirstFrameDecode(), decoder->mNumFrames == 0);

  //data sanity checks already exist below
  png_uint_32 row_num = row_num_unv.copyAndVerify([](png_uint_32 val){ return val; });
  int pass = pass_unv.copyAndVerify([](int val){ 
    if(val < 0 || val > std::numeric_limits<decltype(decoder->mPass)>::max()) {
      printf("Sbox - png row_callback pass value (%d) out of range\n", val);
      abort();
    }
    return val; 
  });

  while (pass > decoder->mPass) {
    // Advance to the next pass. We may have to do this multiple times because
    // libpng will skip passes if the image is so small that no pixels have
    // changed on a given pass, but ADAM7InterpolatingFilter needs to be reset
    // once for every pass to perform interpolation properly.
    decoder->mPipe.ResetToFirstRow();
    decoder->mPass++;
  }

  const png_uint_32 height =
    static_cast<png_uint_32>(decoder->mFrameRect.Height());

  if (row_num >= height) {
    // Bail if we receive extra rows. This is especially important because if we
    // didn't, we might overflow the deinterlacing buffer.
    MOZ_ASSERT_UNREACHABLE("libpng producing extra rows?");
    return;
  }

  // Note that |new_row| may be null here, indicating that this is an interlaced
  // image and |row_callback| is being called for a row that hasn't changed.
  MOZ_ASSERT_IF(!new_row, decoder->interlacebuf);
  auto rowToWrite = new_row;
  uint32_t width = uint32_t(decoder->mFrameRect.Width());

  if (decoder->interlacebuf) {

    // We'll output the deinterlaced version of the row.
    rowToWrite.assignPointerInSandbox(rlbox_sandbox, decoder->interlacebuf + (row_num * decoder->mChannels * width));

    // Update the deinterlaced version of this row with the new data.
    sandbox_invoke_custom(rlbox_sandbox, ctx, png_progressive_combine_row, png_ptr, rowToWrite, new_row);
  }

  auto rowToWriteVer = rowToWrite.UNSAFE_Unverified_Check_Range(rlbox_sandbox, decoder->mChannels * width);
  decoder->WriteRow(rowToWriteVer);
  //pngStartTimerCore();
}

void nsPNGDecoder::WriteRow(uint8_t* aRow)
{
  MOZ_ASSERT(aRow);

  uint8_t* rowToWrite = aRow;
  uint32_t width = uint32_t(mFrameRect.Width());

  // Apply color management to the row, if necessary, before writing it out.
  if (mTransform) {
    if (mCMSLine) {
      qcms_transform_data(mTransform, rowToWrite, mCMSLine, width);

      // Copy alpha over.
      if (HasAlphaChannel()) {
        for (uint32_t i = 0; i < width; ++i) {
          mCMSLine[4 * i + 3] = rowToWrite[mChannels * i + mChannels - 1];
        }
      }

      rowToWrite = mCMSLine;
    } else {
      qcms_transform_data(mTransform, rowToWrite, rowToWrite, width);
    }
  }

  // Write this row to the SurfacePipe.
  DebugOnly<WriteState> result;
  if (HasAlphaChannel()) {
    if (mDisablePremultipliedAlpha) {
      result = mPipe.WritePixelsToRow<uint32_t>([&]{
        return PackUnpremultipliedRGBAPixelAndAdvance(rowToWrite);
      });
    } else {
      result = mPipe.WritePixelsToRow<uint32_t>([&]{
        return PackRGBAPixelAndAdvance(rowToWrite);
      });
    }
  } else {
    result = mPipe.WritePixelsToRow<uint32_t>([&]{
      return PackRGBPixelAndAdvance(rowToWrite);
    });
  }

  MOZ_ASSERT(WriteState(result) != WriteState::FAILURE);

  PostInvalidationIfNeeded();
}

void nsPNGDecoder::DoTerminate(tainted<png_structp, TRLSandboxP> aPNGStruct, TerminalState aState)
{
  // Stop processing data. Note that we intentionally ignore the return value of
  // png_process_data_pause(), which tells us how many bytes of the data that
  // was passed to png_process_data() have not been consumed yet, because now
  // that we've reached a terminal state, we won't do any more decoding or call
  // back into libpng anymore.
  auto rlbox_sandbox = rlbox_sbx->rlbox_sandbox;
  auto ctx = rlbox_sandbox->getSandbox()->get_context();
  sandbox_invoke_custom(rlbox_sandbox, ctx, png_process_data_pause, aPNGStruct, /* save = */ false);

  mNextTransition = aState == TerminalState::SUCCESS
                  ? Transition::TerminateSuccess()
                  : Transition::TerminateFailure();
}

void nsPNGDecoder::DoYield(tainted<png_structp, TRLSandboxP> aPNGStruct)
{
  // Pause data processing. png_process_data_pause() returns how many bytes of
  // the data that was passed to png_process_data() have not been consumed yet.
  // We use this information to tell StreamingLexer where to place us in the
  // input stream when we come back from the yield.
  auto rlbox_sandbox = rlbox_sbx->rlbox_sandbox;
  auto ctx = rlbox_sandbox->getSandbox()->get_context();
  png_size_t pendingBytes = sandbox_invoke_custom(rlbox_sandbox, ctx, png_process_data_pause, aPNGStruct,
                                                    /* save = */ false)
    .copyAndVerify([](png_size_t val){
      return val;
    });

  MOZ_ASSERT(pendingBytes < mLastChunkLength);
  size_t consumedBytes = mLastChunkLength - std::min(pendingBytes, mLastChunkLength);

  mNextTransition =
    Transition::ContinueUnbufferedAfterYield(State::PNG_DATA, consumedBytes);
}

nsresult
nsPNGDecoder::FinishInternal()
{
  // We shouldn't be called in error cases.
  MOZ_ASSERT(!HasError(), "Can't call FinishInternal on error!");

  if (IsMetadataDecode()) {
    return NS_OK;
  }

  int32_t loop_count = 0;
  auto rlbox_sandbox = rlbox_sbx->rlbox_sandbox;
  auto ctx = rlbox_sandbox->getSandbox()->get_context();
#ifdef PNG_APNG_SUPPORTED
  if (
    sandbox_invoke_custom(rlbox_sandbox, ctx, png_get_valid, mPNG, mInfo, PNG_INFO_acTL)
      .copyAndVerify([](png_uint_32 val){ return val; })
  ) {
    int32_t num_plays = sandbox_invoke_custom(rlbox_sandbox, ctx, png_get_num_plays, mPNG, mInfo)
      .copyAndVerify([](int32_t val){ return val; });
    loop_count = num_plays - 1;
  }
#endif

  if(!PngMaybeTooSmall)
  {
    auto diff = PngBench.JustFinish();
    //std::string tag = "PNG_destroy(" + mImageString + ")";
    //printf("Capture_Time:%s,%llu,%llu|\n", tag.c_str(), invPng, diff);
    invPng++;
  }

  if (InFrame()) {
    EndImageFrame();
  }
  PostDecodeDone(loop_count);

  pngRendererSaved = nullptr;

  return NS_OK;
}


#ifdef PNG_APNG_SUPPORTED
// got the header of a new frame that's coming
void nsPNGDecoder::frame_info_callback(RLBoxSandbox<TRLSandboxP>* sandbox, tainted<png_structp, TRLSandboxP> png_ptr, tainted<png_uint_32, TRLSandboxP> frame_num)
{
  nsPNGDecoder* decoder = static_cast<nsPNGDecoder*>(pngRendererSaved);
  auto rlbox_sandbox = decoder->rlbox_sbx->rlbox_sandbox;
  auto ctx = rlbox_sandbox->getSandbox()->get_context();

  // old frame is done
  decoder->EndImageFrame();

  const bool previousFrameWasHidden = decoder->mFrameIsHidden;

  if (!previousFrameWasHidden && decoder->IsFirstFrameDecode()) {
    // We're about to get a second non-hidden frame, but we only want the first.
    // Stop decoding now. (And avoid allocating the unnecessary buffers below.)
    return decoder->DoTerminate(png_ptr, TerminalState::SUCCESS);
  }

  // Only the first frame can be hidden, so unhide unconditionally here.
  decoder->mFrameIsHidden = false;

  // Save the information necessary to create the frame; we'll actually create
  // it when we return from the yield.
  const IntRect frameRect(sandbox_invoke_custom(rlbox_sandbox, ctx, png_get_next_frame_x_offset, png_ptr, decoder->mInfo).copyAndVerify([](png_uint_32 val){ return val; }),
                          sandbox_invoke_custom(rlbox_sandbox, ctx, png_get_next_frame_y_offset, png_ptr, decoder->mInfo).copyAndVerify([](png_uint_32 val){ return val; }),
                          sandbox_invoke_custom(rlbox_sandbox, ctx, png_get_next_frame_width, png_ptr, decoder->mInfo).copyAndVerify([](png_uint_32 val){ return val; }),
                          sandbox_invoke_custom(rlbox_sandbox, ctx, png_get_next_frame_height, png_ptr, decoder->mInfo).copyAndVerify([](png_uint_32 val){ return val; }));
  const bool isInterlaced = bool(decoder->interlacebuf);

#ifndef MOZ_EMBEDDED_LIBPNG
  // if using system library, check frame_width and height against 0
  if (frameRect.width == 0) {
    sandbox_invoke_custom(rlbox_sandbox, ctx, png_error, png_ptr, rlbox_sandbox->stackarr("Frame width must not be 0"));
  }
  if (frameRect.height == 0) {
    sandbox_invoke_custom(rlbox_sandbox, ctx, png_error, png_ptr, rlbox_sandbox->stackarr("Frame height must not be 0"));
  }
#endif

  const FrameInfo info { frameRect, isInterlaced };

  // If the previous frame was hidden, skip the yield (which will mislead the
  // caller, who will think the previous frame was real) and just allocate the
  // new frame here.
  if (previousFrameWasHidden) {
    if (NS_FAILED(decoder->CreateFrame(info))) {
      return decoder->DoTerminate(png_ptr, TerminalState::FAILURE);
    }

    MOZ_ASSERT(decoder->mImageData, "Should have a buffer now");
    return;  // No yield, so we'll just keep decoding.
  }

  // Yield to the caller to notify them that the previous frame is now complete.
  decoder->mNextFrameInfo = Some(info);
  decoder->DoYield(png_ptr);
}
#endif

void nsPNGDecoder::end_callback(RLBoxSandbox<TRLSandboxP>* sandbox, tainted<png_structp, TRLSandboxP> png_ptr, tainted<png_infop, TRLSandboxP> info_ptr)
{
  /* libpng comments:
   *
   * this function is called when the whole image has been read,
   * including any chunks after the image (up to and including
   * the IEND).  You will usually have the same info chunk as you
   * had in the header, although some data may have been added
   * to the comments and time fields.
   *
   * Most people won't do much here, perhaps setting a flag that
   * marks the image as finished.
   */

  nsPNGDecoder* decoder = static_cast<nsPNGDecoder*>(pngRendererSaved);

  // We shouldn't get here if we've hit an error
  MOZ_ASSERT(!decoder->HasError(), "Finishing up PNG but hit error!");

  decoder->DoTerminate(png_ptr, TerminalState::SUCCESS);
}

void nsPNGDecoder::error_callback(RLBoxSandbox<TRLSandboxP>* sandbox, tainted<png_structp, TRLSandboxP> png_ptr, tainted<png_const_charp, TRLSandboxP> error_msg_unv)
{
  nsPNGDecoder* decoder = static_cast<nsPNGDecoder*>(pngRendererSaved);
  auto rlbox_sandbox = decoder->rlbox_sbx->rlbox_sandbox;
  auto ctx = rlbox_sandbox->getSandbox()->get_context();
  const char* error_msg = "";//error_msg_unv.copyAndVerifyString(rlbox_sandbox, [](const char* val) { return (val != nullptr && strlen(val) < 10000)? RLBox_Verify_Status::SAFE : RLBox_Verify_Status::UNSAFE; }, nullptr);
  MOZ_LOG(sPNGLog, LogLevel::Error, ("libpng error: %s\n", error_msg));
  sandbox_invoke_custom(rlbox_sandbox, ctx, png_longjmp, png_ptr, 1);
}


void nsPNGDecoder::warning_callback(RLBoxSandbox<TRLSandboxP>* sandbox, tainted<png_structp, TRLSandboxP> png_ptr, tainted<png_const_charp, TRLSandboxP> warning_msg_unv)
{
  nsPNGDecoder* decoder = static_cast<nsPNGDecoder*>(pngRendererSaved);
  // auto rlbox_sandbox = decoder->rlbox_sbx->rlbox_sandbox;
  const char* warning_msg = "";//warning_msg_unv.copyAndVerifyString(rlbox_sandbox, [](const char* val) { return (val != nullptr && strlen(val) < 10000)? RLBox_Verify_Status::SAFE : RLBox_Verify_Status::UNSAFE; }, nullptr);
  MOZ_LOG(sPNGLog, LogLevel::Warning, ("libpng warning: %s\n", warning_msg));
}

Maybe<Telemetry::HistogramID>
nsPNGDecoder::SpeedHistogram() const
{
  return Some(Telemetry::IMAGE_DECODE_SPEED_PNG);
}

bool
nsPNGDecoder::IsValidICOResource() const
{
  // Only 32-bit RGBA PNGs are valid ICO resources; see here:
  //   http://blogs.msdn.com/b/oldnewthing/archive/2010/10/22/10079192.aspx

  // If there are errors in the call to png_get_IHDR, the error_callback in
  // nsPNGDecoder.cpp is called.  In this error callback we do a longjmp, so
  // we need to save the jump buffer here. Otherwise we'll end up without a
  // proper callstack.
  if(jumpBufferFilledIn)
  {
    printf("nsPNGDecoder::nsPNGDecoder - jump buffer already in usein IsValidICOResource\n");
    abort();
  }

  pngRendererSaved = (void*) this;

  auto rlbox_sandbox = rlbox_sbx->rlbox_sandbox;
  auto ctx = rlbox_sandbox->getSandbox()->get_context();
  sandbox_invoke_custom(rlbox_sandbox, ctx, png_set_longjmp_fn, mPNG, rlbox_sbx->cpp_cb_longjmp_fn, sizeof(jmp_buf)).UNSAFE_Unverified();
  jumpBufferFilledIn = freshMapId();
  if (setjmp(pngJmpBuffers[jumpBufferFilledIn])) {
    // We got here from a longjmp call indirectly from png_get_IHDR
    return false;
  }

  auto p_params = rlbox_sandbox->mallocInSandbox<png_get_IHDR_params>();

  auto png_get_IHDRRet = sandbox_invoke_custom(rlbox_sandbox, ctx, png_get_IHDR, mPNG, mInfo, &(p_params->width), &(p_params->height), &(p_params->bit_depth), &(p_params->color_type), nullptr, nullptr, nullptr)
    .copyAndVerify([this](png_uint_32 val){
      if(val == 0 || val == 1) { return val; }
      printf("Sbox - png_get_IHDRRet value out of range\n");abort();
    });

  if (png_get_IHDRRet) {
    //don't need any complex verification as the code below checks it anyway
    int png_bit_depth  = (&(p_params->bit_depth)).copyAndVerify([this](int* val) { 
      if(val == nullptr) 
      {
        printf("Sbox - png_bit_depth value null"); abort();
      }
      return *val;
    });
    int png_color_type = (&(p_params->color_type)).copyAndVerify([this](int* val) { 
      if(val == nullptr) 
      {
        printf("Sbox - png_color_type value null"); abort();
      }
      return *val; 
    });

    //clear the jmpBufferIndex on the way out as it is unsafe to call setjmp after this
    jumpBufferFilledIn = 0;
    pngRendererSaved = nullptr;

    rlbox_sandbox->freeInSandbox(p_params);

    return ((png_color_type == PNG_COLOR_TYPE_RGB_ALPHA ||
        png_color_type == PNG_COLOR_TYPE_RGB) &&
      png_bit_depth == 8);
  } else {
    //clear the jmpBufferIndex on the way out as it is unsafe to call setjmp after this
    jumpBufferFilledIn = 0;
    pngRendererSaved = nullptr;
    return false;
  }
}

} // namespace image
} // namespace mozilla

#undef sandbox_invoke_custom