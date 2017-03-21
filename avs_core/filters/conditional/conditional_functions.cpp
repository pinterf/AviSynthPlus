
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

#include "conditional_functions.h"
#include "../../core/internal.h"
#include <avs/config.h>
#include <avs/minmax.h>
#include <avs/alignment.h>
#include <emmintrin.h>
#include <limits>
#include <algorithm>
#include <cmath>
#include "../focus.h" // sad

extern const AVSFunction Conditional_funtions_filters[] = {
  {  "AverageLuma",    BUILTIN_FUNC_PREFIX, "c[offset]i", AveragePlane::Create, (void *)PLANAR_Y },
  {  "AverageChromaU", BUILTIN_FUNC_PREFIX, "c[offset]i", AveragePlane::Create, (void *)PLANAR_U },
  {  "AverageChromaV", BUILTIN_FUNC_PREFIX, "c[offset]i", AveragePlane::Create, (void *)PLANAR_V },
  {  "AverageR", BUILTIN_FUNC_PREFIX, "c[offset]i", AveragePlane::Create, (void *)PLANAR_R },
  {  "AverageG", BUILTIN_FUNC_PREFIX, "c[offset]i", AveragePlane::Create, (void *)PLANAR_G },
  {  "AverageB", BUILTIN_FUNC_PREFIX, "c[offset]i", AveragePlane::Create, (void *)PLANAR_B },
  //{  "AverageSat","c[offset]i", AverageSat::Create }, Sum(SatLookup[U,V])/N, SatLookup[U,V]=1.4087*sqrt((U-128)**2+(V-128)**2)
//{  "AverageHue","c[offset]i", AverageHue::Create }, Sum(HueLookup[U,V])/N, HueLookup[U,V]=40.5845*Atan2(U-128,V-128)

  {  "RGBDifference",     BUILTIN_FUNC_PREFIX, "cc", ComparePlane::Create, (void *)-1 },
  {  "LumaDifference",    BUILTIN_FUNC_PREFIX, "cc", ComparePlane::Create, (void *)PLANAR_Y },
  {  "ChromaUDifference", BUILTIN_FUNC_PREFIX, "cc", ComparePlane::Create, (void *)PLANAR_U },
  {  "ChromaVDifference", BUILTIN_FUNC_PREFIX, "cc", ComparePlane::Create, (void *)PLANAR_V },
  {  "RDifference", BUILTIN_FUNC_PREFIX, "cc", ComparePlane::Create, (void *)PLANAR_R },
  {  "GDifference", BUILTIN_FUNC_PREFIX, "cc", ComparePlane::Create, (void *)PLANAR_G },
  {  "BDifference", BUILTIN_FUNC_PREFIX, "cc", ComparePlane::Create, (void *)PLANAR_B },
  //{  "SatDifference","cc", CompareSat::Create }, Sum(Abs(SatLookup[U1,V1]-SatLookup[U2,V2]))/N
//{  "HueDifference","cc", CompareHue::Create }, Sum(Abs(HueLookup[U1,V1]-HueLookup[U2,V2]))/N

  {  "YDifferenceFromPrevious",   BUILTIN_FUNC_PREFIX, "c", ComparePlane::Create_prev, (void *)PLANAR_Y },
  {  "UDifferenceFromPrevious",   BUILTIN_FUNC_PREFIX, "c", ComparePlane::Create_prev, (void *)PLANAR_U },
  {  "VDifferenceFromPrevious",   BUILTIN_FUNC_PREFIX, "c", ComparePlane::Create_prev, (void *)PLANAR_V },
  {  "RGBDifferenceFromPrevious", BUILTIN_FUNC_PREFIX, "c", ComparePlane::Create_prev, (void *)-1 },
  {  "RDifferenceFromPrevious",   BUILTIN_FUNC_PREFIX, "c", ComparePlane::Create_prev, (void *)PLANAR_R },
  {  "GDifferenceFromPrevious",   BUILTIN_FUNC_PREFIX, "c", ComparePlane::Create_prev, (void *)PLANAR_G },
  {  "BDifferenceFromPrevious",   BUILTIN_FUNC_PREFIX, "c", ComparePlane::Create_prev, (void *)PLANAR_B },
  //{  "SatDifferenceFromPrevious","c", CompareSat::Create_prev },
//{  "HueDifferenceFromPrevious","c", CompareHue::Create_prev },

  {  "YDifferenceToNext",   BUILTIN_FUNC_PREFIX, "c[offset]i", ComparePlane::Create_next, (void *)PLANAR_Y },
  {  "UDifferenceToNext",   BUILTIN_FUNC_PREFIX, "c[offset]i", ComparePlane::Create_next, (void *)PLANAR_U },
  {  "VDifferenceToNext",   BUILTIN_FUNC_PREFIX, "c[offset]i", ComparePlane::Create_next, (void *)PLANAR_V },
  {  "RGBDifferenceToNext", BUILTIN_FUNC_PREFIX, "c[offset]i", ComparePlane::Create_next, (void *)-1 },
  {  "RDifferenceToNext",   BUILTIN_FUNC_PREFIX, "c[offset]i", ComparePlane::Create_next, (void *)PLANAR_R },
  {  "GDifferenceToNext",   BUILTIN_FUNC_PREFIX, "c[offset]i", ComparePlane::Create_next, (void *)PLANAR_G },
  {  "BDifferenceToNext",   BUILTIN_FUNC_PREFIX, "c[offset]i", ComparePlane::Create_next, (void *)PLANAR_B },
  //{  "SatDifferenceFromNext","c[offset]i", CompareSat::Create_next },
//{  "HueDifferenceFromNext","c[offset]i", CompareHue::Create_next },
  {  "YPlaneMax",    BUILTIN_FUNC_PREFIX, "c[threshold]f[offset]i", MinMaxPlane::Create_max, (void *)PLANAR_Y },
  {  "YPlaneMin",    BUILTIN_FUNC_PREFIX, "c[threshold]f[offset]i", MinMaxPlane::Create_min, (void *)PLANAR_Y },
  {  "YPlaneMedian", BUILTIN_FUNC_PREFIX, "c[offset]i", MinMaxPlane::Create_median, (void *)PLANAR_Y },
  {  "UPlaneMax",    BUILTIN_FUNC_PREFIX, "c[threshold]f[offset]i", MinMaxPlane::Create_max, (void *)PLANAR_U },
  {  "UPlaneMin",    BUILTIN_FUNC_PREFIX, "c[threshold]f[offset]i", MinMaxPlane::Create_min, (void *)PLANAR_U },
  {  "UPlaneMedian", BUILTIN_FUNC_PREFIX, "c[offset]i", MinMaxPlane::Create_median, (void *)PLANAR_U },
  {  "VPlaneMax",    BUILTIN_FUNC_PREFIX, "c[threshold]f[offset]i", MinMaxPlane::Create_max, (void *)PLANAR_V }, // AVS+! was before: missing offset parameter
  {  "VPlaneMin",    BUILTIN_FUNC_PREFIX, "c[threshold]f[offset]i", MinMaxPlane::Create_min, (void *)PLANAR_V }, // AVS+! was before: missing offset parameter
  {  "VPlaneMedian", BUILTIN_FUNC_PREFIX, "c[offset]i", MinMaxPlane::Create_median, (void *)PLANAR_V },
  {  "RPlaneMax",    BUILTIN_FUNC_PREFIX, "c[threshold]f[offset]i", MinMaxPlane::Create_max, (void *)PLANAR_R },
  {  "RPlaneMin",    BUILTIN_FUNC_PREFIX, "c[threshold]f[offset]i", MinMaxPlane::Create_min, (void *)PLANAR_R },
  {  "RPlaneMedian", BUILTIN_FUNC_PREFIX, "c[offset]i", MinMaxPlane::Create_median, (void *)PLANAR_R },
  {  "GPlaneMax",    BUILTIN_FUNC_PREFIX, "c[threshold]f[offset]i", MinMaxPlane::Create_max, (void *)PLANAR_G },
  {  "GPlaneMin",    BUILTIN_FUNC_PREFIX, "c[threshold]f[offset]i", MinMaxPlane::Create_min, (void *)PLANAR_G },
  {  "GPlaneMedian", BUILTIN_FUNC_PREFIX, "c[offset]i", MinMaxPlane::Create_median, (void *)PLANAR_G },
  {  "BPlaneMax",    BUILTIN_FUNC_PREFIX, "c[threshold]f[offset]i", MinMaxPlane::Create_max, (void *)PLANAR_B },
  {  "BPlaneMin",    BUILTIN_FUNC_PREFIX, "c[threshold]f[offset]i", MinMaxPlane::Create_min, (void *)PLANAR_B },
  {  "BPlaneMedian", BUILTIN_FUNC_PREFIX, "c[offset]i", MinMaxPlane::Create_median, (void *)PLANAR_B },
  {  "YPlaneMinMaxDifference", BUILTIN_FUNC_PREFIX, "c[threshold]f[offset]i", MinMaxPlane::Create_minmax, (void *)PLANAR_Y },
  {  "UPlaneMinMaxDifference", BUILTIN_FUNC_PREFIX, "c[threshold]f[offset]i", MinMaxPlane::Create_minmax, (void *)PLANAR_U }, // AVS+! was before: missing offset parameter
  {  "VPlaneMinMaxDifference", BUILTIN_FUNC_PREFIX, "c[threshold]f[offset]i", MinMaxPlane::Create_minmax, (void *)PLANAR_V }, // AVS+! was before: missing offset parameter
  {  "RPlaneMinMaxDifference", BUILTIN_FUNC_PREFIX, "c[threshold]f[offset]i", MinMaxPlane::Create_minmax, (void *)PLANAR_R },
  {  "GPlaneMinMaxDifference", BUILTIN_FUNC_PREFIX, "c[threshold]f[offset]i", MinMaxPlane::Create_minmax, (void *)PLANAR_G },
  {  "BPlaneMinMaxDifference", BUILTIN_FUNC_PREFIX, "c[threshold]f[offset]i", MinMaxPlane::Create_minmax, (void *)PLANAR_B },

//{  "SatMax","c[threshold]f[offset]i", MinMaxPlane::Create_maxsat },  ++accum[SatLookup[U,V]]
//{  "SatMin","c[threshold]f[offset]i", MinMaxPlane::Create_minsat },
//{  "SatMedian","c[offset]i", MinMaxPlane::Create_mediansat },
//{  "SatMinMaxDifference","c[threshold]f[offset]i", MinMaxPlane::Create_minmaxsat },

//{  "HueMax","c[threshold]f[offset]i", MinMaxPlane::Create_maxhue },  ++accum[HueLookup[U,V]]
//{  "HueMin","c[threshold]f[offset]i", MinMaxPlane::Create_minhue },
//{  "HueMedian","c[offset]i", MinMaxPlane::Create_medianhue },
//{  "HueMinMaxDifference","c[threshold]f[offset]i", MinMaxPlane::Create_minmaxhue },

  { 0 }
};


