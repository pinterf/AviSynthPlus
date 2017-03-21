// Avisynth v1.0 beta.  Copyright 2000 Ben Rudiak-Gould.
// http://www.math.berkeley.edu/~benrg/avisynth.html

//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2000 Avery Lee
//
//	This program is free software; you can redistribute it and/or modify
//	it under the terms of the GNU General Public License as published by
//	the Free Software Foundation; either version 2 of the License, or
//	(at your option) any later version.
//
//	This program is distributed in the hope that it will be useful,
//	but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//	GNU General Public License for more details.
//
//	You should have received a copy of the GNU General Public License
//	along with this program; if not, write to the Free Software
//	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#include <avs/cpuid.h>
#include <intrin.h>

#define IS_BIT_SET(bitfield, bit) ((bitfield) & (1<<(bit)) ? true : false)

#ifndef _XCR_XFEATURE_ENABLED_MASK
#define _XCR_XFEATURE_ENABLED_MASK 0
#endif

static int CPUCheckForExtensions()
{
  int result = 0;
  int cpuinfo[4];

  __cpuid(cpuinfo, 1);
  if (IS_BIT_SET(cpuinfo[3], 0))
    result |= CPUF_FPU;
  if (IS_BIT_SET(cpuinfo[3], 23))
    result |= CPUF_MMX;
  if (IS_BIT_SET(cpuinfo[3], 25))
    result |= CPUF_SSE | CPUF_INTEGER_SSE;
  if (IS_BIT_SET(cpuinfo[3], 26))
    result |= CPUF_SSE2;
  if (IS_BIT_SET(cpuinfo[2], 0))
    result |= CPUF_SSE3;
  if (IS_BIT_SET(cpuinfo[2], 9))
    result |= CPUF_SSSE3;
  if (IS_BIT_SET(cpuinfo[2], 19))
    result |= CPUF_SSE4_1;
  if (IS_BIT_SET(cpuinfo[2], 20))
    result |= CPUF_SSE4_2;
  if (IS_BIT_SET(cpuinfo[2], 22))
    result |= CPUF_MOVBE;
  if (IS_BIT_SET(cpuinfo[2], 23))
    result |= CPUF_POPCNT;
  if (IS_BIT_SET(cpuinfo[2], 25))
    result |= CPUF_AES;
  if (IS_BIT_SET(cpuinfo[2], 29))
    result |= CPUF_F16C;
  // AVX
  bool xgetbv_supported = IS_BIT_SET(cpuinfo[2], 27);
  bool avx_supported = IS_BIT_SET(cpuinfo[2], 28);
  if (xgetbv_supported && avx_supported)
  {
    unsigned long long xgetbv0 = _xgetbv(_XCR_XFEATURE_ENABLED_MASK);
    if ((xgetbv0 & 0x6ull) == 0x6ull) {
      result |= CPUF_AVX;
      if (IS_BIT_SET(cpuinfo[2], 12))
        result |= CPUF_FMA3;
      __cpuid(cpuinfo, 7);
      if (IS_BIT_SET(cpuinfo[1], 5))
        result |= CPUF_AVX2;
    }
    if((xgetbv0 & (0x7ull << 5)) && // OPMASK: upper-256 enabled by OS
       (xgetbv0 & (0x3ull << 1))) { // XMM/YMM enabled by OS
      // Verify that XCR0[7:5] = ‘111b’ (OPMASK state, upper 256-bit of ZMM0-ZMM15 and
      // ZMM16-ZMM31 state are enabled by OS)
      /// and that XCR0[2:1] = ‘11b’ (XMM state and YMM state are enabled by OS).
      __cpuid(cpuinfo, 7);
      if (IS_BIT_SET(cpuinfo[1], 16))
        result |= CPUF_AVX512F;
      if (IS_BIT_SET(cpuinfo[1], 17))
        result |= CPUF_AVX512DQ;
      if (IS_BIT_SET(cpuinfo[1], 21))
        result |= CPUF_AVX512IFMA;
      if (IS_BIT_SET(cpuinfo[1], 26))
        result |= CPUF_AVX512PF;
      if (IS_BIT_SET(cpuinfo[1], 27))
        result |= CPUF_AVX512ER;
      if (IS_BIT_SET(cpuinfo[1], 28))
        result |= CPUF_AVX512CD;
      if (IS_BIT_SET(cpuinfo[1], 30))
        result |= CPUF_AVX512BW;
      if (IS_BIT_SET(cpuinfo[1], 31))
        result |= CPUF_AVX512VL;
      if (IS_BIT_SET(cpuinfo[2], 1)) // [2]!
        result |= CPUF_AVX512VBMI;
    }
  }

  // 3DNow!, 3DNow!, ISSE, FMA4
  __cpuid(cpuinfo, 0x80000000);   
  if (cpuinfo[0] >= 0x80000001)
  {
    __cpuid(cpuinfo, 0x80000001);

    if (IS_BIT_SET(cpuinfo[3], 31))
      result |= CPUF_3DNOW;

    if (IS_BIT_SET(cpuinfo[3], 30))
      result |= CPUF_3DNOW_EXT;

    if (IS_BIT_SET(cpuinfo[3], 22))
      result |= CPUF_INTEGER_SSE;

    if (result & CPUF_AVX) {
      if (IS_BIT_SET(cpuinfo[2], 16))
        result |= CPUF_FMA4;
    }
  }

  return result;
}

int GetCPUFlags() {
  static int lCPUExtensionsAvailable = CPUCheckForExtensions();
  return lCPUExtensionsAvailable;
}
