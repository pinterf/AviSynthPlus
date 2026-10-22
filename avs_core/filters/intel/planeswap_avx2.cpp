// AviSynth+.  Copyright 2026- AviSynth+ Project
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

#include <avs/config.h>
#ifdef AVS_WINDOWS
#include <avs/win.h>
#else
#include <avs/posix.h>
#endif

#include "../planeswap.h"
#include "planeswap_avx2.h"

#if defined(_MSC_VER)
#include <intrin.h> // MSVC
#else 
#include <x86intrin.h> // GCC/MinGW/Clang/LLVM
#endif
#include <immintrin.h>

#include <stdint.h>
#include <type_traits>

// _mm256_set_m128i is not universally available as an intrinsic; define fallback.
#ifndef _mm256_set_m128i
#define _mm256_set_m128i(hi, lo) \
  _mm256_inserti128_si256(_mm256_castsi128_si256(lo), (hi), 1)
#endif

// ---------------------------------------------------------------------------
// Row end of the packed RGB32 / RGB64 extraction: after the AVX2 blocks, aligned SSE blocks while they fit,
// then one unaligned SSE block that ends exactly at width, overlapping pixels already written (same values).
// Rows narrower than one SSE block are copied per pixel. Nothing is read or written beyond the row.
// ---------------------------------------------------------------------------

template<bool aligned>
static AVS_FORCEINLINE __m128i load_si128_a(const BYTE* p)
{
  if constexpr (aligned)
    return _mm_load_si128(reinterpret_cast<const __m128i*>(p));
  else
    return _mm_loadu_si128(reinterpret_cast<const __m128i*>(p));
}

template<bool aligned>
static AVS_FORCEINLINE void store_si128_a(BYTE* p, __m128i v)
{
  if constexpr (aligned)
    _mm_store_si128(reinterpret_cast<__m128i*>(p), v);
  else
    _mm_storeu_si128(reinterpret_cast<__m128i*>(p), v);
}

template<typename pixel_t, int components, int channel_index>
static void extract_packed_rgb_channel_row_c_avx2(const BYTE* srcp8, BYTE* dstp8, int width)
{
  const pixel_t* srcp = reinterpret_cast<const pixel_t*>(srcp8);
  pixel_t* dstp = reinterpret_cast<pixel_t*>(dstp8);
  for (int x = 0; x < width; ++x)
    dstp[x] = srcp[x * components + channel_index];
}

// RGB32, 16 pixels, SSSE3 shuffle (the AVX2 shuffle mask, 128 bit)
template<bool aligned>
static AVS_FORCEINLINE void extract_rgb32_block16_ssse3(const BYTE* s, BYTE* d, __m128i shuf128)
{
  __m128i t0 = _mm_shuffle_epi8(load_si128_a<aligned>(s +  0), shuf128);
  __m128i t1 = _mm_shuffle_epi8(load_si128_a<aligned>(s + 16), shuf128);
  __m128i t2 = _mm_shuffle_epi8(load_si128_a<aligned>(s + 32), shuf128);
  __m128i t3 = _mm_shuffle_epi8(load_si128_a<aligned>(s + 48), shuf128);
  store_si128_a<aligned>(d, _mm_packus_epi16(_mm_packs_epi32(t0, t1), _mm_packs_epi32(t2, t3)));
}

