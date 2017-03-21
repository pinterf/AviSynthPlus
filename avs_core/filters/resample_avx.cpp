// Avisynth v2.5.  Copyright 2002 Ben Rudiak-Gould et al.
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

#include "resample.h"
#include <avs/config.h>
#include "../core/internal.h"

#include "transform.h"
#include "turn.h"
#include <avs/alignment.h>
#include <avs/minmax.h>
#include "../convert/convert_planar.h"
#include "../convert/convert_yuy2.h"

#include <type_traits>
// Intrinsics for SSE4.1, SSSE3, SSE3, SSE2, ISSE and MMX
#include <emmintrin.h>
#include <immintrin.h>
#include <algorithm>
#include "resample_avx.h"

#ifndef _mm256_set_m128i
#define _mm256_set_m128i(v0, v1) _mm256_insertf128_si256(_mm256_castsi128_si256(v1), (v0), 1)
#endif

#ifndef _mm256_set_m128
#define _mm256_set_m128(v0, v1) _mm256_insertf128_ps(_mm256_castps128_ps256(v1), (v0), 1)
#endif

template<bool lessthan16bit, typename pixel_t, bool avx2>
void resizer_h_avx_generic_int16_float(BYTE* dst8, const BYTE* src8, int dst_pitch, int src_pitch, ResamplingProgram* program, int width, int height, int bits_per_pixel) {
  _mm256_zeroupper();
  int filter_size = AlignNumber(program->filter_size, 8) / 8;
  const __m128 zero = _mm_setzero_ps();
  const __m128i zero128 = _mm_setzero_si128();

  const pixel_t *src = reinterpret_cast<const pixel_t *>(src8);
  pixel_t *dst = reinterpret_cast<pixel_t *>(dst8);
  dst_pitch /= sizeof(pixel_t);
  src_pitch /= sizeof(pixel_t);
  
  __m128 clamp_limit;
  if (sizeof(pixel_t) == 2)
    clamp_limit = _mm_set1_ps((float)(((int)1 << bits_per_pixel) - 1)); // clamp limit

  __m128 data_l_single, data_h_single;

  for (int y = 0; y < height; y++) {
    float* current_coeff = program->pixel_coefficient_float;

    for (int x = 0; x < width; x+=4) {
      __m256 result1 = _mm256_setzero_ps();
      __m256 result2 = result1;
      __m256 result3 = result1;
      __m256 result4 = result1;

      int begin1 = program->pixel_offset[x+0];
      int begin2 = program->pixel_offset[x+1];
      int begin3 = program->pixel_offset[x+2];
      int begin4 = program->pixel_offset[x+3];

      // this part is repeated by x4
      // begin1, result1
      for (int i = 0; i < filter_size; i++) {
        __m256 data_single;
        if(sizeof(pixel_t)==2) // word
        {
          __m256i src256;
          if (avx2) {
            src256 = _mm256_cvtepu16_epi32(_mm_loadu_si128(reinterpret_cast<const __m128i*>(src + begin1 + i * 8))); // 8*16->8*32 bits
          }
          else {
            __m128i src_p = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src + begin1 + i * 8)); // uint16_t  8*16=128 8 pixels at a time
            __m128i src_l = _mm_unpacklo_epi16(src_p, zero128); // spread lower  4*uint16_t pixel value -> 4*32 bit
            __m128i src_h = _mm_unpackhi_epi16(src_p, zero128); // spread higher 4*uint16_t pixel value -> 4*32 bit
            src256 = _mm256_set_m128i(src_h, src_l);
          }
          data_single = _mm256_cvtepi32_ps(src256); // Converts the 8x signed 32-bit integer values of a to single-precision, floating-point values.
        }
        else { // float unaligned
          if (avx2) {
            data_single = _mm256_loadu_ps(reinterpret_cast<const float*>(src+begin1+i*8)); // float  8*32=256 8 pixels at a time
          }
          else {
            // using one 256bit load instead of 2x128bit is slower on avx-only Ivy
            data_l_single = _mm_loadu_ps(reinterpret_cast<const float*>(src + begin1 + i * 8)); // float  4*32=128 4 pixels at a time
            data_h_single = _mm_loadu_ps(reinterpret_cast<const float*>(src + begin1 + i * 8 + 4)); // float  4*32=128 4 pixels at a time
            data_single = _mm256_set_m128(data_h_single, data_l_single);
          }
        }
        __m256 coeff;
        if (avx2) {
          // using one 256bit load instead of 2x128bit is slower on avx-only Ivy
          coeff = _mm256_load_ps(reinterpret_cast<const float*>(current_coeff));    // always aligned
        }
        else {
          __m128 coeff_l = _mm_load_ps(reinterpret_cast<const float*>(current_coeff));    // always aligned
          __m128 coeff_h = _mm_load_ps(reinterpret_cast<const float*>(current_coeff + 4));  // always aligned
          coeff = _mm256_set_m128(coeff_h, coeff_l);
        }

        __m256 dst = _mm256_mul_ps(data_single, coeff); // Multiply by coefficient
        result1 = _mm256_add_ps(result1, dst);

        current_coeff += 8;
      }

      // begin2, result2
      for (int i = 0; i < filter_size; i++) {
        __m256 data_single;
        if(sizeof(pixel_t)==2) // word
        {
          __m256i src256;
          if (avx2) {
            src256 = _mm256_cvtepu16_epi32(_mm_loadu_si128(reinterpret_cast<const __m128i*>(src + begin2 + i * 8))); // 8*16->8*32 bits
          }
          else {
            __m128i src_p = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src + begin2 + i * 8)); // uint16_t  8*16=128 8 pixels at a time
            __m128i src_l = _mm_unpacklo_epi16(src_p, zero128); // spread lower  4*uint16_t pixel value -> 4*32 bit
            __m128i src_h = _mm_unpackhi_epi16(src_p, zero128); // spread higher 4*uint16_t pixel value -> 4*32 bit
            src256 = _mm256_set_m128i(src_h, src_l);
          }
          data_single = _mm256_cvtepi32_ps (src256); // Converts the 8x signed 32-bit integer values of a to single-precision, floating-point values.
        }
        else { // float
          if (avx2) {
            data_single = _mm256_loadu_ps(reinterpret_cast<const float*>(src+begin2+i*8)); // float  8*32=256 8 pixels at a time
          }
          else {
            // using one 256bit load instead of 2x128bit is slower on avx-only Ivy
            data_l_single = _mm_loadu_ps(reinterpret_cast<const float*>(src + begin2 + i * 8)); // float  4*32=128 4 pixels at a time
            data_h_single = _mm_loadu_ps(reinterpret_cast<const float*>(src + begin2 + i * 8 + 4)); // float  4*32=128 4 pixels at a time
            data_single = _mm256_set_m128(data_h_single, data_l_single);
          }
        }
        __m256 coeff;
        if (avx2) {
          // using one 256bit load instead of 2x128bit is slower on avx-only Ivy
          coeff = _mm256_load_ps(reinterpret_cast<const float*>(current_coeff));    // always aligned
        }
        else {
          __m128 coeff_l = _mm_load_ps(reinterpret_cast<const float*>(current_coeff));    // always aligned
          __m128 coeff_h = _mm_load_ps(reinterpret_cast<const float*>(current_coeff + 4));  // always aligned
          coeff = _mm256_set_m128(coeff_h, coeff_l);
        }
        __m256 dst = _mm256_mul_ps(data_single, coeff); // Multiply by coefficient
        result2 = _mm256_add_ps(result2, dst);

        current_coeff += 8;
      }

      // begin3, result3
      for (int i = 0; i < filter_size; i++) {
        __m256 data_single;
        if(sizeof(pixel_t)==2) // word
        {
          __m256i src256;
          if (avx2) {
            src256 = _mm256_cvtepu16_epi32(_mm_loadu_si128(reinterpret_cast<const __m128i*>(src + begin3 + i * 8))); // 8*16->8*32 bits
          }
          else {
            __m128i src_p = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src + begin3 + i * 8)); // uint16_t  8*16=128 8 pixels at a time
            __m128i src_l = _mm_unpacklo_epi16(src_p, zero128); // spread lower  4*uint16_t pixel value -> 4*32 bit
            __m128i src_h = _mm_unpackhi_epi16(src_p, zero128); // spread higher 4*uint16_t pixel value -> 4*32 bit
            src256 = _mm256_set_m128i(src_h, src_l);
          }
          data_single = _mm256_cvtepi32_ps (src256); // Converts the 8x signed 32-bit integer values of a to single-precision, floating-point values.
        }
        else { // float
          if (avx2) {
            data_single = _mm256_loadu_ps(reinterpret_cast<const float*>(src+begin3+i*8)); // float  8*32=256 8 pixels at a time
          }
          else {
            // using one 256bit load instead of 2x128bit is slower on avx-only Ivy
            data_l_single = _mm_loadu_ps(reinterpret_cast<const float*>(src + begin3 + i * 8)); // float  4*32=128 4 pixels at a time
            data_h_single = _mm_loadu_ps(reinterpret_cast<const float*>(src + begin3 + i * 8 + 4)); // float  4*32=128 4 pixels at a time
            data_single = _mm256_set_m128(data_h_single, data_l_single);
          }
        }
        __m256 coeff;
        if (avx2) {
          // using one 256bit load instead of 2x128bit is slower on avx-only Ivy
          coeff = _mm256_load_ps(reinterpret_cast<const float*>(current_coeff));    // always aligned
        }
        else {
          __m128 coeff_l = _mm_load_ps(reinterpret_cast<const float*>(current_coeff));    // always aligned
          __m128 coeff_h = _mm_load_ps(reinterpret_cast<const float*>(current_coeff + 4));  // always aligned
          coeff = _mm256_set_m128(coeff_h, coeff_l);
        }
        __m256 dst = _mm256_mul_ps(data_single, coeff); // Multiply by coefficient
        result3 = _mm256_add_ps(result3, dst);

        current_coeff += 8;
      }

      // begin4, result4
      for (int i = 0; i < filter_size; i++) {
        __m256 data_single;
        if(sizeof(pixel_t)==2) // word
        {
          __m256i src256;
          if (avx2) {
            src256 = _mm256_cvtepu16_epi32(_mm_loadu_si128(reinterpret_cast<const __m128i*>(src + begin4 + i * 8))); // 8*16->8*32 bits
          }
          else {
            __m128i src_p = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src + begin4 + i * 8)); // uint16_t  8*16=128 8 pixels at a time
            __m128i src_l = _mm_unpacklo_epi16(src_p, zero128); // spread lower  4*uint16_t pixel value -> 4*32 bit
            __m128i src_h = _mm_unpackhi_epi16(src_p, zero128); // spread higher 4*uint16_t pixel value -> 4*32 bit
            src256 = _mm256_set_m128i(src_h, src_l);
          }
          data_single = _mm256_cvtepi32_ps (src256); // Converts the 8x signed 32-bit integer values of a to single-precision, floating-point values.
        }
        else { // float
          if (avx2) {
            data_single = _mm256_loadu_ps(reinterpret_cast<const float*>(src+begin4+i*8)); // float  8*32=256 8 pixels at a time
          }
          else {
            // using one 256bit load instead of 2x128bit is slower on avx-only Ivy
            data_l_single = _mm_loadu_ps(reinterpret_cast<const float*>(src + begin4 + i * 8)); // float  4*32=128 4 pixels at a time
            data_h_single = _mm_loadu_ps(reinterpret_cast<const float*>(src + begin4 + i * 8 + 4)); // float  4*32=128 4 pixels at a time
            data_single = _mm256_set_m128(data_h_single, data_l_single);
          }
        }
        __m256 coeff;
        if (avx2) {
          // using one 256bit load instead of 2x128bit is slower on avx-only Ivy
          coeff = _mm256_load_ps(reinterpret_cast<const float*>(current_coeff));    // always aligned
        }
        else {
          __m128 coeff_l = _mm_load_ps(reinterpret_cast<const float*>(current_coeff));    // always aligned
          __m128 coeff_h = _mm_load_ps(reinterpret_cast<const float*>(current_coeff + 4));  // always aligned
          coeff = _mm256_set_m128(coeff_h, coeff_l);
        }
        __m256 dst = _mm256_mul_ps(data_single, coeff); // Multiply by coefficient
        result4 = _mm256_add_ps(result4, dst);

        current_coeff += 8;
      }
      
      __m128 result;

      const __m128 sumQuad1 = _mm_add_ps(_mm256_castps256_ps128(result1), _mm256_extractf128_ps(result1, 1));
      const __m128 sumQuad2 = _mm_add_ps(_mm256_castps256_ps128(result2), _mm256_extractf128_ps(result2, 1));
      __m128 result12 = _mm_hadd_ps(sumQuad1, sumQuad2);
      const __m128 sumQuad3 = _mm_add_ps(_mm256_castps256_ps128(result3), _mm256_extractf128_ps(result3, 1));
      const __m128 sumQuad4 = _mm_add_ps(_mm256_castps256_ps128(result4), _mm256_extractf128_ps(result4, 1));
      __m128 result34 = _mm_hadd_ps(sumQuad3, sumQuad4);
      result = _mm_hadd_ps(result12, result34);

      if (sizeof(pixel_t) == 2)
      {
        // clamp!
        if(lessthan16bit)
          result = _mm_min_ps(result, clamp_limit); // for 10-14 bit 
        // low limit or 16 bit limit through pack_us
        // Converts the four single-precision, floating-point values of a to signed 32-bit integer values.
        __m128i result_4x_int32  = _mm_cvtps_epi32(result);  // 4 * 32 bit integers
        __m128i result_4x_uint16 = _mm_packus_epi32(result_4x_int32, zero128); // 4*32+zeros = lower 4*16 OK
        _mm_storel_epi64(reinterpret_cast<__m128i *>(dst + x), result_4x_uint16);
      }
      else { // float
        // aligned
        _mm_store_ps(reinterpret_cast<float*>(dst+x), result); // 4 results at a time
      }

    }

    dst += dst_pitch;
    src += src_pitch;
  }
  _mm256_zeroupper();
}

