/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_image_decoders_nsPNGDecoder_h
#define mozilla_image_decoders_nsPNGDecoder_h

#include "Decoder.h"
#include "png.h"
#include "qcms.h"
#include "StreamingLexer.h"
#include "SurfacePipe.h"

#include <chrono>
#include <atomic>
#include <unordered_map>
#include <mutex>
#include <memory>
#include <unistd.h>
#include <sys/syscall.h>

#include "uswitchsandbox.h"
#include "uswitch.hpp"
#include "RLBox_USwitch.h"
#include "rlbox.h"
#include "pnglib_structs_for_cpp_api_new.h"

using TRLSandboxP = RLBox_USwitch;
using namespace rlbox;

using namespace std::chrono;

struct RLBench
{
  bool InUse = false;
  bool StartCalled = false;
  unsigned long long ElapsedTime = 0;
  high_resolution_clock::time_point StartTime;

  inline void Init()
  {
    if(InUse == true) {
      printf("!!!!!!!Bench already in use!!!!!!!!!!\n");
      abort();
    }
    InUse = true;
    StartCalled = false;
    ElapsedTime = 0;
  }

  inline void Start()
  {
    if(StartCalled) {
      printf("!!!!!!!Start already called!!!!!!!!!!\n");
      abort();
    }
    StartCalled = true;
    StartTime = high_resolution_clock::now();
  }

  inline void StartIfNeeded()
  {
    if(!StartCalled) {
      StartCalled = true;
      StartTime = high_resolution_clock::now();
    }
  }

  inline void Stop()
  {
    if(!StartCalled) {
      printf("!!!!!!!Start not called!!!!!!!!!!\n");
      abort();
    }
    StartCalled = false;
    auto diff = duration_cast<nanoseconds>(high_resolution_clock::now() - StartTime).count();
    ElapsedTime += diff;
  }

  inline unsigned long long JustFinish()
  {
    auto ret = ElapsedTime;
    ElapsedTime = 0;
    InUse = false;
    return ret;
  }

  inline unsigned long long StopAndFinish()
  {
    Stop();
    return JustFinish();
  }
};

namespace mozilla {
namespace image {

inline std::string getImageURIString(RasterImage* aImage)
{
  ImageURL* imageURI = nullptr;

  //Try to retrieve the image URI from the ImageDecoder request
  if(aImage != nullptr) { 
    imageURI = aImage->GetURI();
  }

  //if still null bail out - empty string causes the use of a temporary sandbox
  if(imageURI == nullptr) { return ""; }

  nsCString spec;
  imageURI->GetSpec(spec);
  std::string ret = spec.get();
  return ret;
}

inline std::string getHostStringFromImage(RasterImage* aImage)
{
  ImageURL* imageURI = nullptr;

  //Try to retrieve the image URI from the ImageDecoder request
  if(aImage != nullptr) { 
    imageURI = aImage->GetURI();
  }

  //if still null bail out - empty string causes the use of a temporary sandbox
  if(imageURI == nullptr) { return ""; }

  //if a normal scheme like https, construct the origin
  {
    nsCString scheme;
    nsresult rv;
    rv = imageURI->GetScheme(scheme);
    if(NS_FAILED(rv)){
      return "";
    }
    nsCString host;
    rv = imageURI->GetHost(host);
    if(NS_FAILED(rv)){
      return "";
    }
    std::string hostStr = scheme.get();
    hostStr += "://";
    hostStr += host.get();
    int port = imageURI->GetPort();
    if(port != -1) {
      hostStr += ":";
      hostStr += std::to_string(port);
    }
    return hostStr;
  }
}

class RasterImage;

class PNGSandboxResource;

class nsPNGDecoder : public Decoder
{
public:
  virtual ~nsPNGDecoder();

  /// @return true if this PNG is a valid ICO resource.
  bool IsValidICOResource() const override;

protected:
  nsresult InitInternal() override;
  nsresult FinishInternal() override;
  LexerResult DoDecode(SourceBufferIterator& aIterator,
                       IResumable* aOnResume) override;

  Maybe<Telemetry::HistogramID> SpeedHistogram() const override;

private:
  friend class DecoderFactory;

  // Decoders should only be instantiated via DecoderFactory.
  explicit nsPNGDecoder(RasterImage* aImage, RasterImage* aImageExtra);

  /// The information necessary to create a frame.
  struct FrameInfo
  {
    gfx::IntRect mFrameRect;
    bool mIsInterlaced;
  };

  nsresult CreateFrame(const FrameInfo& aFrameInfo);
  void EndImageFrame();

  bool HasAlphaChannel() const
  {
    return mChannels == 2 || mChannels == 4;
  }

  enum class TransparencyType
  {
    eNone,
    eAlpha,
    eFrameRect
  };

  TransparencyType GetTransparencyType(const gfx::IntRect& aFrameRect);
  void PostHasTransparencyIfNeeded(TransparencyType aTransparencyType);

