// AviSynth+.  Copyright 2025 AviSynth+ Project
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

#include <avs/types.h>
#include <avs/config.h>
#include "../turn.h"
#include <cstdint>

#include <arm_neon.h>

/*
SSE Intrinsic,AArch64 NEON Equivalent,Description
__m128i              uint8x16_t                 128-bit vector of 8-bit unsigned integers (bytes)
_mm_loadl_epi64      vld1_u8 (into uint8x8_t)   Load 64 bits (8 bytes)
_mm_unpacklo_epi8    vzip_u8 (64 bit)
                     vzipq_u16/vzipq_u32 (128 bit)
                     Interleaves the low elements of two vectors
_mm_unpackhi_epi8    Handled implicitly by vzip structure or
                     equivalent to high 64-bit of vzipq output
                     Interleaves the high elements of two vectors
_mm_storel_epi64     vst1_u8 (from uint8x8_t)   Store 64 bits (8 bytes)
_mm_movehl_ps        vget_high_u8               Extracts the high 64 bits from a 128-bit vector
*/

// --------------------------------------------------------------------------
// 8-bit Block Rotate 90 CW (Transpose + Flip Horizontal)
// --------------------------------------------------------------------------
static AVS_FORCEINLINE void transpose_8x8x8_neon(const BYTE* AVS_RESTRICT srcp, BYTE* AVS_RESTRICT dstp, int src_pitch, int dst_pitch)
{
  // 1. Load 8 rows (8x8 block)
  // Assume srcp points to the top left of the block to be rotated.
  uint8x8_t r0 = vld1_u8(srcp); srcp += src_pitch;
  uint8x8_t r1 = vld1_u8(srcp); srcp += src_pitch;
  uint8x8_t r2 = vld1_u8(srcp); srcp += src_pitch;
  uint8x8_t r3 = vld1_u8(srcp); srcp += src_pitch;
  uint8x8_t r4 = vld1_u8(srcp); srcp += src_pitch;
  uint8x8_t r5 = vld1_u8(srcp); srcp += src_pitch;
  uint8x8_t r6 = vld1_u8(srcp); srcp += src_pitch;
  uint8x8_t r7 = vld1_u8(srcp);

  // 2. Transpose logic (standard butterfly)
  // Stage 1: Swap 8-bit neighbors
  uint8x8x2_t t0 = vtrn_u8(r0, r1);
  uint8x8x2_t t1 = vtrn_u8(r2, r3);
  uint8x8x2_t t2 = vtrn_u8(r4, r5);
  uint8x8x2_t t3 = vtrn_u8(r6, r7);

  // Stage 2: Swap 16-bit pairs
  uint16x4x2_t m0 = vtrn_u16(vreinterpret_u16_u8(t0.val[0]), vreinterpret_u16_u8(t1.val[0]));
  uint16x4x2_t m1 = vtrn_u16(vreinterpret_u16_u8(t0.val[1]), vreinterpret_u16_u8(t1.val[1]));
  uint16x4x2_t m2 = vtrn_u16(vreinterpret_u16_u8(t2.val[0]), vreinterpret_u16_u8(t3.val[0]));
  uint16x4x2_t m3 = vtrn_u16(vreinterpret_u16_u8(t2.val[1]), vreinterpret_u16_u8(t3.val[1]));

  // Stage 3: Swap 32-bit quads
  uint32x2x2_t y04 = vtrn_u32(vreinterpret_u32_u16(m0.val[0]), vreinterpret_u32_u16(m2.val[0]));
  uint32x2x2_t y15 = vtrn_u32(vreinterpret_u32_u16(m1.val[0]), vreinterpret_u32_u16(m3.val[0]));
  uint32x2x2_t y26 = vtrn_u32(vreinterpret_u32_u16(m0.val[1]), vreinterpret_u32_u16(m2.val[1]));
  uint32x2x2_t y37 = vtrn_u32(vreinterpret_u32_u16(m1.val[1]), vreinterpret_u32_u16(m3.val[1]));

  // 3. Mirror horizontal, store
  // The transpose put the pixels in order 0,1,2... but Turn Right requires 7,6,5...
  // We reverse the bytes in each 64-bit register.

  auto store_rev = [&](const uint32x2x2_t& v, int idx) {
    // vrev64_u8 reverses bytes in the 64-bit vector (Horizontal Flip)
    uint8x8_t flipped = vrev64_u8(vreinterpret_u8_u32(v.val[idx]));
    vst1_u8(dstp, flipped);
    dstp += dst_pitch;
    };

  store_rev(y04, 0); // Row 0
  store_rev(y15, 0); // Row 1
  store_rev(y26, 0); // Row 2
  store_rev(y37, 0); // Row 3
  store_rev(y04, 1); // Row 4
  store_rev(y15, 1); // Row 5
  store_rev(y26, 1); // Row 6
  store_rev(y37, 1); // Row 7
}

