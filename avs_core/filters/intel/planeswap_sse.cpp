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


// Avisynth filter:  Swap planes
// by Klaus Post
// adapted by Richard Berg (avisynth-dev@richardberg.net)
// iSSE code by Ian Brabham

#include <avs/config.h>
#ifdef AVS_WINDOWS
#include <avs/win.h>
#else
#include <avs/posix.h>
#endif
#include "planeswap_sse.h"

// Intrinsics base header + really required extension headers
#if defined(_MSC_VER)
#include <intrin.h> // MSVC
#else 
#include <x86intrin.h> // GCC/MinGW/Clang/LLVM
#endif
#include <tmmintrin.h> // SSSE3

#include "stdint.h"
#include <type_traits>

// ---------------------------------------------------------------------------
// Packed RGB32 / RGB64 channel extraction — SSE2.
//
// Source rows are bottom-up; srcp must already point at the last (bottom) row.
// Pitch is positive; we step backwards (srcp -= src_pitch) each row.
//
// Row end: whole aligned blocks while they fit, then one unaligned block that ends exactly at
// width, overlapping pixels already written (same values). Rows narrower than one block are
// copied per pixel. Nothing is read or written beyond the row.
// ---------------------------------------------------------------------------

template<bool aligned>
static AVS_FORCEINLINE __m128i load_si128(const BYTE* p)
{
  if constexpr (aligned)
    return _mm_load_si128(reinterpret_cast<const __m128i*>(p));
  else
    return _mm_loadu_si128(reinterpret_cast<const __m128i*>(p));
}

template<bool aligned>
static AVS_FORCEINLINE void store_si128(BYTE* p, __m128i v)
{
  if constexpr (aligned)
    _mm_store_si128(reinterpret_cast<__m128i*>(p), v);
  else
    _mm_storeu_si128(reinterpret_cast<__m128i*>(p), v);
}

// one row per pixel, rows narrower than one SIMD block
template<typename pixel_t, int components, int channel_index>
static void extract_packed_rgb_channel_row_c(const BYTE* srcp8, BYTE* dstp8, int width)
{
  const pixel_t* srcp = reinterpret_cast<const pixel_t*>(srcp8);
  pixel_t* dstp = reinterpret_cast<pixel_t*>(dstp8);
  for (int x = 0; x < width; ++x)
    dstp[x] = srcp[x * components + channel_index];
}

// RGB32, 16 pixels: shift each 32-bit pixel right by channel_index*8, mask the low byte,
// pack 4+4+4+4 = 16 values through packs_epi32 + packus_epi16.
// Source layout (per pixel, 4 bytes): B G R A  (channel_index: B=0 G=1 R=2 A=3)
template<int channel_index, bool aligned>
static AVS_FORCEINLINE void extract_rgb32_block16_sse2(const BYTE* srcp, BYTE* dstp)
{
  const __m128i mask = _mm_set1_epi32(0xFF);
  __m128i v0 = load_si128<aligned>(srcp + 0);
  __m128i v1 = load_si128<aligned>(srcp + 16);
  __m128i v2 = load_si128<aligned>(srcp + 32);
  __m128i v3 = load_si128<aligned>(srcp + 48);
  // Move requested byte to the low byte of each 32-bit dword
  if constexpr (channel_index > 0) {
    v0 = _mm_srli_epi32(v0, channel_index * 8);
    v1 = _mm_srli_epi32(v1, channel_index * 8);
    v2 = _mm_srli_epi32(v2, channel_index * 8);
    v3 = _mm_srli_epi32(v3, channel_index * 8);
  }
  v0 = _mm_and_si128(v0, mask);
  v1 = _mm_and_si128(v1, mask);
  v2 = _mm_and_si128(v2, mask);
  v3 = _mm_and_si128(v3, mask);
  // Pack 4 × 32-bit (values 0-255) → 8 × 16-bit → 16 × 8-bit
  store_si128<aligned>(dstp, _mm_packus_epi16(_mm_packs_epi32(v0, v1), _mm_packs_epi32(v2, v3)));
}

