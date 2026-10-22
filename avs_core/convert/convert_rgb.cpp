// Avisynth v2.5.  Copyright 2002 Ben Rudiak-Gould et al.
// http://avisynth.nl

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA, or visit
// http://www.gnu.org/copyleft/gpl.html .
//
// Linking Avisynth statically or dynamically with other modules is making a
// combined work based on Avisynth.  Thus, the terms and conditions of the GNU
// General Public License cover the whole combination.
//
// As a special exception, the copyright holders of Avisynth give you
// permission to link Avisynth with independent modules that communicate with
// Avisynth solely through the interfaces defined in avisynth.h, regardless of the license
// terms of these independent modules, and to copy and distribute the
// resulting combined work under terms of your choice, provided that
// every copy of the combined work is accompanied by a complete copy of
// the source code of Avisynth (the version of Avisynth used to produce the
// combined work), being distributed under the terms of the GNU General
// Public License plus this exception.  An independent module is a module
// which is not derived from or based on Avisynth, such as 3rd-party filters,
// import and export plugins, or graphical user interfaces.


#include "convert_rgb.h"
#ifdef INTEL_INTRINSICS
#include "intel/convert_rgb_sse.h"
#include "intel/convert_rgb_avx2.h"
#ifdef INTEL_INTRINSICS_AVX512
#include "intel/convert_rgb_avx512.h"
#endif
#elif defined(NEON_INTRINSICS)
#include "aarch64/convert_rgb_neon.h"
#endif
#include <avs/alignment.h>


/*************************************
 *******   RGB Helper Classes   ******
 *************************************/

RGBtoRGBA::RGBtoRGBA(PClip src)
  : GenericVideoFilter(src)
{
  vi.pixel_type = src->GetVideoInfo().ComponentSize() == 1 ? VideoInfo::CS_BGR32 : VideoInfo::CS_BGR64;
}

static void convert_rgb24_to_rgb32_c(const BYTE *srcp, BYTE *dstp, size_t src_pitch, size_t dst_pitch, size_t width, size_t height) {
  for (size_t y = height; y > 0; --y) {
    for (size_t x = 0; x < width; ++x) {
      *reinterpret_cast<int*>(dstp + x*4) = *reinterpret_cast<const int*>(srcp+x*3) | 0xFF000000;
    }
    srcp += src_pitch;
    dstp += dst_pitch;
  }
}

static void convert_rgb48_to_rgb64_c(const BYTE *srcp, BYTE *dstp, size_t src_pitch, size_t dst_pitch, size_t width, size_t height) {
  for (size_t y = height; y > 0; --y) {
    for (size_t x = 0; x < width; ++x) {
      *reinterpret_cast<uint64_t*>(dstp + x*8) = *reinterpret_cast<const uint64_t*>(srcp+x*6) | 0xFFFF000000000000ULL;
    }
    srcp += src_pitch;
    dstp += dst_pitch;
  }
}

PVideoFrame __stdcall RGBtoRGBA::GetFrame(int n, IScriptEnvironment* env)
{
  PVideoFrame src = child->GetFrame(n, env);
  PVideoFrame dst = env->NewVideoFrameP(vi, &src);
  const BYTE *srcp = src->GetReadPtr();
  BYTE *dstp = dst->GetWritePtr();
  const int src_pitch = src->GetPitch();
  const int dst_pitch = dst->GetPitch();

  int pixelsize = vi.ComponentSize();

  using conv_fn_t = void(*)(const BYTE*, BYTE*, size_t, size_t, size_t, size_t);
  conv_fn_t fn;

#ifdef INTEL_INTRINSICS
  if (env->GetCPUFlags() & CPUF_SSSE3)
    fn = (pixelsize == 1) ? convert_rgb24_to_rgb32_ssse3 : convert_rgb48_to_rgb64_ssse3;
  else
#elif defined(NEON_INTRINSICS)
  if (env->GetCPUFlags() & CPUF_ARM_NEON)
    fn = (pixelsize == 1) ? convert_rgb24_to_rgb32_neon : convert_rgb48_to_rgb64_neon;
  else
#endif
    fn = (pixelsize == 1) ? convert_rgb24_to_rgb32_c : convert_rgb48_to_rgb64_c;

  fn(srcp, dstp, src_pitch, dst_pitch, vi.width, vi.height);
  return dst;
}