// --------------------------------------------------------------------------
// 16-bit Block Rotate 90 CW (Transpose + Flip Horizontal)
// --------------------------------------------------------------------------
static AVS_FORCEINLINE void transpose_8x8x16_neon(const BYTE* AVS_RESTRICT srcp, BYTE* AVS_RESTRICT dstp, int src_pitch, int dst_pitch)
{
  auto p16 = [](const BYTE* p) { return reinterpret_cast<const uint16_t*>(p); };
  auto d16 = [](BYTE* p) { return reinterpret_cast<uint16_t*>(p); };

  // 1. Load 8 rows (8x8 block of uint16)
  uint16x8_t r0 = vld1q_u16(p16(srcp)); srcp += src_pitch;
  uint16x8_t r1 = vld1q_u16(p16(srcp)); srcp += src_pitch;
  uint16x8_t r2 = vld1q_u16(p16(srcp)); srcp += src_pitch;
  uint16x8_t r3 = vld1q_u16(p16(srcp)); srcp += src_pitch;
  uint16x8_t r4 = vld1q_u16(p16(srcp)); srcp += src_pitch;
  uint16x8_t r5 = vld1q_u16(p16(srcp)); srcp += src_pitch;
  uint16x8_t r6 = vld1q_u16(p16(srcp)); srcp += src_pitch;
  uint16x8_t r7 = vld1q_u16(p16(srcp));

  // 2. Transpose logic (standard butterfly)
  // Stage 1: Swap 16-bit neighbors
  uint16x8x2_t trn0 = vtrnq_u16(r0, r1);
  uint16x8x2_t trn1 = vtrnq_u16(r2, r3);
  uint16x8x2_t trn2 = vtrnq_u16(r4, r5);
  uint16x8x2_t trn3 = vtrnq_u16(r6, r7);

  // Stage 2: Swap 32-bit pairs
  uint32x4x2_t q0 = vtrnq_u32(vreinterpretq_u32_u16(trn0.val[0]), vreinterpretq_u32_u16(trn1.val[0]));
  uint32x4x2_t q1 = vtrnq_u32(vreinterpretq_u32_u16(trn0.val[1]), vreinterpretq_u32_u16(trn1.val[1]));
  uint32x4x2_t q2 = vtrnq_u32(vreinterpretq_u32_u16(trn2.val[0]), vreinterpretq_u32_u16(trn3.val[0]));
  uint32x4x2_t q3 = vtrnq_u32(vreinterpretq_u32_u16(trn2.val[1]), vreinterpretq_u32_u16(trn3.val[1]));

  // Stage 3: Swap 64-bit halves (Assembly)
  uint64x2_t q0_0 = vreinterpretq_u64_u32(q0.val[0]);
  uint64x2_t q2_0 = vreinterpretq_u64_u32(q2.val[0]);
  uint64x2_t q1_0 = vreinterpretq_u64_u32(q1.val[0]);
  uint64x2_t q3_0 = vreinterpretq_u64_u32(q3.val[0]);

  uint64x2_t q0_1 = vreinterpretq_u64_u32(q0.val[1]);
  uint64x2_t q2_1 = vreinterpretq_u64_u32(q2.val[1]);
  uint64x2_t q1_1 = vreinterpretq_u64_u32(q1.val[1]);
  uint64x2_t q3_1 = vreinterpretq_u64_u32(q3.val[1]);

  // 3. Mirror Horizontal & Store
  // To reverse a 128-bit vector of 16-bit elements: [ABCDEFGH] -> [HGFEDCBA]
  // vrev64 swaps within halves: [DCBA][HGFE]
  // vext(4) swaps the halves:   [HGFE][DCBA]

  auto store_rev = [&](uint64x2_t a, uint64x2_t b, bool high) {
    uint16x8_t row;
    if (!high) row = vreinterpretq_u16_u64(vzip1q_u64(a, b));
    else       row = vreinterpretq_u16_u64(vzip2q_u64(a, b));

    // Flip Horizontal Logic
    uint16x8_t rev_halves = vrev64q_u16(row);
    // Swap the 64-bit halves (index 4 corresponds to 4 * 16-bit = 64 bits)
    uint16x8_t fully_flipped = vextq_u16(rev_halves, rev_halves, 4);

    vst1q_u16(d16(dstp), fully_flipped);
    dstp += dst_pitch;
    };

  store_rev(q0_0, q2_0, false); // Row 0
  store_rev(q1_0, q3_0, false); // Row 1
  store_rev(q0_1, q2_1, false); // Row 2
  store_rev(q1_1, q3_1, false); // Row 3
  store_rev(q0_0, q2_0, true);  // Row 4
  store_rev(q1_0, q3_0, true);  // Row 5
  store_rev(q0_1, q2_1, true);  // Row 6
  store_rev(q1_1, q3_1, true);  // Row 7
}