// RGB64, 8 pixels, SSSE3 shuffle (the AVX2 shuffle mask, 128 bit)
template<bool aligned>
static AVS_FORCEINLINE void extract_rgb64_block8_ssse3(const BYTE* s, BYTE* d, __m128i shuf128)
{
  // each: [{C_even, C_odd}, 0, 0, 0] as dwords
  __m128i t0 = _mm_shuffle_epi8(load_si128_a<aligned>(s +  0), shuf128); // px 0-1
  __m128i t1 = _mm_shuffle_epi8(load_si128_a<aligned>(s + 16), shuf128); // px 2-3
  __m128i t2 = _mm_shuffle_epi8(load_si128_a<aligned>(s + 32), shuf128); // px 4-5
  __m128i t3 = _mm_shuffle_epi8(load_si128_a<aligned>(s + 48), shuf128); // px 6-7
  __m128i q01 = _mm_unpacklo_epi32(t0, t1); // [{C0,C1},{C2,C3}, 0, 0]
  __m128i q23 = _mm_unpacklo_epi32(t2, t3); // [{C4,C5},{C6,C7}, 0, 0]
  store_si128_a<aligned>(d, _mm_unpacklo_epi64(q01, q23)); // [{C0,C1},{C2,C3},{C4,C5},{C6,C7}]
}

// ---------------------------------------------------------------------------
// Packed RGB32 channel extraction — AVX2, 32 pixels per main iteration.
//
// Source layout (per pixel, 4 bytes): B G R A  (channel_index: B=0 G=1 R=2 A=3)
// Source rows are bottom-up; srcp must already point at the last (bottom) row.
// Pitch is positive; we step backwards (srcp -= src_pitch) each row.
//
// Strategy:
//   1. vpshufb: move target byte to byte 0 of each 32-bit dword, zero bytes 1-3.
//      (Mask is the same for both 128-bit lanes — pixel layout is identical.)
//   2. vpackssdw × 2: pack 4+4 dwords → 8 words (per register pair).
//   3. vpackuswb: pack the two word registers → 32 bytes, still interleaved
//      in 4-byte groups due to in-lane packing.
//   4. vpermd with indices [0,4,1,5,2,6,3,7]: restore sequential pixel order.
//
// Main loop handles multiples of 32; the row end: see above.
// ---------------------------------------------------------------------------
template<int channel_index>
void extract_packed_rgb32_channel_avx2(
  const BYTE* srcp, BYTE* dstp, int src_pitch, int dst_pitch, int width, int height)
{
  // Per-lane shuffle mask (repeated for both lanes):
  // channel byte → position 0 of each 4-byte pixel dword; bytes 1-3 → 0x80 (zero).
  constexpr int c = channel_index;
  const __m256i shuf256 = _mm256_set_epi8(
    (char)0x80,(char)0x80,(char)0x80,(char)(c+12),
    (char)0x80,(char)0x80,(char)0x80,(char)(c+ 8),
    (char)0x80,(char)0x80,(char)0x80,(char)(c+ 4),
    (char)0x80,(char)0x80,(char)0x80,(char)(c+ 0),
    (char)0x80,(char)0x80,(char)0x80,(char)(c+12),
    (char)0x80,(char)0x80,(char)0x80,(char)(c+ 8),
    (char)0x80,(char)0x80,(char)0x80,(char)(c+ 4),
    (char)0x80,(char)0x80,(char)0x80,(char)(c+ 0)
  );
  // Dword permutation to fix pack interleaving:
  // After packs+packus, dword order is [px0-3, px8-11, px16-19, px24-27, px4-7, px12-15, px20-23, px28-31].
  // Indices [0,4,1,5,2,6,3,7] restore sequential order.
  const __m256i perm = _mm256_set_epi32(7, 3, 6, 2, 5, 1, 4, 0);

  // Row end blocks: same shuffle but 128-bit (16 pixels, processed with SSE packs — no lane issue).
  const __m128i shuf128 = _mm_set_epi8(
    (char)0x80,(char)0x80,(char)0x80,(char)(c+12),
    (char)0x80,(char)0x80,(char)0x80,(char)(c+ 8),
    (char)0x80,(char)0x80,(char)0x80,(char)(c+ 4),
    (char)0x80,(char)0x80,(char)0x80,(char)(c+ 0)
  );

  for (int y = 0; y < height; ++y) {
    int x = 0;
    // AVX2 main loop: 32 pixels (128 source bytes → 32 output bytes) per iteration.
    for (; x <= width - 32; x += 32) {
      const BYTE* s = srcp + x * 4;
      __m256i v0 = _mm256_load_si256(reinterpret_cast<const __m256i*>(s +  0)); // px  0-7
      __m256i v1 = _mm256_load_si256(reinterpret_cast<const __m256i*>(s + 32)); // px  8-15
      __m256i v2 = _mm256_load_si256(reinterpret_cast<const __m256i*>(s + 64)); // px 16-23
      __m256i v3 = _mm256_load_si256(reinterpret_cast<const __m256i*>(s + 96)); // px 24-31
      v0 = _mm256_shuffle_epi8(v0, shuf256);
      v1 = _mm256_shuffle_epi8(v1, shuf256);
      v2 = _mm256_shuffle_epi8(v2, shuf256);
      v3 = _mm256_shuffle_epi8(v3, shuf256);
      // Pack dwords → words → bytes (in-lane; order fixed by vpermd below).
      __m256i p01 = _mm256_packs_epi32(v0, v1);
      __m256i p23 = _mm256_packs_epi32(v2, v3);
      __m256i packed = _mm256_packus_epi16(p01, p23);
      packed = _mm256_permutevar8x32_epi32(packed, perm);
      _mm256_store_si256(reinterpret_cast<__m256i*>(dstp + x), packed);
    }
    for (; x <= width - 16; x += 16)
      extract_rgb32_block16_ssse3<true>(srcp + x * 4, dstp + x, shuf128);
    if (x < width) {
      if (width >= 16) // overlapping last block
        extract_rgb32_block16_ssse3<false>(srcp + (width - 16) * 4, dstp + (width - 16), shuf128);
      else
        extract_packed_rgb_channel_row_c_avx2<uint8_t, 4, channel_index>(srcp, dstp, width);
    }
    srcp -= src_pitch;
    dstp += dst_pitch;
  }
}