template<int channel_index>
void extract_packed_rgb32_channel_sse2(
  const BYTE* srcp, BYTE* dstp, int src_pitch, int dst_pitch, int width, int height)
{
  for (int y = 0; y < height; ++y) {
    if (width < 16)
      extract_packed_rgb_channel_row_c<uint8_t, 4, channel_index>(srcp, dstp, width);
    else {
      int x = 0;
      for (; x <= width - 16; x += 16)
        extract_rgb32_block16_sse2<channel_index, true>(srcp + x * 4, dstp + x);
      if (x < width) // overlapping last block, unaligned
        extract_rgb32_block16_sse2<channel_index, false>(srcp + (width - 16) * 4, dstp + (width - 16));
    }
    srcp -= src_pitch;
    dstp += dst_pitch;
  }
}
template void extract_packed_rgb32_channel_sse2<0>(const BYTE*, BYTE*, int, int, int, int);
template void extract_packed_rgb32_channel_sse2<1>(const BYTE*, BYTE*, int, int, int, int);
template void extract_packed_rgb32_channel_sse2<2>(const BYTE*, BYTE*, int, int, int, int);
template void extract_packed_rgb32_channel_sse2<3>(const BYTE*, BYTE*, int, int, int, int);

// ---------------------------------------------------------------------------
// RGB64, 8 pixels per block.
//
// Source layout (per pixel, 8 bytes): B G R A  (uint16_t each; B=0 G=1 R=2 A=3)
//
// Strategy: 3-stage deinterleave using unpacklo/hi_epi16, unpacklo/hi_epi32,
// unpacklo/hi_epi64, then unscramble with shufflelo/hi_epi16.
//
// Two register pairs (a,b) and (c,d) each hold 4 pixels:
//   a = [B0 G0 R0 A0 | B1 G1 R1 A1]   (pixel 0, pixel 1 — 8 uint16_t)
//   b = [B2 G2 R2 A2 | B3 G3 R3 A3]
//   c = pixels 4-5, d = pixels 6-7
//
// After unpacklo_epi16(a,b): [B0 B2 | G0 G2 | R0 R2 | A0 A2]
// After unpackhi_epi16(a,b): [B1 B3 | G1 G3 | R1 R3 | A1 A3]
// unpacklo_epi32 of those:   [B0 B2 B1 B3 | G0 G2 G1 G3]   (ch 0,1 in lo/hi 64-bit lanes)
// unpackhi_epi32 of those:   [R0 R2 R1 R3 | A0 A2 A1 A3]   (ch 2,3 in lo/hi 64-bit lanes)
// unpacklo/hi_epi64 selects the right channel across both pairs → [C0 C2 C1 C3 | C4 C6 C5 C7]
// shufflelo + shufflehi with _MM_SHUFFLE(3,1,2,0) unscrambles to [C0 C1 C2 C3 | C4 C5 C6 C7].
// ---------------------------------------------------------------------------
template<int channel_index, bool aligned>
static AVS_FORCEINLINE void extract_rgb64_block8_sse2(const BYTE* srcp, BYTE* dstp)
{
  __m128i a = load_si128<aligned>(srcp + 0);  // px 0-1
  __m128i b = load_si128<aligned>(srcp + 16); // px 2-3
  __m128i c = load_si128<aligned>(srcp + 32); // px 4-5
  __m128i d = load_si128<aligned>(srcp + 48); // px 6-7

  // Stage 1: interleave same-index pixels from each pair
  // t0/t1 from pair (a,b); t4/t5 from pair (c,d)
  __m128i t0 = _mm_unpacklo_epi16(a, b); // [B0 B2 | G0 G2 | R0 R2 | A0 A2]
  __m128i t1 = _mm_unpackhi_epi16(a, b); // [B1 B3 | G1 G3 | R1 R3 | A1 A3]
  __m128i t4 = _mm_unpacklo_epi16(c, d); // [B4 B6 | G4 G6 | R4 R6 | A4 A6]
  __m128i t5 = _mm_unpackhi_epi16(c, d); // [B5 B7 | G5 G7 | R5 R7 | A5 A7]

  // Stage 2: gather channel into 64-bit lanes
  // ch 0,1: lo qword = channel ch%2==0, hi qword = channel ch%2==1
  // ch 2,3: same but from the hi 32-bit positions
  __m128i lo, hi;
  if constexpr (channel_index <= 1) {
    lo = _mm_unpacklo_epi32(t0, t1); // [B0 B2 B1 B3 | G0 G2 G1 G3]
    hi = _mm_unpacklo_epi32(t4, t5); // [B4 B6 B5 B7 | G4 G6 G5 G7]
  } else {
    lo = _mm_unpackhi_epi32(t0, t1); // [R0 R2 R1 R3 | A0 A2 A1 A3]
    hi = _mm_unpackhi_epi32(t4, t5); // [R4 R6 R5 R7 | A4 A6 A5 A7]
  }

  // Stage 3: select even-indexed (ch 0,2) or odd-indexed (ch 1,3) channel lane
  __m128i v;
  if constexpr (channel_index == 0 || channel_index == 2) {
    v = _mm_unpacklo_epi64(lo, hi); // [C0 C2 C1 C3 | C4 C6 C5 C7]
  } else {
    v = _mm_unpackhi_epi64(lo, hi); // [C0 C2 C1 C3 | C4 C6 C5 C7]
  }

  // Stage 4: unscramble [C0 C2 C1 C3 | C4 C6 C5 C7] → [C0 C1 C2 C3 | C4 C5 C6 C7]
  v = _mm_shufflelo_epi16(v, _MM_SHUFFLE(3,1,2,0));
  v = _mm_shufflehi_epi16(v, _MM_SHUFFLE(3,1,2,0));

  store_si128<aligned>(dstp, v);
}

