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

// Limiter, planar: the NEON pair of limit_plane_c / limit_plane_f_c, in-place, bit-identical:
//   x < min ? min : (x > max ? max : x)
// Integer, min <= max (the usual case): that is exactly vmin(vmax(x, min), max).
// Integer, min > max (not rejected by Limiter): C gives min below min and max otherwise, a max-min chain
//   would always give max, so this case uses compare + select like C. (The Intel paths use max-min for both.)
// Float: always compare + select: vmaxq_f32(-0.0f, +0.0f) gives +0.0f where C keeps -0.0f; NaN stays NaN like C.
// The rest of the row (width % 16, % 8, % 4 pixels) is done per pixel with the C formula.

#include "limiter_neon.h"
#include <avs/config.h>
#include <cstdint>

#include <arm_neon.h>

template<typename pixel_t>
static AVS_FORCEINLINE pixel_t limit_pixel(pixel_t v, pixel_t min, pixel_t max)
{
  return v < min ? min : (v > max ? max : v);
}

template<bool min_le_max>
static void limit_plane_uint8_neon(BYTE* srcp, int pitch, int min, int max, int width, int height)
{
  const uint8x16_t minv = vdupq_n_u8(static_cast<uint8_t>(min));
  const uint8x16_t maxv = vdupq_n_u8(static_cast<uint8_t>(max));
  for (int y = 0; y < height; y++) {
    int x = 0;
    for (; x + 16 <= width; x += 16) {
      const uint8x16_t v = vld1q_u8(srcp + x);
      if constexpr (min_le_max)
        vst1q_u8(srcp + x, vminq_u8(vmaxq_u8(v, minv), maxv));
      else
        vst1q_u8(srcp + x, vbslq_u8(vcltq_u8(v, minv), minv, vbslq_u8(vcgtq_u8(v, maxv), maxv, v)));
    }
    for (; x < width; x++)
      srcp[x] = limit_pixel<uint8_t>(srcp[x], static_cast<uint8_t>(min), static_cast<uint8_t>(max));
    srcp += pitch;
  }
}

template<bool min_le_max>
static void limit_plane_uint16_neon(BYTE* srcp8, int pitch, int min, int max, int width, int height)
{
  uint16_t* srcp = reinterpret_cast<uint16_t*>(srcp8);
  pitch /= sizeof(uint16_t);
  const uint16x8_t minv = vdupq_n_u16(static_cast<uint16_t>(min));
  const uint16x8_t maxv = vdupq_n_u16(static_cast<uint16_t>(max));
  for (int y = 0; y < height; y++) {
    int x = 0;
    for (; x + 8 <= width; x += 8) {
      const uint16x8_t v = vld1q_u16(srcp + x);
      if constexpr (min_le_max)
        vst1q_u16(srcp + x, vminq_u16(vmaxq_u16(v, minv), maxv));
      else
        vst1q_u16(srcp + x, vbslq_u16(vcltq_u16(v, minv), minv, vbslq_u16(vcgtq_u16(v, maxv), maxv, v)));
    }
    for (; x < width; x++)
      srcp[x] = limit_pixel<uint16_t>(srcp[x], static_cast<uint16_t>(min), static_cast<uint16_t>(max));
    srcp += pitch;
  }
}

template<typename pixel_t>
void limit_plane_neon(BYTE* srcp, int pitch, int min, int max, int width, int height)
{
  // min, max: 0 .. pixel max (checked in the constructor)
  if constexpr (sizeof(pixel_t) == 1) {
    if (min <= max) limit_plane_uint8_neon<true>(srcp, pitch, min, max, width, height);
    else limit_plane_uint8_neon<false>(srcp, pitch, min, max, width, height);
  }
  else {
    if (min <= max) limit_plane_uint16_neon<true>(srcp, pitch, min, max, width, height);
    else limit_plane_uint16_neon<false>(srcp, pitch, min, max, width, height);
  }
}

template void limit_plane_neon<uint8_t>(BYTE* srcp, int pitch, int min, int max, int width, int height);
template void limit_plane_neon<uint16_t>(BYTE* srcp, int pitch, int min, int max, int width, int height);

void limit_plane_f_neon(BYTE* srcp8, int pitch, float min, float max, int width, int height)
{
  float* srcp = reinterpret_cast<float*>(srcp8);
  pitch /= sizeof(float);
  const float32x4_t minv = vdupq_n_f32(min);
  const float32x4_t maxv = vdupq_n_f32(max);
  for (int y = 0; y < height; y++) {
    int x = 0;
    for (; x + 4 <= width; x += 4) {
      const float32x4_t v = vld1q_f32(srcp + x);
      vst1q_f32(srcp + x, vbslq_f32(vcltq_f32(v, minv), minv, vbslq_f32(vcgtq_f32(v, maxv), maxv, v)));
    }
    for (; x < width; x++)
      srcp[x] = limit_pixel<float>(srcp[x], min, max);
    srcp += pitch;
  }
}
