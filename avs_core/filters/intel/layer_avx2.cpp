// AviSynth+.  Copyright 2026- AviSynth+ Project
// https://avs-plus.net
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

#include "../layer.h"
#include "layer_avx2.h"

#if defined(_MSC_VER)
#include <intrin.h> // MSVC
#else 
#include <x86intrin.h> // GCC/MinGW/Clang/LLVM
#endif
#include <immintrin.h>

#include <cstdint>
#include <vector>

#include <avs/minmax.h>
#include <avs/alignment.h>
#include "../core/internal.h"

// masked_merge_avx2_impl<maskMode> — implementation-include, no guards.
#include "../overlay/intel/masked_merge_avx2_impl.hpp"
#include "../overlay/intel/blend_common_avx2.h"

#include "../convert/convert_planar.h"
#include <algorithm>
#include <vector>
#include <type_traits>

// Mostly RGB32 stuff, unaligned addresses, pixels grouped by 4

static AVS_FORCEINLINE __m128i mask_core_avx2(__m128i& src, __m128i& alpha, __m128i& not_alpha_mask, __m128i& zero, __m128i& matrix, __m128i& round_mask) {
  __m128i not_alpha = _mm_and_si128(src, not_alpha_mask);

  __m128i pixel0 = _mm_unpacklo_epi8(alpha, zero);
  __m128i pixel1 = _mm_unpackhi_epi8(alpha, zero);

  pixel0 = _mm_madd_epi16(pixel0, matrix);
  pixel1 = _mm_madd_epi16(pixel1, matrix);

  __m128i tmp = _mm_castps_si128(_mm_shuffle_ps(_mm_castsi128_ps(pixel0), _mm_castsi128_ps(pixel1), _MM_SHUFFLE(3, 1, 3, 1)));
  __m128i tmp2 = _mm_castps_si128(_mm_shuffle_ps(_mm_castsi128_ps(pixel0), _mm_castsi128_ps(pixel1), _MM_SHUFFLE(2, 0, 2, 0)));

  tmp = _mm_add_epi32(tmp, tmp2);
  tmp = _mm_add_epi32(tmp, round_mask);
  tmp = _mm_srli_epi32(tmp, 15);
  __m128i result_alpha = _mm_slli_epi32(tmp, 24);

  return _mm_or_si128(result_alpha, not_alpha);
}

// called for RGB32
void mask_avx2(BYTE* srcp, const BYTE* alphap, int src_pitch, int alpha_pitch, size_t width, size_t height) {
  __m128i matrix = _mm_set_epi16(0, cyr, cyg, cyb, 0, cyr, cyg, cyb);
  __m128i zero = _mm_setzero_si128();
  __m128i round_mask = _mm_set1_epi32(16384);
  __m128i not_alpha_mask = _mm_set1_epi32(0x00FFFFFF);

  size_t width_bytes = width * 4;
  size_t width_mod16 = width_bytes / 16 * 16;

  for (size_t y = 0; y < height; ++y) {
    for (size_t x = 0; x < width_mod16; x += 16) {
      __m128i src = _mm_load_si128(reinterpret_cast<const __m128i*>(srcp + x));
      __m128i alpha = _mm_load_si128(reinterpret_cast<const __m128i*>(alphap + x));
      __m128i result = mask_core_avx2(src, alpha, not_alpha_mask, zero, matrix, round_mask);

      _mm_store_si128(reinterpret_cast<__m128i*>(srcp + x), result);
    }

    if (width_mod16 < width_bytes) {
      __m128i src = _mm_loadu_si128(reinterpret_cast<const __m128i*>(srcp + width_bytes - 16));
      __m128i alpha = _mm_loadu_si128(reinterpret_cast<const __m128i*>(alphap + width_bytes - 16));
      __m128i result = mask_core_avx2(src, alpha, not_alpha_mask, zero, matrix, round_mask);

      _mm_storeu_si128(reinterpret_cast<__m128i*>(srcp + width_bytes - 16), result);
    }

    srcp += src_pitch;
    alphap += alpha_pitch;
  }
}

void colorkeymask_avx2(BYTE* pf, int pitch, int color, int height, int width, int tolB, int tolG, int tolR) {
  unsigned int t = 0xFF000000 | (tolR << 16) | (tolG << 8) | tolB;
  __m128i tolerance = _mm_set1_epi32(t);
  __m128i colorv = _mm_set1_epi32(color);
  __m128i zero = _mm_setzero_si128();

  BYTE* endp = pf + pitch * height;

  while (pf < endp)
  {
    __m128i src = _mm_load_si128(reinterpret_cast<const __m128i*>(pf));
    __m128i gt = _mm_subs_epu8(colorv, src);
    __m128i lt = _mm_subs_epu8(src, colorv);
    __m128i absdiff = _mm_or_si128(gt, lt); //abs(color - src)

    __m128i not_passed = _mm_subs_epu8(absdiff, tolerance);
    __m128i passed = _mm_cmpeq_epi32(not_passed, zero);
    passed = _mm_slli_epi32(passed, 24);
    __m128i result = _mm_andnot_si128(passed, src);

    _mm_store_si128(reinterpret_cast<__m128i*>(pf), result);

    pf += 16;
  }
}

// by 4 bytes, when rgba mask can be separate FF bytes, for plane FF FF FF FF
// to simple, even C is identical speed
void invert_frame_inplace_avx2(BYTE* frame, int pitch, int width, int height, int mask) {
  __m256i maskv = _mm256_set1_epi32(mask);

  BYTE* endp = frame + pitch * height;
  // geee, no y loop
  while (frame < endp) {
    __m256i src = _mm256_load_si256(reinterpret_cast<const __m256i*>(frame));
    __m256i inv = _mm256_xor_si256(src, maskv);
    _mm256_store_si256(reinterpret_cast<__m256i*>(frame), inv);
    frame += 32;
  }
}