template<int channel_index>
void extract_packed_rgb64_channel_sse2(
  const BYTE* srcp, BYTE* dstp, int src_pitch, int dst_pitch, int width, int height)
{
  for (int y = 0; y < height; ++y) {
    if (width < 8)
      extract_packed_rgb_channel_row_c<uint16_t, 4, channel_index>(srcp, dstp, width);
    else {
      int x = 0;
      for (; x <= width - 8; x += 8)
        extract_rgb64_block8_sse2<channel_index, true>(srcp + x * 8, dstp + x * 2);
      if (x < width) // overlapping last block, unaligned
        extract_rgb64_block8_sse2<channel_index, false>(srcp + (width - 8) * 8, dstp + (width - 8) * 2);
    }
    srcp -= src_pitch;
    dstp += dst_pitch;
  }
}

template void extract_packed_rgb64_channel_sse2<0>(const BYTE*, BYTE*, int, int, int, int);
template void extract_packed_rgb64_channel_sse2<1>(const BYTE*, BYTE*, int, int, int, int);
template void extract_packed_rgb64_channel_sse2<2>(const BYTE*, BYTE*, int, int, int, int);
template void extract_packed_rgb64_channel_sse2<3>(const BYTE*, BYTE*, int, int, int, int);

// Packed RGB24/RGB48 channel extraction
// SSSE3, 48 source bytes per block (16 RGB24 pixels or 8 RGB48 pixels).
// xmm version of extract_packed_rgb_noalpha_channel_avx2
// Row process: aligned blocks while they fit, then one unaligned block that ends exactly at width
// Assumed width >= 16 (RGB24) / 8 (RGB48) - caller's task
template<typename pixel_t, int channel_index, bool aligned>
#if defined(GCC) || defined(CLANG)
__attribute__((__target__("ssse3")))
#endif
static AVS_FORCEINLINE void extract_rgb_noalpha_block48_ssse3(const BYTE* s, BYTE* d)
{
  // 0123456789ABCDEF 0123456789ABCDEF 0123456789ABCDEF
  // BGRBGRBGRBGRBGRB GRBGRBGRBGRBGRBG RBGRBGRBGRBGRBGR // 8 bit
  // B G R B G R B G  R B G R B G R B  G R B G R B G R  // 16 bit
  // 1111111111112222 2222222233333333 3333444444444444  (4x 12 byte windows)
  __m128i mask;
  if constexpr (sizeof(pixel_t) == 1)
    mask = _mm_set_epi8(15, 14, 13, 12, 11, 8, 5, 2, 10, 7, 4, 1, 9, 6, 3, 0);
  else
    mask = _mm_set_epi8(15, 14, 13, 12, 11, 10, 5, 4, 9, 8, 3, 2, 7, 6, 1, 0);
  __m128i v1 = load_si128<aligned>(s + 0);
  __m128i v2 = load_si128<aligned>(s + 16);
  __m128i v3 = load_si128<aligned>(s + 32);
  // each pack: dword 0 = B, 1 = G, 2 = R, 3 = don't care
  const __m128i pack_lo = _mm_shuffle_epi8(v1, mask);                          // window 1
  const __m128i pack_hi = _mm_shuffle_epi8(_mm_alignr_epi8(v2, v1, 12), mask); // window 2
  const __m128i pack_lo2 = _mm_shuffle_epi8(_mm_alignr_epi8(v3, v2, 8), mask); // window 3
  const __m128i pack_hi2 = _mm_shuffle_epi8(_mm_srli_si128(v3, 4), mask);      // window 4
  __m128i res;
  if constexpr (channel_index == 0) // B
    res = _mm_unpacklo_epi64(_mm_unpacklo_epi32(pack_lo, pack_hi), _mm_unpacklo_epi32(pack_lo2, pack_hi2));
  else if constexpr (channel_index == 1) // G
    res = _mm_unpackhi_epi64(_mm_unpacklo_epi32(pack_lo, pack_hi), _mm_unpacklo_epi32(pack_lo2, pack_hi2));
  else // R
    res = _mm_unpacklo_epi64(_mm_unpackhi_epi32(pack_lo, pack_hi), _mm_unpackhi_epi32(pack_lo2, pack_hi2));
  store_si128<aligned>(d, res);
}