RGBAtoRGB::RGBAtoRGB(PClip src)
: GenericVideoFilter(src)
{
  vi.pixel_type = src->GetVideoInfo().ComponentSize() == 1 ? VideoInfo::CS_BGR24 : VideoInfo::CS_BGR48;
}

static void convert_rgb32_to_rgb24_c(const BYTE *srcp, BYTE *dstp, size_t src_pitch, size_t dst_pitch, size_t width, size_t height) {
  for (size_t y = height; y > 0; --y) {
    size_t x;
    for (x = 0; x < width-1; ++x) {
      *reinterpret_cast<int*>(dstp+x*3) = *reinterpret_cast<const int*>(srcp+x*4);
    }
    //last pixel
    dstp[x*3+0] = srcp[x*4+0];
    dstp[x*3+1] = srcp[x*4+1];
    dstp[x*3+2] = srcp[x*4+2];

    srcp += src_pitch;
    dstp += dst_pitch;
  }
}

static void convert_rgb64_to_rgb48_c(const BYTE *srcp, BYTE *dstp, size_t src_pitch, size_t dst_pitch, size_t width, size_t height) {
  for (size_t y = height; y > 0; --y) {
    size_t x;
    for (x = 0; x < width-1; ++x) { // width-1 really!
      *reinterpret_cast<uint64_t*>(dstp+x*6) = *reinterpret_cast<const uint64_t*>(srcp+x*8);
    }
    //last pixel
    reinterpret_cast<uint16_t*>(dstp)[x*3+0] = reinterpret_cast<const uint16_t*>(srcp)[x*4+0];
    reinterpret_cast<uint16_t*>(dstp)[x*3+1] = reinterpret_cast<const uint16_t*>(srcp)[x*4+1];
    reinterpret_cast<uint16_t*>(dstp)[x*3+2] = reinterpret_cast<const uint16_t*>(srcp)[x*4+2];

    srcp += src_pitch;
    dstp += dst_pitch;
  }
}

PVideoFrame __stdcall RGBAtoRGB::GetFrame(int n, IScriptEnvironment* env)
{
  PVideoFrame src = child->GetFrame(n, env);
  PVideoFrame dst = env->NewVideoFrameP(vi, &src);
  const BYTE *srcp = src->GetReadPtr();
  BYTE *dstp = dst->GetWritePtr();
  size_t src_pitch = src->GetPitch();
  size_t dst_pitch = dst->GetPitch();

  int pixelsize = vi.ComponentSize();

  using conv_fn_t = void(*)(const BYTE*, BYTE*, size_t, size_t, size_t, size_t);
  conv_fn_t fn = nullptr;

#ifdef INTEL_INTRINSICS
  if (env->GetCPUFlags() & CPUF_SSSE3)
    fn = (pixelsize == 1) ? convert_rgb32_to_rgb24_ssse3 : convert_rgb64_to_rgb48_ssse3;
  else if ((pixelsize == 1) && (env->GetCPUFlags() & CPUF_SSE2))
    fn = convert_rgb32_to_rgb24_sse2;
  else
#elif defined(NEON_INTRINSICS)
  if (env->GetCPUFlags() & CPUF_ARM_NEON)
    fn = (pixelsize == 1) ? convert_rgb32_to_rgb24_neon : convert_rgb64_to_rgb48_neon;
  else
#endif
    fn = (pixelsize == 1) ? convert_rgb32_to_rgb24_c : convert_rgb64_to_rgb48_c;

  fn(srcp, dstp, src_pitch, dst_pitch, vi.width, vi.height);

  return dst;
}

