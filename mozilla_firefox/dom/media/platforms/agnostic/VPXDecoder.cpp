/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VPXDecoder.h"
#include "TimeUnits.h"
#include "gfx2DGlue.h"
#include "mozilla/PodOperations.h"
#include "mozilla/SyncRunnable.h"
#include "ImageContainer.h"
#include "nsError.h"
#include "prsystem.h"

#include <algorithm>
#include <unistd.h>
#include <sys/syscall.h>

#undef LOG
#define LOG(arg, ...) MOZ_LOG(sPDMLog, mozilla::LogLevel::Debug, ("VPXDecoder(%p)::%s: " arg, this, __func__, ##__VA_ARGS__))

#define sandbox_invoke_custom(sandbox, fnName, ...) sandbox_invoke_custom_helper_vpx(sandbox, (decltype(fnName)*)sandbox->getFunctionPointerFromCache(#fnName, false), ##__VA_ARGS__)

template<typename TFunc, typename... TArgs>
inline typename std::enable_if<!std::is_void<return_argument<TFunc>>::value,
tainted<return_argument<TFunc>, TRLSandboxV>
>::type sandbox_invoke_custom_helper_vpx(RLBoxSandbox<TRLSandboxV>* sandbox, TFunc* fnPtr, TArgs&&... params)
{
  // //vpxStartTimer();
  auto ret = sandbox->invokeWithFunctionPointer(fnPtr, params...);
  // //vpxEndTimer();
  return ret;
}

template<typename TFunc, typename... TArgs>
inline typename std::enable_if<std::is_void<return_argument<TFunc>>::value,
void
>::type sandbox_invoke_custom_helper_vpx(RLBoxSandbox<TRLSandboxV>* sandbox, TFunc* fnPtr, TArgs&&... params)
{
  // //vpxStartTimer();
  sandbox->invokeWithFunctionPointer(fnPtr, params...);
  // //vpxEndTimer();
}

rlbox_load_library_api(vpxlib, TRLSandboxV)

