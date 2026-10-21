// AviSynth+.  Copyright 2026 AviSynth+ Project
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

// Horizontal resizer, planar 8-16 bit, generic (any kernel size): the NEON pair of
// resizer_h_c_generic_uint8_16_vectorized (resample.cpp) and resizer_h_avx2_generic_uint8_t / _uint16_t.
// Same structure and integer arithmetic, bit-identical:
//   out[x] = (sum(src[offset[x] + i] * coeff[x][i]) + rounder) >> FPScale, saturated (10-14 bit: clamped to max)
//   exact 16 bit: the pixels are pivoted to signed (v - 32768, wrapping int16 add like C/AVX2) and
//   32768 << FPScale16bits is added back after the sum (the coefficients sum to 1 << FPScale16bits).
// 8 output pixels per step, each a dot product over the coefficients:
//   vmlal_s16 (widening multiply-accumulate) instead of madd, 8 taps per step;
//   the 8 sums: two vpaddq_s32 rounds per 4 pixels, no cross-lane permute like at AVX2 hadd.
// Overread rules as in C/AVX2: up to safelimit the padded (aligned) filter_size is read, beyond it only the
// real kernel size (8 taps, then the rest per tap). Like C/AVX2, the last group of 8 may store up to 7 pixels
// beyond width, inside the aligned frame area.
// The C version compiles to scalar loads, multiplies and lane inserts on aarch64 (GCC 12), this is the reason for it.

#include "resample_neon.h"
#include <avs/config.h>
#include <cstdint>

#include <arm_neon.h>

// 8 pixels as int16: 8 bit zero-extended, 10-14 bit as is, 16 bit pivoted to signed
template<typename pixel_t, bool lessthan16bit>
static AVS_FORCEINLINE int16x8_t load8_s16(const pixel_t* p)
{
  if constexpr (sizeof(pixel_t) == 1)
    return vreinterpretq_s16_u16(vmovl_u8(vld1_u8(p)));
  else if constexpr (lessthan16bit)
    return vreinterpretq_s16_u16(vld1q_u16(p));
  else
    return vreinterpretq_s16_u16(vaddq_u16(vld1q_u16(p), vdupq_n_u16(0x8000))); // v - 32768 as int16
}

// dot product of one output pixel; 4 partial sums
template<bool safe_aligned_mode, typename pixel_t, bool lessthan16bit>
static AVS_FORCEINLINE int32x4_t dot_one_pixel(const pixel_t* AVS_RESTRICT src, const short* AVS_RESTRICT coeff, int filter_size, int kernel_size)
{
  int32x4_t acc0 = vdupq_n_s32(0);
  int32x4_t acc1 = vdupq_n_s32(0);
  // safe: the coefficients are padded to filter_size (multiple of 16) with zeros, reading that far is allowed
  const int ksmod8 = safe_aligned_mode ? filter_size : kernel_size / 8 * 8;
  int i = 0;
  for (; i < ksmod8; i += 8) {
    const int16x8_t d = load8_s16<pixel_t, lessthan16bit>(src + i);
    const int16x8_t c = vld1q_s16(coeff + i);
    acc0 = vmlal_s16(acc0, vget_low_s16(d), vget_low_s16(c));
    acc1 = vmlal_high_s16(acc1, d, c);
  }
  if constexpr (!safe_aligned_mode) {
    // the rest of the real kernel, per tap: no read beyond the last source pixel
    int rest = 0;
    for (; i < kernel_size; i++) {
      int v = src[i];
      if constexpr (sizeof(pixel_t) == 2 && !lessthan16bit)
        v = (short)(v - 32768);
      rest += v * coeff[i];
    }
    acc0 = vaddq_s32(acc0, vsetq_lane_s32(rest, vdupq_n_s32(0), 0));
  }
  return vaddq_s32(acc0, acc1);
}