// Main loop for 8 bit plane turn
void turn_right_plane_8_neon(const BYTE* srcp, BYTE* dstp, int src_rowsize, int src_height, int src_pitch, int dst_pitch)
{
  const BYTE* s0 = srcp;
  int w = src_rowsize & ~7;
  int h = src_height & ~7;

  for (int y = 0; y < h; y += 8)
  {
    BYTE* d0 = dstp + src_height - 8 - y;
    for (int x = 0; x < w; x += 8)
    {
      transpose_8x8x8_neon(s0 + x, d0, src_pitch, dst_pitch);
      d0 += dst_pitch * 8;
    }
    s0 += src_pitch * 8;
  }

  if (src_rowsize != w)
  {
    // Assuming turn_right_plane_8_c is the C fallback function
    turn_right_plane_8_c(srcp + w, dstp + w * dst_pitch, src_rowsize - w, src_height, src_pitch, dst_pitch);
  }

  if (src_height != h)
  {
    turn_right_plane_8_c(srcp + h * src_pitch, dstp, src_rowsize, src_height - h, src_pitch, dst_pitch);
  }
}

void turn_left_plane_8_neon(const BYTE* srcp, BYTE* dstp, int src_rowsize, int src_height, int src_pitch, int dst_pitch)
{
  // The direction and stride logic is independent of the intrinsics used
  turn_right_plane_8_neon(srcp + (src_height - 1) * src_pitch, dstp + (src_rowsize - 1) * dst_pitch, src_rowsize, src_height, -src_pitch, -dst_pitch);
}

// Main loop for 16 bit plane turn
void turn_right_plane_16_neon(const BYTE* srcp, BYTE* dstp, int src_rowsize, int src_height, int src_pitch, int dst_pitch)
{
  const BYTE* s0 = srcp;
  int w = src_rowsize & ~15;
  int h = src_height & ~7;

  for (int y = 0; y < h; y += 8)
  {
    BYTE* d0 = dstp + src_height * 2 - 16 - y * 2;
    for (int x = 0; x < w; x += 16)
    {
      transpose_8x8x16_neon(s0 + x, d0, src_pitch, dst_pitch);
      d0 += dst_pitch * 8;
    }
    s0 += src_pitch * 8;
  }

  if (src_rowsize != w)
  {
    turn_right_plane_16_c(srcp + w, dstp + w / 2 * dst_pitch, src_rowsize - w, src_height, src_pitch, dst_pitch);
  }

  if (src_height != h)
  {
    turn_right_plane_16_c(srcp + h * src_pitch, dstp, src_rowsize, src_height - h, src_pitch, dst_pitch);
  }
}


void turn_left_plane_16_neon(const BYTE* srcp, BYTE* dstp, int src_rowsize, int src_height, int src_pitch, int dst_pitch)
{
  turn_right_plane_16_neon(srcp + src_pitch * (src_height - 1), dstp + dst_pitch * (src_rowsize / 2 - 1), src_rowsize, src_height, -src_pitch, -dst_pitch);
}