namespace mozilla {

using namespace gfx;
using namespace layers;

static inline std::string getLibraryPath(const std::string &lib) {
  char buf[PATH_MAX];
  ssize_t len = readlink("/proc/self/exe", buf, PATH_MAX);
  if (len == -1) {
    return "";
  }
  std::string path(buf, len);
  size_t pos = path.rfind('/');
  if (pos == std::string::npos) {
    return "";
  }
  path.resize(pos + 1);
  return path + lib;
}

static VPXDecoder::Codec MimeTypeToCodec(const nsACString& aMimeType)
{
  if (aMimeType.EqualsLiteral("video/vp8")) {
    return VPXDecoder::Codec::VP8;
  } else if (aMimeType.EqualsLiteral("video/vp9")) {
    return VPXDecoder::Codec::VP9;
  }
  return VPXDecoder::Codec::Unknown;
}

static nsresult
InitContext(RLBoxSandbox<TRLSandboxV>* rlbox_vpx,
            tainted<vpx_codec_ctx_t*, TRLSandboxV> aCtx,
            const VideoInfo& aInfo,
            const VPXDecoder::Codec aCodec)
{
  int decode_threads = 2;
  tainted<vpx_codec_iface_t*, TRLSandboxV> dx = nullptr;
  if (aCodec == VPXDecoder::Codec::VP8) {
    dx = sandbox_invoke_custom(rlbox_vpx, vpx_codec_vp8_dx);
  }
  else if (aCodec == VPXDecoder::Codec::VP9) {
    dx = sandbox_invoke_custom(rlbox_vpx, vpx_codec_vp9_dx);
    if (aInfo.mDisplay.width >= 2048) {
      decode_threads = 8;
    }
    else if (aInfo.mDisplay.width >= 1024) {
      decode_threads = 4;
    }
  }
  decode_threads = std::min(decode_threads, PR_GetNumberOfProcessors());

  tainted<vpx_codec_dec_cfg_t*, TRLSandboxV> p_config = rlbox_vpx->mallocInSandbox<vpx_codec_dec_cfg_t>();
  p_config->threads = decode_threads;
  p_config->w = p_config->h = 0; // set after decode

  // Unsafe is fine here as worst case, we are just gracefully failing
  if (dx == nullptr || sandbox_invoke_custom(rlbox_vpx, vpx_codec_dec_init_ver, aCtx, dx, p_config, 0, VPX_DECODER_ABI_VERSION).UNSAFE_Unverified()) {
    return NS_ERROR_FAILURE;
  }
  rlbox_vpx->freeInSandbox(p_config);
  return NS_OK;
}

static RLBoxSandbox<TRLSandboxV>* createVPXSandbox() {
  static const std::vector<unsigned int> AllowedSyscalls {__NR_exit, __NR_futex, __NR_sched_yield};
  const std::string &libPath = getLibraryPath("libvpx.so");
  auto rlbox_sandbox = RLBoxSandbox<TRLSandboxV>::createSandbox("", libPath.c_str());
  if (!rlbox_sandbox->getSandbox()->init_seccomp(AllowedSyscalls)) {
    fprintf(stderr, "failed to init seccomp for VPX sandbox\n");
    exit(1);
  }
  return rlbox_sandbox;
}

VPXDecoder::VPXDecoder(const CreateDecoderParams& aParams)
  : mImageContainer(aParams.mImageContainer)
  , mImageAllocator(aParams.mKnowsCompositor)
  , mTaskQueue(aParams.mTaskQueue)
  , mInfo(aParams.VideoConfig())
  , mCodec(MimeTypeToCodec(aParams.VideoConfig().mMimeType))
{
  MOZ_COUNT_CTOR(VPXDecoder);
  uswitch_sandbox_semaphore.wait();
  rlbox_vpx = createVPXSandbox();

  p_mVPX = rlbox_vpx->mallocInSandbox<vpx_codec_ctx_t>();
  p_mVPXAlpha = rlbox_vpx->mallocInSandbox<vpx_codec_ctx_t>();
  // Unsafe is fine here as this is basically a memset
  PodZero(p_mVPX.UNSAFE_Unverified());
  PodZero(p_mVPXAlpha.UNSAFE_Unverified());
}

VPXDecoder::~VPXDecoder()
{
  rlbox_vpx->freeInSandbox(p_mVPX);
  rlbox_vpx->freeInSandbox(p_mVPXAlpha);
  printf("Destroying Vpx Sandbox\n");
  rlbox_vpx->destroySandbox();
  uswitch_sandbox_semaphore.signal();
  rlbox_vpx = nullptr;
  MOZ_COUNT_DTOR(VPXDecoder);
  if (vpxDecodeInvocations > 5) {
    double decode_frame_rate = ((double)1000000000)*vpxDecodeInvocations/timeBetweenVpxDecode;
    double max_single_thread_decode_frame_rate = ((double)1000000000)*vpxDecodeInvocations/timeSpentInVpxDecode;
    printf("Capture_Time:VPX_decode_frame_rate,0,%lf|\n", decode_frame_rate);
    printf("Capture_Time:VPX_max_single_thread_decode_frame_rate,0,%lf|\n", max_single_thread_decode_frame_rate);
  }
}

RefPtr<ShutdownPromise>
VPXDecoder::Shutdown()
{
  RefPtr<VPXDecoder> self = this;
  return InvokeAsync(mTaskQueue, __func__, [self, this]() {
    if (rlbox_vpx) {
      sandbox_invoke_custom(rlbox_vpx, vpx_codec_destroy, p_mVPX);
      sandbox_invoke_custom(rlbox_vpx, vpx_codec_destroy, p_mVPXAlpha);
    }
    return ShutdownPromise::CreateAndResolve(true, __func__);
  });
}

RefPtr<MediaDataDecoder::InitPromise>
VPXDecoder::Init()
{
  if (NS_FAILED(InitContext(rlbox_vpx, p_mVPX, mInfo, mCodec))) {
    return VPXDecoder::InitPromise::CreateAndReject(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                                                    __func__);
  }
  if (mInfo.HasAlpha()) {
    if (NS_FAILED(InitContext(rlbox_vpx, p_mVPXAlpha, mInfo, mCodec))) {
      return VPXDecoder::InitPromise::CreateAndReject(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                                                      __func__);
    }
  }  
  return VPXDecoder::InitPromise::CreateAndResolve(TrackInfo::kVideoTrack,
                                                   __func__);
}

RefPtr<MediaDataDecoder::FlushPromise>
VPXDecoder::Flush()
{
  return InvokeAsync(mTaskQueue, __func__, []() {
    return FlushPromise::CreateAndResolve(true, __func__);
  });
}

RefPtr<MediaDataDecoder::DecodePromise>
VPXDecoder::ProcessDecode(MediaRawData* aSample)
{
  auto now = duration_cast<nanoseconds>(high_resolution_clock::now().time_since_epoch()).count();
  if (vpxDecodeInvocations != 0)
  {
    timeBetweenVpxDecode += now - previousVpxDecodeCall;
  }

  previousVpxDecodeCall = now;
  vpxDecodeInvocations++;

  MOZ_ASSERT(mTaskQueue->IsCurrentThreadIn());

#if defined(DEBUG)
  NS_ASSERTION(IsKeyframe(rlbox_vpx, *aSample, mCodec) == aSample->mKeyframe,
               "VPX Decode Keyframe error sample->mKeyframe and sample data out of sync");
#endif

  const unsigned char* aData = aSample->Data();
  size_t aLength = aSample->Size();

  tainted<unsigned char*, TRLSandboxV> buff_copy = rlbox_vpx->mallocInSandbox<unsigned char>(aLength);
  //unsafe is fine for memcpy
  memcpy(buff_copy.UNSAFE_Unverified(), aData, aLength);
  //unsafe is fine as an incorrect value would cause us to fail gracefully
  vpx_codec_err_t r = sandbox_invoke_custom(rlbox_vpx, vpx_codec_decode, p_mVPX, buff_copy, aLength, nullptr, 0).UNSAFE_Unverified();

  if (r) {
    rlbox_vpx->freeInSandbox(buff_copy);

    const char * str = "";
    LOG("VPX Decode error: %s", str);
    return DecodePromise::CreateAndReject(
      MediaResult(NS_ERROR_DOM_MEDIA_DECODE_ERR,
                  RESULT_DETAIL("VPX error: %s", str)),
      __func__);
  }

  tainted<vpx_codec_iter_t*, TRLSandboxV> p_iter = rlbox_vpx->mallocInSandbox<vpx_codec_iter_t>();
  *p_iter = nullptr;
  tainted<vpx_image_t *, TRLSandboxV> img;
  tainted<vpx_image_t *, TRLSandboxV> img_alpha = nullptr;
  bool alpha_decoded = false;
  DecodedData results;

  int frames = 0;
  for (;; ++frames) {
    img = sandbox_invoke_custom(rlbox_vpx, vpx_codec_get_frame, p_mVPX, p_iter);
    if (img == nullptr) {
      break;
    }

    // safe as it used only as an early sanity check
    auto fmt = img->fmt.UNSAFE_Unverified();
    NS_ASSERTION(fmt == VPX_IMG_FMT_I420 ||
                 fmt == VPX_IMG_FMT_I444,
                 "WebM image format not I420 or I444");
    NS_ASSERTION(!alpha_decoded,
                 "Multiple frames per packet that contains alpha");

    if (aSample->AlphaSize() > 0) {
      if (!alpha_decoded){
        MediaResult rv = DecodeAlpha(&img_alpha, aSample);
        if (NS_FAILED(rv)) {
          return DecodePromise::CreateAndReject(rv, __func__);
        }
        alpha_decoded = true;
      }
    }
    // Chroma shifts are rounded down as per the decoding examples in the SDK
    VideoData::YCbCrBuffer b;
    //Checked in dom/media/MediaData.cpp ValidatePlane, so this is fine
    auto d_w = img->d_w.UNSAFE_Unverified();
    auto d_h = img->d_h.UNSAFE_Unverified();

    // This is fine as this is just a buffer of pixels
    b.mPlanes[0].mData = img->planes[0].UNSAFE_Unverified();
    // This is being checked in dom/media/MediaData.cpp ValidateBufferAndPicture, so no further check needed here
    b.mPlanes[0].mStride = img->stride[0].UNSAFE_Unverified();
    b.mPlanes[0].mHeight = d_h;
    b.mPlanes[0].mWidth = d_w;
    b.mPlanes[0].mOffset = b.mPlanes[0].mSkip = 0;

    // This is fine as this is just a buffer of pixels
    b.mPlanes[1].mData = img->planes[1].UNSAFE_Unverified();
    // This is being checked in dom/media/MediaData.cpp ValidateBufferAndPicture, so no further check needed here
    b.mPlanes[1].mStride = img->stride[1].UNSAFE_Unverified();
    b.mPlanes[1].mOffset = b.mPlanes[1].mSkip = 0;

    // This is fine as this is just a buffer of pixels
    b.mPlanes[2].mData = img->planes[2].UNSAFE_Unverified();
    // This is being checked in dom/media/MediaData.cpp ValidateBufferAndPicture, so no further check needed here
    b.mPlanes[2].mStride = img->stride[2].UNSAFE_Unverified();
    b.mPlanes[2].mOffset = b.mPlanes[2].mSkip = 0;

    if (fmt == VPX_IMG_FMT_I420) {
      // chroma_shifts should be 0 or 1 media/libvpx/libvpx/test/y4m_test.cc - HeaderChecks
      auto y_chroma_shift = img->y_chroma_shift.copyAndVerify([](unsigned int val){
        if(val == 0 || val == 1) { return val; }
        abort();
      });
      auto x_chroma_shift = img->x_chroma_shift.copyAndVerify([](unsigned int val){
        if(val == 0 || val == 1) { return val; }
        abort();
      });
      b.mPlanes[1].mHeight = (d_h + 1) >> y_chroma_shift;
      b.mPlanes[1].mWidth = (d_w + 1) >> x_chroma_shift;

      b.mPlanes[2].mHeight = (d_h + 1) >> y_chroma_shift;
      b.mPlanes[2].mWidth = (d_w + 1) >> x_chroma_shift;
    } else if (fmt == VPX_IMG_FMT_I444) {
      b.mPlanes[1].mHeight = d_h;
      b.mPlanes[1].mWidth = d_w;

      b.mPlanes[2].mHeight = d_h;
      b.mPlanes[2].mWidth = d_w;
    } else {
      LOG("VPX Unknown image format");
      return DecodePromise::CreateAndReject(
        MediaResult(NS_ERROR_DOM_MEDIA_DECODE_ERR,
                    RESULT_DETAIL("VPX Unknown image format")),
        __func__);
    }

    RefPtr<VideoData> v;
    if (!img_alpha) {
      v = VideoData::CreateAndCopyData(mInfo,
                                       mImageContainer,
                                       aSample->mOffset,
                                       aSample->mTime,
                                       aSample->mDuration,
                                       b,
                                       aSample->mKeyframe,
                                       aSample->mTimecode,
                                       mInfo.ScaledImageRect(d_w,
                                                             d_h),
                                       mImageAllocator);
    } else {
      VideoData::YCbCrBuffer::Plane alpha_plane;
      // This is fine as this is just a buffer of pixels
      alpha_plane.mData = img_alpha->planes[0].UNSAFE_Unverified();
      // This is being checked in dom/media/MediaData.cpp ValidateBufferAndPicture, so no further check needed here
      alpha_plane.mStride = img_alpha->stride[0].UNSAFE_Unverified();
      //Checked in dom/media/MediaData.cpp ValidatePlane, so this is fine
      alpha_plane.mHeight = img_alpha->d_h.UNSAFE_Unverified();
      alpha_plane.mWidth = img_alpha->d_w.UNSAFE_Unverified();
      alpha_plane.mOffset = alpha_plane.mSkip = 0;
      v = VideoData::CreateAndCopyData(mInfo,
                                       mImageContainer,
                                       aSample->mOffset,
                                       aSample->mTime,
                                       aSample->mDuration,
                                       b,
                                       alpha_plane,
                                       aSample->mKeyframe,
                                       aSample->mTimecode,
                                       mInfo.ScaledImageRect(d_w,
                                                             d_h));

    }

    if (!v) {
      LOG(
        "Image allocation error source %ux%u display %ux%u picture %ux%u",
        d_w, d_h, mInfo.mDisplay.width, mInfo.mDisplay.height,
        mInfo.mImage.width, mInfo.mImage.height);
      return DecodePromise::CreateAndReject(
        MediaResult(NS_ERROR_OUT_OF_MEMORY, __func__), __func__);
    }
    results.AppendElement(Move(v));
  }

  rlbox_vpx->freeInSandbox(buff_copy);

  rlbox_vpx->freeInSandbox(p_iter);

  auto after_decode_time = duration_cast<nanoseconds>(high_resolution_clock::now().time_since_epoch()).count();
  timeSpentInVpxDecode += after_decode_time - now;
  
  double decode_frame_rate = ((double)1000000000)*vpxDecodeInvocations/timeBetweenVpxDecode;
  double max_single_thread_decode_frame_rate = ((double)1000000000)*vpxDecodeInvocations/timeSpentInVpxDecode;
  printf("Capture_Time:VPX_decode_frame_rate,0,%lf|\n", decode_frame_rate);
  printf("Capture_Time:VPX_max_single_thread_decode_frame_rate,0,%lf|\n", max_single_thread_decode_frame_rate);
  printf("Capture_Time:VPX_decode_frame_time,0,%lf\n", timeSpentInVpxDecode / ((double)1000000) / vpxDecodeInvocations);

  return DecodePromise::CreateAndResolve(Move(results), __func__);
}

RefPtr<MediaDataDecoder::DecodePromise>
VPXDecoder::Decode(MediaRawData* aSample)
{
  return InvokeAsync<MediaRawData*>(mTaskQueue, this, __func__,
                                    &VPXDecoder::ProcessDecode, aSample);
}

RefPtr<MediaDataDecoder::DecodePromise>
VPXDecoder::Drain()
{
  return InvokeAsync(mTaskQueue, __func__, [] {
    return DecodePromise::CreateAndResolve(DecodedData(), __func__);
  });
}

MediaResult
VPXDecoder::DecodeAlpha(tainted<vpx_image_t *, TRLSandboxV>* aImgAlpha, const MediaRawData* aSample)
{
  const unsigned char* aData = aSample->AlphaData();
  size_t aLength = aSample->AlphaSize();

  tainted<unsigned char*, TRLSandboxV> buff_copy = rlbox_vpx->mallocInSandbox<unsigned char>(aLength);
  //unsafe is fine memcpy
  memcpy(buff_copy.UNSAFE_Unverified(), aData, aLength);

  //unsafe is fine as an incorrect value would cause us to fail gracefully
  vpx_codec_err_t r = sandbox_invoke_custom(rlbox_vpx, vpx_codec_decode,
                                       p_mVPXAlpha,
                                       buff_copy,
                                       aLength,
                                       nullptr,
                                       0).UNSAFE_Unverified();
  if (r) {
    const char * str = "";
    LOG("VPX decode alpha error: %s", str);
    return MediaResult(
      NS_ERROR_DOM_MEDIA_DECODE_ERR,
      RESULT_DETAIL("VPX decode alpha error: %s", vpx_codec_err_to_string(r)));
  }

  tainted<vpx_codec_iter_t*, TRLSandboxV> p_iter = rlbox_vpx->mallocInSandbox<vpx_codec_iter_t>();
  *p_iter = nullptr;

  *aImgAlpha = sandbox_invoke_custom(rlbox_vpx, vpx_codec_get_frame, p_mVPXAlpha, p_iter);
  // Safe as this is an early sanity check
  vpx_img_fmt_t fmt = (*aImgAlpha)->fmt.UNSAFE_Unverified();
  NS_ASSERTION(fmt == VPX_IMG_FMT_I420 ||
               fmt == VPX_IMG_FMT_I444,
               "WebM image format not I420 or I444");

  rlbox_vpx->freeInSandbox(p_iter);

  rlbox_vpx->freeInSandbox(buff_copy);

  return NS_OK;
}

/* static */
bool
VPXDecoder::IsVPX(const nsACString& aMimeType, uint8_t aCodecMask)
{
  return ((aCodecMask & VPXDecoder::VP8) &&
          aMimeType.EqualsLiteral("video/vp8")) ||
         ((aCodecMask & VPXDecoder::VP9) &&
          aMimeType.EqualsLiteral("video/vp9"));
}

/* static */
bool
VPXDecoder::IsVP8(const nsACString& aMimeType)
{
  return IsVPX(aMimeType, VPXDecoder::VP8);
}

/* static */
bool
VPXDecoder::IsVP9(const nsACString& aMimeType)
{
  return IsVPX(aMimeType, VPXDecoder::VP9);
}

RLBoxSandbox<TRLSandboxV>* VPXDecoder::getKeyframeSandbox() {
  static RLBoxSandbox<TRLSandboxV>* keyframeSandbox = createVPXSandbox();
  return keyframeSandbox;
}

/* static */
bool
VPXDecoder::IsKeyframe(RLBoxSandbox<TRLSandboxV>* rlbox_vpx, Span<const uint8_t> aBuffer, Codec aCodec)
{
  auto p_si = rlbox_vpx->mallocInSandbox<vpx_codec_stream_info_t>();
  // Safe as this is just a memset
  PodZero(p_si.UNSAFE_Unverified());
  p_si->sz = sizeof(*p_si);

  auto aData = aBuffer.Elements();
  auto aLength = aBuffer.Length();
  tainted<uint8_t*, TRLSandboxV> buff_copy = rlbox_vpx->mallocInSandbox<uint8_t>(aLength);
  //unsafe is fine memcpy
  memcpy(buff_copy.UNSAFE_Unverified(), aData, aLength);

  bool retBool = false;
  if (aCodec == Codec::VP8) {
    tainted<vpx_codec_iface_t *, TRLSandboxV> ret = sandbox_invoke_custom(rlbox_vpx, vpx_codec_vp8_dx);
    sandbox_invoke_custom(rlbox_vpx, vpx_codec_peek_stream_info, ret, buff_copy, aLength, p_si);
    retBool = bool(p_si->is_kf.UNSAFE_Unverified(/* TODO */));
  } else if (aCodec == Codec::VP9) {
    tainted<vpx_codec_iface_t *, TRLSandboxV> ret = sandbox_invoke_custom(rlbox_vpx, vpx_codec_vp9_dx);
    sandbox_invoke_custom(rlbox_vpx, vpx_codec_peek_stream_info, ret, buff_copy, aLength, p_si);
    retBool = bool(p_si->is_kf.UNSAFE_Unverified(/* TODO */));
  }

  rlbox_vpx->freeInSandbox(p_si);
  rlbox_vpx->freeInSandbox(buff_copy);

  return retBool;
}

/* static */
gfx::IntSize
VPXDecoder::GetFrameSize(Span<const uint8_t> aBuffer, Codec aCodec)
{
  auto rlbox_vpx = getKeyframeSandbox();
  auto p_si = rlbox_vpx->mallocInSandbox<vpx_codec_stream_info_t>();
  // Safe as this is just a memset
  PodZero(p_si.UNSAFE_Unverified());
  p_si->sz = sizeof(*p_si);

  auto aData = aBuffer.Elements();
  auto aLength = aBuffer.Length();
  tainted<uint8_t*, TRLSandboxV> buff_copy = rlbox_vpx->mallocInSandbox<uint8_t>(aLength);
  //unsafe is fine memcpy
  memcpy(buff_copy.UNSAFE_Unverified(), aData, aLength);

  if (aCodec == Codec::VP8) {
    tainted<vpx_codec_iface_t *, TRLSandboxV> ret = sandbox_invoke_custom(rlbox_vpx, vpx_codec_vp8_dx);
    sandbox_invoke_custom(rlbox_vpx, vpx_codec_peek_stream_info, ret, buff_copy, aLength, p_si);
  } else if (aCodec == Codec::VP9) {
    tainted<vpx_codec_iface_t *, TRLSandboxV> ret = sandbox_invoke_custom(rlbox_vpx, vpx_codec_vp9_dx);
    sandbox_invoke_custom(rlbox_vpx, vpx_codec_peek_stream_info, ret, buff_copy, aLength, p_si);
  }
  auto retSize = gfx::IntSize(p_si->w.UNSAFE_Unverified(), p_si->h.UNSAFE_Unverified());
  rlbox_vpx->freeInSandbox(p_si);
  rlbox_vpx->freeInSandbox(buff_copy);
  return retSize;
}

} // namespace mozilla
#undef LOG

#undef sandbox_invoke_custom