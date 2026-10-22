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



// Avisynth filter: Layer
// by "poptones" (poptones@myrealbox.com)

#include "layer_sse.h"
#include "../layer.h"

#ifdef AVS_WINDOWS
#include <avs/win.h>
#else
#include <avs/posix.h>
#endif

#include <avs/minmax.h>
#include <avs/alignment.h>
#include "../core/internal.h"

// Intrinsics base header + really required extension headers
#if defined(_MSC_VER)
#include <intrin.h> // MSVC
#else 
#include <x86intrin.h> // GCC/MinGW/Clang/LLVM
#endif

#include "../convert/convert_planar.h"
#include <algorithm>


static AVS_FORCEINLINE __m128i mask_core_sse2(__m128i& src, __m128i& alpha, __m128i& not_alpha_mask, __m128i& zero, __m128i& matrix, __m128i& round_mask) {
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

void mask_sse2(BYTE* srcp, const BYTE* alphap, int src_pitch, int alpha_pitch, size_t width, size_t height) {
  __m128i matrix = _mm_set_epi16(0, cyr, cyg, cyb, 0, cyr, cyg, cyb);
  __m128i zero = _mm_setzero_si128();
  __m128i round_mask = _mm_set1_epi32(16384);
  __m128i not_alpha_mask = _mm_set1_epi32(0x00FFFFFF);

  // width must be >= 4 (width_bytes >= 16): the row-tail step below re-processes the last 16 bytes
  // Checked before calling this function
  size_t width_bytes = width * 4;
  size_t width_mod16 = width_bytes / 16 * 16;

  for (size_t y = 0; y < height; ++y) {
    for (size_t x = 0; x < width_mod16; x += 16) {
      __m128i src = _mm_load_si128(reinterpret_cast<const __m128i*>(srcp + x));
      __m128i alpha = _mm_load_si128(reinterpret_cast<const __m128i*>(alphap + x));
      __m128i result = mask_core_sse2(src, alpha, not_alpha_mask, zero, matrix, round_mask);

      _mm_store_si128(reinterpret_cast<__m128i*>(srcp + x), result);
    }

    if (width_mod16 < width_bytes) {
      __m128i src = _mm_loadu_si128(reinterpret_cast<const __m128i*>(srcp + width_bytes - 16));
      __m128i alpha = _mm_loadu_si128(reinterpret_cast<const __m128i*>(alphap + width_bytes - 16));
      __m128i result = mask_core_sse2(src, alpha, not_alpha_mask, zero, matrix, round_mask);

      _mm_storeu_si128(reinterpret_cast<__m128i*>(srcp + width_bytes - 16), result);
    }

    srcp += src_pitch;
    alphap += alpha_pitch;
  }
}

void colorkeymask_sse2(BYTE* pf, int pitch, int color, int height, int width, int tolB, int tolG, int tolR) {
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


// RGB64: 2 pixels of 4x16 bit per 128 bit
void colorkeymask_uint16_sse2(BYTE* pf, int pitch, uint64_t color64, int height, int tolB, int tolG, int tolR) {
  const uint64_t t = 0xFFFF000000000000ull | ((uint64_t)tolR << 32) | ((uint64_t)tolG << 16) | (uint64_t)tolB; // alpha always passes
  __m128i tolerance = _mm_set1_epi64x(t);
  __m128i colorv = _mm_set1_epi64x(color64);
  __m128i zero = _mm_setzero_si128();

  BYTE* endp = pf + pitch * height;

  while (pf < endp)
  {
    __m128i src = _mm_load_si128(reinterpret_cast<const __m128i*>(pf));
    __m128i gt = _mm_subs_epu16(colorv, src);
    __m128i lt = _mm_subs_epu16(src, colorv);
    __m128i absdiff = _mm_or_si128(gt, lt); //abs(color - src)

    __m128i not_passed = _mm_subs_epu16(absdiff, tolerance);
    __m128i passed32 = _mm_cmpeq_epi32(not_passed, zero);
    // both 32 bit halves of the 64 bit pixel (no _mm_cmpeq_epi64 in SSE2)
    __m128i passed = _mm_and_si128(passed32, _mm_shuffle_epi32(passed32, _MM_SHUFFLE(2, 3, 0, 1)));
    // no key pixel: no store, halves the memory traffic (a frame is far larger than the cache)
    if (_mm_movemask_epi8(passed) != 0) {
      passed = _mm_slli_epi64(passed, 48); // alpha word only
      __m128i result = _mm_andnot_si128(passed, src);
      _mm_store_si128(reinterpret_cast<__m128i*>(pf), result);
    }

    pf += 16;
  }
}

void invert_frame_inplace_sse2(BYTE* frame, int pitch, int width, int height, int mask) {
  __m128i maskv = _mm_set1_epi32(mask);

  BYTE* endp = frame + pitch * height;

  while (frame < endp) {
    __m128i src = _mm_load_si128(reinterpret_cast<const __m128i*>(frame));
    __m128i inv = _mm_xor_si128(src, maskv);
    _mm_store_si128(reinterpret_cast<__m128i*>(frame), inv);
    frame += 16;
  }
}

void invert_frame_uint16_inplace_sse2(BYTE* frame, int pitch, int width, int height, uint64_t mask64) {
  __m128i maskv = _mm_set_epi32((uint32_t)(mask64 >> 32), (uint32_t)mask64, (uint32_t)(mask64 >> 32), (uint32_t)mask64);

  BYTE* endp = frame + pitch * height;

  while (frame < endp) {
    __m128i src = _mm_load_si128(reinterpret_cast<const __m128i*>(frame));
    __m128i inv = _mm_xor_si128(src, maskv);
    _mm_store_si128(reinterpret_cast<__m128i*>(frame), inv);
    frame += 16;
  }
}

template<bool chroma>
void invert_plane_sse2_u8(uint8_t* dstp, const uint8_t* srcp, int src_pitch, int dst_pitch, int width, int height, int /*bits_per_pixel*/)
{
  const __m128i v_ff = _mm_set1_epi8((char)0xFF);
  const __m128i v_one = _mm_set1_epi8(1);
  for (int y = 0; y < height; ++y) {
    
    for (int x = 0; x < width; x += 16) {
      __m128i s = _mm_load_si128(reinterpret_cast<const __m128i*>(srcp + x));
      __m128i r;
      if constexpr (chroma)
        r = _mm_xor_si128(_mm_subs_epu8(s, v_one), v_ff);
      else
        r = _mm_xor_si128(s, v_ff);
      _mm_store_si128(reinterpret_cast<__m128i*>(dstp + x), r);
    }
    srcp += src_pitch;
    dstp += dst_pitch;
  }
}

template<bool chroma>
void invert_plane_sse2_u16(uint8_t* dstp, const uint8_t* srcp, int src_pitch, int dst_pitch, int width, int height, int bits_per_pixel)
{
  const int max_pixel_value = (1 << bits_per_pixel) - 1;
  const __m128i v_max = _mm_set1_epi16((short)max_pixel_value);
  const __m128i v_one = _mm_set1_epi16(1);
  for (int y = 0; y < height; ++y) {
    const uint16_t* s16 = reinterpret_cast<const uint16_t*>(srcp);
    uint16_t* d16 = reinterpret_cast<uint16_t*>(dstp);
    for (int x = 0; x < width; x += 8) {
      __m128i s = _mm_load_si128(reinterpret_cast<const __m128i*>(s16 + x));
      __m128i r;
      if constexpr (chroma)
        r = _mm_xor_si128(_mm_subs_epu16(s, v_one), v_max);
      else
        r = _mm_xor_si128(s, v_max);
      _mm_store_si128(reinterpret_cast<__m128i*>(d16 + x), r);
    }
    srcp += src_pitch;
    dstp += dst_pitch;
  }
}

template<bool chroma>
void invert_plane_sse2_f32(uint8_t* dstp, const uint8_t* srcp, int src_pitch, int dst_pitch, int width, int height, int /*bits_per_pixel*/)
{
  const __m128 v_one = _mm_set1_ps(1.0f);
  const __m128 v_sign = _mm_set1_ps(-0.0f);
  for (int y = 0; y < height; ++y) {
    const float* sf = reinterpret_cast<const float*>(srcp);
    float* df = reinterpret_cast<float*>(dstp);
    for (int x = 0; x < width; x += 4) {
      __m128 s = _mm_load_ps(sf + x);
      __m128 r = chroma ? _mm_xor_ps(s, v_sign) : _mm_sub_ps(v_one, s);
      _mm_store_ps(df + x, r);
    }
    srcp += src_pitch; dstp += dst_pitch;
  }
}

template void invert_plane_sse2_u8<false>(uint8_t*, const uint8_t*, int, int, int, int, int);
template void invert_plane_sse2_u8<true>(uint8_t*, const uint8_t*, int, int, int, int, int);
template void invert_plane_sse2_u16<false>(uint8_t*, const uint8_t*, int, int, int, int, int);
template void invert_plane_sse2_u16<true>(uint8_t*, const uint8_t*, int, int, int, int, int);
template void invert_plane_sse2_f32<false>(uint8_t*, const uint8_t*, int, int, int, int, int);
template void invert_plane_sse2_f32<true>(uint8_t*, const uint8_t*, int, int, int, int, int);

/*******************************
 *******   Layer Filter   ******
 *******************************/

void layer_rgb32_fast_sse2(BYTE* dstp, const BYTE* ovrp, int dst_pitch, int overlay_pitch, int width, int height, int level) {
  AVS_UNUSED(level);
  int width_bytes = width * 4;
  int width_mod16 = width_bytes / 16 * 16;

  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width_mod16; x += 16) {
      __m128i src = _mm_load_si128(reinterpret_cast<const __m128i*>(dstp + x));
      __m128i ovr = _mm_load_si128(reinterpret_cast<const __m128i*>(ovrp + x));

      _mm_store_si128(reinterpret_cast<__m128i*>(dstp + x), _mm_avg_epu8(src, ovr));
    }

    for (int x = width_mod16; x < width_bytes; ++x) {
      dstp[x] = (dstp[x] + ovrp[x] + 1) / 2;
    }

    dstp += dst_pitch;
    ovrp += overlay_pitch;
  }
}