AVSValue AveragePlane::Create(AVSValue args, void* user_data, IScriptEnvironment* env) {
  int plane = (int)reinterpret_cast<intptr_t>(user_data);
  return AvgPlane(args[0], user_data, plane, args[1].AsInt(0), env);
}

// Average plane
template<typename pixel_t>
static double get_sum_of_pixels_c(const BYTE* srcp8, size_t height, size_t width, size_t pitch) {
  typedef typename std::conditional < sizeof(pixel_t) == 4, double, __int64>::type sum_t;
  sum_t accum = 0; // int32 holds sum of maximum 16 Mpixels for 8 bit, and 65536 pixels for uint16_t pixels
  const pixel_t *srcp = reinterpret_cast<const pixel_t *>(srcp8);
  pitch /= sizeof(pixel_t);
  for (size_t y = 0; y < height; y++) {
    for (size_t x = 0; x < width; x++) {
      accum += srcp[x];
    }
    srcp += pitch;
  }
  return (double)accum;
}

// sum: sad with zero
static double get_sum_of_pixels_sse2(const BYTE* srcp, size_t height, size_t width, size_t pitch) {
  size_t mod16_width = width / 16 * 16;
  __int64 result = 0;
  __m128i sum = _mm_setzero_si128();
  __m128i zero = _mm_setzero_si128();

  for (size_t y = 0; y < height; ++y) {
    for (size_t x = 0; x < mod16_width; x+=16) {
      __m128i src = _mm_load_si128(reinterpret_cast<const __m128i*>(srcp + x));
      __m128i sad = _mm_sad_epu8(src, zero);
      sum = _mm_add_epi32(sum, sad);
    }

    for (size_t x = mod16_width; x < width; ++x) {
      result += srcp[x];
    }

    srcp += pitch;
  }
  __m128i upper = _mm_castps_si128(_mm_movehl_ps(_mm_setzero_ps(), _mm_castsi128_ps(sum)));
  sum = _mm_add_epi32(sum, upper);
  result += _mm_cvtsi128_si32(sum);
  return (double)result;
}