template void extract_packed_rgb32_channel_avx2<0>(const BYTE*, BYTE*, int, int, int, int);
template void extract_packed_rgb32_channel_avx2<1>(const BYTE*, BYTE*, int, int, int, int);
template void extract_packed_rgb32_channel_avx2<2>(const BYTE*, BYTE*, int, int, int, int);
template void extract_packed_rgb32_channel_avx2<3>(const BYTE*, BYTE*, int, int, int, int);

// ---------------------------------------------------------------------------
// Packed RGB64 channel extraction — AVX2, 16 pixels per main iteration.
//
// Source layout (per pixel, 8 bytes): B G R A  (uint16_t each; B=0 G=1 R=2 A=3)
// Source rows are bottom-up; srcp must already point at the last (bottom) row.
//
// Per 16-byte lane (2 pixels): channel word at bytes 2c, 2c+1 (px0) and 2c+8, 2c+9 (px1).
//
// Strategy:
//   1. vpshufb: extract the two uint16 channel words from each 2-pixel pair
//      into bytes 0-3 of each lane, zero the rest.
//      Per lane result: [C0lo,C0hi, C1lo,C1hi, 0,...,0]
//   2. vpunpckldq × 2: interleave dwords from matching lanes across two registers.
//   3. vpunpcklqdq: take the valid 64-bit halves of each lane.
//      After steps 2-3 we have all 16 channel words in the register but still
//      interleaved in dword-pair groups, mirroring the RGB32 pattern.
//   4. vpermd with indices [0,4,1,5,2,6,3,7]: restore sequential pixel order.
//
// Main loop handles multiples of 16; the row end: see extract_rgb64_block8_ssse3 above.
// ---------------------------------------------------------------------------
template<int channel_index>
void extract_packed_rgb64_channel_avx2(
  const BYTE* srcp, BYTE* dstp, int src_pitch, int dst_pitch, int width, int height)
{
  constexpr int c = channel_index;
  // Shuffle mask for one 16-byte lane (2 pixels × 8 bytes each):
  // target channel is at byte offset 2c (lo) and 2c+1 (hi) within each 8-byte pixel.
  // Extract words from pixel 0 (offset 0) and pixel 1 (offset 8) to bytes 0-3.
  const __m256i shuf256 = _mm256_set_epi8(
    (char)0x80,(char)0x80,(char)0x80,(char)0x80,
    (char)0x80,(char)0x80,(char)0x80,(char)0x80,
    (char)0x80,(char)0x80,(char)0x80,(char)0x80,
    (char)(2*c+9),(char)(2*c+8),(char)(2*c+1),(char)(2*c+0),
    (char)0x80,(char)0x80,(char)0x80,(char)0x80,
    (char)0x80,(char)0x80,(char)0x80,(char)0x80,
    (char)0x80,(char)0x80,(char)0x80,(char)0x80,
    (char)(2*c+9),(char)(2*c+8),(char)(2*c+1),(char)(2*c+0)
  );
  const __m256i perm = _mm256_set_epi32(7, 3, 6, 2, 5, 1, 4, 0);

  // Row end blocks mask (same pattern, 128-bit).
  const __m128i shuf128 = _mm_set_epi8(
    (char)0x80,(char)0x80,(char)0x80,(char)0x80,
    (char)0x80,(char)0x80,(char)0x80,(char)0x80,
    (char)0x80,(char)0x80,(char)0x80,(char)0x80,
    (char)(2*c+9),(char)(2*c+8),(char)(2*c+1),(char)(2*c+0)
  );

  for (int y = 0; y < height; ++y) {
    int x = 0;
    // AVX2 main loop: 16 pixels (128 source bytes → 32 output bytes) per iteration.
    for (; x <= width - 16; x += 16) {
      const BYTE* s = srcp + x * 8;
      __m256i v0 = _mm256_load_si256(reinterpret_cast<const __m256i*>(s +  0)); // px  0-3
      __m256i v1 = _mm256_load_si256(reinterpret_cast<const __m256i*>(s + 32)); // px  4-7
      __m256i v2 = _mm256_load_si256(reinterpret_cast<const __m256i*>(s + 64)); // px  8-11
      __m256i v3 = _mm256_load_si256(reinterpret_cast<const __m256i*>(s + 96)); // px 12-15
      // After shuffle, each lane: [C_even_lo, C_even_hi, C_odd_lo, C_odd_hi, 0...0]
      // i.e., dword 0 of each lane holds the 2 channel uint16 values for that pixel pair.
      v0 = _mm256_shuffle_epi8(v0, shuf256);
      v1 = _mm256_shuffle_epi8(v1, shuf256);
      v2 = _mm256_shuffle_epi8(v2, shuf256);
      v3 = _mm256_shuffle_epi8(v3, shuf256);
      // Interleave valid dwords from matching lanes: result dwords = [{C0,C1},{C4,C5},0,0 | {C2,C3},{C6,C7},0,0]
      __m256i p01 = _mm256_unpacklo_epi32(v0, v1);
      __m256i p23 = _mm256_unpacklo_epi32(v2, v3);
      // Take the valid 64-bit half from each lane of both registers.
      // lo lane: [{C0,C1},{C4,C5}, {C8,C9},{C12,C13}]
      // hi lane: [{C2,C3},{C6,C7}, {C10,C11},{C14,C15}]
      __m256i gathered = _mm256_unpacklo_epi64(p01, p23);
      // Fix interleaving: dwords = [{C0,C1},{C4,C5},{C8,C9},{C12,C13}, {C2,C3},{C6,C7},{C10,C11},{C14,C15}]
      // Want sequential: [{C0,C1},{C2,C3},{C4,C5},{C6,C7}, {C8,C9},{C10,C11},{C12,C13},{C14,C15}]
      gathered = _mm256_permutevar8x32_epi32(gathered, perm);
      _mm256_store_si256(reinterpret_cast<__m256i*>(dstp + x * 2), gathered);
    }
    for (; x <= width - 8; x += 8)
      extract_rgb64_block8_ssse3<true>(srcp + x * 8, dstp + x * 2, shuf128);
    if (x < width) {
      if (width >= 8) // overlapping last block
        extract_rgb64_block8_ssse3<false>(srcp + (width - 8) * 8, dstp + (width - 8) * 2, shuf128);
      else
        extract_packed_rgb_channel_row_c_avx2<uint16_t, 4, channel_index>(srcp, dstp, width);
    }
    srcp -= src_pitch;
    dstp += dst_pitch;
  }
}