template<typename pixel_t, int channel_index>
#if defined(GCC) || defined(CLANG)
__attribute__((__target__("ssse3")))
#endif
void extract_packed_rgb_noalpha_channel_ssse3(
  const BYTE* srcp, BYTE* dstp, int src_pitch, int dst_pitch, int width, int height)
{
  static_assert(channel_index >= 0 && channel_index <= 2,
    "RGB24/48 have no alpha; channel_index must be 0 (B), 1 (G), or 2 (R)");
  constexpr int pixels_per_block = (sizeof(pixel_t) == 1) ? 16 : 8; // 48 source bytes
  const int wmod = (width / pixels_per_block) * pixels_per_block;
  for (int y = 0; y < height; ++y) {
    // source rows start 64-byte aligned, a block is 48 bytes: the block loads stay 16-byte aligned
    for (int x = 0; x < wmod; x += pixels_per_block)
      extract_rgb_noalpha_block48_ssse3<pixel_t, channel_index, true>(srcp + x * 3 * (int)sizeof(pixel_t), dstp + x * (int)sizeof(pixel_t));
    if (wmod < width) { // overlapping last block
      const int x = width - pixels_per_block;
      extract_rgb_noalpha_block48_ssse3<pixel_t, channel_index, false>(srcp + x * 3 * (int)sizeof(pixel_t), dstp + x * (int)sizeof(pixel_t));
    }
    srcp -= src_pitch;
    dstp += dst_pitch;
  }
}
template void extract_packed_rgb_noalpha_channel_ssse3<uint8_t, 0>(const BYTE*, BYTE*, int, int, int, int);
template void extract_packed_rgb_noalpha_channel_ssse3<uint8_t, 1>(const BYTE*, BYTE*, int, int, int, int);
template void extract_packed_rgb_noalpha_channel_ssse3<uint8_t, 2>(const BYTE*, BYTE*, int, int, int, int);
template void extract_packed_rgb_noalpha_channel_ssse3<uint16_t, 0>(const BYTE*, BYTE*, int, int, int, int);
template void extract_packed_rgb_noalpha_channel_ssse3<uint16_t, 1>(const BYTE*, BYTE*, int, int, int, int);
template void extract_packed_rgb_noalpha_channel_ssse3<uint16_t, 2>(const BYTE*, BYTE*, int, int, int, int);