PackedRGBtoPlanarRGB::PackedRGBtoPlanarRGB(PClip src, bool _sourceHasAlpha, bool _targetHasAlpha)
  : GenericVideoFilter(src), sourceHasAlpha(_sourceHasAlpha), targetHasAlpha(_targetHasAlpha)
{
  vi.pixel_type = src->GetVideoInfo().ComponentSize() == 1 ?
    (targetHasAlpha ? VideoInfo::CS_RGBAP : VideoInfo::CS_RGBP) :
    (targetHasAlpha ? VideoInfo::CS_RGBAP16 : VideoInfo::CS_RGBP16);
}

template<typename pixel_t, int src_numcomponents, bool targetHasAlpha>
static void convert_rgb_to_rgbp_c(const BYTE* srcp, BYTE* (&dstp)[4], int src_pitch, int(&dst_pitch)[4], int width, int height) {
  constexpr int bits_per_pixel = sizeof(pixel_t) == 1 ? 8 : 16;
  constexpr int max_pixel_value = (1 << bits_per_pixel) - 1;
  // Local plane pointers: written through dstp[] directly, an 8 bit (char) store may alias the pointer
  // array itself, the pointers were reloaded per pixel and the loop was not vectorized.
  BYTE* dstp_g = dstp[0];
  BYTE* dstp_b = dstp[1];
  BYTE* dstp_r = dstp[2];
  BYTE* dstp_a = targetHasAlpha ? dstp[3] : nullptr;
  for (int y = height; y > 0; --y) {
    const pixel_t* s = reinterpret_cast<const pixel_t*>(srcp);
    pixel_t* g = reinterpret_cast<pixel_t*>(dstp_g);
    pixel_t* b = reinterpret_cast<pixel_t*>(dstp_b);
    pixel_t* r = reinterpret_cast<pixel_t*>(dstp_r);
    pixel_t* a = reinterpret_cast<pixel_t*>(dstp_a);
    int x;
    for (x = 0; x < width; ++x) {
      pixel_t B = s[x * src_numcomponents + 0];
      pixel_t G = s[x * src_numcomponents + 1];
      pixel_t R = s[x * src_numcomponents + 2];
      pixel_t A = 0;
      if constexpr (targetHasAlpha)
        A = (src_numcomponents == 4) ? s[x * src_numcomponents + 3] : max_pixel_value;
      g[x] = G;
      b[x] = B;
      r[x] = R;
      if constexpr (targetHasAlpha)
        a[x] = A;
    }

    srcp -= src_pitch; // source packed RGB is upside down
    dstp_g += dst_pitch[0];
    dstp_b += dst_pitch[1];
    dstp_r += dst_pitch[2];
    if constexpr (targetHasAlpha)
      dstp_a += dst_pitch[3];
  }
}