// to simple, even C is identical speed
void invert_frame_uint16_inplace_avx2(BYTE* frame, int pitch, int width, int height, uint64_t mask64) {
  __m256i maskv = _mm256_set_epi32(
    (uint32_t)(mask64 >> 32), (uint32_t)mask64, (uint32_t)(mask64 >> 32), (uint32_t)mask64,
    (uint32_t)(mask64 >> 32), (uint32_t)mask64, (uint32_t)(mask64 >> 32), (uint32_t)mask64);

  BYTE* endp = frame + pitch * height;
  // geee, no y loop
  while (frame < endp) {
    __m256i src = _mm256_load_si256(reinterpret_cast<const __m256i*>(frame));
    __m256i inv = _mm256_xor_si256(src, maskv);
    _mm256_store_si256(reinterpret_cast<__m256i*>(frame), inv);
    frame += 32;
  }
}

// ---------------------------------------------------------------------------
// invert_plane_avx2_u8: 8-bit luma/chroma inversion.
//   Luma:   result = 255 - src   (XOR with 0xFF)
//   Chroma: result = min(256 - src, 255)
//             = XOR(saturating_sub(src, 1), 0xFF)
//             reason: 256-src overflows uint8 for src=0 → clamp to 255;
//             all other src values: 256-src ∈ [1,255] fits exactly.
// ---------------------------------------------------------------------------
template<bool chroma>
void invert_plane_avx2_u8(uint8_t* dstp, const uint8_t* srcp, int src_pitch, int dst_pitch, int width, int height, int /*bits_per_pixel*/)
{
  const __m256i v_ff  = _mm256_set1_epi8((char)0xFF);
  const __m256i v_one = _mm256_set1_epi8(1);

  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; x += 32) {
      __m256i s = _mm256_load_si256(reinterpret_cast<const __m256i*>(srcp + x));
      __m256i r;
      if constexpr (chroma)
        r = _mm256_xor_si256(_mm256_subs_epu8(s, v_one), v_ff);
      else
        r = _mm256_xor_si256(s, v_ff);
      _mm256_store_si256(reinterpret_cast<__m256i*>(dstp + x), r);
    }
    srcp += src_pitch;
    dstp += dst_pitch;
  }
}

// ---------------------------------------------------------------------------
// invert_plane_avx2_u16: 10/12/14/16-bit luma/chroma inversion.
//   Luma:   result = max_pixel_value - src
//             = XOR(src, v_max)  (works because max = (1<<bpp)-1 = all ones in bpp bits)
//   Chroma: result = min(2*half - src, max) = min((1<<bpp) - src, max)
//             = XOR(saturating_sub(src, 1), v_max)
//             same overflow-at-zero reasoning as u8; saturating_sub_u16 exists in AVX2.
// ---------------------------------------------------------------------------
template<bool chroma>
void invert_plane_avx2_u16(uint8_t* dstp, const uint8_t* srcp, int src_pitch, int dst_pitch, int width, int height, int bits_per_pixel)
{
  const int max_pixel_value      = (1 << bits_per_pixel) - 1;
  const __m256i v_max = _mm256_set1_epi16((short)max_pixel_value);
  const __m256i v_one = _mm256_set1_epi16(1);

  for (int y = 0; y < height; ++y) {
    const uint16_t* s16 = reinterpret_cast<const uint16_t*>(srcp);
          uint16_t* d16 = reinterpret_cast<      uint16_t*>(dstp);
    
    for (int x = 0; x < width; x += 16) {
      __m256i s = _mm256_load_si256(reinterpret_cast<const __m256i*>(s16 + x));
      __m256i r;
      if constexpr (chroma)
        r = _mm256_xor_si256(_mm256_subs_epu16(s, v_one), v_max);
      else
        r = _mm256_xor_si256(s, v_max);
      _mm256_store_si256(reinterpret_cast<__m256i*>(d16 + x), r);
    }
    srcp += src_pitch;
    dstp += dst_pitch;
  }
}

// ---------------------------------------------------------------------------
// invert_plane_avx2_f32: 32-bit float luma/chroma inversion.
//   Luma:   result = 1.0f - src   (sub from 1.0)
//   Chroma: result = -src          (flip sign bit via XOR)
// ---------------------------------------------------------------------------
template<bool chroma>
void invert_plane_avx2_f32(uint8_t* dstp, const uint8_t* srcp, int src_pitch, int dst_pitch, int width, int height, int /*bits_per_pixel*/)
{
  const __m256 v_one  = _mm256_set1_ps(1.0f);
  const __m256 v_sign = _mm256_set1_ps(-0.0f); // 0x80000000 — sign-flip mask

  for (int y = 0; y < height; ++y) {
    const float* sf = reinterpret_cast<const float*>(srcp);
          float* df = reinterpret_cast<      float*>(dstp);
    
    for (int x = 0; x < width; x += 8) {
      __m256 s = _mm256_load_ps(sf + x);
      __m256 r;
      if constexpr (chroma)
        r = _mm256_xor_ps(s, v_sign);
      else
        r = _mm256_sub_ps(v_one, s);
      _mm256_store_ps(df + x, r);
    }
    srcp += src_pitch;
    dstp += dst_pitch;
  }
}

template void invert_plane_avx2_u8<false>(uint8_t*, const uint8_t*, int, int, int, int, int);
template void invert_plane_avx2_u8<true> (uint8_t*, const uint8_t*, int, int, int, int, int);
template void invert_plane_avx2_u16<false>(uint8_t*, const uint8_t*, int, int, int, int, int);
template void invert_plane_avx2_u16<true> (uint8_t*, const uint8_t*, int, int, int, int, int);
template void invert_plane_avx2_f32<false>(uint8_t*, const uint8_t*, int, int, int, int, int);
template void invert_plane_avx2_f32<true> (uint8_t*, const uint8_t*, int, int, int, int, int);