// for uint16_t and float. Both uses float arithmetic and coefficients
// see the same in resample_avx2
template<bool lessthan16bit, typename pixel_t, bool avx2>
void resize_v_avx_planar_16or32(BYTE* dst0, const BYTE* src0, int dst_pitch, int src_pitch, ResamplingProgram* program, int width, int target_height, int bits_per_pixel, const int* pitch_table, const void* storage)
{
  _mm256_zeroupper();
  int filter_size = program->filter_size;
  //short* current_coeff = program->pixel_coefficient;
  float* current_coeff_float = program->pixel_coefficient_float;

  int wMod8 = (width / 8) * 8; // uint16/float: 8 at a time (byte was 16 byte at a time)

  __m128i zero = _mm_setzero_si128();
  __m256i zero256 = _mm256_setzero_si256();

  const pixel_t* src = (pixel_t *)src0;
  pixel_t* dst = (pixel_t *)dst0;
  dst_pitch = dst_pitch / sizeof(pixel_t);
  src_pitch = src_pitch / sizeof(pixel_t);

  __m256 clamp_limit; // test, finally not used
  __m128i clamp_limit_i16;
  float limit;
  if (sizeof(pixel_t) == 2) {
    int max_pixel_value = ((int)1 << bits_per_pixel) - 1;
    limit = (float)max_pixel_value;
    clamp_limit = _mm256_set1_ps(limit); // clamp limit
    clamp_limit_i16 = _mm_set1_epi16(max_pixel_value); // clamp limit
  }

  for (int y = 0; y < target_height; y++) {
    int offset = program->pixel_offset[y];
    const pixel_t* src_ptr = src + pitch_table[offset]/sizeof(pixel_t);

    for (int x = 0; x < wMod8; x+=8) {
      __m256 result_single = _mm256_set1_ps(0.0f);

      const pixel_t* src2_ptr = src_ptr+x;

      for (int i = 0; i < filter_size; i++) {
        __m256 src_single;
        if (sizeof(pixel_t) == 2) // word
        {
          // avx solution is chosen is pointers are aligned
          __m128i src_p = _mm_load_si128(reinterpret_cast<const __m128i*>(src2_ptr)); // uint16_t  8*16=128 8 pixels at a time
          __m256i src256;
          if (avx2)
          {
            // AVX2: 
            //_mm256_unpacklo is not good, because it works on the 2 * lower_64_bit of the two 128bit halves
            src256 = _mm256_cvtepu16_epi32(src_p);
          }
          else {
            // simple avx
            __m128i src_l = _mm_unpacklo_epi16(src_p, zero); // spread lower  4*uint16_t pixel value -> 4*32 bit
            __m128i src_h = _mm_unpackhi_epi16(src_p, zero); // spread higher 4*uint16_t pixel value -> 4*32 bit
            src256 = _mm256_set_m128i(src_h, src_l);
          }
          src_single = _mm256_cvtepi32_ps(src256); // Converts the eight signed 32-bit integer values of avx to single-precision, floating-point values.
        }
        else { // float
          // avx solution is chosen is pointers are aligned
          __m128 src_l_single = _mm_load_ps(reinterpret_cast<const float*>(src2_ptr));   // float  4*32=128 4 pixels at a time
          __m128 src_h_single = _mm_load_ps(reinterpret_cast<const float*>(src2_ptr+4)); // float  4*32=128 4 pixels at a time
          src_single = _mm256_set_m128(src_h_single, src_l_single);
          // using one 256bit load instead of 2x128bit is slower on avx-only Ivy
          //src_single = _mm256_load_ps(reinterpret_cast<const float*>(src2_ptr)); // float  8*32=256 8 pixels at a time
        }
        __m256 coeff = _mm256_broadcast_ss(reinterpret_cast<const float*>(current_coeff_float+i)); // loads 1, fills all 8 floats
        __m256 dst = _mm256_mul_ps(src_single, coeff); // Multiply by coefficient // 8*(32bit*32bit=32bit)
        result_single = _mm256_add_ps(result_single, dst); // accumulate result.

        src2_ptr += src_pitch;
      }

      if(sizeof(pixel_t)==2) // word
      {
        // clamp! no! later at uint16 stage
        // result_single = _mm256_min_ps(result_single, clamp_limit_256); // mainly for 10-14 bit 
        // result = _mm_max_ps(result, zero); low limit through pack_us
        // Converts the 8 single-precision, floating-point values of a to signed 32-bit integer values.
        __m256i result256  = _mm256_cvtps_epi32(result_single);
        // Pack and store
        __m128i result = _mm_packus_epi32(_mm256_extractf128_si256(result256, 0), _mm256_extractf128_si256(result256, 1)); // 4*32+4*32 = 8*16
        if(lessthan16bit)
          result = _mm_min_epu16(result, clamp_limit_i16); // unsigned clamp here
        _mm_stream_si128(reinterpret_cast<__m128i*>(dst+x), result);
      }
      else { // float
        _mm256_stream_ps(reinterpret_cast<float*>(dst+x), result_single);
      }
    }

    // Leftover
    for (int x = wMod8; x < width; x++) {
      float result = 0;
      for (int i = 0; i < filter_size; i++) {
        result += (src_ptr+pitch_table[i]/sizeof(pixel_t))[x] * current_coeff_float[i];
      }
      if (!std::is_floating_point<pixel_t>::value) {  // floats are unscaled and uncapped
        result = clamp(result, 0.0f, limit);
      }
      dst[x] = (pixel_t) result;
    }

    dst += dst_pitch;
    current_coeff_float += filter_size;
  }
  _mm256_zeroupper();
}

