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

// NEON row kernels for the Layer mul / mulovr / lighten / darken C templates (layer.hpp, LAYER_ROW_KERNELS),
// included by layer_neon.cpp only.
// 10-16 bit integer and 32 bit float; 8 bit returns 0 (the C loop, vectorized by GCC/Clang).
// The C loops compile to scalar code there: the 16 bit magic division needs a 32x32->64 multiply,
// and the float lighten/darken with mask has a conditional.
// Bit-identical to C:
//   magic_div_rt<uint16_t>(v) = ((uint64_t)v * div) >> (32 + shift): the high halves of vmull_u32, then >> shift.
//   dst * (max - alpha) + ovr * alpha + half is calculated as half + dst * max - dst * alpha + ovr * alpha,
//   which is the same modulo 2^32 as the C uint32 arithmetic, also for out-of-range pixel / mask values.
//   float: the GCC (and -ffp-contract=fast Clang) order: luma = (b * cb + g * cg) + r * cr fused,
//   dst + (ovr - dst) * alpha fused, alpha = 0 when the threshold isn't met (the blend is still calculated).

#ifndef __Layer_Row_Kernels_NEON_H__
#define __Layer_Row_Kernels_NEON_H__

#include <avs/config.h>
#include "../../layer.h"       // cyb.., cyb_f.., LIGHTEN, MagicDiv (blend_common.h)
#include <arm_neon.h>
#include <cstdint>