/*******************************
 *******   Layer Filter   ******
 *******************************/

 // chroma placement to mask helpers
 // yuv add, subtract, mul, lighten, darken
 // included in base and the avx2 source module, to get different optimizations
 // Use AVX2 SIMD rowprep variants — masked_rowprep_avx2.hpp is already included above
 // via masked_merge_avx2_impl.hpp.  Both prepare_effective_mask_for_row_avx2 and
 // prepare_effective_mask_for_row_level_baked_avx2 are therefore visible here.
#define LAYER_ROWPREP_FN       prepare_effective_mask_for_row_avx2
#include "../layer.hpp"
#undef LAYER_ROWPREP_FN


// Wrapper function that calls the local scoped get_layer_yuv_mul_functions
// This ensures the AVX2-compiled versions of the functions are selected
void get_layer_yuv_mul_functions_avx2(
  bool is_chroma, bool hasAlpha,
  int placement, VideoInfo& vi, int bits_per_pixel,
  /*out*/layer_yuv_mul_c_t** layer_fn,
  /*out*/layer_yuv_mul_f_c_t** layer_f_fn)
{
  get_layer_yuv_mul_functions(is_chroma, hasAlpha, placement, vi, bits_per_pixel, layer_fn, layer_f_fn);
}

// ---------------------------------------------------------------------------
// AVX2 Layer add dispatcher.
// ---------------------------------------------------------------------------
void get_layer_yuv_masked_add_functions_avx2(
  bool is_chroma, 
  int placement, VideoInfo& vi, int bits_per_pixel,
  /*out*/masked_merge_fn_t** layer_fn,
  /*out*/masked_merge_float_fn_t** layer_f_fn)
{
  // only masked merge is left here, simple weighted blend is already handled

  // Use the unified (Layer,Overlay) masked merge functions
  MaskMode maskMode = is_chroma ? resolveChromaMaskMode(placement, vi) : MASK444;
  *layer_fn = get_overlay_blend_masked_fn_avx2(is_chroma, maskMode);
  *layer_f_fn = get_overlay_blend_masked_float_fn_avx2(is_chroma, maskMode);
}


// AVX2 lighten/darken dispatcher.
void get_layer_planarrgb_lighten_darken_functions_avx2(bool isLighten, bool hasAlpha, bool blendAlpha, int bits_per_pixel, /*out*/layer_planarrgb_lighten_darken_c_t** layer_fn, /*out*/layer_planarrgb_lighten_darken_f_c_t** layer_f_fn) {
  // FIXME: add AVX2 see layer_rgb32_lighten_darken_avx2, but use then opacity_i, so
  // look into the planar_rgb c implementation
  get_layer_planarrgb_lighten_darken_functions(isLighten, hasAlpha, blendAlpha, bits_per_pixel, layer_fn, layer_f_fn);
}


// Planar RGB add — AVX2 per-plane wrappers.
// All planar RGB planes are at full luma resolution (MASK444).
// maskp8 is the per-pixel blend weight (overlay A, or saved pre-Subtract A from mask_child).
// ovrp8[i] is the blend target for each colour plane; for blend_alpha, ovrp8[3] is the alpha target.
// Subtract is handled by pre-inverting the overlay in Layer::Create.
// chroma=false (blend-toward-neutral luma) and float fall back to C templates.

static void layer_planarrgb_add_avx2_3plane(
  BYTE** dstp8, const BYTE** ovrp8, const BYTE* maskp8,
  int dst_pitch, int overlay_pitch, int mask_pitch,
  int width, int height, int opacity_i, int bits_per_pixel)
{
  for (int i = 0; i < 3; i++)
    masked_merge_avx2_impl<MASK444>(
      dstp8[i], ovrp8[i], maskp8,
      dst_pitch, overlay_pitch, mask_pitch,
      width, height, opacity_i, bits_per_pixel);
}

static void layer_planarrgb_add_avx2_4plane(
  BYTE** dstp8, const BYTE** ovrp8, const BYTE* maskp8,
  int dst_pitch, int overlay_pitch, int mask_pitch,
  int width, int height, int opacity_i, int bits_per_pixel)
{
  for (int i = 0; i < 4; i++)
    masked_merge_avx2_impl<MASK444>(
      dstp8[i], ovrp8[i], maskp8,
      dst_pitch, overlay_pitch, mask_pitch,
      width, height, opacity_i, bits_per_pixel);
}

// In layer_avx2.cpp — subtract handled by pre-inverted overlay in Layer::Create.
void get_layer_planarrgb_add_functions_avx2(
  bool chroma, bool hasAlpha, bool blendAlpha, int bits_per_pixel,
  /*out*/layer_planarrgb_add_c_t** layer_fn,
  /*out*/layer_planarrgb_add_f_c_t** layer_f_fn)
{
  // chroma is true: Layer can use the unified masked and weighted blend routines
  // chroma is false: Layer-specific extension
  // Integer + hasAlpha + chroma=true: dispatch per-plane to masked_merge_avx2_impl (MASK444).
  // chroma=false (blend toward luma) has a different formula — keep C template.
  // float: keep C template (float perf is usually fine; could add later).
  if (chroma && bits_per_pixel != 32) {
    if (hasAlpha) {
      *layer_fn = blendAlpha ? layer_planarrgb_add_avx2_4plane : layer_planarrgb_add_avx2_3plane;
      return;
    }
    // no alpha: standard weighted merge, to be added later.
  }
  get_layer_planarrgb_add_functions(chroma, hasAlpha, blendAlpha, bits_per_pixel, layer_fn, layer_f_fn);
}


void get_layer_planarrgb_mul_functions_avx2(
  bool chroma, bool hasAlpha, bool blendAlpha, int bits_per_pixel,
  /*out*/layer_planarrgb_mul_c_t** layer_fn,
  /*out*/layer_planarrgb_mul_f_c_t** layer_f_fn)
{
    get_layer_planarrgb_mul_functions(chroma, hasAlpha, blendAlpha, bits_per_pixel, layer_fn, layer_f_fn);
}

