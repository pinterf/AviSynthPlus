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

// Legacy direct 8 bit YV24 -> packed RGB24/RGB32, the NEON pair of convert_yv24_to_rgb_c.
// Same arithmetic as C, bit-identical:
//   b = (y_b*Y + u_b*U + v_b*V + 4096 + (offset_rgb << 13)) >> 13, then clip to 0..255
//   with Y = y + offset_y, U = u - 128, V = v - 128.
// Coefficients and inputs fit int16, the products are summed in int32 (vmull_s16/vmlal_s16).
// >> 13 is arithmetic like the C int shift; vqmovun_s32 + vqmovn_u16 clip like PixelClip.

#include "convert_planar_neon.h"
#include <avs/config.h>
#include <cstdint>

#include <arm_neon.h>

namespace {

struct Coeffs {
  int16x4_t yb, ub, vb, yg, ug, vg, yr, ur, vr;
  int32x4_t rounder;
};

static AVS_FORCEINLINE int16x4_t c16(int c) { return vdup_n_s16(static_cast<int16_t>(c)); }

// one output channel for 8 pixels
static AVS_FORCEINLINE uint8x8_t channel8(int16x8_t Y, int16x8_t U, int16x8_t V,
  int16x4_t cy, int16x4_t cu, int16x4_t cv, int32x4_t rounder)
{
  int32x4_t lo = vmlal_s16(vmlal_s16(vmlal_s16(rounder, vget_low_s16(Y), cy), vget_low_s16(U), cu), vget_low_s16(V), cv);
  int32x4_t hi = vmlal_s16(vmlal_s16(vmlal_s16(rounder, vget_high_s16(Y), cy), vget_high_s16(U), cu), vget_high_s16(V), cv);
  lo = vshrq_n_s32(lo, 13);
  hi = vshrq_n_s32(hi, 13);
  const uint16x8_t u16 = vcombine_u16(vqmovun_s32(lo), vqmovun_s32(hi)); // < 0 -> 0
  return vqmovn_u16(u16); // > 255 -> 255
}

static AVS_FORCEINLINE uint8_t clip8(int v) { return static_cast<uint8_t>(v < 0 ? 0 : (v > 255 ? 255 : v)); }

} // namespace

