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

// Packed RGB channel extraction (SwapUVToY, RToY8/GToY8/BToY8/AToY8): the NEON pair of the C copy loop
// and extract_packed_rgb*_channel_sse2/avx2. A de-interleaving load (vld3/vld4) gives the channels in
// separate registers, the wanted one is stored: 16 pixels per step (8 bit), 8 pixels (16 bit).
// The rest of the row (< 16 or < 8 pixels) is copied per pixel, like the C.

#include "planeswap_neon.h"
#include <avs/config.h>
#include <cstdint>

#include <arm_neon.h>

template<int components, int channel_index>
static void extract_packed_rgb8_channel_neon(const BYTE* srcp, BYTE* dstp, int src_pitch, int dst_pitch, int width, int height)
{
  const int wmod16 = width / 16 * 16;
  for (int y = 0; y < height; ++y) {
    int x = 0;
    for (; x < wmod16; x += 16) {
      if constexpr (components == 4)
        vst1q_u8(dstp + x, vld4q_u8(srcp + x * 4).val[channel_index]);
      else
        vst1q_u8(dstp + x, vld3q_u8(srcp + x * 3).val[channel_index]);
    }
    for (; x < width; ++x)
      dstp[x] = srcp[x * components + channel_index];
    srcp -= src_pitch;
    dstp += dst_pitch;
  }
}

template<int components, int channel_index>
static void extract_packed_rgb16_channel_neon(const BYTE* srcp8, BYTE* dstp8, int src_pitch, int dst_pitch, int width, int height)
{
  const int wmod8 = width / 8 * 8;
  for (int y = 0; y < height; ++y) {
    const uint16_t* srcp = reinterpret_cast<const uint16_t*>(srcp8);
    uint16_t* dstp = reinterpret_cast<uint16_t*>(dstp8);
    int x = 0;
    for (; x < wmod8; x += 8) {
      if constexpr (components == 4)
        vst1q_u16(dstp + x, vld4q_u16(srcp + x * 4).val[channel_index]);
      else
        vst1q_u16(dstp + x, vld3q_u16(srcp + x * 3).val[channel_index]);
    }
    for (; x < width; ++x)
      dstp[x] = srcp[x * components + channel_index];
    srcp8 -= src_pitch;
    dstp8 += dst_pitch;
  }
}

template<int components, int channel_index>
static void extract_channel(const BYTE* srcp, BYTE* dstp, int src_pitch, int dst_pitch, int width, int height, int pixelsize)
{
  if (pixelsize == 1)
    extract_packed_rgb8_channel_neon<components, channel_index>(srcp, dstp, src_pitch, dst_pitch, width, height);
  else
    extract_packed_rgb16_channel_neon<components, channel_index>(srcp, dstp, src_pitch, dst_pitch, width, height);
}

void extract_packed_rgb_channel_neon(const BYTE* srcp, BYTE* dstp, int src_pitch, int dst_pitch,
  int width, int height, int pixelsize, int components, int channel_index)
{
  if (components == 4) {
    switch (channel_index) {
    case 0: extract_channel<4, 0>(srcp, dstp, src_pitch, dst_pitch, width, height, pixelsize); break;
    case 1: extract_channel<4, 1>(srcp, dstp, src_pitch, dst_pitch, width, height, pixelsize); break;
    case 2: extract_channel<4, 2>(srcp, dstp, src_pitch, dst_pitch, width, height, pixelsize); break;
    default: extract_channel<4, 3>(srcp, dstp, src_pitch, dst_pitch, width, height, pixelsize); break;
    }
  }
  else { // 3 components, no alpha
    switch (channel_index) {
    case 0: extract_channel<3, 0>(srcp, dstp, src_pitch, dst_pitch, width, height, pixelsize); break;
    case 1: extract_channel<3, 1>(srcp, dstp, src_pitch, dst_pitch, width, height, pixelsize); break;
    default: extract_channel<3, 2>(srcp, dstp, src_pitch, dst_pitch, width, height, pixelsize); break;
    }
  }
}
