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

// ConvertBits ordered dither, 8-16 bit -> 8-16 bit: the NEON pair of do_convert_ordered_dither_uint_c.
// Same arithmetic as C, bit-identical:
//   optional range conversion (fulls != fulld): src = clamp((int)((src - src_offset) * mul_factor + dst_offset + 0.5f), 0, src_max)
//   new = (src + corr) >> dither_bit_diff
//     low dither bits (< 8): new = (int)(src + (corr - half_maxcorr)) >> dither_bit_diff, in float
//   back to the target bit depth (dither bits < target bits): new = min(new, max_dithered), then
//     << (target - dither bits), or for low dither bits: (int)(new * max_target / max_dithered + 0.5f)
//   dst = min(new, max_target)
// The multiply-adds are fused like GCC contracts the C expressions on aarch64 (the x86 SIMD paths multiply
// and add separately).
// Differences from C and Intel:
// - corr: one 16 byte row of the dither matrix per line (the cycle of all matrices divides 16), the same rows
//   as the Intel paths use; C indexes the small matrix per pixel.
// - (src + corr) >> dither_bit_diff: Intel and C compute it in 32 bit (src + corr can exceed 16 bits).
//   Here it stays in 16 bit lanes: vhaddq_u16 is (src + corr) >> 1 without overflow, then >> (dither_bit_diff - 1),
//   floor(floor(a / 2) / 2^(n-1)) == floor(a / 2^n). Only the range conversion and the low dither bit paths need float.
// - Intel processes the row rounded up to 16 pixels (relying on the frame alignment), here the vector loop
//   stops at the last full 16 pixels and the rest is done per pixel with the C formulas.

#include "convert_bits_neon.h"
#include "../convert_bits.h"
#include "../convert_helper.h"
#include <avs/config.h>
#include <avs/minmax.h>
#include <cmath>
#include <cstdint>

#include <arm_neon.h>