// ---------------------------------------------------------------------------
// Packed RGBA (RGB32) magic-div blend — AVX2, 8-bit only.
//
// Processes 8 BGRA pixels (32 bytes) per iteration.
//
// Per-pixel blend weight source (compile-time):
//   has_separate_mask=false → alpha from ovr[x*4+3]  (Add: overlay's own alpha)
//   has_separate_mask=true  → alpha from maskp8[x]   (Subtract: original alpha
//                              extracted before pre-inverting overlay in Create)
//
// Per-channel result: (dst * (255-aeff) + ovr * aeff + 127) / 255
// For Subtract, ovr is already pre-inverted (max - original) by Create().
//
// ÷255 is implemented as  mulhi_epu16(x, 0x8081) >> 7  — the standard
// "magic multiply" for dividing 16-bit values by 255 (exact for [0..65280]).
// All intermediate sums stay within uint16 because:
//   alpha_eff in [0..255], inv = 255-alpha_eff
//   dst*inv + ovr*alpha ≤ 255*(inv+alpha) = 255*255 = 65025 < 65536
//   + half(127) → max 65152 < 65536
//
// 16-bit pixels (RGB64) fall through to the C reference via the dispatcher.
// ---------------------------------------------------------------------------
template<bool has_separate_mask>
static void masked_blend_packedrgba_avx2_u8(
  BYTE* dstp8, const BYTE* ovrp8, const BYTE* maskp8,
  int dst_pitch, int ovr_pitch, int mask_pitch,
  int width, int height, int opacity_i)
{
  // Shuffle mask: replicate byte 3 (A) of each BGRA pixel across all 4 bytes.
  // AVX2 _mm256_shuffle_epi8 operates independently in each 128-bit lane,
  // so lanes 0 and 1 use identical 16-byte shuffle patterns.
  const __m256i shuf_alpha_bgra = _mm256_set_epi8(
    15,15,15,15, 11,11,11,11,  7, 7, 7, 7,  3, 3, 3, 3,   // lane 1 (pixels 4-7)
    15,15,15,15, 11,11,11,11,  7, 7, 7, 7,  3, 3, 3, 3);  // lane 0 (pixels 0-3)
  // Shuffle for separate mask: 8 bytes m0..m7 → each byte broadcast to 4 positions.
  // Applies within each 128-bit lane independently:
  //   lane 0: bytes 0-7 of mask → pixels 0-3 of lane (m0,m0,m0,m0, m1,m1,m1,m1, ...)
  //   lane 1: bytes 0-7 of mask → pixels 4-7 of lane (m4,m4,m4,m4, m5,m5,m5,m5, ...)
  const __m256i shuf_alpha_mask = _mm256_set_epi8(
     3, 3, 3, 3,  2, 2, 2, 2,  1, 1, 1, 1,  0, 0, 0, 0,  // lane 1: mask bytes 4-7
     3, 3, 3, 3,  2, 2, 2, 2,  1, 1, 1, 1,  0, 0, 0, 0); // lane 0: mask bytes 0-3

  const __m256i v_opacity = _mm256_set1_epi16((short)opacity_i);
  const __m256i v_half    = _mm256_set1_epi16(127);
  const __m256i v_max     = _mm256_set1_epi16(255);
  const __m256i v_magic   = _mm256_set1_epi16((short)0x8081u); // magic for ÷255

  // magic ÷255: mulhi_epu16(x, 0x8081) >> 7  ≡  (x * 32897) >> 23
  auto div255 = [&](__m256i x) -> __m256i {
    return _mm256_srli_epi16(_mm256_mulhi_epu16(x, v_magic), 7);
  };

  const int mod8_width = width / 8 * 8;

  for (int y = 0; y < height; ++y) {
    int x = 0;
    for (; x < mod8_width; x += 8) {
      __m256i dst8 = _mm256_loadu_si256((const __m256i*)(dstp8 + x * 4));
      __m256i ovr8 = _mm256_loadu_si256((const __m256i*)(ovrp8 + x * 4));

      // Broadcast per-pixel alpha to all 4 channel bytes within each pixel.
      __m256i alpha_bcast;
      if constexpr (has_separate_mask) {
        // Load 8 mask bytes (one per pixel), distribute across two 128-bit lanes.
        __m128i mask8b = _mm_loadl_epi64((const __m128i*)(maskp8 + x)); // lo 8 bytes
        // Place bytes 0-3 in lane 0 and bytes 4-7 in lane 1 of a 256-bit register.
        __m256i mask_wide = _mm256_inserti128_si256(
          _mm256_castsi128_si256(mask8b),
          _mm_srli_si128(mask8b, 4), 1);
        alpha_bcast = _mm256_shuffle_epi8(mask_wide, shuf_alpha_mask);
      } else {
        alpha_bcast = _mm256_shuffle_epi8(ovr8, shuf_alpha_bgra);
      }

      // Expand all channels to 16-bit (two 256-bit registers: lo=px0-3, hi=px4-7).
      __m256i dst_lo  = _mm256_cvtepu8_epi16(_mm256_castsi256_si128(dst8));
      __m256i dst_hi  = _mm256_cvtepu8_epi16(_mm256_extracti128_si256(dst8, 1));
      __m256i ovr_lo  = _mm256_cvtepu8_epi16(_mm256_castsi256_si128(ovr8));
      __m256i ovr_hi  = _mm256_cvtepu8_epi16(_mm256_extracti128_si256(ovr8, 1));
      __m256i a_lo    = _mm256_cvtepu8_epi16(_mm256_castsi256_si128(alpha_bcast));
      __m256i a_hi    = _mm256_cvtepu8_epi16(_mm256_extracti128_si256(alpha_bcast, 1));

      // alpha_eff = (alpha * opacity_i + 127) / 255
      __m256i aeff_lo = div255(_mm256_add_epi16(_mm256_mullo_epi16(a_lo, v_opacity), v_half));
      __m256i aeff_hi = div255(_mm256_add_epi16(_mm256_mullo_epi16(a_hi, v_opacity), v_half));
      __m256i inv_lo  = _mm256_sub_epi16(v_max, aeff_lo);
      __m256i inv_hi  = _mm256_sub_epi16(v_max, aeff_hi);

      // b = ovr (overlay pre-inverted for Subtract; plain for Add)
      const __m256i b_lo = ovr_lo;
      const __m256i b_hi = ovr_hi;

      // result = (dst * inv + b * aeff + 127) / 255
      __m256i res_lo = div255(_mm256_add_epi16(
        _mm256_add_epi16(_mm256_mullo_epi16(dst_lo, inv_lo),
                         _mm256_mullo_epi16(b_lo,   aeff_lo)), v_half));
      __m256i res_hi = div255(_mm256_add_epi16(
        _mm256_add_epi16(_mm256_mullo_epi16(dst_hi, inv_hi),
                         _mm256_mullo_epi16(b_hi,   aeff_hi)), v_half));

      // Pack 16→8-bit.  _mm256_packus_epi16 interleaves lanes: [p0p1 p4p5 | p2p3 p6p7].
      // _mm256_permute4x64_epi64(..., 0xD8) reorders 64-bit groups [0,2,1,3] to [p0-p7].
      __m256i result = _mm256_permute4x64_epi64(
        _mm256_packus_epi16(res_lo, res_hi), 0xD8);

      _mm256_storeu_si256((__m256i*)(dstp8 + x * 4), result);
    }

    // Scalar tail (width not a multiple of 8).
    constexpr MagicDiv magic8 = get_magic_div(8);
    for (; x < width; ++x) {
      const uint32_t alpha_src = has_separate_mask
        ? (uint32_t)maskp8[x]
        : (uint32_t)ovrp8[x * 4 + 3];
      const uint32_t ae = (uint32_t)magic_div_rt<uint8_t>(
        alpha_src * (uint32_t)opacity_i + 127u, magic8);
      const uint32_t iv = 255u - ae;
      for (int ch = 0; ch < 4; ++ch) {
        dstp8[x * 4 + ch] = (BYTE)magic_div_rt<uint8_t>(
          (uint32_t)dstp8[x * 4 + ch] * iv + (uint32_t)ovrp8[x * 4 + ch] * ae + 127u, magic8);
      }
    }

    dstp8 += dst_pitch;
    ovrp8 += ovr_pitch;
    if constexpr (has_separate_mask) maskp8 += mask_pitch;
  }
}