#ifdef X86_32
static double get_sum_of_pixels_isse(const BYTE* srcp, size_t height, size_t width, size_t pitch) {
  size_t mod8_width = width / 8 * 8;
  __int64 result = 0;
  __m64 sum = _mm_setzero_si64();
  __m64 zero = _mm_setzero_si64();

  for (size_t y = 0; y < height; ++y) {
    for (size_t x = 0; x < mod8_width; x+=8) {
      __m64 src = *reinterpret_cast<const __m64*>(srcp + x);
      __m64 sad = _mm_sad_pu8(src, zero);
      sum = _mm_add_pi32(sum, sad);
    }

    for (size_t x = mod8_width; x < width; ++x) {
      result += srcp[x];
    }

    srcp += pitch;
  }
  result += _mm_cvtsi64_si32(sum);
  _mm_empty();
  return (double)result;
}
#endif



AVSValue AveragePlane::AvgPlane(AVSValue clip, void* user_data, int plane, int offset, IScriptEnvironment* env)
{
  if (!clip.IsClip())
    env->ThrowError("Average Plane: No clip supplied!");

  PClip child = clip.AsClip();
  VideoInfo vi = child->GetVideoInfo();

  if (!vi.IsPlanar())
    env->ThrowError("Average Plane: Only planar YUV or planar RGB images supported!");

  AVSValue cn = env->GetVarDef("current_frame");
  if (!cn.IsInt())
    env->ThrowError("Average Plane: This filter can only be used within run-time filters");

  int n = cn.AsInt();
  n = min(max(n+offset,0), vi.num_frames-1);

  PVideoFrame src = child->GetFrame(n,env);

  int pixelsize = vi.ComponentSize();

  const BYTE* srcp = src->GetReadPtr(plane);
  int height = src->GetHeight(plane);
  int width = src->GetRowSize(plane) / pixelsize;
  int pitch = src->GetPitch(plane);

  if (width == 0 || height == 0)
    env->ThrowError("Average Plane: plane does not exist!");

  double sum = 0.0;
  

  int total_pixels = width*height;
  bool sum_in_32bits;
  if (pixelsize == 4)
    sum_in_32bits = false;
  else // worst case
    sum_in_32bits = ((__int64)total_pixels * (pixelsize == 1 ? 255 : 65535)) <= std::numeric_limits<int>::max();

  if ((pixelsize==1) && sum_in_32bits && (env->GetCPUFlags() & CPUF_SSE2) && IsPtrAligned(srcp, 16) && width >= 16) {
    sum = get_sum_of_pixels_sse2(srcp, height, width, pitch);
  } else 
#ifdef X86_32
  if ((pixelsize==1) && sum_in_32bits && (env->GetCPUFlags() & CPUF_INTEGER_SSE) && width >= 8) {
    sum = get_sum_of_pixels_isse(srcp, height, width, pitch);
  } else 
#endif
  {
    if(pixelsize==1)
      sum = get_sum_of_pixels_c<uint8_t>(srcp, height, width, pitch);
    else if(pixelsize==2)
      sum = get_sum_of_pixels_c<uint16_t>(srcp, height, width, pitch);
    else // pixelsize==4
      sum = get_sum_of_pixels_c<float>(srcp, height, width, pitch);
  }

  float f = (float)(sum / (height * width));

  return (AVSValue)f;
}

