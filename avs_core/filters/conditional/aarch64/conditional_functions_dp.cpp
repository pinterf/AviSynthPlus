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

// 8 bit plane sum with UDOT (FEAT_DotProd): the dot product pair of get_sum_of_pixels_neon<uint8_t>.
// vdotq_u32(acc, pixels, ones) adds 16 bytes straight into 4 x 32 bit lanes, no 16 bit stage and no flush.
// Two accumulators, 32 bytes per step. Integer, so the total is the same as the C one.
// This file is compiled with -march=armv8.2-a+dotprod (_dp suffix, see CMakeLists.txt), the caller checks CPUF_ARM_DOTPROD.

#include "conditional_functions_dp.h"
#include <avs/config.h>
#include <cstdint>

#include <arm_neon.h>

double get_sum_of_pixels_u8_dp(const BYTE* srcp, size_t height, size_t width, size_t pitch)
{
  const size_t wmod32 = width / 32 * 32;
  const size_t wmod16 = width / 16 * 16;
  const uint8x16_t ones = vdupq_n_u8(1);
  int64_t total = 0;
  for (size_t y = 0; y < height; ++y) {
    uint32x4_t acc0 = vdupq_n_u32(0);
    uint32x4_t acc1 = vdupq_n_u32(0);
    size_t x = 0;
    for (; x < wmod32; x += 32) {
      acc0 = vdotq_u32(acc0, vld1q_u8(srcp + x), ones);
      acc1 = vdotq_u32(acc1, vld1q_u8(srcp + x + 16), ones);
    }
    if (x < wmod16) {
      acc0 = vdotq_u32(acc0, vld1q_u8(srcp + x), ones);
      x += 16;
    }
    int64_t rowsum = (int64_t)vaddlvq_u32(acc0) + (int64_t)vaddlvq_u32(acc1);
    for (; x < width; ++x)
      rowsum += srcp[x];
    total += rowsum;
    srcp += pitch;
  }
  return (double)total;
}
