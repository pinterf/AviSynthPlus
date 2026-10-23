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

// Blur/Sharpen horizontal pass, planar, in-place: the NEON pair of af_horizontal_planar_c / _float_c.
// out[x] = f(in[x-1], in[x], in[x+1]), the edge pixel is repeated at both ends (like C).
//
// Integer kernel in the "pivot" form of focus.h, exactly equal to the C linear form:
//   center + (((left + right - 2*center) * outer_weight_15 + 0x4000) >> 15), clipped
// vqrdmulhq_s16 is (a*b + 0x4000) >> 15 (pmulhrsw), it saturates only for -32768 * -32768, which never occurs.
//
// Differences from the C and the Intel SIMD versions (the results are the same, bit by bit):
// - Neighbours. C walks the row keeping the original left pixel in a variable. Intel reloads the
//   overlapping data unaligned (x-1 and x+1) before storing the block. Here every pixel is loaded once:
//   the previous and the next original vectors are kept in registers, vext builds the left/right
//   neighbour vectors. The next block is loaded before the current one is stored (in-place).
// - Right edge. Intel does the last block in SIMD only when the width is mod16, otherwise in C.
//   Here the last full block is always SIMD, its right neighbour is the next original pixel or, at the
//   row end, the repeated edge pixel; only the remaining width % 16 (8 bit), % 8 (16 bit), % 4 (float)
//   pixels are done per pixel, with the C formulas.
// - 10-14 bit lanes. C uses int, Intel 32 bit lanes for all 10-16 bit. Here 10-14 bit stay in
//   16 bit lanes like 8 bit (8 pixels per multiply instead of 4), only 16 bit needs 32 bit lanes.
//   Range, with outer_weight_15 in -16384..10923 (Sharpen 1.0 .. Blur 1.58, so |m| <= |d| / 2):
//     d = left + right - 2*center: +-(2*max)        14 bit: +-32766, fits int16
//     center + m: -max .. 2*max (Sharpen extremes)  14 bit: -16383 .. 32766, fits int16
//   16 bit: d is up to +-131070, int32 is needed.

#include "focus_neon.h"
#include "../focus.h"
#include "../../core/internal.h"
#include <avs/config.h>
#include <cmath>
#include <cstdlib>
#include <cstdint>

#include <arm_neon.h>

// --- scalar reference for the row ends: integer af_kernel_int (focus.h), the same as focus.cpp ---

static AVS_FORCEINLINE float af_pixel_f(float left, float center, float right, float center_weight, float outer_weight)
{
  // explicit fma: GCC contracts the C expression center*cw + (left+right)*ow as fma(center, cw, (left+right)*ow),
  // a plain expression here may be contracted the other way
  return std::fma(center, center_weight, (left + right) * outer_weight);
}

// --- 8 bit ---

static AVS_FORCEINLINE uint8x16_t af_block_8(uint8x16_t left, uint8x16_t center, uint8x16_t right, int16x8_t ow15)
{
  // d = left + right - 2*center in int16, result = center + rounding_mulhi(d, ow15), saturated to 0..255
  const int16x8_t c_lo = vreinterpretq_s16_u16(vmovl_u8(vget_low_u8(center)));
  const int16x8_t c_hi = vreinterpretq_s16_u16(vmovl_u8(vget_high_u8(center)));
  const int16x8_t lr_lo = vreinterpretq_s16_u16(vaddl_u8(vget_low_u8(left), vget_low_u8(right)));
  const int16x8_t lr_hi = vreinterpretq_s16_u16(vaddl_u8(vget_high_u8(left), vget_high_u8(right)));
  const int16x8_t d_lo = vsubq_s16(lr_lo, vshlq_n_s16(c_lo, 1));
  const int16x8_t d_hi = vsubq_s16(lr_hi, vshlq_n_s16(c_hi, 1));
  const int16x8_t r_lo = vaddq_s16(c_lo, vqrdmulhq_s16(d_lo, ow15));
  const int16x8_t r_hi = vaddq_s16(c_hi, vqrdmulhq_s16(d_hi, ow15));
  return vcombine_u8(vqmovun_s16(r_lo), vqmovun_s16(r_hi));
}