PVideoFrame __stdcall PackedRGBtoPlanarRGB::GetFrame(int n, IScriptEnvironment* env)
{
  PVideoFrame src = child->GetFrame(n, env);
  PVideoFrame dst = env->NewVideoFrameP(vi, &src);
  int src_pitch = src->GetPitch();
  const BYTE* srcp = src->GetReadPtr();
  BYTE* dstp[4] = {
    dst->GetWritePtr(PLANAR_G), dst->GetWritePtr(PLANAR_B),
    dst->GetWritePtr(PLANAR_R), dst->GetWritePtr(PLANAR_A)
  };
  int dst_pitch[4] = {
    dst->GetPitch(PLANAR_G), dst->GetPitch(PLANAR_B),
    dst->GetPitch(PLANAR_R), dst->GetPitch(PLANAR_A)
  };
  int pixelsize = vi.ComponentSize();
  srcp += src_pitch * (vi.height - 1); // start from bottom: packed RGB is upside down

  using conv_fn_t = void(*)(const BYTE*, BYTE* (&)[4], int, int(&)[4], int, int);
  conv_fn_t fn = nullptr;

  const bool targetHasAlpha = vi.IsPlanarRGBA();

#ifdef INTEL_INTRINSICS
  auto CPU = env->GetCPUFlagsEx();
  const bool hasSSE2 = (CPU & CPUF_SSE2) != 0;
  const bool hasSSSE3 = (CPU & CPUF_SSSE3) != 0;
  const bool hasAVX2 = (CPU & CPUF_AVX2) != 0;
#ifdef INTEL_INTRINSICS_AVX512
  //const bool has_AVX512_base = (CPU & CPUF_AVX512_BASE) == CPUF_AVX512_BASE; // group flag!
  const bool has_AVX512_fast = (CPU & CPUF_AVX512_FAST) == CPUF_AVX512_FAST; // group flag!
#endif
#endif

  // for RGBA there is no width limit, we do full scanlines, Avisynth's 64 byte alignment allows
  // processing 16 RGB32 pixels or 8 RGB64 pixels at a time
  // RGB is tricky, remainder handling is needed.

  if (pixelsize == 1) {
    if (sourceHasAlpha) {
      // RGB32->
#ifdef INTEL_INTRINSICS
#ifdef INTEL_INTRINSICS_AVX512
      if (has_AVX512_fast)  fn = targetHasAlpha ? convert_rgba_to_rgbp_avx512vbmi<uint8_t, true> : convert_rgba_to_rgbp_avx512vbmi<uint8_t, false>;
      else
#endif
      if (hasAVX2)  fn = targetHasAlpha ? convert_rgba_to_rgbp_avx2<uint8_t, true> : convert_rgba_to_rgbp_avx2<uint8_t, false>;
      else if (hasSSSE3)  fn = targetHasAlpha ? convert_rgba_to_rgbp_ssse3<uint8_t, true> : convert_rgba_to_rgbp_ssse3<uint8_t, false>;
      else if (hasSSE2)  fn = targetHasAlpha ? convert_rgba_to_rgbp_sse2 <uint8_t, true> : convert_rgba_to_rgbp_sse2 <uint8_t, false>;
      else
#endif
        fn = targetHasAlpha ? convert_rgb_to_rgbp_c<uint8_t, 4, true> : convert_rgb_to_rgbp_c<uint8_t, 4, false>;
    }
    else {
      // RGB24->
#ifdef INTEL_INTRINSICS
      // RGB24->RGB(A)P8, works with 48byte blocks (16xRGB), min width is 16 (SSSE3, 32 (AVX2), width constraint is due to remainder handling, not alignment)
      if (hasAVX2 && vi.width >= 32) fn = targetHasAlpha ? convert_rgb_to_rgbp_avx2 <uint8_t, true> : convert_rgb_to_rgbp_avx2 <uint8_t, false>;
      else if (hasSSSE3 && vi.width >= 16) fn = targetHasAlpha ? convert_rgb_to_rgbp_ssse3<uint8_t, true> : convert_rgb_to_rgbp_ssse3<uint8_t, false>;
      else if (hasSSE2 && vi.width >= 16) fn = targetHasAlpha ? convert_rgb_to_rgbp_sse2 <uint8_t, true> : convert_rgb_to_rgbp_sse2 <uint8_t, false>;
      else
#endif
        fn = targetHasAlpha ? convert_rgb_to_rgbp_c<uint8_t, 3, true> : convert_rgb_to_rgbp_c<uint8_t, 3, false>;
    }
  }
  else { // pixelsize == 2
    if (sourceHasAlpha) {
      // RGB64->
#ifdef INTEL_INTRINSICS
#ifdef INTEL_INTRINSICS_AVX512
      if (has_AVX512_fast)  fn = targetHasAlpha ? convert_rgba_to_rgbp_avx512vbmi<uint16_t, true> : convert_rgba_to_rgbp_avx512vbmi<uint16_t, false>;
      else
#endif
      if (hasAVX2)  fn = targetHasAlpha ? convert_rgba_to_rgbp_avx2<uint16_t, true> : convert_rgba_to_rgbp_avx2<uint16_t, false>;
      else if (hasSSSE3)  fn = targetHasAlpha ? convert_rgba_to_rgbp_ssse3<uint16_t, true> : convert_rgba_to_rgbp_ssse3<uint16_t, false>;
      else if (hasSSE2)  fn = targetHasAlpha ? convert_rgba_to_rgbp_sse2 <uint16_t, true> : convert_rgba_to_rgbp_sse2 <uint16_t, false>;
      else
#endif
        fn = targetHasAlpha ? convert_rgb_to_rgbp_c<uint16_t, 4, true> : convert_rgb_to_rgbp_c<uint16_t, 4, false>;
    }
    else {
      // RGB48->
      // width constraint is due to remainder handling, not alignment, AVX2 needs 16 pixels (48 bytes) to process, SSE2/SSSE3 need 8 pixels (48 bytes) to process
#ifdef INTEL_INTRINSICS
      if (hasAVX2 && vi.width >= 16) fn = targetHasAlpha ? convert_rgb_to_rgbp_avx2 <uint16_t, true> : convert_rgb_to_rgbp_avx2 <uint16_t, false>;
      else if (hasSSSE3 && vi.width >= 8)  fn = targetHasAlpha ? convert_rgb_to_rgbp_ssse3<uint16_t, true> : convert_rgb_to_rgbp_ssse3<uint16_t, false>;
      else if (hasSSE2 && vi.width >= 8)  fn = targetHasAlpha ? convert_rgb_to_rgbp_sse2 <uint16_t, true> : convert_rgb_to_rgbp_sse2 <uint16_t, false>;
      else
#endif
        fn = targetHasAlpha ? convert_rgb_to_rgbp_c<uint16_t, 3, true> : convert_rgb_to_rgbp_c<uint16_t, 3, false>;
    }
  }

  fn(srcp, dstp, src_pitch, dst_pitch, vi.width, vi.height);
  return dst;
}