template<int pixel_step, bool source_has_alpha>
void convert_yv24_to_rgb_neon(BYTE* dstp, const BYTE* srcY, const BYTE* srcU, const BYTE* srcV, const BYTE* srcA, size_t dst_pitch, size_t src_pitch_y, size_t src_pitch_uv, size_t src_pitch_a, size_t width, size_t height, const ConversionMatrix& matrix)
{
  // The 13 bit coefficients (below 2.2 * 8192) and offset_y are generated in range: int16 is enough.
  const int rounder_i = 4096 + (matrix.offset_rgb << 13);
  Coeffs k;
  k.yb = c16(matrix.y_b); k.ub = c16(matrix.u_b); k.vb = c16(matrix.v_b);
  k.yg = c16(matrix.y_g); k.ug = c16(matrix.u_g); k.vg = c16(matrix.v_g);
  k.yr = c16(matrix.y_r); k.ur = c16(matrix.u_r); k.vr = c16(matrix.v_r);
  k.rounder = vdupq_n_s32(rounder_i);
  const int16x8_t offset_y = vdupq_n_s16(static_cast<int16_t>(matrix.offset_y));
  const int16x8_t uv_offset = vdupq_n_s16(128);
  const uint8x16_t alpha_max = vdupq_n_u8(255);

  const size_t wmod16 = width / 16 * 16;
  dstp += dst_pitch * (height - 1); // packed RGB is bottom-up
  for (size_t y = 0; y < height; y++) {
    size_t x = 0;
    for (; x < wmod16; x += 16) {
      const uint8x16_t y8 = vld1q_u8(srcY + x);
      const uint8x16_t u8 = vld1q_u8(srcU + x);
      const uint8x16_t v8 = vld1q_u8(srcV + x);
      uint8x8_t b[2], g[2], r[2];
      for (int h = 0; h < 2; h++) {
        const uint8x8_t yh = h == 0 ? vget_low_u8(y8) : vget_high_u8(y8);
        const uint8x8_t uh = h == 0 ? vget_low_u8(u8) : vget_high_u8(u8);
        const uint8x8_t vh = h == 0 ? vget_low_u8(v8) : vget_high_u8(v8);
        const int16x8_t Y = vaddq_s16(vreinterpretq_s16_u16(vmovl_u8(yh)), offset_y);
        const int16x8_t U = vsubq_s16(vreinterpretq_s16_u16(vmovl_u8(uh)), uv_offset);
        const int16x8_t V = vsubq_s16(vreinterpretq_s16_u16(vmovl_u8(vh)), uv_offset);
        b[h] = channel8(Y, U, V, k.yb, k.ub, k.vb, k.rounder);
        g[h] = channel8(Y, U, V, k.yg, k.ug, k.vg, k.rounder);
        r[h] = channel8(Y, U, V, k.yr, k.ur, k.vr, k.rounder);
      }
      if constexpr (pixel_step == 4) {
        uint8x16x4_t bgra;
        bgra.val[0] = vcombine_u8(b[0], b[1]);
        bgra.val[1] = vcombine_u8(g[0], g[1]);
        bgra.val[2] = vcombine_u8(r[0], r[1]);
        bgra.val[3] = source_has_alpha ? vld1q_u8(srcA + x) : alpha_max;
        vst4q_u8(dstp + x * 4, bgra);
      }
      else {
        uint8x16x3_t bgr;
        bgr.val[0] = vcombine_u8(b[0], b[1]);
        bgr.val[1] = vcombine_u8(g[0], g[1]);
        bgr.val[2] = vcombine_u8(r[0], r[1]);
        vst3q_u8(dstp + x * 3, bgr);
      }
    }
    for (; x < width; x++) {
      const int Y = srcY[x] + matrix.offset_y;
      const int U = srcU[x] - 128;
      const int V = srcV[x] - 128;
      dstp[x * pixel_step + 0] = clip8((matrix.y_b * Y + matrix.u_b * U + matrix.v_b * V + rounder_i) >> 13);
      dstp[x * pixel_step + 1] = clip8((matrix.y_g * Y + matrix.u_g * U + matrix.v_g * V + rounder_i) >> 13);
      dstp[x * pixel_step + 2] = clip8((matrix.y_r * Y + matrix.u_r * U + matrix.v_r * V + rounder_i) >> 13);
      if constexpr (pixel_step == 4)
        dstp[x * 4 + 3] = source_has_alpha ? srcA[x] : 255;
    }
    dstp -= dst_pitch;
    srcY += src_pitch_y;
    srcU += src_pitch_uv;
    srcV += src_pitch_uv;
    if constexpr (source_has_alpha)
      srcA += src_pitch_a;
  }
}

template void convert_yv24_to_rgb_neon<3, false>(BYTE* dstp, const BYTE* srcY, const BYTE* srcU, const BYTE* srcV, const BYTE* srcA, size_t dst_pitch, size_t src_pitch_y, size_t src_pitch_uv, size_t src_pitch_a, size_t width, size_t height, const ConversionMatrix& matrix);
template void convert_yv24_to_rgb_neon<4, false>(BYTE* dstp, const BYTE* srcY, const BYTE* srcU, const BYTE* srcV, const BYTE* srcA, size_t dst_pitch, size_t src_pitch_y, size_t src_pitch_uv, size_t src_pitch_a, size_t width, size_t height, const ConversionMatrix& matrix);
template void convert_yv24_to_rgb_neon<4, true>(BYTE* dstp, const BYTE* srcY, const BYTE* srcU, const BYTE* srcV, const BYTE* srcA, size_t dst_pitch, size_t src_pitch_y, size_t src_pitch_uv, size_t src_pitch_a, size_t width, size_t height, const ConversionMatrix& matrix);
