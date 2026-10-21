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

// Packed RGB24 <-> RGB32 and RGB48 <-> RGB64 with the NEON de-interleaving loads/stores:
// vld3 splits B,G,R into three registers, vst4 interleaves them again with alpha (and vice versa).
// The vector part reads and writes only within the row; the rest of the row is done per pixel.

#include "convert_rgb_neon.h"
#include <avs/config.h>
#include <cstdint>

#include <arm_neon.h>

/* asm list shows a nice gain
(gcc 12 -O3
Old C: 5 intruction / 1 pixel
18: ldr  w4, [x7], #3          ; unaligned 4-byte load, step 3 bytes
1c: orr  w4, w4, #0xff000000   ; alpha
20: str  w4, [x6], #4          ; 4-byte store
24: cmp  x8, x6
28: b.ne 18

NEON: 7 instruction/16 pixels
98: ld3  {v4.16b-v6.16b}, [x6], #48    ; 16 pixels, split into B, G, R
9c: mvni v3.4s, #0                      ; alpha = 0xFF..
a0: mov  v0.16b, v4.16b                 ; \
a4: mov  v1.16b, v5.16b                 ;  } st4 needs 4 consecutive registers v0-v3
a8: mov  v2.16b, v6.16b                 ; /
ac: st4  {v0.16b-v3.16b}, [x7], #64     ; 16 pixels, interleaved with alpha
b0: cmp / b.ne
*/
void convert_rgb24_to_rgb32_neon(const BYTE* srcp, BYTE* dstp, size_t src_pitch, size_t dst_pitch, size_t width, size_t height)
{
  const uint8x16_t alpha = vdupq_n_u8(0xFF);
  const size_t wmod16 = width / 16 * 16;
  for (size_t y = 0; y < height; y++) {
    size_t x = 0;
    for (; x < wmod16; x += 16) {
      const uint8x16x3_t bgr = vld3q_u8(srcp + x * 3);
      uint8x16x4_t bgra;
      bgra.val[0] = bgr.val[0];
      bgra.val[1] = bgr.val[1];
      bgra.val[2] = bgr.val[2];
      bgra.val[3] = alpha;
      vst4q_u8(dstp + x * 4, bgra);
    }
    for (; x < width; x++) {
      dstp[x * 4 + 0] = srcp[x * 3 + 0];
      dstp[x * 4 + 1] = srcp[x * 3 + 1];
      dstp[x * 4 + 2] = srcp[x * 3 + 2];
      dstp[x * 4 + 3] = 0xFF;
    }
    srcp += src_pitch;
    dstp += dst_pitch;
  }
}

void convert_rgb48_to_rgb64_neon(const BYTE* srcp, BYTE* dstp, size_t src_pitch, size_t dst_pitch, size_t width, size_t height)
{
  const uint16x8_t alpha = vdupq_n_u16(0xFFFF);
  const size_t wmod8 = width / 8 * 8;
  for (size_t y = 0; y < height; y++) {
    const uint16_t* src16 = reinterpret_cast<const uint16_t*>(srcp);
    uint16_t* dst16 = reinterpret_cast<uint16_t*>(dstp);
    size_t x = 0;
    for (; x < wmod8; x += 8) {
      const uint16x8x3_t bgr = vld3q_u16(src16 + x * 3);
      uint16x8x4_t bgra;
      bgra.val[0] = bgr.val[0];
      bgra.val[1] = bgr.val[1];
      bgra.val[2] = bgr.val[2];
      bgra.val[3] = alpha;
      vst4q_u16(dst16 + x * 4, bgra);
    }
    for (; x < width; x++) {
      dst16[x * 4 + 0] = src16[x * 3 + 0];
      dst16[x * 4 + 1] = src16[x * 3 + 1];
      dst16[x * 4 + 2] = src16[x * 3 + 2];
      dst16[x * 4 + 3] = 0xFFFF;
    }
    srcp += src_pitch;
    dstp += dst_pitch;
  }
}