template<typename pixel_t_s, typename pixel_t_d, bool chroma, bool fulls, bool fulld, bool TEMPLATE_NEED_BACKSCALE, bool TEMPLATE_LOW_DITHER_BITDEPTH>
static void do_convert_ordered_dither_uint_neon(const BYTE* srcp8, BYTE* dstp8, int src_rowsize, int src_height, int src_pitch, int dst_pitch, int source_bitdepth, int target_bitdepth, int dither_target_bitdepth)
{
  const pixel_t_s* srcp = reinterpret_cast<const pixel_t_s*>(srcp8);
  pixel_t_d* dstp = reinterpret_cast<pixel_t_d*>(dstp8);
  dst_pitch = dst_pitch / sizeof(pixel_t_d);
  src_pitch = src_pitch / sizeof(pixel_t_s);
  const int src_width = src_rowsize / sizeof(pixel_t_s);

  if constexpr (sizeof(pixel_t_s) == 1)
    source_bitdepth = 8;
  if constexpr (sizeof(pixel_t_d) == 1) {
    target_bitdepth = 8;
    if (!TEMPLATE_NEED_BACKSCALE)
      dither_target_bitdepth = 8;
  }

  const int max_pixel_value_target = (1 << target_bitdepth) - 1;
  const int max_pixel_value_dithered = (1 << dither_target_bitdepth) - 1;
  const bool odd_diff = (source_bitdepth - dither_target_bitdepth) & 1;
  const int dither_bit_diff = (source_bitdepth - dither_target_bitdepth); // >= 1
  const int dither_order = (dither_bit_diff + 1) / 2;
  const int dither_mask = (1 << dither_order) - 1;
  // matrix rows of 16 bytes, the cycle (2, 4, 8, 16) is repeated
  const BYTE* matrix16;
  switch (dither_order) {
  case 1: matrix16 = reinterpret_cast<const BYTE*>(odd_diff ? dither2x2a_data_sse2 : dither2x2_data_sse2); break;
  case 2: matrix16 = reinterpret_cast<const BYTE*>(odd_diff ? dither4x4a_data_sse2 : dither4x4_data_sse2); break;
  case 3: matrix16 = reinterpret_cast<const BYTE*>(odd_diff ? dither8x8a_data_sse2 : dither8x8_data_sse2); break;
  case 4: matrix16 = reinterpret_cast<const BYTE*>(odd_diff ? dither16x16a_data : dither16x16_data); break;
  default: return; // n/a
  }

  const int bitdiff_between_dither_and_target = target_bitdepth - dither_target_bitdepth;
  const float half_maxcorr_value = ((1 << dither_bit_diff) - 1) / 2.0f;
  const int source_max = (1 << source_bitdepth) - 1;

  // range conversion is done at the source bit depth
  bits_conv_constants d;
  get_bits_conv_constants(d, chroma, fulls, fulld, source_bitdepth, source_bitdepth);
  const float dst_offset_plus_round = d.dst_offset + 0.5f;
  const float mul_factor_backfromlowdither = (float)max_pixel_value_target / max_pixel_value_dithered;

  const int32x4_t src_offset_v = vdupq_n_s32(d.src_offset_i);
  const float32x4_t mul_factor_v = vdupq_n_f32(d.mul_factor);
  const float32x4_t dst_offset_plus_round_v = vdupq_n_f32(dst_offset_plus_round);
  const uint16x8_t source_max_v = vdupq_n_u16(static_cast<uint16_t>(source_max));
  const float32x4_t half_maxcorr_v = vdupq_n_f32(half_maxcorr_value);
  const int16x8_t shr_diff_minus1 = vdupq_n_s16(static_cast<int16_t>(-(dither_bit_diff - 1)));
  const int32x4_t shr_diff = vdupq_n_s32(-dither_bit_diff);
  const uint16x8_t max_dithered_v = vdupq_n_u16(static_cast<uint16_t>(max_pixel_value_dithered));
  const int16x8_t shl_backscale = vdupq_n_s16(static_cast<int16_t>(bitdiff_between_dither_and_target));
  const float32x4_t mul_back_v = vdupq_n_f32(mul_factor_backfromlowdither);
  const float32x4_t half_v = vdupq_n_f32(0.5f);
  const uint16x8_t max_target_v = vdupq_n_u16(static_cast<uint16_t>(max_pixel_value_target));

  // fulls != fulld: (int)((src - src_offset) * mul_factor + dst_offset + 0.5f), clamped to 0..source_max
  auto range_convert = [&](uint16x8_t s) {
    const int32x4_t lo = vsubq_s32(vreinterpretq_s32_u32(vmovl_u16(vget_low_u16(s))), src_offset_v);
    const int32x4_t hi = vsubq_s32(vreinterpretq_s32_u32(vmovl_high_u16(s)), src_offset_v);
    const int32x4_t r_lo = vcvtq_s32_f32(vfmaq_f32(dst_offset_plus_round_v, vcvtq_f32_s32(lo), mul_factor_v));
    const int32x4_t r_hi = vcvtq_s32_f32(vfmaq_f32(dst_offset_plus_round_v, vcvtq_f32_s32(hi), mul_factor_v));
    return vminq_u16(vcombine_u16(vqmovun_s32(r_lo), vqmovun_s32(r_hi)), source_max_v); // < 0 -> 0
  };

  // the dithered value in 16 bit lanes
  auto dither8 = [&](uint16x8_t s, uint8x8_t corr8) {
    if constexpr (TEMPLATE_LOW_DITHER_BITDEPTH) {
      const uint16x8_t c = vmovl_u8(corr8);
      const float32x4_t corr_lo = vsubq_f32(vcvtq_f32_u32(vmovl_u16(vget_low_u16(c))), half_maxcorr_v);
      const float32x4_t corr_hi = vsubq_f32(vcvtq_f32_u32(vmovl_high_u16(c)), half_maxcorr_v);
      const int32x4_t n_lo = vshlq_s32(vcvtq_s32_f32(vaddq_f32(vcvtq_f32_u32(vmovl_u16(vget_low_u16(s))), corr_lo)), shr_diff);
      const int32x4_t n_hi = vshlq_s32(vcvtq_s32_f32(vaddq_f32(vcvtq_f32_u32(vmovl_high_u16(s)), corr_hi)), shr_diff);
      return vcombine_u16(vqmovun_s32(n_lo), vqmovun_s32(n_hi));
    }
    else {
      return vshlq_u16(vhaddq_u16(s, vmovl_u8(corr8)), shr_diff_minus1);
    }
  };

  auto backscale = [&](uint16x8_t n) {
    if constexpr (TEMPLATE_NEED_BACKSCALE) {
      n = vminq_u16(n, max_dithered_v);
      if constexpr (TEMPLATE_LOW_DITHER_BITDEPTH) {
        const uint32x4_t r_lo = vcvtq_u32_f32(vfmaq_f32(half_v, vcvtq_f32_u32(vmovl_u16(vget_low_u16(n))), mul_back_v));
        const uint32x4_t r_hi = vcvtq_u32_f32(vfmaq_f32(half_v, vcvtq_f32_u32(vmovl_high_u16(n)), mul_back_v));
        n = vcombine_u16(vqmovn_u32(r_lo), vqmovn_u32(r_hi));
      }
      else {
        n = vshlq_u16(n, shl_backscale);
      }
    }
    return vminq_u16(n, max_target_v);
  };

  for (int y = 0; y < src_height; y++)
  {
    const BYTE* corr_row = matrix16 + ((y & dither_mask) << 4);
    const uint8x16_t corr = vld1q_u8(corr_row);
    int x = 0;
    for (; x + 16 <= src_width; x += 16)
    {
      uint16x8_t s_lo, s_hi;
      if constexpr (sizeof(pixel_t_s) == 1) {
        const uint8x16_t s = vld1q_u8(srcp + x);
        s_lo = vmovl_u8(vget_low_u8(s));
        s_hi = vmovl_high_u8(s);
      }
      else {
        s_lo = vld1q_u16(srcp + x);
        s_hi = vld1q_u16(srcp + x + 8);
      }
      if constexpr (fulls != fulld) {
        s_lo = range_convert(s_lo);
        s_hi = range_convert(s_hi);
      }
      const uint16x8_t n_lo = backscale(dither8(s_lo, vget_low_u8(corr)));
      const uint16x8_t n_hi = backscale(dither8(s_hi, vget_high_u8(corr)));
      if constexpr (sizeof(pixel_t_d) == 1) {
        vst1q_u8(dstp + x, vcombine_u8(vmovn_u16(n_lo), vmovn_u16(n_hi))); // <= 255 after the min
      }
      else {
        vst1q_u16(dstp + x, n_lo);
        vst1q_u16(dstp + x + 8, n_hi);
      }
    }
    // rest of the row: the C formulas, explicit fma where GCC contracts the C expression
    for (; x < src_width; x++)
    {
      const int corr_s = corr_row[x & 15];
      int src_pixel = srcp[x];
      if constexpr (fulls != fulld) {
        const float val = std::fma((float)(srcp[x] - d.src_offset_i), d.mul_factor, dst_offset_plus_round);
        src_pixel = clamp((int)val, 0, source_max);
      }
      int new_pixel;
      if constexpr (TEMPLATE_LOW_DITHER_BITDEPTH) {
        const float corr_f = corr_s - half_maxcorr_value;
        new_pixel = (int)(src_pixel + corr_f) >> dither_bit_diff;
      }
      else
        new_pixel = (src_pixel + corr_s) >> dither_bit_diff;
      if constexpr (TEMPLATE_NEED_BACKSCALE) {
        new_pixel = min(new_pixel, max_pixel_value_dithered);
        if constexpr (TEMPLATE_LOW_DITHER_BITDEPTH)
          new_pixel = (int)std::fma((float)new_pixel, mul_factor_backfromlowdither, 0.5f);
        else
          new_pixel = new_pixel << bitdiff_between_dither_and_target;
      }
      dstp[x] = (pixel_t_d)(max(min(new_pixel, max_pixel_value_target), 0));
    }
    dstp += dst_pitch;
    srcp += src_pitch;
  }
}