AVSValue ComparePlane::Create(AVSValue args, void* user_data, IScriptEnvironment* env) {
  int plane = (int)reinterpret_cast<intptr_t>(user_data);
  return CmpPlane(args[0],args[1], user_data, plane, env);
}

AVSValue ComparePlane::Create_prev(AVSValue args, void* user_data, IScriptEnvironment* env) {
  int plane = (int)reinterpret_cast<intptr_t>(user_data);
  return CmpPlaneSame(args[0], user_data, -1, plane, env);
}

AVSValue ComparePlane::Create_next(AVSValue args, void* user_data, IScriptEnvironment* env) {
  int plane = (int)reinterpret_cast<intptr_t>(user_data);
  return CmpPlaneSame(args[0], user_data, args[1].AsInt(1), plane, env);
}


template<typename pixel_t>
static double get_sad_c(const BYTE* c_plane8, const BYTE* t_plane8, size_t height, size_t width, size_t c_pitch, size_t t_pitch) {
  const pixel_t *c_plane = reinterpret_cast<const pixel_t *>(c_plane8);
  const pixel_t *t_plane = reinterpret_cast<const pixel_t *>(t_plane8);
  c_pitch /= sizeof(pixel_t);
  t_pitch /= sizeof(pixel_t);
  typedef typename std::conditional < sizeof(pixel_t) == 4, double, __int64>::type sum_t;
  sum_t accum = 0; // int32 holds sum of maximum 16 Mpixels for 8 bit, and 65536 pixels for uint16_t pixels

  for (size_t y = 0; y < height; y++) {
    for (size_t x = 0; x < width; x++) {
      accum += std::abs(t_plane[x] - c_plane[x]);
    }
    c_plane += c_pitch;
    t_plane += t_pitch;
  }
  return (double)accum;

}

template<typename pixel_t>
static double get_sad_rgb_c(const BYTE* c_plane8, const BYTE* t_plane8, size_t height, size_t width, size_t c_pitch, size_t t_pitch) {
  const pixel_t *c_plane = reinterpret_cast<const pixel_t *>(c_plane8);
  const pixel_t *t_plane = reinterpret_cast<const pixel_t *>(t_plane8);
  c_pitch /= sizeof(pixel_t);
  t_pitch /= sizeof(pixel_t);
  __int64 accum = 0; // packed rgb: integer type only
  for (size_t y = 0; y < height; y++) {
    for (size_t x = 0; x < width; x+=4) {
      accum += std::abs(t_plane[x] - c_plane[x]);
      accum += std::abs(t_plane[x+1] - c_plane[x+1]);
      accum += std::abs(t_plane[x+2] - c_plane[x+2]);
    }
    c_plane += c_pitch;
    t_plane += t_pitch;
  }
  return (double)accum;

}

#if 0
// duplicate code, let's use sad from focus.cpp, which is good for big sads (int64)
static size_t get_sad_sse2(const BYTE* src_ptr, const BYTE* other_ptr, size_t height, size_t width, size_t src_pitch, size_t other_pitch) {
  size_t mod16_width = width / 16 * 16;
  size_t result = 0;
  __m128i sum = _mm_setzero_si128();
  for (size_t y = 0; y < height; ++y) {
    for (size_t x = 0; x < mod16_width; x+=16) {
      __m128i src = _mm_load_si128(reinterpret_cast<const __m128i*>(src_ptr + x));
      __m128i other = _mm_load_si128(reinterpret_cast<const __m128i*>(other_ptr + x));
      __m128i sad = _mm_sad_epu8(src, other);
      sum = _mm_add_epi32(sum, sad);
    }

    for (size_t x = mod16_width; x < width; ++x) {
      result += std::abs(src_ptr[x] - other_ptr[x]);
    }

    src_ptr += src_pitch;
    other_ptr += other_pitch;
  }
  __m128i upper = _mm_castps_si128(_mm_movehl_ps(_mm_setzero_ps(), _mm_castsi128_ps(sum)));
  sum = _mm_add_epi32(sum, upper);
  result += _mm_cvtsi128_si32(sum);
  return result;
}
#endif