void af_horizontal_planar_neon(BYTE* dstp, size_t height, size_t pitch, size_t row_size, size_t half_amount)
{
  const int ow15_i = af_outer_weight_q15(half_amount);
  const int16x8_t ow15 = vdupq_n_s16(static_cast<int16_t>(ow15_i));
  const size_t width = row_size;

  for (size_t y = 0; y < height; y++) {
    uint8_t* p = dstp;
    size_t x = 0;
    uint8_t left_orig = p[0]; // original value of p[x-1], the edge is repeated
    if (width >= 16) {
      uint8x16_t prev = vdupq_n_u8(p[0]);
      uint8x16_t cur = vld1q_u8(p);
      for (; x + 32 <= width; x += 16) {
        const uint8x16_t next = vld1q_u8(p + x + 16); // still original
        vst1q_u8(p + x, af_block_8(vextq_u8(prev, cur, 15), cur, vextq_u8(cur, next, 1), ow15));
        prev = cur;
        cur = next;
      }
      // last full block: only lane 0 of next is used, the next original pixel or the repeated edge
      const uint8x16_t next = vdupq_n_u8(x + 16 < width ? p[x + 16] : vgetq_lane_u8(cur, 15));
      vst1q_u8(p + x, af_block_8(vextq_u8(prev, cur, 15), cur, vextq_u8(cur, next, 1), ow15));
      left_orig = vgetq_lane_u8(cur, 15);
      x += 16;
    }
    for (; x < width; x++) {
      const int center = p[x];
      const int right = x + 1 < width ? p[x + 1] : center;
      p[x] = static_cast<uint8_t>(af_kernel_int(left_orig, center, right, ow15_i, 255));
      left_orig = static_cast<uint8_t>(center);
    }
    dstp += pitch;
  }
}

// --- 10-16 bit ---

// 10-14 bit (lessThan16bit): 16 bit lanes, ranges see the top of the file
static AVS_FORCEINLINE uint16x8_t af_block_16_lanes16(uint16x8_t left, uint16x8_t center, uint16x8_t right, int16x8_t ow15, int16x8_t maxv)
{
  const int16x8_t c = vreinterpretq_s16_u16(center);
  const int16x8_t lr = vreinterpretq_s16_u16(vaddq_u16(left, right));
  const int16x8_t d = vsubq_s16(lr, vshlq_n_s16(c, 1));
  int16x8_t r = vaddq_s16(c, vqrdmulhq_s16(d, ow15));
  r = vminq_s16(vmaxq_s16(r, vdupq_n_s16(0)), maxv);
  return vreinterpretq_u16_s16(r);
}

// 16 bit: 32 bit lanes
static AVS_FORCEINLINE uint16x4_t af_half_16_lanes32(uint16x4_t left, uint16x4_t center, uint16x4_t right, int32x4_t ow15, int32x4_t maxv)
{
  const int32x4_t c = vreinterpretq_s32_u32(vmovl_u16(center));
  const int32x4_t lr = vreinterpretq_s32_u32(vaddl_u16(left, right));
  const int32x4_t d = vsubq_s32(lr, vshlq_n_s32(c, 1));
  int32x4_t r = vaddq_s32(c, vshrq_n_s32(vmlaq_s32(vdupq_n_s32(0x4000), d, ow15), 15)); // 0x4000: rounder
  r = vminq_s32(vmaxq_s32(r, vdupq_n_s32(0)), maxv);
  return vmovn_u32(vreinterpretq_u32_s32(r));
}

