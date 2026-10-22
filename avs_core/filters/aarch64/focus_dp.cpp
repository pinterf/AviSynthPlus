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

// 8 bit SAD with UDOT (FEAT_DotProd): the dot product pair of calculate_sad_8_or_16_neon<uint8_t, ...>.
// vabd, then vdotq_u32(acc, diff, ones) adds the 16 differences straight into 4 x 32 bit lanes: no 16 bit stage,
// no flush, and udot issues on both vector pipes where uadalp has one (e.g. Cortex-A76).
// Two accumulators, 32 bytes per step. packedRGB3264: alpha (every 4th byte) masked after the absolute difference.
// Integer, so the total is the same as the C one.
// This file is compiled with -march=armv8.2-a+dotprod (_dp suffix, see CMakeLists.txt), the caller checks CPUF_ARM_DOTPROD.

#include "focus_dp.h"
#include <avs/config.h>
#include <cstdlib>

#include <arm_neon.h>

template<bool packedRGB3264>
int64_t calculate_sad_8_dp(const BYTE* cur_ptr, const BYTE* other_ptr, int cur_pitch, int other_pitch, size_t rowsize, size_t height)
{
  const size_t mod32 = rowsize / 32 * 32;
  const size_t mod16 = rowsize / 16 * 16;
  const uint8x16_t mask8 = vreinterpretq_u8_u32(vdupq_n_u32(0x00FFFFFF)); // RGB32: B G R kept, A zeroed
  const uint8x16_t ones = vdupq_n_u8(1);
  int64_t total = 0;
  for (size_t y = 0; y < height; ++y) {
    uint32x4_t acc0 = vdupq_n_u32(0);
    uint32x4_t acc1 = vdupq_n_u32(0);
    size_t x = 0;
    for (; x < mod32; x += 32) {
      uint8x16_t d0 = vabdq_u8(vld1q_u8(cur_ptr + x), vld1q_u8(other_ptr + x));
      uint8x16_t d1 = vabdq_u8(vld1q_u8(cur_ptr + x + 16), vld1q_u8(other_ptr + x + 16));
      if constexpr (packedRGB3264) {
        d0 = vandq_u8(d0, mask8);
        d1 = vandq_u8(d1, mask8);
      }
      acc0 = vdotq_u32(acc0, d0, ones);
      acc1 = vdotq_u32(acc1, d1, ones);
    }
    if (x < mod16) {
      uint8x16_t d = vabdq_u8(vld1q_u8(cur_ptr + x), vld1q_u8(other_ptr + x));
      if constexpr (packedRGB3264)
        d = vandq_u8(d, mask8);
      acc0 = vdotq_u32(acc0, d, ones);
      x += 16;
    }
    int64_t rowsum = (int64_t)vaddlvq_u32(acc0) + (int64_t)vaddlvq_u32(acc1);
    for (size_t i = x; i < rowsize; ++i) {
      if constexpr (packedRGB3264) {
        if ((i & 3) == 3)
          continue; // alpha
      }
      rowsum += std::abs((int)cur_ptr[i] - (int)other_ptr[i]);
    }
    total += rowsum;
    cur_ptr += cur_pitch;
    other_ptr += other_pitch;
  }
  return total;
}

template int64_t calculate_sad_8_dp<false>(const BYTE*, const BYTE*, int, int, size_t, size_t);
template int64_t calculate_sad_8_dp<true>(const BYTE*, const BYTE*, int, int, size_t, size_t);