// Dispatcher: 8-bit only; 16-bit falls back to C reference.
// has_separate_mask=false → Add  (alpha from ovrp8[x*4+3])
// has_separate_mask=true  → Subtract (alpha from maskp8[x], overlay pre-inverted)
void get_layer_packedrgb_blend_functions_avx2(
  bool has_separate_mask, int bits_per_pixel,
  layer_packedrgb_blend_c_t** fn)
{
  if (bits_per_pixel == 8) {
    *fn = has_separate_mask ? masked_blend_packedrgba_avx2_u8<true>
                            : masked_blend_packedrgba_avx2_u8<false>;
    return;
  }
  get_layer_packedrgb_blend_functions(has_separate_mask, bits_per_pixel, fn);
}






// "fast" blend is simple averaging
template<typename pixel_t>
void layer_genericplane_fast_avx2(BYTE* dstp, const BYTE* ovrp, int dst_pitch, int overlay_pitch, int width, int height, int level) {
  AVS_UNUSED(level);
  int width_bytes = width * sizeof(pixel_t);
  int width_mod32 = width_bytes / 32 * 32;

  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width_mod32; x += 32) {
      __m256i src = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(dstp + x));
      __m256i ovr = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(ovrp + x));
      if constexpr (sizeof(pixel_t) == 1)
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(dstp + x), _mm256_avg_epu8(src, ovr));
      else
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(dstp + x), _mm256_avg_epu16(src, ovr));
    }

    for (int x = width_mod32 / sizeof(pixel_t); x < width; ++x) {
      reinterpret_cast<pixel_t*>(dstp)[x] = (reinterpret_cast<pixel_t*>(dstp)[x] + reinterpret_cast<const pixel_t*>(ovrp)[x] + 1) / 2;
    }

    dstp += dst_pitch;
    ovrp += overlay_pitch;
  }
}

// instantiate
template void layer_genericplane_fast_avx2<uint8_t>(BYTE* dstp, const BYTE* ovrp, int dst_pitch, int overlay_pitch, int width, int height, int level);
template void layer_genericplane_fast_avx2<uint16_t>(BYTE* dstp, const BYTE* ovrp, int dst_pitch, int overlay_pitch, int width, int height, int level);

/* RGB32 */

//src format: xx xx xx xx | xx xx xx xx | a1 xx xx xx | a0 xx xx xx
//level_vector and one should be vectors of 32bit packed integers
static AVS_FORCEINLINE __m128i calculate_monochrome_alpha_avx2(const __m128i& src, const __m128i& level_vector, const __m128i& one) {
  __m128i alpha = _mm_srli_epi32(src, 24);
  alpha = _mm_mullo_epi16(alpha, level_vector);
  alpha = _mm_add_epi32(alpha, one);
  alpha = _mm_srli_epi32(alpha, 8);
  alpha = _mm_shufflelo_epi16(alpha, _MM_SHUFFLE(2, 2, 0, 0));
  return _mm_shuffle_epi32(alpha, _MM_SHUFFLE(1, 1, 0, 0));
}