/*
C (convert_rgb64_to_rgb48_c): an inner loop of 5 instructions per pixel, scalar.
188: ldr  x8, [x0, x6, lsl #3]   ; 8-byte load: B,G,R,A
18c: add  x6, x6, #1
190: str  x8, [x7], #6           ; 8-byte store, but step only 6 bytes
194: cmp  x6, x9
198: b.ne 188
This is the trick from the C source: each 8-byte store also writes the alpha into the next pixel's slot, and the next store overwrites it. That's why the last pixel is done separately with three ldrh/strh pairs (0x1b4–0x1d0): its 8-byte store would write 2 bytes past the row. GCC didn't vectorize this either, because of the overlapping stores.

NEON: a hot loop of 7 instructions per 8 pixels (0xa68–0xa80).
a68: ld4  {v4.8h-v7.8h}, [x7], #64   ; 8 pixels, split into B,G,R,A
a6c: mov  v1.16b, v4.16b             ; \
a70: mov  v2.16b, v5.16b             ;  } st3 needs 3 consecutive registers
a74: mov  v3.16b, v6.16b             ; /
a78: st3  {v1.8h-v3.8h}, [x6], #48   ; 8 pixels B,G,R
a7c: cmp / b.ne

┌────────────────────────────────┬───────────────────────┬───────────────┐
│                                │           C           │     NEON      │
├────────────────────────────────┼───────────────────────┼───────────────┤
│ Instructions per 8 pixels      │ 40                    │ 7             │
├────────────────────────────────┼───────────────────────┼───────────────┤
│ Memory operations per 8 pixels │ 8 loads + 8 stores    │ 1 ld4 + 1 st3 │
├────────────────────────────────┼───────────────────────┼───────────────┤
│ Bytes written per 8 pixels     │ 64 (with the overlap) │ 48            │
└────────────────────────────────┴───────────────────────┴───────────────┘
*/
void convert_rgb32_to_rgb24_neon(const BYTE* srcp, BYTE* dstp, size_t src_pitch, size_t dst_pitch, size_t width, size_t height)
{
  const size_t wmod16 = width / 16 * 16;
  for (size_t y = 0; y < height; y++) {
    size_t x = 0;
    for (; x < wmod16; x += 16) {
      const uint8x16x4_t bgra = vld4q_u8(srcp + x * 4);
      uint8x16x3_t bgr;
      bgr.val[0] = bgra.val[0];
      bgr.val[1] = bgra.val[1];
      bgr.val[2] = bgra.val[2];
      vst3q_u8(dstp + x * 3, bgr);
    }
    for (; x < width; x++) {
      dstp[x * 3 + 0] = srcp[x * 4 + 0];
      dstp[x * 3 + 1] = srcp[x * 4 + 1];
      dstp[x * 3 + 2] = srcp[x * 4 + 2];
    }
    srcp += src_pitch;
    dstp += dst_pitch;
  }
}

void convert_rgb64_to_rgb48_neon(const BYTE* srcp, BYTE* dstp, size_t src_pitch, size_t dst_pitch, size_t width, size_t height)
{
  const size_t wmod8 = width / 8 * 8;
  for (size_t y = 0; y < height; y++) {
    const uint16_t* src16 = reinterpret_cast<const uint16_t*>(srcp);
    uint16_t* dst16 = reinterpret_cast<uint16_t*>(dstp);
    size_t x = 0;
    for (; x < wmod8; x += 8) {
      const uint16x8x4_t bgra = vld4q_u16(src16 + x * 4);
      uint16x8x3_t bgr;
      bgr.val[0] = bgra.val[0];
      bgr.val[1] = bgra.val[1];
      bgr.val[2] = bgra.val[2];
      vst3q_u16(dst16 + x * 3, bgr);
    }
    for (; x < width; x++) {
      dst16[x * 3 + 0] = src16[x * 4 + 0];
      dst16[x * 3 + 1] = src16[x * 4 + 1];
      dst16[x * 3 + 2] = src16[x * 4 + 2];
    }
    srcp += src_pitch;
    dstp += dst_pitch;
  }
}