#if 0
// duplicate code, let's use sad from focus.cpp, which is good for big sads (int64)
// for RGB32/64
template<typename pixel_t>
static size_t get_sad_rgb_sse2(const BYTE* src_ptr, const BYTE* other_ptr, size_t height, size_t width, size_t src_pitch, size_t other_pitch) {
  // width is rowsize here
  size_t mod16_width = width / 16 * 16;
  size_t result = 0;
  __m128i zero = _mm_setzero_si128();
  __m128i sum = _mm_setzero_si128();
  __m128i rgb_mask;
  if(sizeof(pixel_t) == 1)
    rgb_mask = _mm_set1_epi32(0x00FFFFFF);
  else
    rgb_mask = _mm_set_epi32(0x0000FFFF,0xFFFFFFFF,0x0000FFFF,0xFFFFFFFF);

  for (size_t y = 0; y < height; ++y) {
    for (size_t x = 0; x < mod16_width; x+=16) {
      __m128i src = _mm_load_si128(reinterpret_cast<const __m128i*>(src_ptr + x));
      __m128i other = _mm_load_si128(reinterpret_cast<const __m128i*>(other_ptr + x));
      src = _mm_and_si128(src, rgb_mask);
      other = _mm_and_si128(other, rgb_mask);
      if (sizeof(pixel_t) == 1) {
        __m128i sad = _mm_sad_epu8(src, other); // Sads in lo64/hi64
        sum = _mm_add_epi32(sum, sad);
      }
      else {
        __m128i greater_t = _mm_subs_epu16(src, other); // unsigned sub with saturation
        __m128i smaller_t = _mm_subs_epu16(other, src);
        __m128i absdiff = _mm_or_si128(greater_t, smaller_t); //abs(s1-s2)  == (satsub(s1,s2) | satsub(s2,s1))
                                                              // 8 x uint16 absolute differences
        sum = _mm_add_epi32(sum, _mm_unpacklo_epi16(absdiff, zero));
        sum = _mm_add_epi32(sum, _mm_unpackhi_epi16(absdiff, zero));
        // sum0_32, sum1_32, sum2_32, sum3_32
      }
    }

    for (size_t x = mod16_width; x < width; ++x) {
      result += std::abs(src_ptr[x] - other_ptr[x]);
    }

    src_ptr += src_pitch;
    other_ptr += other_pitch;
  }
  // summing up partial sums,
  if(sizeof(pixel_t) == 2) {
    // at 16 bits: we have 4 integers for sum: a0 a1 a2 a3
    __m128i a0_a1 = _mm_unpacklo_epi32(sum, zero); // a0 0 a1 0
    __m128i a2_a3 = _mm_unpackhi_epi32(sum, zero); // a2 0 a3 0
    sum = _mm_add_epi32( a0_a1, a2_a3 ); // a0+a2, 0, a1+a3, 0
                                         /* SSSE3: told to be not too fast
                                         sum = _mm_hadd_epi32(sum, zero);  // A1+A2, B1+B2, 0+0, 0+0
                                         sum = _mm_hadd_epi32(sum, zero);  // A1+A2+B1+B2, 0+0+0+0, 0+0+0+0, 0+0+0+0
                                         */
  }

  __m128i upper = _mm_castps_si128(_mm_movehl_ps(_mm_setzero_ps(), _mm_castsi128_ps(sum)));
  sum = _mm_add_epi32(sum, upper);
  result += _mm_cvtsi128_si32(sum);
  return result;
}
#endif

#ifdef X86_32

static size_t get_sad_isse(const BYTE* src_ptr, const BYTE* other_ptr, size_t height, size_t width, size_t src_pitch, size_t other_pitch) {
  size_t mod8_width = width / 8 * 8;
  size_t result = 0;
  __m64 sum = _mm_setzero_si64();

  for (size_t y = 0; y < height; ++y) {
    for (size_t x = 0; x < mod8_width; x+=8) {
      __m64 src = *reinterpret_cast<const __m64*>(src_ptr + x);
      __m64 other = *reinterpret_cast<const __m64*>(other_ptr + x);
      __m64 sad = _mm_sad_pu8(src, other);
      sum = _mm_add_pi32(sum, sad);
    }

    for (size_t x = mod8_width; x < width; ++x) {
      result += std::abs(src_ptr[x] - other_ptr[x]);
    }

    src_ptr += src_pitch;
    other_ptr += other_pitch;
  }
  result += _mm_cvtsi64_si32(sum);
  _mm_empty();
  return result;
}

static size_t get_sad_rgb_isse(const BYTE* src_ptr, const BYTE* other_ptr, size_t height, size_t width, size_t src_pitch, size_t other_pitch) {
  size_t mod8_width = width / 8 * 8;
  size_t result = 0;
  __m64 rgb_mask = _mm_set1_pi32(0x00FFFFFF);
  __m64 sum = _mm_setzero_si64();

  for (size_t y = 0; y < height; ++y) {
    for (size_t x = 0; x < mod8_width; x+=8) {
      __m64 src = *reinterpret_cast<const __m64*>(src_ptr + x);
      __m64 other = *reinterpret_cast<const __m64*>(other_ptr + x);
      src = _mm_and_si64(src, rgb_mask);
      other = _mm_and_si64(other, rgb_mask);
      __m64 sad = _mm_sad_pu8(src, other);
      sum = _mm_add_pi32(sum, sad);
    }

    for (size_t x = mod8_width; x < width; ++x) {
      result += std::abs(src_ptr[x] - other_ptr[x]);
    }

    src_ptr += src_pitch;
    other_ptr += other_pitch;
  }
  result += _mm_cvtsi64_si32(sum);
  _mm_empty();
  return result;
}

#endif