template<bool lessThan16bit>
static void af_horizontal_planar_uint16_t_neon_impl(BYTE* dstp8, size_t height, size_t pitch, size_t row_size, size_t half_amount, int bits_per_pixel)
{
  const int ow15_i = af_outer_weight_q15(half_amount);
  const int max_pixel_value = (1 << bits_per_pixel) - 1;
  const int16x8_t ow15_16 = vdupq_n_s16(static_cast<int16_t>(ow15_i));
  const int16x8_t maxv_16 = vdupq_n_s16(static_cast<int16_t>(max_pixel_value));
  const int32x4_t ow15_32 = vdupq_n_s32(ow15_i);
  const int32x4_t maxv_32 = vdupq_n_s32(max_pixel_value);
  const size_t width = row_size / sizeof(uint16_t);

  auto block = [&](uint16x8_t left, uint16x8_t cur, uint16x8_t right) {
    if constexpr (lessThan16bit)
      return af_block_16_lanes16(left, cur, right, ow15_16, maxv_16);
    else
      return vcombine_u16(
        af_half_16_lanes32(vget_low_u16(left), vget_low_u16(cur), vget_low_u16(right), ow15_32, maxv_32),
        af_half_16_lanes32(vget_high_u16(left), vget_high_u16(cur), vget_high_u16(right), ow15_32, maxv_32));
  };

  for (size_t y = 0; y < height; y++) {
    uint16_t* p = reinterpret_cast<uint16_t*>(dstp8);
    size_t x = 0;
    uint16_t left_orig = p[0];
    if (width >= 8) {
      uint16x8_t prev = vdupq_n_u16(p[0]);
      uint16x8_t cur = vld1q_u16(p);
      for (; x + 16 <= width; x += 8) {
        const uint16x8_t next = vld1q_u16(p + x + 8);
        vst1q_u16(p + x, block(vextq_u16(prev, cur, 7), cur, vextq_u16(cur, next, 1)));
        prev = cur;
        cur = next;
      }
      const uint16x8_t next = vdupq_n_u16(x + 8 < width ? p[x + 8] : vgetq_lane_u16(cur, 7));
      vst1q_u16(p + x, block(vextq_u16(prev, cur, 7), cur, vextq_u16(cur, next, 1)));
      left_orig = vgetq_lane_u16(cur, 7);
      x += 8;
    }
    for (; x < width; x++) {
      const int center = p[x];
      const int right = x + 1 < width ? p[x + 1] : center;
      p[x] = static_cast<uint16_t>(af_kernel_int(left_orig, center, right, ow15_i, max_pixel_value));
      left_orig = static_cast<uint16_t>(center);
    }
    dstp8 += pitch;
  }
}

void af_horizontal_planar_uint16_t_neon(BYTE* dstp8, size_t height, size_t pitch, size_t row_size, size_t half_amount, int bits_per_pixel)
{
  if (bits_per_pixel <= 14)
    af_horizontal_planar_uint16_t_neon_impl<true>(dstp8, height, pitch, row_size, half_amount, bits_per_pixel);
  else
    af_horizontal_planar_uint16_t_neon_impl<false>(dstp8, height, pitch, row_size, half_amount, bits_per_pixel);
}

// --- float ---

static AVS_FORCEINLINE float32x4_t af_block_f(float32x4_t left, float32x4_t cur, float32x4_t right, float32x4_t cw, float32x4_t ow)
{
  // center*cw + (left+right)*ow, fused like GCC contracts the C expression: fma(center, cw, (left+right)*ow)
  return vfmaq_f32(vmulq_f32(vaddq_f32(left, right), ow), cur, cw);
}

void af_horizontal_planar_float_neon(BYTE* dstp8, size_t height, size_t pitch, size_t row_size, float amount)
{
  const float center_weight = amount;
  const float outer_weight = (1.0f - amount) / 2.0f;
  const float32x4_t cw = vdupq_n_f32(center_weight);
  const float32x4_t ow = vdupq_n_f32(outer_weight);
  const size_t width = row_size / sizeof(float);

  for (size_t y = 0; y < height; y++) {
    float* p = reinterpret_cast<float*>(dstp8);
    size_t x = 0;
    float left_orig = p[0];
    if (width >= 4) {
      float32x4_t prev = vdupq_n_f32(p[0]);
      float32x4_t cur = vld1q_f32(p);
      for (; x + 8 <= width; x += 4) {
        const float32x4_t next = vld1q_f32(p + x + 4);
        vst1q_f32(p + x, af_block_f(vextq_f32(prev, cur, 3), cur, vextq_f32(cur, next, 1), cw, ow));
        prev = cur;
        cur = next;
      }
      const float32x4_t next = vdupq_n_f32(x + 4 < width ? p[x + 4] : vgetq_lane_f32(cur, 3));
      vst1q_f32(p + x, af_block_f(vextq_f32(prev, cur, 3), cur, vextq_f32(cur, next, 1), cw, ow));
      left_orig = vgetq_lane_f32(cur, 3);
      x += 4;
    }
    for (; x < width; x++) {
      const float center = p[x];
      const float right = x + 1 < width ? p[x + 1] : center;
      p[x] = af_pixel_f(left_orig, center, right, center_weight, outer_weight);
      left_orig = center;
    }
    dstp8 += pitch;
  }
}