/**************************************
 *  Swap - swaps UV on planar maps
 **************************************/

void yuy2_swap_sse2(const BYTE* srcp, BYTE* dstp, int src_pitch, int dst_pitch, int width, int height)
{
  const __m128i mask = _mm_set1_epi16(0x00FF);

  for (int y = 0; y < height; ++y ) {
    for (int x = 0; x < width; x += 16) {
      __m128i src = _mm_load_si128(reinterpret_cast<const __m128i*>(srcp + x));
      __m128i swapped = _mm_shufflelo_epi16(src, _MM_SHUFFLE(2, 3, 0, 1));
      swapped = _mm_shufflehi_epi16(swapped, _MM_SHUFFLE(2, 3, 0, 1));
      swapped = _mm_or_si128(_mm_and_si128(mask, src), _mm_andnot_si128(mask, swapped));
      _mm_stream_si128(reinterpret_cast<__m128i*>(dstp + x), swapped);
    }

    dstp += dst_pitch;
    srcp += src_pitch;
  }
}

#if defined(GCC) || defined(CLANG)
__attribute__((__target__("ssse3")))
#endif
void yuy2_swap_ssse3(const BYTE* srcp, BYTE* dstp, int src_pitch, int dst_pitch, int width, int height)
{
  const __m128i mask = _mm_set_epi8(13, 14, 15, 12, 9, 10, 11, 8, 5, 6, 7, 4, 1, 2, 3, 0);

  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; x += 16) {
      __m128i src = _mm_load_si128(reinterpret_cast<const __m128i*>(srcp + x));
      __m128i dst = _mm_shuffle_epi8(src, mask);
      _mm_stream_si128(reinterpret_cast<__m128i*>(dstp + x), dst);
    }

    dstp += dst_pitch;
    srcp += src_pitch;
  }
}

#ifdef X86_32
void yuy2_swap_isse(const BYTE* srcp, BYTE* dstp, int src_pitch, int dst_pitch, int width, int height)
{
  __m64 mask = _mm_set1_pi16(0x00FF);

  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; x+= 8) {
      __m64 src = *reinterpret_cast<const __m64*>(srcp+x);
      __m64 swapped = _mm_shuffle_pi16(src, _MM_SHUFFLE(2, 3, 0, 1));
      swapped = _mm_or_si64(_mm_and_si64(mask, src), _mm_andnot_si64(mask, swapped));
      *reinterpret_cast<__m64*>(dstp + x) = swapped;
    }

    dstp += dst_pitch;
    srcp += src_pitch;
  }
  _mm_empty();
}
#endif

void yuy2_uvtoy_sse2(const BYTE* srcp, BYTE* dstp, int src_pitch, int dst_pitch, int dst_width, int height, int pos)
{
  const __m128i chroma = _mm_set1_epi32(0x80008000);
  const __m128i mask = _mm_set1_epi32(0x000000FF);
  pos *= 8;

  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < dst_width; x += 16) {
      __m128i s0 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(srcp + 2 * x));
      __m128i s1 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(srcp + 2 * x + 16));
      s0 = _mm_and_si128(mask, _mm_srli_epi32(s0, pos));
      s1 = _mm_and_si128(mask, _mm_srli_epi32(s1, pos));
      s0 = _mm_packs_epi32(s0, s1);
      s0 = _mm_or_si128(s0, chroma);
      _mm_stream_si128(reinterpret_cast<__m128i*>(dstp + x), s0);
    }
    srcp += src_pitch;
    dstp += dst_pitch;
  }
}