namespace layer_neon_detail {

// MagicDiv prepared once per row for magic_div_neon:
//   div: stays scalar, vmull_n_u32 takes the multiplier as a scalar operand.
//   neg_shift: -shift in all lanes. The shift depends on bits_per_pixel (runtime), and NEON has no
//   right shift by a variable count, only vshlq_u32, which shifts right for negative counts.
struct MagicNeon {
  uint32_t div;
  int32x4_t neg_shift;
  explicit MagicNeon(MagicDiv m) : div(m.div), neg_shift(vdupq_n_s32(-(int)m.shift)) {}
};

// magic_div_rt<uint16_t> for 4 uint32 values: ((uint64_t)v * div) >> (32 + shift)
//   32x32->64 products (vmull), their high halves (vuzp2q: the >> 32), then >> shift.
static AVS_FORCEINLINE uint32x4_t magic_div_neon(uint32x4_t v, const MagicNeon& m) {
  const uint64x2_t lo = vmull_n_u32(vget_low_u32(v), m.div);
  const uint64x2_t hi = vmull_high_n_u32(v, m.div);
  return vshlq_u32(vuzp2q_u32(vreinterpretq_u32_u64(lo), vreinterpretq_u32_u64(hi)), m.neg_shift);
}

// (pixel_t)magic_div_rt(lo, hi), truncated to 16 bits like the C cast
static AVS_FORCEINLINE uint16x8_t magic_div_narrow(uint32x4_t lo, uint32x4_t hi, const MagicNeon& m) {
  return vcombine_u16(vmovn_u32(magic_div_neon(lo, m)), vmovn_u32(magic_div_neon(hi, m)));
}

// (pixel_t)magic_div_rt(dst * (max - alpha) + target * alpha + half)
static AVS_FORCEINLINE uint16x8_t blend8_u16(uint16x8_t dst, uint16x8_t target, uint16x8_t alpha, uint16_t max_pixel_value, uint32x4_t half, const MagicNeon& m) {
  uint32x4_t lo = vmlal_n_u16(half, vget_low_u16(dst), max_pixel_value);
  uint32x4_t hi = vmlal_high_n_u16(half, dst, max_pixel_value);
  lo = vmlsl_u16(lo, vget_low_u16(dst), vget_low_u16(alpha));
  hi = vmlsl_high_u16(hi, dst, alpha);
  lo = vmlal_u16(lo, vget_low_u16(target), vget_low_u16(alpha));
  hi = vmlal_high_u16(hi, target, alpha);
  return magic_div_narrow(lo, hi, m);
}

// (pixel_t)magic_div_rt(a * b), no rounding
static AVS_FORCEINLINE uint16x8_t mul8_u16(uint16x8_t a, uint16x8_t b, const MagicNeon& m) {
  return magic_div_narrow(vmull_u16(vget_low_u16(a), vget_low_u16(b)), vmull_high_u16(a, b), m);
}

// magic_div_rt(mask * opacity_i + half)
static AVS_FORCEINLINE uint16x8_t mask_alpha8_u16(uint16x8_t mask, uint16_t opacity_i, uint32x4_t half, const MagicNeon& m) {
  return magic_div_narrow(vmlal_n_u16(half, vget_low_u16(mask), opacity_i), vmlal_high_n_u16(half, mask, opacity_i), m);
}

// (cyb * b + cyg * g + cyr * r) >> 15; the coefficients sum to 32768, the sum fits in 31 bits
static AVS_FORCEINLINE void luma8_u32(uint16x8_t b, uint16x8_t g, uint16x8_t r, uint32x4_t& lo, uint32x4_t& hi) {
  lo = vmlal_n_u16(vmlal_n_u16(vmull_n_u16(vget_low_u16(b), cyb), vget_low_u16(g), cyg), vget_low_u16(r), cyr);
  hi = vmlal_high_n_u16(vmlal_high_n_u16(vmull_high_n_u16(b, cyb), g, cyg), r, cyr);
  lo = vshrq_n_u32(lo, 15);
  hi = vshrq_n_u32(hi, 15);
}

// lighten: ovr > src + thresh, darken: ovr < src - thresh; signed int compare like C, as a 16 bit lane mask
template<int mode>
static AVS_FORCEINLINE uint16x8_t threshold_mask8(uint32x4_t ovr_lo, uint32x4_t ovr_hi, uint32x4_t src_lo, uint32x4_t src_hi, int32x4_t thresh) {
  uint32x4_t c_lo, c_hi;
  if constexpr (mode == LIGHTEN) {
    c_lo = vcgtq_s32(vreinterpretq_s32_u32(ovr_lo), vaddq_s32(vreinterpretq_s32_u32(src_lo), thresh));
    c_hi = vcgtq_s32(vreinterpretq_s32_u32(ovr_hi), vaddq_s32(vreinterpretq_s32_u32(src_hi), thresh));
  }
  else {
    c_lo = vcltq_s32(vreinterpretq_s32_u32(ovr_lo), vsubq_s32(vreinterpretq_s32_u32(src_lo), thresh));
    c_hi = vcltq_s32(vreinterpretq_s32_u32(ovr_hi), vsubq_s32(vreinterpretq_s32_u32(src_hi), thresh));
  }
  return vcombine_u16(vmovn_u32(c_lo), vmovn_u32(c_hi));
}

template<int mode>
static AVS_FORCEINLINE uint32x4_t threshold_mask4_f(float32x4_t ovr, float32x4_t src, float32x4_t thresh) {
  if constexpr (mode == LIGHTEN)
    return vcgtq_f32(ovr, vaddq_f32(src, thresh));
  else
    return vcltq_f32(ovr, vsubq_f32(src, thresh));
}

// cond ? alpha : +0.0f
static AVS_FORCEINLINE float32x4_t select_alpha_f(uint32x4_t cond, float32x4_t alpha) {
  return vreinterpretq_f32_u32(vandq_u32(cond, vreinterpretq_u32_f32(alpha)));
}

// dst + (ovr - dst) * alpha, fused
static AVS_FORCEINLINE float32x4_t blend4_f(float32x4_t dst, float32x4_t ovr, float32x4_t alpha) {
  return vfmaq_f32(dst, vsubq_f32(ovr, dst), alpha);
}

static AVS_FORCEINLINE float32x4_t luma4_f(float32x4_t b, float32x4_t g, float32x4_t r) {
  return vfmaq_n_f32(vfmaq_n_f32(vmulq_n_f32(b, cyb_f), g, cyg_f), r, cyr_f);
}

} // namespace layer_neon_detail

// The YUV kernels are kept out of line: their callers' row loops call the mask row preparation, and
// across calls only the low 64 bits of v8-v15 are preserved. Inlined, GCC hoists the 128 bit constants
// out of the row loop, spills them across the calls, and reloads them in the pixel loop. Out of line,
// they are set up once per row call. One call per row.
#if defined(_MSC_VER) && !defined(__clang__)
#  define LAYER_NEON_ROW_NOINLINE __declspec(noinline)
#else
#  define LAYER_NEON_ROW_NOINLINE __attribute__((noinline))
#endif