// --- TemporalSoften: accumulate_line ---
// The NEON pair of accumulate_line_c, same arithmetic, bit-identical:
// sum = current + for each other frame (p if |p - current| <= threshold, else current; maxThreshold: always p),
// added in the C order (planes-1 .. 0, matters for float only), then
//   8 bit:    (sum * div + 16384) >> 15, div = 32768 / (planes + 1); sum <= 15 * 255 fits int16:
//             vqrdmulhq_s16 is exactly this ((a*b + 0x4000) >> 15)
//   10-16 bit: (int)(sum * (1.0f / (planes + 1)) + 0.5f), fused like GCC contracts the C expression on aarch64.
//             The x86 SIMD paths multiply and add separately: same result, for planes + 1 = 2..15 the float
//             reciprocal is exact or slightly above 1/n, so no product is rounded across an x.5 boundary.
//   float:    sum / (planes + 1)
// Intel has no float path, the float C loop is not vectorized by GCC either (the plane loop is the inner one).
// The rest of the row (width % 16, % 8, % 4 pixels) is done per pixel with the C formulas.

template<typename pixel_t, bool maxThreshold>
void accumulate_line_neon(BYTE* c_plane8, const BYTE** planeP, int planes, size_t rowsize, BYTE threshold8, int div, int bits_per_pixel)
{
  pixel_t* c_plane = reinterpret_cast<pixel_t*>(c_plane8);
  const size_t width = rowsize / sizeof(pixel_t);
  size_t x = 0;

  if constexpr (sizeof(pixel_t) == 1) {
    const uint8x16_t th = vdupq_n_u8(threshold8);
    const int16x8_t divv = vdupq_n_s16(static_cast<int16_t>(div));
    for (; x + 16 <= width; x += 16) {
      const uint8x16_t cur = vld1q_u8(c_plane + x);
      uint16x8_t lo = vmovl_u8(vget_low_u8(cur));
      uint16x8_t hi = vmovl_high_u8(cur);
      for (int plane = planes - 1; plane >= 0; plane--) {
        uint8x16_t p = vld1q_u8(planeP[plane] + x);
        if constexpr (!maxThreshold)
          p = vbslq_u8(vcleq_u8(vabdq_u8(p, cur), th), p, cur);
        lo = vaddw_u8(lo, vget_low_u8(p));
        hi = vaddw_high_u8(hi, p);
      }
      const int16x8_t r_lo = vqrdmulhq_s16(vreinterpretq_s16_u16(lo), divv);
      const int16x8_t r_hi = vqrdmulhq_s16(vreinterpretq_s16_u16(hi), divv);
      vst1q_u8(c_plane + x, vcombine_u8(vqmovun_s16(r_lo), vqmovun_s16(r_hi)));
    }
    for (; x < width; x++) {
      const int current = c_plane[x];
      int sum = current;
      for (int plane = planes - 1; plane >= 0; plane--) {
        const int p = planeP[plane][x];
        sum += (maxThreshold || std::abs(current - p) <= threshold8) ? p : current;
      }
      c_plane[x] = static_cast<uint8_t>((sum * div + 16384) >> 15);
    }
  }
  else if constexpr (sizeof(pixel_t) == 2) {
    const int threshold = threshold8 * (1 << (bits_per_pixel - 8));
    const float average_multiplier = 1.0f / static_cast<float>(planes + 1);
    const uint16x8_t th = vdupq_n_u16(static_cast<uint16_t>(threshold));
    const float32x4_t mul = vdupq_n_f32(average_multiplier);
    const float32x4_t half = vdupq_n_f32(0.5f);
    for (; x + 8 <= width; x += 8) {
      const uint16x8_t cur = vld1q_u16(c_plane + x);
      uint32x4_t lo = vmovl_u16(vget_low_u16(cur));
      uint32x4_t hi = vmovl_high_u16(cur);
      for (int plane = planes - 1; plane >= 0; plane--) {
        uint16x8_t p = vld1q_u16(reinterpret_cast<const uint16_t*>(planeP[plane]) + x);
        if constexpr (!maxThreshold)
          p = vbslq_u16(vcleq_u16(vabdq_u16(p, cur), th), p, cur);
        lo = vaddw_u16(lo, vget_low_u16(p));
        hi = vaddw_high_u16(hi, p);
      }
      const uint32x4_t r_lo = vcvtq_u32_f32(vfmaq_f32(half, vcvtq_f32_u32(lo), mul)); // truncate like (int)
      const uint32x4_t r_hi = vcvtq_u32_f32(vfmaq_f32(half, vcvtq_f32_u32(hi), mul));
      vst1q_u16(c_plane + x, vcombine_u16(vqmovn_u32(r_lo), vqmovn_u32(r_hi)));
    }
    for (; x < width; x++) {
      const int current = c_plane[x];
      int sum = current;
      for (int plane = planes - 1; plane >= 0; plane--) {
        const int p = reinterpret_cast<const uint16_t*>(planeP[plane])[x];
        sum += (maxThreshold || std::abs(current - p) <= threshold) ? p : current;
      }
      c_plane[x] = static_cast<uint16_t>(static_cast<int>(std::fma(static_cast<float>(sum), average_multiplier, 0.5f)));
    }
  }
  else {
    const float threshold = threshold8 / 255.0f;
    const float divisor = static_cast<float>(planes + 1);
    const float32x4_t th = vdupq_n_f32(threshold);
    const float32x4_t divisorv = vdupq_n_f32(divisor);
    for (; x + 4 <= width; x += 4) {
      const float32x4_t cur = vld1q_f32(c_plane + x);
      float32x4_t sum = cur;
      for (int plane = planes - 1; plane >= 0; plane--) {
        float32x4_t p = vld1q_f32(reinterpret_cast<const float*>(planeP[plane]) + x);
        if constexpr (!maxThreshold)
          p = vbslq_f32(vcleq_f32(vabdq_f32(cur, p), th), p, cur);
        sum = vaddq_f32(sum, p);
      }
      vst1q_f32(c_plane + x, vdivq_f32(sum, divisorv));
    }
    for (; x < width; x++) {
      const float current = c_plane[x];
      float sum = current;
      for (int plane = planes - 1; plane >= 0; plane--) {
        const float p = reinterpret_cast<const float*>(planeP[plane])[x];
        sum += (maxThreshold || std::abs(current - p) <= threshold) ? p : current;
      }
      c_plane[x] = sum / divisor;
    }
  }
}