void yuy2_uvtoy8_sse2(const BYTE* srcp, BYTE* dstp, int src_pitch, int dst_pitch, int dst_width, int height, int pos)
{
  const __m128i mask = _mm_set1_epi32(0x000000FF);
  pos *= 8;

  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < dst_width; x += 8) {
      __m128i s0 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(srcp + 4 * x));
      __m128i s1 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(srcp + 4 * x + 16));
      s0 = _mm_and_si128(mask, _mm_srli_epi32(s0, pos));
      s1 = _mm_and_si128(mask, _mm_srli_epi32(s1, pos));
      s0 = _mm_packs_epi32(s0, s1);
      s0 = _mm_packus_epi16(s0, s0);
      _mm_storel_epi64(reinterpret_cast<__m128i*>(dstp + x), s0);
    }
    srcp += src_pitch;
    dstp += dst_pitch;
  }
}

template <bool has_clipY>
void yuy2_ytouv_sse2(const BYTE* srcp_y, const BYTE* srcp_u, const BYTE* srcp_v, BYTE* dstp, int pitch_y, int pitch_u, int pitch_v, int dst_pitch, int dst_rowsize, int height)
{
  const __m128i mask = _mm_set1_epi16(0x00FF);
  const __m128i zero = _mm_setzero_si128();
  const __m128i fill = _mm_set1_epi16(0x007e);

  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < dst_rowsize; x += 32) {
      __m128i u = _mm_loadu_si128(reinterpret_cast<const __m128i*>(srcp_u + x / 2));
      __m128i v = _mm_loadu_si128(reinterpret_cast<const __m128i*>(srcp_v + x / 2));
      __m128i uv = _mm_or_si128(_mm_and_si128(u, mask), _mm_slli_epi16(v, 8));
      __m128i uv_lo = _mm_unpacklo_epi8(zero, uv);
      __m128i uv_hi = _mm_unpackhi_epi8(zero, uv);
      if (has_clipY) {
        __m128i y_lo = _mm_loadu_si128(reinterpret_cast<const __m128i*>(srcp_y + x));
        __m128i y_hi = _mm_loadu_si128(reinterpret_cast<const __m128i*>(srcp_y + x + 16));
        uv_lo = _mm_or_si128(uv_lo, _mm_and_si128(y_lo, mask));
        uv_hi = _mm_or_si128(uv_hi, _mm_and_si128(y_hi, mask));
      }
      else {
        uv_lo = _mm_or_si128(uv_lo, fill);
        uv_hi = _mm_or_si128(uv_hi, fill);
      }
      _mm_stream_si128(reinterpret_cast<__m128i*>(dstp + x), uv_lo);
      _mm_stream_si128(reinterpret_cast<__m128i*>(dstp + x + 16), uv_hi);
    }
    srcp_y += pitch_y;
    srcp_u += pitch_u;
    srcp_v += pitch_v;
    dstp += dst_pitch;
  }
}

template void yuy2_ytouv_sse2<false>(const BYTE* srcp_y, const BYTE* srcp_u, const BYTE* srcp_v, BYTE* dstp, int pitch_y, int pitch_u, int pitch_v, int dst_pitch, int dst_rowsize, int height);
template void yuy2_ytouv_sse2<true>(const BYTE* srcp_y, const BYTE* srcp_u, const BYTE* srcp_v, BYTE* dstp, int pitch_y, int pitch_u, int pitch_v, int dst_pitch, int dst_rowsize, int height);