struct LayerRowKernelsNeon {
  // layer_yuv_mul_c_inner: one plane, alpha from the prepared mask row or the flat opacity
  template<typename pixel_t, bool is_chroma, bool has_alpha>
  LAYER_NEON_ROW_NOINLINE static int yuv_mul(pixel_t* dstp, const pixel_t* ovrp, const pixel_t* effective_mask_ptr, int width,
    int opacity_i, int max_pixel_value, int half, MagicDiv magic)
  {
    using namespace layer_neon_detail;
    if constexpr (sizeof(pixel_t) != 2)
      return 0;
    else {
      const MagicNeon m(magic);
      const uint32x4_t half_v = vdupq_n_u32(half);
      const uint16x8_t opacity_v = vdupq_n_u16((uint16_t)opacity_i);
      const int wmod8 = width / 8 * 8;
      for (int x = 0; x < wmod8; x += 8) {
        const uint16x8_t dst = vld1q_u16(dstp + x);
        const uint16x8_t ovr = vld1q_u16(ovrp + x);
        uint16x8_t alpha = opacity_v;
        if constexpr (has_alpha)
          alpha = vld1q_u16(effective_mask_ptr + x);
        uint16x8_t target = ovr;
        if constexpr (!is_chroma)
          target = mul8_u16(ovr, dst, m);
        vst1q_u16(dstp + x, blend8_u16(dst, target, alpha, (uint16_t)max_pixel_value, half_v, m));
      }
      return wmod8;
    }
  }

  // layer_yuv_lighten_darken_c, one chroma row: decision on the prepared ovr/src rows, blend U, V (and Y for 444)
  template<int mode, typename pixel_t, bool lumaonly, bool has_alpha, bool update_luma>
  LAYER_NEON_ROW_NOINLINE static int yuv_lighten_darken(pixel_t* dstp, pixel_t* dstp_u, pixel_t* dstp_v,
    const pixel_t* ovrp, const pixel_t* ovrp_u, const pixel_t* ovrp_v,
    const pixel_t* ovr_ptr, const pixel_t* src_ptr, const pixel_t* effective_mask_ptr,
    int cwidth, int opacity_i, int thresh, int max_pixel_value, int half, MagicDiv magic)
  {
    using namespace layer_neon_detail;
    if constexpr (sizeof(pixel_t) != 2)
      return 0;
    else {
      const MagicNeon m(magic);
      const uint32x4_t half_v = vdupq_n_u32(half);
      const uint16x8_t opacity_v = vdupq_n_u16((uint16_t)opacity_i);
      const int32x4_t thresh_v = vdupq_n_s32(thresh);
      const uint16_t maxv = (uint16_t)max_pixel_value;
      const int wmod8 = cwidth / 8 * 8;
      for (int x = 0; x < wmod8; x += 8) {
        // 444: src_ptr is dstp and ovr_ptr is ovrp; src is read before the store
        const uint16x8_t ovr = vld1q_u16(ovr_ptr + x);
        const uint16x8_t src = vld1q_u16(src_ptr + x);
        uint16x8_t alpha = opacity_v;
        if constexpr (has_alpha)
          alpha = mask_alpha8_u16(vld1q_u16(effective_mask_ptr + x), (uint16_t)opacity_i, half_v, m);
        alpha = vandq_u16(alpha, threshold_mask8<mode>(vmovl_u16(vget_low_u16(ovr)), vmovl_high_u16(ovr),
          vmovl_u16(vget_low_u16(src)), vmovl_high_u16(src), thresh_v));
        if constexpr (!lumaonly) {
          vst1q_u16(dstp_u + x, blend8_u16(vld1q_u16(dstp_u + x), vld1q_u16(ovrp_u + x), alpha, maxv, half_v, m));
          vst1q_u16(dstp_v + x, blend8_u16(vld1q_u16(dstp_v + x), vld1q_u16(ovrp_v + x), alpha, maxv, half_v, m));
        }
        if constexpr (update_luma)
          vst1q_u16(dstp + x, blend8_u16(vld1q_u16(dstp + x), vld1q_u16(ovrp + x), alpha, maxv, half_v, m));
      }
      return wmod8;
    }
  }