// instantiate here
// avx 16,32bit
template void resizer_h_avx_generic_int16_float<false, uint16_t>(BYTE* dst8, const BYTE* src8, int dst_pitch, int src_pitch, ResamplingProgram* program, int width, int height, int bits_per_pixel);
template void resizer_h_avx_generic_int16_float<false, float   >(BYTE* dst8, const BYTE* src8, int dst_pitch, int src_pitch, ResamplingProgram* program, int width, int height, int bits_per_pixel);
// avx 10-14bit
template void resizer_h_avx_generic_int16_float<true, uint16_t>(BYTE* dst8, const BYTE* src8, int dst_pitch, int src_pitch, ResamplingProgram* program, int width, int height, int bits_per_pixel);

// avx 16,32bit
template void resize_v_avx_planar_16or32<false, uint16_t>(BYTE* dst0, const BYTE* src0, int dst_pitch, int src_pitch, ResamplingProgram* program, int width, int target_height, int bits_per_pixel, const int* pitch_table, const void* storage);
template void resize_v_avx_planar_16or32<false, float>(BYTE* dst0, const BYTE* src0, int dst_pitch, int src_pitch, ResamplingProgram* program, int width, int target_height, int bits_per_pixel, const int* pitch_table, const void* storage);
// avx 10-14bit
template void resize_v_avx_planar_16or32<true, uint16_t>(BYTE* dst0, const BYTE* src0, int dst_pitch, int src_pitch, ResamplingProgram* program, int width, int target_height, int bits_per_pixel, const int* pitch_table, const void* storage);