template void accumulate_line_neon<uint8_t, false>(BYTE* c_plane, const BYTE** planeP, int planes, size_t rowsize, BYTE threshold, int div, int bits_per_pixel);
template void accumulate_line_neon<uint8_t, true>(BYTE* c_plane, const BYTE** planeP, int planes, size_t rowsize, BYTE threshold, int div, int bits_per_pixel);
template void accumulate_line_neon<uint16_t, false>(BYTE* c_plane, const BYTE** planeP, int planes, size_t rowsize, BYTE threshold, int div, int bits_per_pixel);
template void accumulate_line_neon<uint16_t, true>(BYTE* c_plane, const BYTE** planeP, int planes, size_t rowsize, BYTE threshold, int div, int bits_per_pixel);
template void accumulate_line_neon<float, false>(BYTE* c_plane, const BYTE** planeP, int planes, size_t rowsize, BYTE threshold, int div, int bits_per_pixel);
template void accumulate_line_neon<float, true>(BYTE* c_plane, const BYTE** planeP, int planes, size_t rowsize, BYTE threshold, int div, int bits_per_pixel);

// Sum of absolute differences, 8 / 16 bit: the NEON pair of calculate_sad_c (focus.cpp), get_sad_c /
// get_sad_rgb_c (conditional_functions.cpp) and calculate_sad_8_or_16_sse2. Integer, so any order gives the
// same total. rowsize is in bytes. packedRGB3264: the alpha (every 4th component) is skipped, like get_sad_rgb_c:
// the absolute difference is masked (the mask is byte / lane granular, the same as masking both inputs).
// 32 bytes per step into two independent accumulators (one accumulator chain is latency bound).
//   8 bit: vabd + vpadal into 16 bit lanes, flushed into 32 bit lanes every 2 x 64 steps (at most 64 * 2 * 255).
//   16 bit: vabal / vabal_high into 32 bit lanes (no pairwise add); RGB64 (masked): vabd + and + vpadal.
// Row totals are added as 64 bit. The rest of the row (< 32 bytes): one 16 byte step, then per component.
template<typename pixel_t, bool packedRGB3264>
int64_t calculate_sad_8_or_16_neon(const BYTE* cur_ptr, const BYTE* other_ptr, int cur_pitch, int other_pitch, size_t rowsize, size_t height)
{
  const size_t mod32 = rowsize / 32 * 32;
  const size_t mod16 = rowsize / 16 * 16;
  const size_t width = rowsize / sizeof(pixel_t); // components
  const uint8x16_t mask8 = vreinterpretq_u8_u32(vdupq_n_u32(0x00FFFFFF));              // RGB32: B G R kept, A zeroed
  const uint16x8_t mask16 = vreinterpretq_u16_u64(vdupq_n_u64(0x0000FFFFFFFFFFFFull)); // RGB64: B G R kept, A zeroed
  int64_t total = 0;
  for (size_t y = 0; y < height; ++y) {
    uint32x4_t acc32a = vdupq_n_u32(0);
    uint32x4_t acc32b = vdupq_n_u32(0);
    size_t x = 0;
    if constexpr (sizeof(pixel_t) == 1) {
      auto absdiff = [&](size_t pos) {
        uint8x16_t d = vabdq_u8(vld1q_u8(cur_ptr + pos), vld1q_u8(other_ptr + pos));
        if constexpr (packedRGB3264)
          d = vandq_u8(d, mask8);
        return d;
      };
      while (x < mod32) {
        uint16x8_t acc16a = vdupq_n_u16(0);
        uint16x8_t acc16b = vdupq_n_u16(0);
        const size_t block_end = x + 64 * 32 < mod32 ? x + 64 * 32 : mod32;
        for (; x < block_end; x += 32) {
          acc16a = vpadalq_u8(acc16a, absdiff(x));
          acc16b = vpadalq_u8(acc16b, absdiff(x + 16));
        }
        acc32a = vpadalq_u16(acc32a, acc16a);
        acc32b = vpadalq_u16(acc32b, acc16b);
      }
      if (x < mod16) {
        acc32a = vpadalq_u16(acc32a, vpaddlq_u8(absdiff(x)));
        x += 16;
      }
    }
    else {
      auto step = [&](size_t pos, uint32x4_t& acc_lo, uint32x4_t& acc_hi) {
        const uint16x8_t a = vld1q_u16(reinterpret_cast<const uint16_t*>(cur_ptr + pos));
        const uint16x8_t b = vld1q_u16(reinterpret_cast<const uint16_t*>(other_ptr + pos));
        if constexpr (packedRGB3264) {
          acc_lo = vpadalq_u16(acc_lo, vandq_u16(vabdq_u16(a, b), mask16));
        }
        else {
          acc_lo = vabal_u16(acc_lo, vget_low_u16(a), vget_low_u16(b));
          acc_hi = vabal_high_u16(acc_hi, a, b);
        }
      };
      for (; x < mod32; x += 32) {
        step(x, acc32a, acc32b);
        step(x + 16, packedRGB3264 ? acc32b : acc32a, packedRGB3264 ? acc32a : acc32b);
      }
      if (x < mod16) {
        step(x, acc32a, acc32b);
        x += 16;
      }
    }
    int64_t rowsum = (int64_t)vaddlvq_u32(acc32a) + (int64_t)vaddlvq_u32(acc32b);
    const pixel_t* p1 = reinterpret_cast<const pixel_t*>(cur_ptr);
    const pixel_t* p2 = reinterpret_cast<const pixel_t*>(other_ptr);
    for (size_t i = x / sizeof(pixel_t); i < width; ++i) {
      if constexpr (packedRGB3264) {
        if ((i & 3) == 3)
          continue; // alpha
      }
      rowsum += std::abs((int)p1[i] - (int)p2[i]);
    }
    total += rowsum;
    cur_ptr += cur_pitch;
    other_ptr += other_pitch;
  }
  return total;
}

template int64_t calculate_sad_8_or_16_neon<uint8_t, false>(const BYTE*, const BYTE*, int, int, size_t, size_t);
template int64_t calculate_sad_8_or_16_neon<uint8_t, true>(const BYTE*, const BYTE*, int, int, size_t, size_t);
template int64_t calculate_sad_8_or_16_neon<uint16_t, false>(const BYTE*, const BYTE*, int, int, size_t, size_t);
template int64_t calculate_sad_8_or_16_neon<uint16_t, true>(const BYTE*, const BYTE*, int, int, size_t, size_t);