AVSValue ComparePlane::CmpPlane(AVSValue clip, AVSValue clip2, void* user_data, int plane, IScriptEnvironment* env)
{
  if (!clip.IsClip())
    env->ThrowError("Plane Difference: No clip supplied!");
  if (!clip2.IsClip())
    env->ThrowError("Plane Difference: Second parameter is not a clip!");

  PClip child = clip.AsClip();
  VideoInfo vi = child->GetVideoInfo();
  PClip child2 = clip2.AsClip();
  VideoInfo vi2 = child2->GetVideoInfo();
  if (plane !=-1 ) {
    if (!vi.IsPlanar() || !vi2.IsPlanar())
      env->ThrowError("Plane Difference: Only planar YUV or planar RGB images supported!");
  } else {
    if(vi.IsPlanarRGB() || vi.IsPlanarRGBA())
      env->ThrowError("RGB Difference: Planar RGB is not supported here (clip 1)");
    if(vi2.IsPlanarRGB() || vi2.IsPlanarRGBA())
      env->ThrowError("RGB Difference: Planar RGB is not supported here (clip 2)");
    if (!vi.IsRGB())
      env->ThrowError("RGB Difference: RGB difference can only be tested on RGB images! (clip 1)");
    if (!vi2.IsRGB())
      env->ThrowError("RGB Difference: RGB difference can only be tested on RGB images! (clip 2)");
    plane = 0;
  }

  AVSValue cn = env->GetVarDef("current_frame");
  if (!cn.IsInt())
    env->ThrowError("Plane Difference: This filter can only be used within run-time filters");

  int n = cn.AsInt();
  n = clamp(n,0,vi.num_frames-1);

  PVideoFrame src = child->GetFrame(n,env);
  PVideoFrame src2 = child2->GetFrame(n,env);

  int pixelsize = vi.ComponentSize();
  int bits_per_pixel = vi.BitsPerComponent();

  const BYTE* srcp = src->GetReadPtr(plane);
  const BYTE* srcp2 = src2->GetReadPtr(plane);
  const int height = src->GetHeight(plane);
  const int rowsize = src->GetRowSize(plane);
  const int width = rowsize / pixelsize;
  const int pitch = src->GetPitch(plane);
  const int height2 = src2->GetHeight(plane);
  const int rowsize2 = src2->GetRowSize(plane);
  const int width2 = rowsize2 / pixelsize;
  const int pitch2 = src2->GetPitch(plane);

  if(vi.ComponentSize() != vi2.ComponentSize())
    env->ThrowError("Plane Difference: Bit-depth are not the same!");

  if (width == 0 || height == 0)
    env->ThrowError("Plane Difference: plane does not exist!");

  if (height != height2 || width != width2)
    env->ThrowError("Plane Difference: Images are not the same size!");

  int total_pixels = width*height;
  bool sum_in_32bits;
  if (pixelsize == 4)
    sum_in_32bits = false;
  else // worst case check
    sum_in_32bits = ((__int64)total_pixels * ((1 << bits_per_pixel) - 1)) <= std::numeric_limits<int>::max();

  double sad = 0.0;

  // for c: width, for sse: rowsize
  if (vi.IsRGB32() || vi.IsRGB64()) {
    if ((pixelsize == 2) && (env->GetCPUFlags() & CPUF_SSE2) && IsPtrAligned(srcp, 16) && IsPtrAligned(srcp2, 16) && rowsize >= 16) {
      // int64 internally, no sum_in_32bits
      sad = (double)calculate_sad_8_or_16_sse2<uint16_t,true>(srcp, srcp2, pitch, pitch2, width*pixelsize, height); // in focus. 21.68/21.39
    } else if ((pixelsize == 1) && (env->GetCPUFlags() & CPUF_SSE2) && IsPtrAligned(srcp, 16) && IsPtrAligned(srcp2, 16) && rowsize >= 16) {
      sad = (double)calculate_sad_8_or_16_sse2<uint8_t,true>(srcp, srcp2, pitch, pitch2, rowsize, height); // in focus, no overflow
    } else
#ifdef X86_32
      if ((pixelsize==1) && sum_in_32bits && (env->GetCPUFlags() & CPUF_INTEGER_SSE) && width >= 8) {
        sad = get_sad_rgb_isse(srcp, srcp2, height, rowsize, pitch, pitch2);
      } else 
#endif
      {
        if (pixelsize == 1)
          sad = (double)get_sad_rgb_c<uint8_t>(srcp, srcp2, height, width, pitch, pitch2);
        else // pixelsize==2
          sad = (double)get_sad_rgb_c<uint16_t>(srcp, srcp2, height, width, pitch, pitch2);
      }
  } else {
    if ((pixelsize==2) && (env->GetCPUFlags() & CPUF_SSE2) && IsPtrAligned(srcp, 16) && IsPtrAligned(srcp2, 16) && rowsize >= 16) {
      sad = (double)calculate_sad_8_or_16_sse2<uint16_t,false>(srcp, srcp2, pitch, pitch2, rowsize, height); // in focus, no overflow
    } else
      if ((pixelsize==1) && (env->GetCPUFlags() & CPUF_SSE2) && IsPtrAligned(srcp, 16) && IsPtrAligned(srcp2, 16) && rowsize >= 16) {
      sad = (double)calculate_sad_8_or_16_sse2<uint8_t,false>(srcp, srcp2, pitch, pitch2, rowsize, height); // in focus, no overflow
    } else
#ifdef X86_32
      if ((pixelsize==1) && sum_in_32bits && (env->GetCPUFlags() & CPUF_INTEGER_SSE) && width >= 8) {
        sad = get_sad_isse(srcp, srcp2, height, rowsize, pitch, pitch2);
      } else 
#endif
      {
        if(pixelsize==1)
          sad = get_sad_c<uint8_t>(srcp, srcp2, height, width, pitch, pitch2);
        else if(pixelsize==2)
          sad = get_sad_c<uint16_t>(srcp, srcp2, height, width, pitch, pitch2);
        else // pixelsize==4
          sad = get_sad_c<float>(srcp, srcp2, height, width, pitch, pitch2);
      }
  }

  float f;

  if (vi.IsRGB32() || vi.IsRGB64())
    f = (float)((sad * 4) / (height * width * 3)); // why * 4/3? alpha plane was masked out, anyway
  else
    f = (float)(sad / (height * width));

  return (AVSValue)f;
}