template<bool safe_aligned_mode, typename pixel_t, bool lessthan16bit>
static AVS_FORCEINLINE void process_eight_pixels_h_neon(const pixel_t* AVS_RESTRICT src, int x, const short* AVS_RESTRICT coeff_base, int filter_size,
  int kernel_size, const int* AVS_RESTRICT pixel_offset, int32x4_t rounder, uint16x8_t clamp_limit, pixel_t* AVS_RESTRICT dst)
{
  const short* coeff = coeff_base + x * filter_size;
  int32x4_t r[8];
  // unrolled: keeps the 8 partial sums in registers (otherwise GCC 12 spills them to the stack)
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC unroll 8
#endif
  for (int j = 0; j < 8; j++)
    r[j] = dot_one_pixel<safe_aligned_mode, pixel_t, lessthan16bit>(src + pixel_offset[x + j], coeff + j * filter_size, filter_size, kernel_size);
  // horizontal sums: lane j = sum of r[j]
  int32x4_t s0123 = vpaddq_s32(vpaddq_s32(r[0], r[1]), vpaddq_s32(r[2], r[3]));
  int32x4_t s4567 = vpaddq_s32(vpaddq_s32(r[4], r[5]), vpaddq_s32(r[6], r[7]));
  // rounder (+ the pivot correction for exact 16 bit)
  s0123 = vaddq_s32(s0123, rounder);
  s4567 = vaddq_s32(s4567, rounder);
  constexpr int fp_scale_bits = sizeof(pixel_t) == 1 ? FPScale8bits : FPScale16bits;
  s0123 = vshrq_n_s32(s0123, fp_scale_bits);
  s4567 = vshrq_n_s32(s4567, fp_scale_bits);
  uint16x8_t res = vcombine_u16(vqmovun_s32(s0123), vqmovun_s32(s4567)); // < 0 -> 0, > 65535 -> 65535
  if constexpr (sizeof(pixel_t) == 1) {
    vst1_u8(dst + x, vqmovn_u16(res)); // > 255 -> 255
  }
  else {
    if constexpr (lessthan16bit)
      res = vminq_u16(res, clamp_limit);
    vst1q_u16(dst + x, res);
  }
}

template<typename pixel_t, bool lessthan16bit>
void resizer_h_neon_generic_uint8_16(BYTE* dst8, const BYTE* src8, int dst_pitch, int src_pitch, ResamplingProgram* program, int width, int height, int bits_per_pixel)
{
  const int filter_size = program->filter_size;
  const int kernel_size = program->filter_size_real;
  constexpr int fp_scale_bits = sizeof(pixel_t) == 1 ? FPScale8bits : FPScale16bits;
  int rounder_i = 1 << (fp_scale_bits - 1);
  if constexpr (sizeof(pixel_t) == 2 && !lessthan16bit)
    rounder_i += 32768 << FPScale16bits; // back from the signed pivot
  const int32x4_t rounder = vdupq_n_s32(rounder_i);
  const uint16x8_t clamp_limit = vdupq_n_u16((uint16_t)((1 << bits_per_pixel) - 1));

  const pixel_t* AVS_RESTRICT src = reinterpret_cast<const pixel_t*>(src8);
  pixel_t* AVS_RESTRICT dst = reinterpret_cast<pixel_t*>(dst8);
  dst_pitch /= sizeof(pixel_t);
  src_pitch /= sizeof(pixel_t);

  const int w_safe_mod8 = (program->safelimit_filter_size_aligned.overread_possible ? program->safelimit_filter_size_aligned.source_overread_beyond_targetx : width) / 8 * 8;
  const int* pixel_offset = program->pixel_offset.data();
  const short* coeff_base = program->pixel_coefficient;

  for (int y = 0; y < height; y++) {
    int x = 0;
    for (; x < w_safe_mod8; x += 8)
      process_eight_pixels_h_neon<true, pixel_t, lessthan16bit>(src, x, coeff_base, filter_size, kernel_size, pixel_offset, rounder, clamp_limit, dst);
    // beyond the safe limit: real kernel size only; pixel_offset has entries up to the next multiple of 8
    for (; x < width; x += 8)
      process_eight_pixels_h_neon<false, pixel_t, lessthan16bit>(src, x, coeff_base, filter_size, kernel_size, pixel_offset, rounder, clamp_limit, dst);
    dst += dst_pitch;
    src += src_pitch;
  }
}

template void resizer_h_neon_generic_uint8_16<uint8_t, true>(BYTE* dst8, const BYTE* src8, int dst_pitch, int src_pitch, ResamplingProgram* program, int width, int height, int bits_per_pixel);
template void resizer_h_neon_generic_uint8_16<uint16_t, true>(BYTE* dst8, const BYTE* src8, int dst_pitch, int src_pitch, ResamplingProgram* program, int width, int height, int bits_per_pixel);
template void resizer_h_neon_generic_uint8_16<uint16_t, false>(BYTE* dst8, const BYTE* src8, int dst_pitch, int src_pitch, ResamplingProgram* program, int width, int height, int bits_per_pixel);
