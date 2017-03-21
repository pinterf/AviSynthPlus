// Avisynth v2.5.  Copyright 2002-2009 Ben Rudiak-Gould et al.
// http://www.avisynth.org

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


#include "convert.h"
#include "convert_planar.h"
#include "convert_rgb.h"
#include "convert_yv12.h"
#include "convert_yuy2.h"
#include <avs/alignment.h>
#include <avs/win.h>
#include <avs/minmax.h>
#include <emmintrin.h>
#include <immintrin.h>
#include <tuple>
#include <map>

#ifndef _mm256_set_m128i
#define _mm256_set_m128i(v0, v1) _mm256_insertf128_si256(_mm256_castsi128_si256(v1), (v0), 1)
#endif

#ifndef _mm256_set_m128
#define _mm256_set_m128(v0, v1) _mm256_insertf128_ps(_mm256_castps128_ps256(v1), (v0), 1)
#endif

#include "convert_avx2.h"

template<typename pixel_t, uint8_t targetbits>
void convert_32_to_uintN_c_avx2(const BYTE *srcp, BYTE *dstp, int src_rowsize, int src_height, int src_pitch, int dst_pitch, float float_range)
{
  _mm256_zeroupper();
  const float *srcp0 = reinterpret_cast<const float *>(srcp);
  pixel_t *dstp0 = reinterpret_cast<pixel_t *>(dstp);

  src_pitch = src_pitch / sizeof(float);
  dst_pitch = dst_pitch / sizeof(pixel_t);

  int src_width = src_rowsize / sizeof(float);

  float max_dst_pixelvalue = (float)((1<<targetbits) - 1); // 255, 1023, 4095, 16383, 65535.0

  float factor = 1.0f / float_range * max_dst_pixelvalue;

  for(int y=0; y<src_height; y++)
  {
    for (int x = 0; x < src_width; x++)
    {
      float pixel = srcp0[x] * factor + 0.5f; // 0.5f: keep the neutral grey level of float 0.5
      dstp0[x] = pixel_t(clamp(pixel, 0.0f, max_dst_pixelvalue)); // we clamp here!
    }
    dstp0 += dst_pitch;
    srcp0 += src_pitch;
  }
  _mm256_zeroupper();
}

template void convert_32_to_uintN_c_avx2<uint8_t, 8>(const BYTE *srcp, BYTE *dstp, int src_rowsize, int src_height, int src_pitch, int dst_pitch, float float_range);
template void convert_32_to_uintN_c_avx2<uint16_t, 10>(const BYTE *srcp, BYTE *dstp, int src_rowsize, int src_height, int src_pitch, int dst_pitch, float float_range);
template void convert_32_to_uintN_c_avx2<uint16_t, 12>(const BYTE *srcp, BYTE *dstp, int src_rowsize, int src_height, int src_pitch, int dst_pitch, float float_range);
template void convert_32_to_uintN_c_avx2<uint16_t, 14>(const BYTE *srcp, BYTE *dstp, int src_rowsize, int src_height, int src_pitch, int dst_pitch, float float_range);
template void convert_32_to_uintN_c_avx2<uint16_t, 16>(const BYTE *srcp, BYTE *dstp, int src_rowsize, int src_height, int src_pitch, int dst_pitch, float float_range);

// YUV: bit shift 10-12-14-16 <=> 10-12-14-16 bits
// shift right or left, depending on expandrange template param
template<bool expandrange, uint8_t shiftbits>
void convert_uint16_to_uint16_c_avx2(const BYTE *srcp, BYTE *dstp, int src_rowsize, int src_height, int src_pitch, int dst_pitch, float float_range)
{
    _mm256_zeroupper();
    const uint16_t *srcp0 = reinterpret_cast<const uint16_t *>(srcp);
    uint16_t *dstp0 = reinterpret_cast<uint16_t *>(dstp);

    src_pitch = src_pitch / sizeof(uint16_t);
    dst_pitch = dst_pitch / sizeof(uint16_t);

    const int src_width = src_rowsize / sizeof(uint16_t);

    for(int y=0; y<src_height; y++)
    {
        for (int x = 0; x < src_width; x++)
        {
            if(expandrange)
                dstp0[x] = srcp0[x] << shiftbits;  // expand range. No clamp before, source is assumed to have valid range
            else
                dstp0[x] = srcp0[x] >> shiftbits;  // reduce range
        }
        dstp0 += dst_pitch;
        srcp0 += src_pitch;
    }
    _mm256_zeroupper();
}

// instantiate them
template void convert_uint16_to_uint16_c_avx2<false, 2>(const BYTE *srcp, BYTE *dstp, int src_rowsize, int src_height, int src_pitch, int dst_pitch, float float_range);
template void convert_uint16_to_uint16_c_avx2<false, 4>(const BYTE *srcp, BYTE *dstp, int src_rowsize, int src_height, int src_pitch, int dst_pitch, float float_range);
template void convert_uint16_to_uint16_c_avx2<false, 6>(const BYTE *srcp, BYTE *dstp, int src_rowsize, int src_height, int src_pitch, int dst_pitch, float float_range);
template void convert_uint16_to_uint16_c_avx2<true, 2>(const BYTE *srcp, BYTE *dstp, int src_rowsize, int src_height, int src_pitch, int dst_pitch, float float_range);
template void convert_uint16_to_uint16_c_avx2<true, 4>(const BYTE *srcp, BYTE *dstp, int src_rowsize, int src_height, int src_pitch, int dst_pitch, float float_range);
template void convert_uint16_to_uint16_c_avx2<true, 6>(const BYTE *srcp, BYTE *dstp, int src_rowsize, int src_height, int src_pitch, int dst_pitch, float float_range);