template void extract_packed_rgb64_channel_avx2<0>(const BYTE*, BYTE*, int, int, int, int);
template void extract_packed_rgb64_channel_avx2<1>(const BYTE*, BYTE*, int, int, int, int);
template void extract_packed_rgb64_channel_avx2<2>(const BYTE*, BYTE*, int, int, int, int);
template void extract_packed_rgb64_channel_avx2<3>(const BYTE*, BYTE*, int, int, int, int);

// ---------------------------------------------------------------------------
// Packed RGB24/RGB48 single-channel extraction — AVX2.
//
// pixel_t = uint8_t  → RGB24, 3 bytes/pixel.  32 pixels per main iteration.
// pixel_t = uint16_t → RGB48, 6 bytes/pixel.  16 pixels per main iteration.
// channel_index: B=0 G=1 R=2  (no alpha in these formats)
//
// Source rows are bottom-up; srcp must already point at the last (bottom) row.
//
// Both formats share the same deinterleave skeleton because 3-channel pixels
// pack perfectly into 48-byte groups (16 RGB24 pixels or 8 RGB48 pixels) and
// the same three cross-boundary realignment shifts (12, 8, 4) apply to both.
//
// Strategy — process two 48-byte half-groups per iteration using AVX2 lanes:
//   1. Load 6 × 16-byte SSE registers (3 per half-group) into 3 × __m256i, one
//      half per 128-bit lane.  Both lanes execute the identical sequence below.
//   2. vpshufb with a compile-time mask deinterleaves each aligned 12-byte window
//      (4 pixels / 2 pixels for u8/u16) so that:
//        bytes  0- 3: B values (or B uint16 pairs for RGB48)
//        bytes  4- 7: G values
//        bytes  8-11: R values
//        bytes 12-15: cross-boundary overflow (used by valignr, discarded after)
//   3. Three vpalignr shifts create four aligned windows from the three registers:
//        window 1: BGRA_1[0:15]         (no shift)
//        window 2: BGRA_1[12:15] ++ BGRA_2[0:11]   (alignr 12)
//        window 3: BGRA_2[ 8:15] ++ BGRA_3[0: 7]   (alignr  8)
//        window 4: BGRA_3[4:15] shifted   (srli 4)
//   4. unpacklo/hi_epi32 × 2 + unpacklo/hi_epi64 (selected by channel_index at
//      compile time via constexpr if) combine the four packs into 32 output bytes.
//
// Remainder: if width is not a multiple of pixels_per_iter, one final iteration
// with unaligned loads/stores covers the last pixels_per_iter pixels, overlapping
// the already-written end of the destination (safe — values are idempotent).
// Caller must ensure width >= pixels_per_iter (32 for RGB24, 16 for RGB48).
// ---------------------------------------------------------------------------
template<typename pixel_t, int channel_index>
void extract_packed_rgb_noalpha_channel_avx2(
  const BYTE* srcp, BYTE* dstp, int src_pitch, int dst_pitch, int width, int height)
{
  static_assert(channel_index >= 0 && channel_index <= 2,
    "RGB24/48 have no alpha; channel_index must be 0 (B), 1 (G), or 2 (R)");

  // pixels_per_iter = 32 (RGB24) or 16 (RGB48); both consume exactly 96 source bytes.
  constexpr int pixels_per_iter = (sizeof(pixel_t) == 1) ? 32 : 16;
  const int wmod = (width / pixels_per_iter) * pixels_per_iter;

  // Shuffle mask — same pattern for both 128-bit lanes.
  //
  // RGB24 (uint8_t): 16-byte window = 4 complete BGR pixels (12 bytes) + 4 overflow bytes.
  //   Deinterleave: B→bytes 0-3, G→bytes 4-7, R→bytes 8-11, overflow at 12-15.
  //   Byte source indices: B at 0,3,6,9; G at 1,4,7,10; R at 2,5,8,11; overflow at 12-15.
  //
  // RGB48 (uint16_t): 16-byte window = 2 complete BGR pixels (12 bytes) + 4 overflow bytes.
  //   Deinterleave: B uint16→bytes 0-3, G uint16→bytes 4-7, R uint16→bytes 8-11.
  //   Byte source indices: B0 at 0-1, B1 at 6-7; G0 at 2-3, G1 at 8-9; R0 at 4-5, R1 at 10-11.
  __m256i mask;
  if constexpr (sizeof(pixel_t) == 1)
    mask = _mm256_set_epi8(
      15,14,13,12, 11,8,5,2, 10,7,4,1, 9,6,3,0,   // hi lane (same pattern)
      15,14,13,12, 11,8,5,2, 10,7,4,1, 9,6,3,0);  // lo lane
  else
    mask = _mm256_set_epi8(
      15,14,13,12, 11,10,5,4, 9,8,3,2, 7,6,1,0,   // hi lane
      15,14,13,12, 11,10,5,4, 9,8,3,2, 7,6,1,0);  // lo lane

  // Inner kernel: given three __m256i covering two 48-byte half-groups (one per lane),
  // deinterleave and return 32 bytes of the requested channel.
  auto kernel = [&](__m256i v1, __m256i v2, __m256i v3) -> __m256i {
    // Window 1: v1 as-is.
    auto pack_lo  = _mm256_shuffle_epi8(v1, mask);
    // Window 2: BGRA_1[12:15] ++ BGRA_2[0:11] — per lane.
    v1 = _mm256_alignr_epi8(v2, v1, 12);
    auto pack_hi  = _mm256_shuffle_epi8(v1, mask);
    // Window 3: BGRA_2[8:15] ++ BGRA_3[0:7].
    v2 = _mm256_alignr_epi8(v3, v2, 8);
    auto pack_lo2 = _mm256_shuffle_epi8(v2, mask);
    // Window 4: BGRA_3 shifted right 4 bytes — suffix of v3 aligned to window start.
    v3 = _mm256_srli_si256(v3, 4);
    auto pack_hi2 = _mm256_shuffle_epi8(v3, mask);

    // After the four shuffles each pack register contains per lane:
    //   dword 0 = channel-B values (4 × u8  or 2 × u16)
    //   dword 1 = channel-G values
    //   dword 2 = channel-R values
    //   dword 3 = overflow / don't-care
    //
    // unpacklo_epi32 selects dwords 0 and 1 (B and G groups);
    // unpackhi_epi32 selects dwords 2 and 3 (R and overflow).
    // The final unpacklo/hi_epi64 picks the correct 64-bit half (B vs G, or R).
    if constexpr (channel_index == 0) {  // B: lo dword of each pack
      auto q1 = _mm256_unpacklo_epi32(pack_lo, pack_hi);    // [B_w1,B_w2, G_w1,G_w2] per lane
      auto q2 = _mm256_unpacklo_epi32(pack_lo2, pack_hi2);  // [B_w3,B_w4, G_w3,G_w4]
      return _mm256_unpacklo_epi64(q1, q2);                 // [B_w1,B_w2,B_w3,B_w4] per lane
    } else if constexpr (channel_index == 1) {  // G: hi half of the lo-dword pairs
      auto q1 = _mm256_unpacklo_epi32(pack_lo, pack_hi);
      auto q2 = _mm256_unpacklo_epi32(pack_lo2, pack_hi2);
      return _mm256_unpackhi_epi64(q1, q2);
    } else {  // R: lo dword of the hi-dword pairs
      auto q1 = _mm256_unpackhi_epi32(pack_lo, pack_hi);    // [R_w1,R_w2, overflow] per lane
      auto q2 = _mm256_unpackhi_epi32(pack_lo2, pack_hi2);
      return _mm256_unpacklo_epi64(q1, q2);                 // R values, overflow discarded
    }
  };

  for (int y = 0; y < height; ++y) {
    // Main loop — aligned loads (source row starts 64-byte aligned; x*3*sizeof advances
    // by multiples of 96 bytes = 6×16, so all six loads remain 16-byte aligned).
    int x = 0;
    for (; x < wmod; x += pixels_per_iter) {
      const BYTE* s = srcp + x * 3 * (int)sizeof(pixel_t);
      // Pack lo half (bytes 0-47) into lo 128-bit lane and hi half (bytes 48-95) into hi lane.
      auto v1 = _mm256_set_m128i(
        _mm_load_si128(reinterpret_cast<const __m128i*>(s + 48 +  0)),
        _mm_load_si128(reinterpret_cast<const __m128i*>(s       +  0)));
      auto v2 = _mm256_set_m128i(
        _mm_load_si128(reinterpret_cast<const __m128i*>(s + 48 + 16)),
        _mm_load_si128(reinterpret_cast<const __m128i*>(s       + 16)));
      auto v3 = _mm256_set_m128i(
        _mm_load_si128(reinterpret_cast<const __m128i*>(s + 48 + 32)),
        _mm_load_si128(reinterpret_cast<const __m128i*>(s       + 32)));
      _mm256_store_si256(
        reinterpret_cast<__m256i*>(dstp + x * (int)sizeof(pixel_t)),
        kernel(v1, v2, v3));
    }
    // Remainder — at most (pixels_per_iter - 1) pixels left; handle by re-processing the
    // last pixels_per_iter pixels with unaligned loads/stores (may overlap previous writes).
    if (wmod < width) {
      x = width - pixels_per_iter;
      const BYTE* s = srcp + x * 3 * (int)sizeof(pixel_t);
      auto v1 = _mm256_set_m128i(
        _mm_loadu_si128(reinterpret_cast<const __m128i*>(s + 48 +  0)),
        _mm_loadu_si128(reinterpret_cast<const __m128i*>(s       +  0)));
      auto v2 = _mm256_set_m128i(
        _mm_loadu_si128(reinterpret_cast<const __m128i*>(s + 48 + 16)),
        _mm_loadu_si128(reinterpret_cast<const __m128i*>(s       + 16)));
      auto v3 = _mm256_set_m128i(
        _mm_loadu_si128(reinterpret_cast<const __m128i*>(s + 48 + 32)),
        _mm_loadu_si128(reinterpret_cast<const __m128i*>(s       + 32)));
      _mm256_storeu_si256(
        reinterpret_cast<__m256i*>(dstp + x * (int)sizeof(pixel_t)),
        kernel(v1, v2, v3));
    }
    srcp -= src_pitch;
    dstp += dst_pitch;
  }
}

// Explicit instantiations: RGB24 (uint8_t) channels B, G, R
template void extract_packed_rgb_noalpha_channel_avx2<uint8_t,  0>(const BYTE*, BYTE*, int, int, int, int);
template void extract_packed_rgb_noalpha_channel_avx2<uint8_t,  1>(const BYTE*, BYTE*, int, int, int, int);
template void extract_packed_rgb_noalpha_channel_avx2<uint8_t,  2>(const BYTE*, BYTE*, int, int, int, int);
// Explicit instantiations: RGB48 (uint16_t) channels B, G, R
template void extract_packed_rgb_noalpha_channel_avx2<uint16_t, 0>(const BYTE*, BYTE*, int, int, int, int);
template void extract_packed_rgb_noalpha_channel_avx2<uint16_t, 1>(const BYTE*, BYTE*, int, int, int, int);
template void extract_packed_rgb_noalpha_channel_avx2<uint16_t, 2>(const BYTE*, BYTE*, int, int, int, int);