  // layer_yuv_lighten_darken_f_c, one chroma row
  template<int mode, bool lumaonly, bool has_alpha, bool update_luma>
  LAYER_NEON_ROW_NOINLINE static int yuv_lighten_darken_f(float* dstp, float* dstp_u, float* dstp_v,
    const float* ovrp, const float* ovrp_u, const float* ovrp_v,
    const float* ovr_ptr, const float* src_ptr, const float* effective_mask_ptr,
    int cwidth, float opacity, float thresh)
  {
    using namespace layer_neon_detail;
    const float32x4_t opacity_v = vdupq_n_f32(opacity);
    const float32x4_t thresh_v = vdupq_n_f32(thresh);
    const int wmod4 = cwidth / 4 * 4;
    for (int x = 0; x < wmod4; x += 4) {
      const float32x4_t ovr = vld1q_f32(ovr_ptr + x);
      const float32x4_t src = vld1q_f32(src_ptr + x);
      float32x4_t alpha = opacity_v;
      if constexpr (has_alpha)
        alpha = vmulq_f32(vld1q_f32(effective_mask_ptr + x), opacity_v);
      alpha = select_alpha_f(threshold_mask4_f<mode>(ovr, src, thresh_v), alpha);
      if constexpr (!lumaonly) {
        vst1q_f32(dstp_u + x, blend4_f(vld1q_f32(dstp_u + x), vld1q_f32(ovrp_u + x), alpha));
        vst1q_f32(dstp_v + x, blend4_f(vld1q_f32(dstp_v + x), vld1q_f32(ovrp_v + x), alpha));
      }
      if constexpr (update_luma)
        vst1q_f32(dstp + x, blend4_f(vld1q_f32(dstp + x), vld1q_f32(ovrp + x), alpha));
    }
    return wmod4;
  }

  // layer_planarrgb_lighten_darken_c
  template<int mode, typename pixel_t, bool has_alpha, bool blend_alpha>
  static int planarrgb_lighten_darken(pixel_t* dstp_g, pixel_t* dstp_b, pixel_t* dstp_r, pixel_t* dstp_a,
    const pixel_t* ovrp_g, const pixel_t* ovrp_b, const pixel_t* ovrp_r, const pixel_t* maskp, const pixel_t* alpha_target,
    int width, int opacity_i, int thresh, int max_pixel_value, int half, MagicDiv magic)
  {
    using namespace layer_neon_detail;
    if constexpr (sizeof(pixel_t) != 2)
      return 0;
    else {
      const MagicNeon m(magic);
      const uint32x4_t half_v = vdupq_n_u32(half);
      const uint16x8_t opacity_v = vdupq_n_u16((uint16_t)opacity_i);
      const int32x4_t thresh_v = vdupq_n_s32(thresh);
      const uint16_t maxv = (uint16_t)max_pixel_value;
      const int wmod8 = width / 8 * 8;
      for (int x = 0; x < wmod8; x += 8) {
        uint16x8_t alpha = opacity_v;
        if constexpr (has_alpha)
          alpha = mask_alpha8_u16(vld1q_u16(maskp + x), (uint16_t)opacity_i, half_v, m);
        // overlay: the layer put over the base clip
        const uint16x8_t og = vld1q_u16(ovrp_g + x), ob = vld1q_u16(ovrp_b + x), orr = vld1q_u16(ovrp_r + x);
        // destination: in-place, it holds the base clip pixels, the "src" of the luma decision and of the blend.
        // Loaded before the stores below, like the C reads dstp_r/g/b[x] before writing it.
        const uint16x8_t dg = vld1q_u16(dstp_g + x), db = vld1q_u16(dstp_b + x), dr = vld1q_u16(dstp_r + x);
        uint32x4_t ovr_lo, ovr_hi, src_lo, src_hi;
        luma8_u32(ob, og, orr, ovr_lo, ovr_hi);
        luma8_u32(db, dg, dr, src_lo, src_hi);
        alpha = vandq_u16(alpha, threshold_mask8<mode>(ovr_lo, ovr_hi, src_lo, src_hi, thresh_v));
        vst1q_u16(dstp_r + x, blend8_u16(dr, orr, alpha, maxv, half_v, m));
        vst1q_u16(dstp_g + x, blend8_u16(dg, og, alpha, maxv, half_v, m));
        vst1q_u16(dstp_b + x, blend8_u16(db, ob, alpha, maxv, half_v, m));
        if constexpr (blend_alpha)
          vst1q_u16(dstp_a + x, blend8_u16(vld1q_u16(dstp_a + x), vld1q_u16(alpha_target + x), alpha, maxv, half_v, m));
      }
      return wmod8;
    }
  }