static AVS_FORCEINLINE __m128i calculate_luma_avx2(const __m128i& src, const __m128i& rgb_coeffs, const __m128i& zero) {
  AVS_UNUSED(zero);
  __m128i temp = _mm_madd_epi16(src, rgb_coeffs);
  __m128i low = _mm_shuffle_epi32(temp, _MM_SHUFFLE(3, 3, 1, 1));
  temp = _mm_add_epi32(low, temp);
  temp = _mm_srli_epi32(temp, 15);
  __m128i result = _mm_shufflelo_epi16(temp, _MM_SHUFFLE(0, 0, 0, 0));
  return _mm_shufflehi_epi16(result, _MM_SHUFFLE(0, 0, 0, 0));
}

// must be unaligned load/store
template<bool use_chroma>
void layer_rgb32_mul_avx2(BYTE* dstp, const BYTE* ovrp, int dst_pitch, int overlay_pitch, int width, int height, int level) {
  int mod2_width = width / 2 * 2;

  __m128i zero = _mm_setzero_si128();
  __m128i level_vector = _mm_set1_epi32(level);
  __m128i one = _mm_set1_epi32(1);
  __m128i rgb_coeffs = _mm_set_epi16(0, cyr, cyg, cyb, 0, cyr, cyg, cyb);

  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < mod2_width; x += 2) {
      __m128i src = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(dstp + x * 4));
      __m128i ovr = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(ovrp + x * 4));

      __m128i alpha = calculate_monochrome_alpha_avx2(ovr, level_vector, one);

      src = _mm_unpacklo_epi8(src, zero);
      ovr = _mm_unpacklo_epi8(ovr, zero);

      __m128i luma;
      if (use_chroma) {
        luma = ovr;
      }
      else {
        luma = calculate_luma_avx2(ovr, rgb_coeffs, zero);
      }

      __m128i dst = _mm_mullo_epi16(luma, src);
      dst = _mm_srli_epi16(dst, 8);
      dst = _mm_subs_epi16(dst, src);
      dst = _mm_mullo_epi16(dst, alpha);
      dst = _mm_srli_epi16(dst, 8);
      dst = _mm_add_epi8(src, dst);

      dst = _mm_packus_epi16(dst, zero);

      _mm_storel_epi64(reinterpret_cast<__m128i*>(dstp + x * 4), dst);
    }

    if (width != mod2_width) {
      int x = mod2_width;
      int alpha = (ovrp[x * 4 + 3] * level + 1) >> 8;

      if (use_chroma) {
        dstp[x * 4] = dstp[x * 4] + (((((ovrp[x * 4] * dstp[x * 4]) >> 8) - dstp[x * 4]) * alpha) >> 8);
        dstp[x * 4 + 1] = dstp[x * 4 + 1] + (((((ovrp[x * 4 + 1] * dstp[x * 4 + 1]) >> 8) - dstp[x * 4 + 1]) * alpha) >> 8);
        dstp[x * 4 + 2] = dstp[x * 4 + 2] + (((((ovrp[x * 4 + 2] * dstp[x * 4 + 2]) >> 8) - dstp[x * 4 + 2]) * alpha) >> 8);
        dstp[x * 4 + 3] = dstp[x * 4 + 3] + (((((ovrp[x * 4 + 3] * dstp[x * 4 + 3]) >> 8) - dstp[x * 4 + 3]) * alpha) >> 8);
      }
      else {
        int luma = (cyb * ovrp[x * 4] + cyg * ovrp[x * 4 + 1] + cyr * ovrp[x * 4 + 2]) >> 15;

        dstp[x * 4] = dstp[x * 4] + (((((luma * dstp[x * 4]) >> 8) - dstp[x * 4]) * alpha) >> 8);
        dstp[x * 4 + 1] = dstp[x * 4 + 1] + (((((luma * dstp[x * 4 + 1]) >> 8) - dstp[x * 4 + 1]) * alpha) >> 8);
        dstp[x * 4 + 2] = dstp[x * 4 + 2] + (((((luma * dstp[x * 4 + 2]) >> 8) - dstp[x * 4 + 2]) * alpha) >> 8);
        dstp[x * 4 + 3] = dstp[x * 4 + 3] + (((((luma * dstp[x * 4 + 3]) >> 8) - dstp[x * 4 + 3]) * alpha) >> 8);
      }
    }

    dstp += dst_pitch;
    ovrp += overlay_pitch;
  }
}

// instantiate
template void layer_rgb32_mul_avx2<false>(BYTE* dstp, const BYTE* ovrp, int dst_pitch, int overlay_pitch, int width, int height, int level);
template void layer_rgb32_mul_avx2<true>(BYTE* dstp, const BYTE* ovrp, int dst_pitch, int overlay_pitch, int width, int height, int level);