// Transposes 4 rows of 4x32-bit values (words) into 4 rows of 4x32-bit values.
// float helper
static AVS_FORCEINLINE void transpose_32x4x4_neon(const BYTE* srcp, BYTE* dstp, const int src_pitch, const int dst_pitch)
{
  // Load 4 128-bit vectors (4 x 32-bit elements per vector)
  // SSE: _mm_loadu_si128 (unaligned load)
  uint32x4_t a03 = vld1q_u32(reinterpret_cast<const uint32_t*>(srcp + src_pitch * 0));
  uint32x4_t b03 = vld1q_u32(reinterpret_cast<const uint32_t*>(srcp + src_pitch * 1));
  uint32x4_t c03 = vld1q_u32(reinterpret_cast<const uint32_t*>(srcp + src_pitch * 2));
  uint32x4_t d03 = vld1q_u32(reinterpret_cast<const uint32_t*>(srcp + src_pitch * 3));

  uint32x4x2_t ab = vtrnq_u32(a03, b03);
  uint32x4x2_t cd = vtrnq_u32(c03, d03);

  uint32x4_t t0 = vcombine_u32(vget_low_u32(ab.val[0]), vget_low_u32(cd.val[0]));
  uint32x4_t t1 = vcombine_u32(vget_low_u32(ab.val[1]), vget_low_u32(cd.val[1]));
  uint32x4_t t2 = vcombine_u32(vget_high_u32(ab.val[0]), vget_high_u32(cd.val[0]));
  uint32x4_t t3 = vcombine_u32(vget_high_u32(ab.val[1]), vget_high_u32(cd.val[1]));

  uint32x4x4_t result = { { t0, t1, t2, t3 } };

  // Store results
  // SSE: _mm_storeu_si128 (unaligned store)
  vst1q_u32(reinterpret_cast<uint32_t*>(dstp + dst_pitch * 0), result.val[0]);
  vst1q_u32(reinterpret_cast<uint32_t*>(dstp + dst_pitch * 1), result.val[1]);
  vst1q_u32(reinterpret_cast<uint32_t*>(dstp + dst_pitch * 2), result.val[2]);
  vst1q_u32(reinterpret_cast<uint32_t*>(dstp + dst_pitch * 3), result.val[3]);
}


void turn_right_plane_32_neon(const BYTE* srcp, BYTE* dstp, int src_rowsize, int src_height, int src_pitch, int dst_pitch)
{
  const BYTE* s0 = srcp + src_pitch * (src_height - 1);
  int w = src_rowsize & ~15; // 16 bytes for 32-bit elements (4 elements)
  int h = src_height & ~3; // mod4 height

  for (int y = 0; y < h; y += 4)
  {
    BYTE* d0 = dstp + y * 4;
    // in 4x4 units, 16 bytes = 4x32-bit elements
    for (int x = 0; x < w; x += 16)
    {
      transpose_32x4x4_neon(s0 + x, d0, -src_pitch, dst_pitch);
      d0 += 4 * dst_pitch;
    }
    s0 -= 4 * src_pitch;
  }
  // rest non-mod4
  if (src_rowsize != w)
  {
    turn_right_plane_32_c(srcp + w, dstp + w / 4 * dst_pitch, src_rowsize - w, src_height, src_pitch, dst_pitch);
  }

  if (src_height != h)
  {
    turn_right_plane_32_c(srcp, dstp + h * 4, src_rowsize, src_height - h, src_pitch, dst_pitch);
  }
}

void turn_left_plane_32_neon(const BYTE* srcp, BYTE* dstp, int src_rowsize, int src_height, int src_pitch, int dst_pitch)
{
  turn_right_plane_32_neon(srcp + src_pitch * (src_height - 1), dstp + dst_pitch * (src_rowsize / 4 - 1), src_rowsize, src_height, -src_pitch, -dst_pitch);
}