  // layer_planarrgb_lighten_darken_f_c
  template<int mode, bool has_alpha, bool blend_alpha>
  static int planarrgb_lighten_darken_f(float* dstp_g, float* dstp_b, float* dstp_r, float* dstp_a,
    const float* ovrp_g, const float* ovrp_b, const float* ovrp_r, const float* maskp, const float* alpha_target,
    int width, float opacity, float thresh)
  {
    using namespace layer_neon_detail;
    const float32x4_t opacity_v = vdupq_n_f32(opacity);
    const float32x4_t thresh_v = vdupq_n_f32(thresh);
    const int wmod4 = width / 4 * 4;
    for (int x = 0; x < wmod4; x += 4) {
      float32x4_t alpha = opacity_v;
      if constexpr (has_alpha)
        alpha = vmulq_f32(vld1q_f32(maskp + x), opacity_v);
      // overlay: the layer put over the base clip
      const float32x4_t og = vld1q_f32(ovrp_g + x), ob = vld1q_f32(ovrp_b + x), orr = vld1q_f32(ovrp_r + x);
      // destination: in-place, it holds the base clip pixels, the "src" of the luma decision and of the blend.
      // Loaded before the stores below, like the C reads dstp_r/g/b[x] before writing it.
      const float32x4_t dg = vld1q_f32(dstp_g + x), db = vld1q_f32(dstp_b + x), dr = vld1q_f32(dstp_r + x);
      alpha = select_alpha_f(threshold_mask4_f<mode>(luma4_f(ob, og, orr), luma4_f(db, dg, dr), thresh_v), alpha);
      vst1q_f32(dstp_r + x, blend4_f(dr, orr, alpha));
      vst1q_f32(dstp_g + x, blend4_f(dg, og, alpha));
      vst1q_f32(dstp_b + x, blend4_f(db, ob, alpha));
      if constexpr (blend_alpha)
        vst1q_f32(dstp_a + x, blend4_f(vld1q_f32(dstp_a + x), vld1q_f32(alpha_target + x), alpha));
    }
    return wmod4;
  }

  // layer_planarrgb_mul_c
  template<typename pixel_t, bool chroma, bool has_alpha, bool blend_alpha>
  static int planarrgb_mul(pixel_t* dstp_g, pixel_t* dstp_b, pixel_t* dstp_r, pixel_t* dstp_a,
    const pixel_t* ovrp_g, const pixel_t* ovrp_b, const pixel_t* ovrp_r, const pixel_t* maskp, const pixel_t* alpha_target,
    int width, int opacity_i, int max_pixel_value, int half, MagicDiv magic)
  {
    using namespace layer_neon_detail;
    if constexpr (sizeof(pixel_t) != 2)
      return 0;
    else {
      const MagicNeon m(magic);
      const uint32x4_t half_v = vdupq_n_u32(half);
      const uint16x8_t opacity_v = vdupq_n_u16((uint16_t)opacity_i);
      const uint16_t maxv = (uint16_t)max_pixel_value;
      const int wmod8 = width / 8 * 8;
      for (int x = 0; x < wmod8; x += 8) {
        uint16x8_t alpha = opacity_v;
        if constexpr (has_alpha)
          alpha = mask_alpha8_u16(vld1q_u16(maskp + x), (uint16_t)opacity_i, half_v, m);
        // overlay: the layer put over the base clip
        const uint16x8_t og = vld1q_u16(ovrp_g + x), ob = vld1q_u16(ovrp_b + x), orr = vld1q_u16(ovrp_r + x);
        // destination: in-place, it holds the base clip pixels, multiplied with the overlay, then blended.
        // Loaded before the stores below, like the C reads dstp_r/g/b[x] before writing it.
        const uint16x8_t dg = vld1q_u16(dstp_g + x), db = vld1q_u16(dstp_b + x), dr = vld1q_u16(dstp_r + x);
        uint16x8_t pr, pg, pb;
        if constexpr (chroma) {
          pr = mul8_u16(orr, dr, m);
          pg = mul8_u16(og, dg, m);
          pb = mul8_u16(ob, db, m);
        }
        else {
          // the overlay's luma instead of its colors, at most 65535
          uint32x4_t lo, hi;
          luma8_u32(ob, og, orr, lo, hi);
          const uint16x8_t luma = vcombine_u16(vmovn_u32(lo), vmovn_u32(hi));
          pr = mul8_u16(luma, dr, m);
          pg = mul8_u16(luma, dg, m);
          pb = mul8_u16(luma, db, m);
        }
        vst1q_u16(dstp_r + x, blend8_u16(dr, pr, alpha, maxv, half_v, m));
        vst1q_u16(dstp_g + x, blend8_u16(dg, pg, alpha, maxv, half_v, m));
        vst1q_u16(dstp_b + x, blend8_u16(db, pb, alpha, maxv, half_v, m));
        if constexpr (blend_alpha) {
          const uint16x8_t da = vld1q_u16(dstp_a + x);
          vst1q_u16(dstp_a + x, blend8_u16(da, mul8_u16(vld1q_u16(alpha_target + x), da, m), alpha, maxv, half_v, m));
        }
      }
      return wmod8;
    }
  }

