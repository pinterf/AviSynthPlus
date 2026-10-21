// Avisynth v2.5.  Copyright 2002 Ben Rudiak-Gould et al.
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

#ifndef __Convert_bits_H__
#define __Convert_bits_H__

#include <avisynth.h>
#include <stdint.h>
#include "convert.h"


// in convert_bits.cpp
// repeated 8x for sse size 16
extern const BYTE dither2x2a_data[4];
// cycle: 2
extern const BYTE dither2x2a_data_sse2[2 * 16];
// e.g. 10->8 bits
// repeated 8x for sse size 16
extern const BYTE dither2x2_data[4];
// cycle: 2
extern const BYTE dither2x2_data_sse2[2 * 16];
// e.g. 8->5 bits
extern const BYTE dither4x4a_data[16];
// cycle: 4
extern const BYTE dither4x4a_data_sse2[4 * 16];
// e.g. 12->8 bits
extern const BYTE dither4x4_data[16];
// cycle: 4
extern const BYTE dither4x4_data_sse2[4 * 16];
// e.g. 14->9 bits
extern const BYTE dither8x8a_data[8][8];
// cycle: 8
extern const BYTE dither8x8a_data_sse2[8][16];
// e.g. 14->8 bits
extern const BYTE dither8x8_data[8][8];
// cycle: 8
extern const BYTE dither8x8_data_sse2[8][16];
// e.g. 16->9 or 8->1 bits
// cycle: 16x. No special 16 byte sse2
extern const BYTE dither16x16a_data[16][16];
// 16->8
// cycle: 16x. No special 16 byte sse2
extern const BYTE dither16x16_data[16][16];

typedef void (*BitDepthConvFuncPtr)(const BYTE *srcp, BYTE *dstp, int src_rowsize, int src_height, int src_pitch, int dst_pitch, int source_bitdepth, int target_bitdepth, int dither_target_bitdepth);

class ConvertBits : public GenericVideoFilter
{
public:
  ConvertBits(PClip _child, const int _dither_mode, const int _target_bitdepth, bool _truerange, int _ColorRange_src, int _ColorRange_dest, int _dither_bitdepth, IScriptEnvironment* env);
  PVideoFrame __stdcall GetFrame(int n,IScriptEnvironment* env) override;

  int __stdcall SetCacheHints(int cachehints, int frame_range) override {
    AVS_UNUSED(frame_range);
    return cachehints == CACHE_GET_MTMODE ? MT_NICE_FILTER : 0;
  }

  static AVSValue __cdecl Create(AVSValue args, void*, IScriptEnvironment* env);
private:
  BitDepthConvFuncPtr conv_function;
  BitDepthConvFuncPtr conv_function_chroma; // 32bit float YUV chroma
  BitDepthConvFuncPtr conv_function_a;
  int target_bitdepth;
  int dither_mode;
  int dither_bitdepth;
  bool fulls; // source is full range (defaults: rgb=true, yuv=false (bit shift))
  bool fulld; // destination is full range (defaults: rgb=true, yuv=false (bit shift))
  bool dynamic_range; // per-frame range detection mode
  bool dest_fulld_pinned; // dynamic_range only: true if fulld was given explicitly, so it stays fixed instead of mirroring the per-frame fulls
  bool dest_fulld_pinned_value; // dynamic_range only: the fixed fulld value when dest_fulld_pinned
  BitDepthConvFuncPtr conv_function_table[2][2];        // [fulls][fulld], only when dynamic_range
  BitDepthConvFuncPtr conv_function_chroma_table[2][2]; // 32bit float YUV chroma
  BitDepthConvFuncPtr conv_function_a_table[2][2];
  bool truerange; // if 16->10 range reducing or e.g. 14->16 bit range expansion needed
  int pixelsize;
  int bits_per_pixel;
  bool format_change_only;
};

void get_convert_any_bits_functions(int dither_mode, int source_bitdepth, int target_bitdepth, bool fulls, bool fulld,
#ifdef INTEL_INTRINSICS
  bool sse2, bool sse4, bool avx2,
#elif defined(NEON_INTRINSICS)
  bool neon,
#endif
  BitDepthConvFuncPtr& conv_function, BitDepthConvFuncPtr& conv_function_chroma, BitDepthConvFuncPtr& conv_function_a);

/**********************************
******  Bitdepth conversions  *****
**********************************/

#endif // __Convert_bits_H__
