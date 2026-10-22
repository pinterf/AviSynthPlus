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

// Plane sum (AveragePlane): the NEON pair of get_sum_of_pixels_c and get_sum_of_pixels_sse2 / _uint16_sse2.
// Integer, so any order gives the same total as the C int64 sum.
// 32 bytes per step into two independent accumulators (one accumulator chain is latency bound).
//   8 bit: vpadal into 16 bit lanes, flushed into 32 bit lanes every 2 x 64 steps (at most 64 * 2 * 255).
//   16 bit: vpadal into 32 bit lanes (at most 2 * 65535 per step).
// Row totals are added as 64 bit. The rest of the row: one 16 byte step, then per pixel.

#include "conditional_functions_neon.h"
#include <avs/config.h>
#include <cstdint>

#include <arm_neon.h>

// dual lane optimization
template<typename pixel_t>
double get_sum_of_pixels_neon(const BYTE* srcp8, size_t height, size_t width, size_t pitch)
{
  constexpr size_t step = 16 / sizeof(pixel_t); // pixels in 16 bytes
  const size_t wmod2 = width / (2 * step) * (2 * step);
  const size_t wmod = width / step * step;
  int64_t total = 0;
  for (size_t y = 0; y < height; ++y) {
    const pixel_t* srcp = reinterpret_cast<const pixel_t*>(srcp8);
    uint32x4_t acc32a = vdupq_n_u32(0);
    uint32x4_t acc32b = vdupq_n_u32(0);
    size_t x = 0;
    if constexpr (sizeof(pixel_t) == 1) {
      while (x < wmod2) {
        uint16x8_t acc16a = vdupq_n_u16(0);
        uint16x8_t acc16b = vdupq_n_u16(0);
        const size_t block_end = x + 64 * 32 < wmod2 ? x + 64 * 32 : wmod2;
        for (; x < block_end; x += 32) {
          acc16a = vpadalq_u8(acc16a, vld1q_u8(srcp + x));
          acc16b = vpadalq_u8(acc16b, vld1q_u8(srcp + x + 16));
        }
        acc32a = vpadalq_u16(acc32a, acc16a);
        acc32b = vpadalq_u16(acc32b, acc16b);
      }
      if (x < wmod) {
        acc32a = vpadalq_u16(acc32a, vpaddlq_u8(vld1q_u8(srcp + x)));
        x += 16;
      }
    }
    else {
      for (; x < wmod2; x += 16) {
        acc32a = vpadalq_u16(acc32a, vld1q_u16(srcp + x));
        acc32b = vpadalq_u16(acc32b, vld1q_u16(srcp + x + 8));
      }
      if (x < wmod) {
        acc32a = vpadalq_u16(acc32a, vld1q_u16(srcp + x));
        x += 8;
      }
    }
    int64_t rowsum = (int64_t)vaddlvq_u32(acc32a) + (int64_t)vaddlvq_u32(acc32b);
    for (; x < width; ++x)
      rowsum += srcp[x];
    total += rowsum;
    srcp8 += pitch;
  }
  return (double)total;
}

template double get_sum_of_pixels_neon<uint8_t>(const BYTE* srcp, size_t height, size_t width, size_t pitch);
template double get_sum_of_pixels_neon<uint16_t>(const BYTE* srcp, size_t height, size_t width, size_t pitch);