template<typename pixel_t_s, typename pixel_t_d, bool chroma, bool fulls, bool fulld>
void convert_ordered_dither_uint_neon(const BYTE* srcp8, BYTE* dstp8, int src_rowsize, int src_height, int src_pitch, int dst_pitch, int source_bitdepth, int target_bitdepth, int dither_target_bitdepth)
{
  const bool need_backscale = target_bitdepth != dither_target_bitdepth; // dither to x, target to y
  const bool low_dither_bitdepth = dither_target_bitdepth < 8; // 1-7 bits dither targets, need_backscale is always true
  if (need_backscale) {
    if (low_dither_bitdepth)
      do_convert_ordered_dither_uint_neon<pixel_t_s, pixel_t_d, chroma, fulls, fulld, true, true>(srcp8, dstp8, src_rowsize, src_height, src_pitch, dst_pitch, source_bitdepth, target_bitdepth, dither_target_bitdepth);
    else
      do_convert_ordered_dither_uint_neon<pixel_t_s, pixel_t_d, chroma, fulls, fulld, true, false>(srcp8, dstp8, src_rowsize, src_height, src_pitch, dst_pitch, source_bitdepth, target_bitdepth, dither_target_bitdepth);
  }
  else {
    do_convert_ordered_dither_uint_neon<pixel_t_s, pixel_t_d, chroma, fulls, fulld, false, false>(srcp8, dstp8, src_rowsize, src_height, src_pitch, dst_pitch, source_bitdepth, target_bitdepth, dither_target_bitdepth);
  }
}