PlanarRGBtoPackedRGB::PlanarRGBtoPackedRGB(PClip src, bool _targetHasAlpha)
  : GenericVideoFilter(src), targetHasAlpha(_targetHasAlpha)
{
  vi.pixel_type = src->GetVideoInfo().ComponentSize() == 1 ?
    (targetHasAlpha ? VideoInfo::CS_BGR32 : VideoInfo::CS_BGR24) : // PlanarRGB(A)->RGB24/32
    (targetHasAlpha ? VideoInfo::CS_BGR64 : VideoInfo::CS_BGR48);  // PlanarRGB(A)->RGB48/64
}

template<typename pixel_t, int target_numcomponents, bool hasSrcAlpha>
static void convert_rgbp_to_rgb_c(const BYTE *(&srcp)[4], BYTE * dstp, int (&src_pitch)[4], int dst_pitch, int width, int height) {

  constexpr int bits_per_pixel = sizeof(pixel_t) == 1 ? 8 : 16;
  constexpr int max_pixel_value = (1 << bits_per_pixel) - 1;

  // Local plane pointers: read through srcp[] directly, an 8 bit (char) store may alias the pointer
  // array itself, the pointers were reloaded per pixel and the loop was not vectorized.
  const BYTE* srcp_g = srcp[0];
  const BYTE* srcp_b = srcp[1];
  const BYTE* srcp_r = srcp[2];
  const BYTE* srcp_a = hasSrcAlpha ? srcp[3] : nullptr;
  for (int y = 0; y < height; y++) {
    const pixel_t* g = reinterpret_cast<const pixel_t*>(srcp_g);
    const pixel_t* b = reinterpret_cast<const pixel_t*>(srcp_b);
    const pixel_t* r = reinterpret_cast<const pixel_t*>(srcp_r);
    const pixel_t* a = reinterpret_cast<const pixel_t*>(srcp_a);
    pixel_t* d = reinterpret_cast<pixel_t*>(dstp);
    int x;
    for (x = 0; x < width; ++x) {
      const pixel_t G = g[x];
      const pixel_t B = b[x];
      const pixel_t R = r[x];
      d[x*target_numcomponents+0] = B;
      d[x*target_numcomponents+1] = G;
      d[x*target_numcomponents+2] = R;
      if constexpr (target_numcomponents == 4) { // either from A channel or default transparent constant
        pixel_t A;
        if constexpr (hasSrcAlpha)
          A = a[x];
        else
          A = max_pixel_value; // 255/65535
        d[x*target_numcomponents + 3] = A;
      }
    }

    dstp -= dst_pitch; // source packed RGB is upside down
    srcp_g += src_pitch[0];
    srcp_b += src_pitch[1];
    srcp_r += src_pitch[2];
    if constexpr (hasSrcAlpha)
      srcp_a += src_pitch[3];
  }
}