template<typename pixel_t>
void layer_genericplane_fast_sse2(BYTE* dstp, const BYTE* ovrp, int dst_pitch, int overlay_pitch, int width, int height, int level) {
  AVS_UNUSED(level);
  int width_bytes = width * sizeof(pixel_t);
  int width_mod16 = width_bytes / 16 * 16;

  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width_mod16; x += 16) {
      __m128i src = _mm_loadu_si128(reinterpret_cast<const __m128i*>(dstp + x));
      __m128i ovr = _mm_loadu_si128(reinterpret_cast<const __m128i*>(ovrp + x));
      if constexpr (sizeof(pixel_t) == 1)
        _mm_storeu_si128(reinterpret_cast<__m128i*>(dstp + x), _mm_avg_epu8(src, ovr));
      else
        _mm_storeu_si128(reinterpret_cast<__m128i*>(dstp + x), _mm_avg_epu16(src, ovr));
    }

    for (int x = width_mod16 / sizeof(pixel_t); x < width; ++x) {
      reinterpret_cast<pixel_t*>(dstp)[x] = (reinterpret_cast<pixel_t*>(dstp)[x] + reinterpret_cast<const pixel_t*>(ovrp)[x] + 1) / 2;
    }

    dstp += dst_pitch;
    ovrp += overlay_pitch;
  }
}