AVSValue ComparePlane::CmpPlaneSame(AVSValue clip, void* user_data, int offset, int plane, IScriptEnvironment* env)
{
  if (!clip.IsClip())
    env->ThrowError("Plane Difference: No clip supplied!");

  PClip child = clip.AsClip();
  VideoInfo vi = child->GetVideoInfo();
  if (plane ==-1 ) {
    if (!vi.IsRGB() || vi.IsPlanarRGB() || vi.IsPlanarRGBA())
      env->ThrowError("RGB Difference: RGB difference can only be calculated on packed RGB images");
    plane = 0;
  } else {
    if (!vi.IsPlanar())
      env->ThrowError("Plane Difference: Only planar YUV or planar RGB images images supported!");
  }

  AVSValue cn = env->GetVarDef("current_frame");
  if (!cn.IsInt())
    env->ThrowError("Plane Difference: This filter can only be used within run-time filters");

  int n = cn.AsInt();
  n = clamp(n,0,vi.num_frames-1);
  int n2 = clamp(n+offset,0,vi.num_frames-1);

  PVideoFrame src = child->GetFrame(n,env);
  PVideoFrame src2 = child->GetFrame(n2,env);

  int pixelsize = vi.ComponentSize();
  int bits_per_pixel = vi.BitsPerComponent();

  const BYTE* srcp = src->GetReadPtr(plane);
  const BYTE* srcp2 = src2->GetReadPtr(plane);
  int height = src->GetHeight(plane);
  int rowsize = src->GetRowSize(plane);
  int width = rowsize / pixelsize;
  int pitch = src->GetPitch(plane);
  int pitch2 = src2->GetPitch(plane);

  if (width == 0 || height == 0)
    env->ThrowError("Plane Difference: No chroma planes in greyscale clip!");

  int total_pixels = width*height;
  bool sum_in_32bits;
  if (pixelsize == 4)
    sum_in_32bits = false;
  else // worst case check
    sum_in_32bits = ((__int64)total_pixels * ((1 << bits_per_pixel) - 1)) <= std::numeric_limits<int>::max();

  double sad = 0;
  // for c: width, for sse: rowsize
  if (vi.IsRGB32() || vi.IsRGB64()) {
    if ((pixelsize == 2) && (env->GetCPUFlags() & CPUF_SSE2) && IsPtrAligned(srcp, 16) && IsPtrAligned(srcp2, 16) && rowsize >= 16) {
      // int64 internally, no sum_in_32bits
      sad = (double)calculate_sad_8_or_16_sse2<uint16_t,true>(srcp, srcp2, pitch, pitch2, rowsize, height); // in focus. 21.68/21.39
    } else if ((pixelsize == 1) && (env->GetCPUFlags() & CPUF_SSE2) && IsPtrAligned(srcp, 16) && IsPtrAligned(srcp2, 16) && rowsize >= 16) {
      sad = (double)calculate_sad_8_or_16_sse2<uint8_t,true>(srcp, srcp2, pitch, pitch2, rowsize, height); // in focus, no overflow
    } else
#ifdef X86_32
      if ((pixelsize==1) && sum_in_32bits && (env->GetCPUFlags() & CPUF_INTEGER_SSE) && width >= 8) {
        sad = get_sad_rgb_isse(srcp, srcp2, height, rowsize, pitch, pitch2);
      } else 
#endif
      {
        if(pixelsize==1)
          sad = get_sad_rgb_c<uint8_t>(srcp, srcp2, height, width, pitch, pitch2);
        else
          sad = get_sad_rgb_c<uint16_t>(srcp, srcp2, height, width, pitch, pitch2);
      }
  } else {
    if ((pixelsize==2) && (env->GetCPUFlags() & CPUF_SSE2) && IsPtrAligned(srcp, 16) && IsPtrAligned(srcp2, 16) && rowsize >= 16) {
      sad = (double)calculate_sad_8_or_16_sse2<uint16_t,false>(srcp, srcp2, pitch, pitch2, rowsize, height); // in focus, no overflow
    } else if ((pixelsize==1) && (env->GetCPUFlags() & CPUF_SSE2) && IsPtrAligned(srcp, 16) && IsPtrAligned(srcp2, 16) && rowsize >= 16) {
      sad = (double)calculate_sad_8_or_16_sse2<uint8_t,false>(srcp, srcp2, pitch, pitch2, rowsize, height); // in focus, no overflow
    } else
#ifdef X86_32
      if ((pixelsize==1) && sum_in_32bits && (env->GetCPUFlags() & CPUF_INTEGER_SSE) && width >= 8) {
        sad = get_sad_isse(srcp, srcp2, height, width, pitch, pitch2);
      } else 
#endif
      {
        if(pixelsize==1)
          sad = get_sad_c<uint8_t>(srcp, srcp2, height, width, pitch, pitch2);
        else if (pixelsize==2)
          sad = get_sad_c<uint16_t>(srcp, srcp2, height, width, pitch, pitch2);
        else // pixelsize==4
          sad = get_sad_c<float>(srcp, srcp2, height, width, pitch, pitch2);
      }
  }

  float f;

  if (vi.IsRGB32() || vi.IsRGB64())
    f = (float)((sad * 4) / (height * width * 3));
  else
    f = (float)(sad / (height * width));

  return (AVSValue)f;
}


AVSValue MinMaxPlane::Create_max(AVSValue args, void* user_data, IScriptEnvironment* env) {
  int plane = (int)reinterpret_cast<intptr_t>(user_data);
  return MinMax(args[0], user_data, args[1].AsDblDef(0.0), args[2].AsInt(0), plane, MAX, env);
}