#define convert_ordered_dither_uint_neon_functions(uint_X_t, uint_X_dest_t) \
template void convert_ordered_dither_uint_neon<uint_X_t, uint_X_dest_t, false, false, false>(const BYTE* srcp, BYTE* dstp, int src_rowsize, int src_height, int src_pitch, int dst_pitch, int source_bitdepth, int target_bitdepth, int dither_target_bitdepth); \
template void convert_ordered_dither_uint_neon<uint_X_t, uint_X_dest_t, false, false, true>(const BYTE* srcp, BYTE* dstp, int src_rowsize, int src_height, int src_pitch, int dst_pitch, int source_bitdepth, int target_bitdepth, int dither_target_bitdepth); \
template void convert_ordered_dither_uint_neon<uint_X_t, uint_X_dest_t, false, true, false>(const BYTE* srcp, BYTE* dstp, int src_rowsize, int src_height, int src_pitch, int dst_pitch, int source_bitdepth, int target_bitdepth, int dither_target_bitdepth); \
template void convert_ordered_dither_uint_neon<uint_X_t, uint_X_dest_t, false, true, true>(const BYTE* srcp, BYTE* dstp, int src_rowsize, int src_height, int src_pitch, int dst_pitch, int source_bitdepth, int target_bitdepth, int dither_target_bitdepth); \
template void convert_ordered_dither_uint_neon<uint_X_t, uint_X_dest_t, true, false, false>(const BYTE* srcp, BYTE* dstp, int src_rowsize, int src_height, int src_pitch, int dst_pitch, int source_bitdepth, int target_bitdepth, int dither_target_bitdepth); \
template void convert_ordered_dither_uint_neon<uint_X_t, uint_X_dest_t, true, false, true>(const BYTE* srcp, BYTE* dstp, int src_rowsize, int src_height, int src_pitch, int dst_pitch, int source_bitdepth, int target_bitdepth, int dither_target_bitdepth); \
template void convert_ordered_dither_uint_neon<uint_X_t, uint_X_dest_t, true, true, false>(const BYTE* srcp, BYTE* dstp, int src_rowsize, int src_height, int src_pitch, int dst_pitch, int source_bitdepth, int target_bitdepth, int dither_target_bitdepth); \
template void convert_ordered_dither_uint_neon<uint_X_t, uint_X_dest_t, true, true, true>(const BYTE* srcp, BYTE* dstp, int src_rowsize, int src_height, int src_pitch, int dst_pitch, int source_bitdepth, int target_bitdepth, int dither_target_bitdepth);

convert_ordered_dither_uint_neon_functions(uint8_t, uint8_t)
convert_ordered_dither_uint_neon_functions(uint16_t, uint8_t)
convert_ordered_dither_uint_neon_functions(uint16_t, uint16_t)

#undef convert_ordered_dither_uint_neon_functions