// instantiate
template void layer_genericplane_fast_sse2<uint8_t>(BYTE* dstp, const BYTE* ovrp, int dst_pitch, int overlay_pitch, int width, int height, int level);
template void layer_genericplane_fast_sse2<uint16_t>(BYTE* dstp, const BYTE* ovrp, int dst_pitch, int overlay_pitch, int width, int height, int level);

/* RGB32 */

//src format: xx xx xx xx | xx xx xx xx | a1 xx xx xx | a0 xx xx xx
//level_vector and one should be vectors of 32bit packed integers
static AVS_FORCEINLINE __m128i calculate_monochrome_alpha_sse2(const __m128i& src, const __m128i& level_vector, const __m128i& one) {
  __m128i alpha = _mm_srli_epi32(src, 24);
  alpha = _mm_mullo_epi16(alpha, level_vector);
  alpha = _mm_add_epi32(alpha, one);
  alpha = _mm_srli_epi32(alpha, 8);
  alpha = _mm_shufflelo_epi16(alpha, _MM_SHUFFLE(2, 2, 0, 0));
  return _mm_shuffle_epi32(alpha, _MM_SHUFFLE(1, 1, 0, 0));
}

static AVS_FORCEINLINE __m128i calculate_luma_sse2(const __m128i& src, const __m128i& rgb_coeffs, const __m128i& zero) {
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
void layer_rgb32_mul_sse2(BYTE* dstp, const BYTE* ovrp, int dst_pitch, int overlay_pitch, int width, int height, int level) {
  int mod2_width = width / 2 * 2;

  __m128i zero = _mm_setzero_si128();
  __m128i level_vector = _mm_set1_epi32(level);
  __m128i one = _mm_set1_epi32(1);
  __m128i rgb_coeffs = _mm_set_epi16(0, cyr, cyg, cyb, 0, cyr, cyg, cyb);

  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < mod2_width; x += 2) {
      __m128i src = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(dstp + x * 4));
      __m128i ovr = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(ovrp + x * 4));

      __m128i alpha = calculate_monochrome_alpha_sse2(ovr, level_vector, one);

      src = _mm_unpacklo_epi8(src, zero);
      ovr = _mm_unpacklo_epi8(ovr, zero);

      __m128i luma;
      if (use_chroma) {
        luma = ovr;
      }
      else {
        luma = calculate_luma_sse2(ovr, rgb_coeffs, zero);
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
template void layer_rgb32_mul_sse2<false>(BYTE* dstp, const BYTE* ovrp, int dst_pitch, int overlay_pitch, int width, int height, int level);
template void layer_rgb32_mul_sse2<true>(BYTE* dstp, const BYTE* ovrp, int dst_pitch, int overlay_pitch, int width, int height, int level);


// must be unaligned load/store
template<bool use_chroma>
void layer_rgb32_add_sse2(BYTE* dstp, const BYTE* ovrp, int dst_pitch, int overlay_pitch, int width, int height, int level) {
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

      __m128i alpha = calculate_monochrome_alpha_sse2(ovr, level_vector, one);

      src = _mm_unpacklo_epi8(src, zero);
      ovr = _mm_unpacklo_epi8(ovr, zero);

      __m128i luma;
      if (use_chroma) {
        luma = ovr;
      }
      else {
        luma = calculate_luma_sse2(ovr, rgb_coeffs, zero);
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
template void layer_rgb32_add_sse2<false>(BYTE* dstp, const BYTE* ovrp, int dst_pitch, int overlay_pitch, int width, int height, int level);
template void layer_rgb32_add_sse2<true>(BYTE* dstp, const BYTE* ovrp, int dst_pitch, int overlay_pitch, int width, int height, int level);



template<bool use_chroma>
void layer_rgb32_subtract_sse2(BYTE* dstp, const BYTE* ovrp, int dst_pitch, int overlay_pitch, int width, int height, int level) {
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

      __m128i alpha = calculate_monochrome_alpha_sse2(ovr, level_vector, one);

      src = _mm_unpacklo_epi8(src, zero);
      ovr = _mm_unpacklo_epi8(ovr, zero);

      __m128i luma;
      if (use_chroma) {
        luma = _mm_subs_epi16(ff, ovr);
      }
      else {
        luma = calculate_luma_sse2(_mm_andnot_si128(ovr, ff), rgb_coeffs, zero);
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
template void layer_rgb32_subtract_sse2<false>(BYTE* dstp, const BYTE* ovrp, int dst_pitch, int overlay_pitch, int width, int height, int level);
template void layer_rgb32_subtract_sse2<true>(BYTE* dstp, const BYTE* ovrp, int dst_pitch, int overlay_pitch, int width, int height, int level);


template<int mode>
void layer_rgb32_lighten_darken_sse2(BYTE* dstp, const BYTE* ovrp, int dst_pitch, int overlay_pitch, int width, int height, int level, int thresh) {
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

      __m128i alpha = calculate_monochrome_alpha_sse2(ovr, level_vector, one);

      src = _mm_unpacklo_epi8(src, zero);
      ovr = _mm_unpacklo_epi8(ovr, zero);

      __m128i luma_ovr = calculate_luma_sse2(ovr, rgb_coeffs, zero);
      __m128i luma_src = calculate_luma_sse2(src, rgb_coeffs, zero);

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
template void layer_rgb32_lighten_darken_sse2<LIGHTEN>(BYTE* dstp, const BYTE* ovrp, int dst_pitch, int overlay_pitch, int width, int height, int level, int thresh);
template void layer_rgb32_lighten_darken_sse2<DARKEN>(BYTE* dstp, const BYTE* ovrp, int dst_pitch, int overlay_pitch, int width, int height, int level, int thresh);