// The 64-bit transpose: 2x2 double-word transpose
// rgb64 helper
static inline void turn_right_plane_64_neon(const BYTE* srcp, BYTE* dstp, int src_rowsize, int src_height, int src_pitch, int dst_pitch)
{
  const BYTE* s0 = srcp + src_pitch * (src_height - 1);
  int w = src_rowsize & ~15; // 16 bytes = 2x64-bit elements
  int h = src_height & ~1;

  for (int y = 0; y < h; y += 2)
  {
    BYTE* d0 = dstp + y * 8;
    for (int x = 0; x < w; x += 16)
    {
      // Load two 128-bit vectors
      uint64x2_t a01 = vld1q_u64(reinterpret_cast<const uint64_t*>(s0 + x));
      uint64x2_t b01 = vld1q_u64(reinterpret_cast<const uint64_t*>(s0 + x - src_pitch));

      // Manual 2x2 64-bit element transpose
      uint64x2x2_t ab;
      ab.val[0] = vcombine_u64(vget_low_u64(a01), vget_low_u64(b01));   // {a0, b0}
      ab.val[1] = vcombine_u64(vget_high_u64(a01), vget_high_u64(b01)); // {a1, b1}

      vst1q_u64(reinterpret_cast<uint64_t*>(d0), ab.val[0]);         // a0 b0
      vst1q_u64(reinterpret_cast<uint64_t*>(d0 + dst_pitch), ab.val[1]); // a1 b1
      d0 += 2 * dst_pitch;
    }
    s0 -= 2 * src_pitch;
  }

  if (src_rowsize != w)
  {
    turn_right_plane_c<uint64_t>(srcp + w, dstp + w / 8 * dst_pitch, 8, src_height, src_pitch, dst_pitch);
  }

  if (src_height != h)
  {
    turn_right_plane_c<uint64_t>(srcp, dstp + h * 8, src_rowsize, 1, src_pitch, dst_pitch);
  }

}


void turn_left_rgb64_neon(const BYTE* srcp, BYTE* dstp, int src_rowsize, int src_height, int src_pitch, int dst_pitch)
{
  turn_right_plane_64_neon(srcp, dstp, src_rowsize, src_height, src_pitch, dst_pitch);
}


void turn_right_rgb64_neon(const BYTE* srcp, BYTE* dstp, int src_rowsize, int src_height, int src_pitch, int dst_pitch)
{
  turn_right_plane_64_neon(srcp + src_pitch * (src_height - 1), dstp + dst_pitch * (src_rowsize / 8 - 1), src_rowsize, src_height, -src_pitch, -dst_pitch);
}


// Helper: swap the two 64-bit halves of a 128-bit vector.
// vextq_* with an 8-byte offset concatenates the vector with itself and takes the high half.
AVS_FORCEINLINE static uint8x16_t swap64_u8(uint8x16_t v) {
  return vextq_u8(v, v, 8);
}

// lookup tables for byte and word reversal
static const uint8_t idx_data_for_uint8[16] = { 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0 };
static const uint8_t idx_data_for_uint16[16] = { 14, 15, 12, 13, 10, 11, 8, 9, 6, 7, 4, 5, 2, 3, 0, 1 };