// must be unaligned load/store
template<bool use_chroma>
void layer_rgb32_add_avx2(BYTE* dstp, const BYTE* ovrp, int dst_pitch, int overlay_pitch, int width, int height, int level) {
  int mod2_width = width / 2 * 2;

  __m128i zero = _mm_setzero_si128();
  __m128i level_vector = _mm_set1_epi32(level);
  __m128i one = _mm_set1_epi32(1);
  __m128i rgb_coeffs = _mm_set_epi16(0, cyr, cyg, cyb, 0, cyr, cyg, cyb);

  constexpr int rounder = 128;
  const __m128i rounder_simd = _mm_set1_epi16(rounder);

  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < mod2_width; x += 2) {
      __m128i src = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(dstp + x * 4));
      __m128i ovr = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(ovrp + x * 4));

      __m128i alpha = calculate_monochrome_alpha_avx2(ovr, level_vector, one);

      src = _mm_unpacklo_epi8(src, zero);
      ovr = _mm_unpacklo_epi8(ovr, zero);

      __m128i luma;
      if (use_chroma) {
        luma = ovr;
      }
      else {
        luma = calculate_luma_avx2(ovr, rgb_coeffs, zero);
      }

      __m128i dst = _mm_subs_epi16(luma, src);
      dst = _mm_mullo_epi16(dst, alpha);
      dst = _mm_add_epi16(dst, rounder_simd);
      dst = _mm_srli_epi16(dst, 8);
      dst = _mm_add_epi8(src, dst);

      dst = _mm_packus_epi16(dst, zero);

      _mm_storel_epi64(reinterpret_cast<__m128i*>(dstp + x * 4), dst);
    }

    if (width != mod2_width) {
      int x = mod2_width;
      int alpha = (ovrp[x * 4 + 3] * level + 1) >> 8;

      if (use_chroma) {
        dstp[x * 4] = dstp[x * 4] + (((ovrp[x * 4] - dstp[x * 4]) * alpha + rounder) >> 8);
        dstp[x * 4 + 1] = dstp[x * 4 + 1] + (((ovrp[x * 4 + 1] - dstp[x * 4 + 1]) * alpha + rounder) >> 8);
        dstp[x * 4 + 2] = dstp[x * 4 + 2] + (((ovrp[x * 4 + 2] - dstp[x * 4 + 2]) * alpha + rounder) >> 8);
        dstp[x * 4 + 3] = dstp[x * 4 + 3] + (((ovrp[x * 4 + 3] - dstp[x * 4 + 3]) * alpha + rounder) >> 8);
      }
      else {
        int luma = (cyb * ovrp[x * 4] + cyg * ovrp[x * 4 + 1] + cyr * ovrp[x * 4 + 2]) >> 15;

        dstp[x * 4] = dstp[x * 4] + (((luma - dstp[x * 4]) * alpha + rounder) >> 8);
        dstp[x * 4 + 1] = dstp[x * 4 + 1] + (((luma - dstp[x * 4 + 1]) * alpha + rounder) >> 8);
        dstp[x * 4 + 2] = dstp[x * 4 + 2] + (((luma - dstp[x * 4 + 2]) * alpha + rounder) >> 8);
        dstp[x * 4 + 3] = dstp[x * 4 + 3] + (((luma - dstp[x * 4 + 3]) * alpha + rounder) >> 8);
      }
    }

    dstp += dst_pitch;
    ovrp += overlay_pitch;
  }
}

// instantiate
template void layer_rgb32_add_avx2<false>(BYTE* dstp, const BYTE* ovrp, int dst_pitch, int overlay_pitch, int width, int height, int level);
template void layer_rgb32_add_avx2<true>(BYTE* dstp, const BYTE* ovrp, int dst_pitch, int overlay_pitch, int width, int height, int level);

// unlike sse2 alignment is not required. avx2 has no such big penalty
void layer_yuy2_or_rgb32_fast_avx2(BYTE* dstp, const BYTE* ovrp, int dst_pitch, int overlay_pitch, int width, int height, int level) {
  AVS_UNUSED(level);
  int width_bytes = width * 2;
  int width_mod32 = width_bytes / 32 * 32;

  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width_mod32; x += 32) {
      __m256i src = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(dstp + x));
      __m256i ovr = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(ovrp + x));

      _mm256_storeu_si256(reinterpret_cast<__m256i*>(dstp + x), _mm256_avg_epu8(src, ovr));
    }

    for (int x = width_mod32; x < width_bytes; ++x) {
      dstp[x] = (dstp[x] + ovrp[x] + 1) / 2;
    }

    dstp += dst_pitch;
    ovrp += overlay_pitch;
  }
}

// aligned ptr not required
void layer_rgb32_fast_avx2(BYTE* dstp, const BYTE* ovrp, int dst_pitch, int overlay_pitch, int width, int height, int level) {
  layer_yuy2_or_rgb32_fast_avx2(dstp, ovrp, dst_pitch, overlay_pitch, width * 2, height, level);
}

// unaligned addresses
template<bool use_chroma>
void layer_rgb32_subtract_avx2(BYTE* dstp, const BYTE* ovrp, int dst_pitch, int overlay_pitch, int width, int height, int level) {
  int mod2_width = width / 2 * 2;

  __m128i zero = _mm_setzero_si128();
  __m128i level_vector = _mm_set1_epi32(level);
  __m128i one = _mm_set1_epi32(1);
  __m128i rgb_coeffs = _mm_set_epi16(0, cyr, cyg, cyb, 0, cyr, cyg, cyb);
  __m128i ff = _mm_set1_epi16(0x00FF);

  constexpr int rounder = 128;
  const __m128i rounder_simd = _mm_set1_epi16(rounder);

  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < mod2_width; x += 2) {
      __m128i src = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(dstp + x * 4));
      __m128i ovr = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(ovrp + x * 4));

      __m128i alpha = calculate_monochrome_alpha_avx2(ovr, level_vector, one);

      src = _mm_unpacklo_epi8(src, zero);
      ovr = _mm_unpacklo_epi8(ovr, zero);

      __m128i luma;
      if (use_chroma) {
        luma = _mm_subs_epi16(ff, ovr);
      }
      else {
        luma = calculate_luma_avx2(_mm_andnot_si128(ovr, ff), rgb_coeffs, zero);
      }

      __m128i dst = _mm_subs_epi16(luma, src);
      dst = _mm_mullo_epi16(dst, alpha);
      dst = _mm_add_epi16(dst, rounder_simd);
      dst = _mm_srli_epi16(dst, 8);
      dst = _mm_add_epi8(src, dst);

      dst = _mm_packus_epi16(dst, zero);

      _mm_storel_epi64(reinterpret_cast<__m128i*>(dstp + x * 4), dst);
    }

    if (width != mod2_width) {
      int x = mod2_width;
      int alpha = (ovrp[x * 4 + 3] * level + 1) >> 8;

      if (use_chroma) {
        dstp[x * 4] = dstp[x * 4] + (((255 - ovrp[x * 4] - dstp[x * 4]) * alpha + rounder) >> 8);
        dstp[x * 4 + 1] = dstp[x * 4 + 1] + (((255 - ovrp[x * 4 + 1] - dstp[x * 4 + 1]) * alpha + rounder) >> 8);
        dstp[x * 4 + 2] = dstp[x * 4 + 2] + (((255 - ovrp[x * 4 + 2] - dstp[x * 4 + 2]) * alpha + rounder) >> 8);
        dstp[x * 4 + 3] = dstp[x * 4 + 3] + (((255 - ovrp[x * 4 + 3] - dstp[x * 4 + 3]) * alpha + rounder) >> 8);
      }
      else {
        int luma = (cyb * (255 - ovrp[x * 4]) + cyg * (255 - ovrp[x * 4 + 1]) + cyr * (255 - ovrp[x * 4 + 2])) >> 15;

        dstp[x * 4] = dstp[x * 4] + (((luma - dstp[x * 4]) * alpha + rounder) >> 8);
        dstp[x * 4 + 1] = dstp[x * 4 + 1] + (((luma - dstp[x * 4 + 1]) * alpha + rounder) >> 8);
        dstp[x * 4 + 2] = dstp[x * 4 + 2] + (((luma - dstp[x * 4 + 2]) * alpha + rounder) >> 8);
        dstp[x * 4 + 3] = dstp[x * 4 + 3] + (((luma - dstp[x * 4 + 3]) * alpha + rounder) >> 8);
      }
    }

    dstp += dst_pitch;
    ovrp += overlay_pitch;
  }
}