  // layer_yuv_mulovr_c, one chroma row: darken toward black / neutral chroma by the overlay luma.
  // darken_factor and inv_keep stay 32 bit like in C: for an out-of-range overlay (> max) max - ovr wraps,
  // and darken_factor can exceed 16 bits. The last blend is uint32 arithmetic, the same modulo 2^32.
  template<typename pixel_t, bool lumaonly, bool has_alpha, bool update_luma>
  LAYER_NEON_ROW_NOINLINE static int yuv_mulovr(pixel_t* dstp, pixel_t* dstp_u, pixel_t* dstp_v,
    const pixel_t* ovr_y_ptr, const pixel_t* mask_ptr,
    int cwidth, int opacity_i, int max_pixel_value, int half, MagicDiv magic)
  {
    using namespace layer_neon_detail;
    if constexpr (sizeof(pixel_t) != 2)
      return 0;
    else {
      const MagicNeon m(magic);
      const uint32x4_t half_v = vdupq_n_u32(half);
      const uint32x4_t max_v = vdupq_n_u32(max_pixel_value);
      const uint32_t half_pix = (uint32_t)(max_pixel_value / 2); // integer chroma neutral
      const uint16x8_t opacity_v = vdupq_n_u16((uint16_t)opacity_i);
      const uint16_t maxv = (uint16_t)max_pixel_value;
      const int wmod8 = cwidth / 8 * 8;
      for (int x = 0; x < wmod8; x += 8) {
        uint16x8_t alpha = opacity_v;
        if constexpr (has_alpha)
          alpha = mask_alpha8_u16(vld1q_u16(mask_ptr + x), (uint16_t)opacity_i, half_v, m);
        const uint16x8_t ovr = vld1q_u16(ovr_y_ptr + x);
        // darken_factor = magic_div_rt(alpha * (max - ovr) + half), as half + alpha * max - alpha * ovr
        uint32x4_t d_lo = vmlal_n_u16(half_v, vget_low_u16(alpha), maxv);
        uint32x4_t d_hi = vmlal_high_n_u16(half_v, alpha, maxv);
        d_lo = magic_div_neon(vmlsl_u16(d_lo, vget_low_u16(alpha), vget_low_u16(ovr)), m);
        d_hi = magic_div_neon(vmlsl_high_u16(d_hi, alpha, ovr), m);
        const uint32x4_t keep_lo = vsubq_u32(max_v, d_lo); // inv_keep
        const uint32x4_t keep_hi = vsubq_u32(max_v, d_hi);
        if constexpr (!lumaonly) {
          // (pixel_t)magic_div_rt(dst * inv_keep + half_pix * darken_factor + half)
          const uint32x4_t n_lo = vmlaq_n_u32(half_v, d_lo, half_pix);
          const uint32x4_t n_hi = vmlaq_n_u32(half_v, d_hi, half_pix);
          const uint16x8_t u = vld1q_u16(dstp_u + x);
          const uint16x8_t v = vld1q_u16(dstp_v + x);
          vst1q_u16(dstp_u + x, magic_div_narrow(vmlaq_u32(n_lo, vmovl_u16(vget_low_u16(u)), keep_lo),
            vmlaq_u32(n_hi, vmovl_high_u16(u), keep_hi), m));
          vst1q_u16(dstp_v + x, magic_div_narrow(vmlaq_u32(n_lo, vmovl_u16(vget_low_u16(v)), keep_lo),
            vmlaq_u32(n_hi, vmovl_high_u16(v), keep_hi), m));
        }
        if constexpr (update_luma) {
          // (pixel_t)magic_div_rt(dst * inv_keep + half)
          const uint16x8_t yv = vld1q_u16(dstp + x);
          vst1q_u16(dstp + x, magic_div_narrow(vmlaq_u32(half_v, vmovl_u16(vget_low_u16(yv)), keep_lo),
            vmlaq_u32(half_v, vmovl_high_u16(yv), keep_hi), m));
        }
      }
      return wmod8;
    }
  }
};

#undef LAYER_NEON_ROW_NOINLINE

#endif // __Layer_Row_Kernels_NEON_H__