PVideoFrame __stdcall PlanarRGBtoPackedRGB::GetFrame(int n, IScriptEnvironment* env)
{
  PVideoFrame src = child->GetFrame(n, env);
  PVideoFrame dst = env->NewVideoFrameP(vi, &src);
  int dst_pitch = dst->GetPitch();
  BYTE* dstp = dst->GetWritePtr();
  const BYTE* srcp[4] = { src->GetReadPtr(PLANAR_G), src->GetReadPtr(PLANAR_B),
                         src->GetReadPtr(PLANAR_R), src->GetReadPtr(PLANAR_A) };
  int src_pitch[4] = { src->GetPitch(PLANAR_G), src->GetPitch(PLANAR_B),
                      src->GetPitch(PLANAR_R), src->GetPitch(PLANAR_A) };
  int pixelsize = vi.ComponentSize();
  dstp += dst_pitch * (vi.height - 1); // start from bottom: packed RGB is upside down

  const bool hasSrcAlpha = (src_pitch[3] != 0); // planar RGB_A_ source
  const bool hasTargetAlpha = (vi.NumComponents() == 4); // RGB32 or RGB64 target

  using conv_fn_t = void(*)(const BYTE* (&)[4], BYTE*, int(&)[4], int, int, int);
  conv_fn_t fn = nullptr;

#ifdef INTEL_INTRINSICS
  const bool hasAVX2 = (env->GetCPUFlags() & CPUF_AVX2) != 0;
  const bool hasSSE2 = (env->GetCPUFlags() & CPUF_SSE2) != 0;
#endif

  if (pixelsize == 1) {
    if (!hasTargetAlpha) {  // RGB24 — no SIMD path exists
      fn = hasSrcAlpha ? convert_rgbp_to_rgb_c<uint8_t, 3, true> : convert_rgbp_to_rgb_c<uint8_t, 3, false>;
    }
    else {  // RGBA32
#ifdef INTEL_INTRINSICS
      if (hasAVX2)
        fn = hasSrcAlpha ? convert_rgbp_to_rgba_avx2<uint8_t, true> : convert_rgbp_to_rgba_avx2<uint8_t, false>;
      else if (hasSSE2)
        fn = hasSrcAlpha ? convert_rgbp_to_rgba_sse2<uint8_t, true> : convert_rgbp_to_rgba_sse2<uint8_t, false>;
      else
#endif
        fn = hasSrcAlpha ? convert_rgbp_to_rgb_c<uint8_t, 4, true> : convert_rgbp_to_rgb_c<uint8_t, 4, false>;
    }
  }
  else {  // pixelsize == 2
    if (!hasTargetAlpha) {  // RGB48 — no SIMD path exists
      fn = hasSrcAlpha ? convert_rgbp_to_rgb_c<uint16_t, 3, true> : convert_rgbp_to_rgb_c<uint16_t, 3, false>;
    }
    else {  // RGBA64
#ifdef INTEL_INTRINSICS
      if (hasAVX2)
        fn = hasSrcAlpha ? convert_rgbp_to_rgba_avx2<uint16_t, true> : convert_rgbp_to_rgba_avx2<uint16_t, false>;
      else if (hasSSE2)
        fn = hasSrcAlpha ? convert_rgbp_to_rgba_sse2<uint16_t, true> : convert_rgbp_to_rgba_sse2<uint16_t, false>;
      else
#endif
        fn = hasSrcAlpha ? convert_rgbp_to_rgb_c<uint16_t, 4, true> : convert_rgbp_to_rgb_c<uint16_t, 4, false>;
    }
  }

  fn(srcp, dstp, src_pitch, dst_pitch, vi.width, vi.height);
  return dst;
}