  void PostInvalidationIfNeeded();

  void WriteRow(uint8_t* aRow);

  // Convenience methods to make interacting with StreamingLexer from inside
  // a libpng callback easier.
  void DoTerminate(tainted<png_structp, TRLSandboxP> aPNGStruct, TerminalState aState);
  void DoYield(tainted<png_structp, TRLSandboxP> aPNGStruct);

  enum class State
  {
    PNG_DATA,
    FINISHED_PNG_DATA
  };

  LexerTransition<State> ReadPNGData(const char* aData, size_t aLength);
  LexerTransition<State> FinishedPNGData();

  StreamingLexer<State> mLexer;

  // The next lexer state transition. We need to store it here because we can't
  // directly return arbitrary values from libpng callbacks.
  LexerTransition<State> mNextTransition;

  // We yield to the caller every time we finish decoding a frame. When this
  // happens, we need to allocate the next frame after returning from the yield.
  // |mNextFrameInfo| is used to store the information needed to allocate the
  // next frame.
  Maybe<FrameInfo> mNextFrameInfo;

  // The length of the last chunk of data passed to ReadPNGData(). We use this
  // to arrange to arrive back at the correct spot in the data after yielding.
  size_t mLastChunkLength;

public:
  std::string mImageString;
  std::shared_ptr<PNGSandboxResource> rlbox_sbx_shared;
  PNGSandboxResource* rlbox_sbx;
  tainted<png_structp, TRLSandboxP> mPNG;
  tainted<png_infop, TRLSandboxP> mInfo;
  nsIntRect mFrameRect;
  uint8_t* mCMSLine;
  uint8_t* interlacebuf;
  qcms_profile* mInProfile;
  qcms_transform* mTransform;
  gfx::SurfaceFormat mFormat;

  // whether CMS or premultiplied alpha are forced off
  uint32_t mCMSMode;

  uint8_t mChannels;
  uint8_t mPass;
  bool mFrameIsHidden;
  bool mDisablePremultipliedAlpha;

  struct AnimFrameInfo
  {
    AnimFrameInfo();
#ifdef PNG_APNG_SUPPORTED
    AnimFrameInfo(RLBoxSandbox<TRLSandboxP>* rlbox_png, tainted<png_structp, TRLSandboxP> aPNG, tainted<png_infop, TRLSandboxP> aInfo);
#endif

    DisposalMethod mDispose;
    BlendMethod mBlend;
    int32_t mTimeout;
  };

  AnimFrameInfo mAnimInfo;

  SurfacePipe mPipe;  /// The SurfacePipe used to write to the output surface.

  // The number of frames we've finished.
  uint32_t mNumFrames;

  RLBench PngBench;
  bool PngMaybeTooSmall;

  // libpng callbacks
  // We put these in the class so that they can access protected members.
  static void PNGAPI info_callback(RLBoxSandbox<TRLSandboxP>* sandbox, tainted<png_structp, TRLSandboxP> png_ptr, tainted<png_infop, TRLSandboxP> info_ptr);
  static void PNGAPI row_callback(RLBoxSandbox<TRLSandboxP>* sandbox, tainted<png_structp, TRLSandboxP> png_ptr, tainted<png_bytep, TRLSandboxP> new_row, tainted<png_uint_32, TRLSandboxP> row_num, tainted<int, TRLSandboxP> pass);
#ifdef PNG_APNG_SUPPORTED
  static void PNGAPI frame_info_callback(RLBoxSandbox<TRLSandboxP>* sandbox, tainted<png_structp, TRLSandboxP> png_ptr, tainted<png_uint_32, TRLSandboxP> frame_num);
#endif
  static void PNGAPI end_callback(RLBoxSandbox<TRLSandboxP>* sandbox, tainted<png_structp, TRLSandboxP> png_ptr, tainted<png_infop, TRLSandboxP> info_ptr);
  static void PNGAPI error_callback(RLBoxSandbox<TRLSandboxP>* sandbox, tainted<png_structp, TRLSandboxP> png_ptr, tainted<png_const_charp, TRLSandboxP> error_msg);
  static void PNGAPI warning_callback(RLBoxSandbox<TRLSandboxP>* sandbox, tainted<png_structp, TRLSandboxP> png_ptr, tainted<png_const_charp, TRLSandboxP> warning_msg);
  static void PNGAPI checked_longjmp(RLBoxSandbox<TRLSandboxP>* sandbox, tainted<jmp_buf, TRLSandboxP> unv_env, tainted<int, TRLSandboxP> unv_status);

  // This is defined in the PNG spec as an invariant. We use it to
  // do manual validation without libpng.
  static const uint8_t pngSignatureBytes[];
};

} // namespace image
} // namespace mozilla

#endif // mozilla_image_decoders_nsPNGDecoder_h