AVSValue MinMaxPlane::Create_min(AVSValue args, void* user_data, IScriptEnvironment* env) {
  int plane = (int)reinterpret_cast<intptr_t>(user_data);
  return MinMax(args[0], user_data, args[1].AsDblDef(0.0), args[2].AsInt(0), plane, MIN, env);
}

AVSValue MinMaxPlane::Create_median(AVSValue args, void* user_data, IScriptEnvironment* env) {
  int plane = (int)reinterpret_cast<intptr_t>(user_data);
  return MinMax(args[0], user_data, 50.0, args[1].AsInt(0), plane, MIN, env);
}

AVSValue MinMaxPlane::Create_minmax(AVSValue args, void* user_data, IScriptEnvironment* env) {
  int plane = (int)reinterpret_cast<intptr_t>(user_data);
  return MinMax(args[0], user_data, args[1].AsDblDef(0.0), args[2].AsInt(0), plane, MINMAX_DIFFERENCE, env);
}


AVSValue MinMaxPlane::MinMax(AVSValue clip, void* user_data, double threshold, int offset, int plane, int mode, IScriptEnvironment* env) {

  if (!clip.IsClip())
    env->ThrowError("MinMax: No clip supplied!");

  PClip child = clip.AsClip();
  VideoInfo vi = child->GetVideoInfo();

  if (!vi.IsPlanar())
    env->ThrowError("MinMax: Image must be planar");

  int pixelsize = vi.ComponentSize();
  int buffersize = pixelsize == 1 ? 256 : 65536; // 65536 for float, too, reason for 10-14 bits: avoid overflow
  int real_buffersize = pixelsize == 4 ? 65536 : (1 << vi.BitsPerComponent());
  uint32_t *accum_buf = new uint32_t[buffersize];

  // Get current frame number
  AVSValue cn = env->GetVarDef("current_frame");
  if (!cn.IsInt())
    env->ThrowError("MinMax: This filter can only be used within run-time filters");

  int n = cn.AsInt();
  n = min(max(n+offset,0),vi.num_frames-1);

#ifdef DEBUG_GSCRIPTCLIP_MT
  _RPT3(0, "Inside MinMax getFrame cn=%d n=%d thread=%d\r", cn.AsInt(), n, GetCurrentThreadId());
#endif
  // Prepare the source
  PVideoFrame src = child->GetFrame(n, env);
#ifdef DEBUG_GSCRIPTCLIP_MT
  _RPT2(0, "After MinMax getFrame cn=%d n=%d\r", cn.AsInt(), n, GetCurrentThreadId());
#endif

  const BYTE* srcp = src->GetReadPtr(plane);
  int pitch = src->GetPitch(plane);
  int w = src->GetRowSize(plane) / pixelsize;
  int h = src->GetHeight(plane);

  if (w == 0 || h == 0)
    env->ThrowError("MinMax: plane does not exist!");

  // Reset accumulators
  std::fill_n(accum_buf, buffersize, 0);

  // Count each component
  if(pixelsize==1) {
    for (int y=0;y<h;y++) {
      for (int x=0;x<w;x++) {
         accum_buf[srcp[x]]++;
      }
      srcp+=pitch;
    }
  }
  else if (pixelsize == 2) {
    for (int y=0;y<h;y++) {
      for (int x=0;x<w;x++) {
        accum_buf[reinterpret_cast<const uint16_t *>(srcp)[x]]++;
      }
      srcp+=pitch;
    }
  } else { //pixelsize==4 float
    // for float results are always checked with 16 bit precision only
    // or else we cannot populate non-digital steps with this standard method
    for (int y=0;y<h;y++) {
      for (int x=0;x<w;x++) {
        accum_buf[clamp((int)(reinterpret_cast<const float *>(srcp)[x] * 65535.0f), 0, 65535)]++;
      }
      srcp+=pitch;
    }
  }

  int pixels = w*h;
  threshold /=100.0;  // Thresh now 0-1
  threshold = clamp(threshold, 0.0, 1.0);

  unsigned int tpixels = (unsigned int)(pixels*threshold);

  int retval;

    // Find the value we need.
  if (mode == MIN) {
    unsigned int counted=0;
    retval = real_buffersize - 1;
    for (int i = 0; i< real_buffersize;i++) {
      counted += accum_buf[i];
      if (counted>tpixels) {
        retval = i;
        break;
      }
    }
  } else if (mode == MAX) {
    unsigned int counted=0;
    retval = 0;
    for (int i = real_buffersize-1; i>=0;i--) {
      counted += accum_buf[i];
      if (counted>tpixels) {
        retval = i;
        break;
      }
    }
  } else if (mode == MINMAX_DIFFERENCE) {
    unsigned int counted=0;
    int i, t_min = 0;
    // Find min
    for (i = 0; i < real_buffersize;i++) {
      counted += accum_buf[i];
      if (counted>tpixels) {
        t_min=i;
        break;
      }
    }

    // Find max
    counted=0;
    int t_max = real_buffersize-1;
    for (i = real_buffersize-1; i>=0;i--) {
      counted += accum_buf[i];
      if (counted>tpixels) {
        t_max=i;
        break;
      }
    }

    retval = t_max - t_min; // results <0 will be returned if threshold > 50
  }
  else {
    retval = -1;
  }

  delete[] accum_buf;
  _RPT2(0, "End of MinMax cn=%d n=%d\r", cn.AsInt(), n);

  if (pixelsize == 4)
    return AVSValue((double)retval / (real_buffersize-1)); // convert back to float, /65535
  else
    return AVSValue(retval);
}
