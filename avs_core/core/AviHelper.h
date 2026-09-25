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

#ifndef __AviHelper_H__
#define __AviHelper_H__

#include <avisynth.h>
#include <stdint.h>

int AviHelper_ImageSize(const VideoInfo *vi, bool AVIPadScanlines, bool v210, bool v410, bool r210, bool R10k, bool v308, bool v408, bool Y410);

template<bool hasAlpha>
void ToY416_sse2(uint8_t *outbuf, int out_pitch, const uint8_t *yptr, int ypitch, const uint8_t *uptr, const uint8_t *vptr, int uvpitch, const uint8_t *aptr, int apitch, int width, int height);

template<bool hasAlpha>
void ToY416_c(uint8_t *outbuf8, int out_pitch, const uint8_t *yptr, int ypitch, const uint8_t *uptr, const uint8_t *vptr, int uvpitch, const uint8_t *aptr, int apitch, int width, int height);
template<bool hasAlpha>
void FromY416_c(uint8_t *yptr, int ypitch, uint8_t *uptr, uint8_t *vptr, int uvpitch, uint8_t *aptr, int apitch, const uint8_t *srcp8, int srcpitch, int width, int height);

template<bool hasAlpha>
void ToY410_c(uint8_t* outbuf8, int out_pitch, const uint8_t* yptr, int ypitch, const uint8_t* uptr, const uint8_t* vptr, int uvpitch, const uint8_t* aptr, int apitch, int width, int height);
template<bool hasAlpha>
void FromY410_c(uint8_t* yptr, int ypitch, uint8_t* uptr, uint8_t* vptr, int uvpitch, uint8_t* aptr, int apitch, const uint8_t* srcp8, int srcpitch, int width, int height);

void From_r210_c(uint8_t *rptr, uint8_t *gptr, uint8_t *bptr, int pitch, uint8_t *srcp8, int srcpitch, int width, int height);
void From_R10k_c(uint8_t *rptr, uint8_t *gptr, uint8_t *bptr, int pitch, uint8_t *srcp8, int srcpitch, int width, int height);

void bgr_to_rgbBE_c(uint8_t* pdst, int dstpitch, const uint8_t *src, int srcpitch, int width, int height);

void bgra_to_argbBE_ssse3(uint8_t* pdst, int dstpitch, const uint8_t *src, int srcpitch, int width, int height);
void bgra_to_argbBE_sse2(uint8_t* pdst, int dstpitch, const uint8_t *src, int srcpitch, int width, int height);

void bgra_to_argbBE_c(uint8_t* pdst, int dstpitch, const uint8_t *src, int srcpitch, int width, int height);
void v410_to_yuv444p10(BYTE *dstp_y, int dstpitch, BYTE *dstp_u, BYTE *dstp_v, int dstpitch_uv, const BYTE *srcp, int width, int height);
void v210_to_yuv422p10(BYTE *dstp_y, int dstpitch, BYTE *dstp_u, BYTE *dstp_v, int dstpitch_uv, const BYTE *srcp, int width, int height);
void v308_to_yuv444p8(BYTE* dstp_y, int dstpitch, BYTE* dstp_u, BYTE* dstp_v, int dstpitch_uv, const BYTE* srcp, int width, int height);
void v408_to_yuva444p8(BYTE* dstp_y, int dstpitch, BYTE* dstp_u, BYTE* dstp_v, BYTE* dstp_a, int dstpitch_uv, int dstpitch_a, const BYTE* srcp, int width, int height);
void yuv422p10_to_v210(BYTE *dstp, const BYTE *srcp_y, int srcpitch, const BYTE *srcp_u, const BYTE *srcp_v, int srcpitch_uv, int width, int height);
void planar_ya8_to_packed_ya8(BYTE *dstp, const BYTE *srcp_y, int srcpitch_y, const BYTE *srcp_a, int srcpitch_a, int width, int height);
void packed_ya8_to_planar_ya8(BYTE *dstp_y, int dstpitch_y, BYTE *dstp_a, int dstpitch_a, const BYTE *srcp, int width, int height);
void yuv42xp10_16_to_Px10_16(BYTE *dstp, int dstpitch, const BYTE *srcp_y, int srcpitch,
  const BYTE *srcp_u, const BYTE *srcp_v, int srcpitch_uv,
  int width, int height, int cheight, bool semi_packed_p16, IScriptEnvironment *env);
void Px10_16_to_yuv42xp10_16(BYTE *dstp_y, int dstpitch, BYTE *dstp_u, BYTE *dstp_v, int dstpitch_uv,
  const BYTE *srcp, int srcpitch,
  int width, int height, int cheight, bool semi_packed_p16, IScriptEnvironment *env);

#endif  // __AviHelper_H__