template <typename T>
void turn_180_plane_neon(const BYTE* srcp, BYTE* dstp,
  int src_rowsize, int src_height,
  int src_pitch, int dst_pitch)
{
  const uint8_t* AVS_RESTRICT s0 = srcp;
  // always process 16 bytes at a time, 16 uint8_t or 8 uint16_t or 4 uint32_t or 2 uint64_t
  uint8_t* AVS_RESTRICT d0 = dstp + dst_pitch * (src_height - 1) + src_rowsize;

  const int mod16width = src_rowsize / 16 * 16; // mod16 width
  const int mod8width = src_rowsize / 8 * 8;  // mod8 width
  const uint8x16_t idx_8 = vld1q_u8(idx_data_for_uint8);
  const uint8x16_t idx_16 = vld1q_u8(idx_data_for_uint16);

  for (int y = 0; y < src_height; ++y) {
    for (int x = 0; x < mod16width; x += 16) {
      // load 16 bytes
      uint8x16_t src = vld1q_u8(reinterpret_cast<const uint8_t*>(s0 + x));

      uint8x16_t dst;

      if constexpr (sizeof(T) == 8) {
        // 64-bit pixels: reverse pixel ORDER but DO NOT reverse bytes per pixel.
        // Swap the two 64-bit lanes only.
        uint64x2_t src64 = vreinterpretq_u64_u8(src);
        uint64x2_t rev = vextq_u64(src64, src64, 1);  // [hi, lo]
        dst = vreinterpretq_u8_u64(rev);
      }
      else if constexpr (sizeof(T) == 4) {
        // 32-bit pixels: reverse order of the 4 dwords.
        //   1) reverse 32-bit elements within each 64-bit lane
        //   2) swap the 64-bit lanes
        uint32x4_t t1 = vrev64q_u32(vreinterpretq_u32_u8(src));  // per-lane 32-bit reverse
        uint8x16_t t1b = vreinterpretq_u8_u32(t1);
        dst = swap64_u8(t1b);  // swap halves via vextq_u8(t1b, t1b, 8)
      }
      else if constexpr (sizeof(T) == 2) {
        // 16-bit pixels: reverse order of the 8 words.
        //   1) reverse 16-bit elements within each 64-bit lane
        //   2) swap the 64-bit lanes
        /* slower:
        uint16x8_t t1 = vrev64q_u16(vreinterpretq_u16_u8(src));
        uint8x16_t t1b = vreinterpretq_u8_u16(t1);
        dst = swap64_u8(t1b);  // swap halves
        */
        // do table lookup for full 16-byte reverse
        dst = vqtbl1q_u8(src, idx_16);
      }
      else { // sizeof(T) == 1
        // Use NEON table lookup for full 16-byte reverse
        // Define the index array as a static const array
        // let's hope, compiler optimizes this well
        dst = vqtbl1q_u8(src, idx_8);
        // this is said to have lower throughput than the vrev + vext method
        // but benchmark shows it quicker on RPi5
      }

      // store reversed block to the mirror position in the destination row
      vst1q_u8(reinterpret_cast<uint8_t*>(d0 - 16 - x), dst); // -16 SIMD width
    }
    for (int x = mod16width; x < mod8width; x+=8) {
      // process 8 bytes at a time
      uint8x8_t src = vld1_u8(reinterpret_cast<const uint8_t*>(s0 + x));

      uint8x8_t dst;

      if constexpr (sizeof(T) == 8) {
        // mod8 block, size is 8, nothing to change.
        dst = src; // Copy src to dst directly.
      }
      else if constexpr (sizeof(T) == 4) {
        // 1) reverse 32-bit elements within the 64-bit lane. This is the whole job.
        uint32x2_t t1 = vrev64_u32(vreinterpret_u32_u8(src));
        dst = vreinterpret_u8_u32(t1); // Reinterpret and assign directly
      }
      else if constexpr (sizeof(T) == 2) {
        // 16-bit pixels: reverse order of the 4 words.
        dst = vtbl1_u8(src, vld1_u8(&idx_data_for_uint16[8])); // use upper half of idx
      }
      else { // sizeof(T) == 1
        dst = vtbl1_u8(src, vld1_u8(&idx_data_for_uint8[8])); // use upper half of idx
      }
      vst1_u8(reinterpret_cast<uint8_t*>(d0 - 8 - x), dst); // -8 SIMD width
    }

    if (mod8width < src_rowsize) {
      // plain C fallback for remaining bytes
      const T* s00 = reinterpret_cast<const T*>(s0 + mod8width);
      T* d00 = reinterpret_cast<T*>(d0 - sizeof(T) - mod8width); // - sizeof(T) "SIMD" width

      for (int x = mod8width; x < src_rowsize; x += sizeof(T)) {
        *d00-- = *s00++;
      }
    }

    s0 += src_pitch;   // next source row
    d0 -= dst_pitch;   // previous destination row (since we started at the last)
  }

}


// instantiate
template void turn_180_plane_neon<uint8_t>(const BYTE* srcp, BYTE* dstp, int src_rowsize, int src_height, int src_pitch, int dst_pitch);
template void turn_180_plane_neon<uint16_t>(const BYTE* srcp, BYTE* dstp, int src_rowsize, int src_height, int src_pitch, int dst_pitch);
template void turn_180_plane_neon<uint32_t>(const BYTE* srcp, BYTE* dstp, int src_rowsize, int src_height, int src_pitch, int dst_pitch);
template void turn_180_plane_neon<uint64_t>(const BYTE* srcp, BYTE* dstp, int src_rowsize, int src_height, int src_pitch, int dst_pitch);



// RGB functions (just calling the plane functions)
void turn_left_rgb32_neon(const BYTE* srcp, BYTE* dstp, int src_rowsize, int src_height, int src_pitch, int dst_pitch)
{
  turn_right_plane_32_neon(srcp, dstp, src_rowsize, src_height, src_pitch, dst_pitch);
}


void turn_right_rgb32_neon(const BYTE* srcp, BYTE* dstp, int src_rowsize, int src_height, int src_pitch, int dst_pitch)
{
  turn_left_plane_32_neon(srcp, dstp, src_rowsize, src_height, src_pitch, dst_pitch);
}