// instantiate
template void layer_rgb32_subtract_avx2<false>(BYTE* dstp, const BYTE* ovrp, int dst_pitch, int overlay_pitch, int width, int height, int level);
template void layer_rgb32_subtract_avx2<true>(BYTE* dstp, const BYTE* ovrp, int dst_pitch, int overlay_pitch, int width, int height, int level);

// unaligned adresses
template<int mode>
void layer_rgb32_lighten_darken_avx2(BYTE* dstp, const BYTE* ovrp, int dst_pitch, int overlay_pitch, int width, int height, int level, int thresh) {
  int mod2_width = width / 2 * 2;

  __m128i zero = _mm_setzero_si128();
  __m128i level_vector = _mm_set1_epi32(level);
  __m128i one = _mm_set1_epi32(1);
  __m128i rgb_coeffs = _mm_set_epi16(0, cyr, cyg, cyb, 0, cyr, cyg, cyb);
  __m128i threshold = _mm_set1_epi16(thresh);

  constexpr int rounder = 128;
  const __m128i rounder_simd = _mm_set1_epi16(rounder);

  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < mod2_width; x += 2) {
      __m128i src = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(dstp + x * 4));
      __m128i ovr = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(ovrp + x * 4));

      __m128i alpha = calculate_monochrome_alpha_avx2(ovr, level_vector, one);

      src = _mm_unpacklo_epi8(src, zero);
      ovr = _mm_unpacklo_epi8(ovr, zero);

      __m128i luma_ovr = calculate_luma_avx2(ovr, rgb_coeffs, zero);
      __m128i luma_src = calculate_luma_avx2(src, rgb_coeffs, zero);

      __m128i mask;
      if constexpr (mode == LIGHTEN) {
        __m128i tmp = _mm_add_epi16(luma_src, threshold);
        mask = _mm_cmpgt_epi16(luma_ovr, tmp);
      }
      else {
        __m128i tmp = _mm_sub_epi16(luma_src, threshold);
        mask = _mm_cmpgt_epi16(tmp, luma_ovr);
      }

      alpha = _mm_and_si128(alpha, mask);

      __m128i dst = _mm_subs_epi16(ovr, src);
      dst = _mm_mullo_epi16(dst, alpha);
      dst = _mm_add_epi16(dst, rounder_simd);
      dst = _mm_srli_epi16(dst, 8);
      dst = _mm_add_epi8(src, dst);

      dst = _mm_packus_epi16(dst, zero);

      _mm_storel_epi64(reinterpret_cast<__m128i*>(dstp + x * 4), dst);
    }

    if (width != mod2_width) {
      int x = mod2_width;
      int alpha = (ovrp[x * 4 + 3] * level + 1) >> 8;
      int luma_ovr = (cyb * ovrp[x * 4] + cyg * ovrp[x * 4 + 1] + cyr * ovrp[x * 4 + 2]) >> 15;
      int luma_src = (cyb * dstp[x * 4] + cyg * dstp[x * 4 + 1] + cyr * dstp[x * 4 + 2]) >> 15;

      if constexpr (mode == LIGHTEN)
        alpha = luma_ovr > luma_src + thresh ? alpha : 0;
      else // DARKEN
        alpha = luma_ovr < luma_src - thresh ? alpha : 0;

      dstp[x * 4] = dstp[x * 4] + (((ovrp[x * 4] - dstp[x * 4]) * alpha + rounder) >> 8);
      dstp[x * 4 + 1] = dstp[x * 4 + 1] + (((ovrp[x * 4 + 1] - dstp[x * 4 + 1]) * alpha + rounder) >> 8);
      dstp[x * 4 + 2] = dstp[x * 4 + 2] + (((ovrp[x * 4 + 2] - dstp[x * 4 + 2]) * alpha + rounder) >> 8);
      dstp[x * 4 + 3] = dstp[x * 4 + 3] + (((ovrp[x * 4 + 3] - dstp[x * 4 + 3]) * alpha + rounder) >> 8);
    }

    dstp += dst_pitch;
    ovrp += overlay_pitch;
  }
}

// instantiate
template void layer_rgb32_lighten_darken_avx2<LIGHTEN>(BYTE* dstp, const BYTE* ovrp, int dst_pitch, int overlay_pitch, int width, int height, int level, int thresh);
template void layer_rgb32_lighten_darken_avx2<DARKEN>(BYTE* dstp, const BYTE* ovrp, int dst_pitch, int overlay_pitch, int width, int height, int level, int thresh);

