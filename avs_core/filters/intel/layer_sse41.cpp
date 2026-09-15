// AviSynth+  Copyright 2026- AviSynth+ Project
// SPDX-License-Identifier: GPL-2.0-or-later
//
// SSE4.1 Layer add/subtract dispatcher + SSE2 invert_plane kernels.
// Named *_sse41.cpp so the CMake handle_arch_flags(SSE41) glob assigns
// per-file -msse4.1 (GCC/Clang) flags to this translation unit.
// That means layer.hpp C templates included here are auto-vectorized for SSE4.1,
// matching the AVX2 pattern in layer_avx2.cpp.
// invert_plane_sse2_* are marked __target__("sse2") on GCC/Clang so the compiler
// does not emit SSE4.1 instructions inside them despite the TU's -msse4.1 flag.

#include "../layer.h"       // layer_yuv_add_c_t/f, PLACEMENT_*, pulls in blend_common.h
#include "layer_sse41.h"    // declaration of get_layer_yuv_add_functions_sse41

#if defined(_MSC_VER)
#include <intrin.h>         // MSVC
#else
#include <x86intrin.h>      // GCC/MinGW/Clang/LLVM
#endif
#include <smmintrin.h>      // SSE4.1

#include <cstdint>
#include <type_traits>
#include <vector>

#include "../core/internal.h"   // DISABLE_WARNING_PUSH/POP — needed by layer.hpp

// masked_merge_sse41_impl<maskMode> — static, SSE4.1.
// Including here (after intrinsic headers) compiles it with SSE4.1 flags for this TU.
#include "../overlay/intel/masked_merge_sse41_impl.hpp"
#include "../overlay/intel/blend_common_sse.h"

// C template dispatcher: get_layer_yuv_add_masked_functions<is_subtract>.
// static functions — this TU's copy gets -msse4.1 auto-vectorization (GCC/Clang).
// Use SSE4.1 SIMD rowprep variants — masked_rowprep_sse41.hpp is already included above
// via masked_merge_sse41_impl.hpp.
#define LAYER_ROWPREP_FN       prepare_effective_mask_for_row_sse41
#include "../layer.hpp"
#undef LAYER_ROWPREP_FN

// ---------------------------------------------------------------------------
// Planar RGB add — SSE4.1 per-plane wrappers (mirrors AVX2 counterparts).
// All planar RGB planes are MASK444. maskp8 is the per-pixel weight.
// chroma=false and float fall back to C templates.

static void layer_planarrgb_add_sse41_3plane(
  BYTE** dstp8, const BYTE** ovrp8, const BYTE* maskp8,
  int dst_pitch, int overlay_pitch, int mask_pitch,
  int width, int height, int opacity_i, int bits_per_pixel)
{
  for (int i = 0; i < 3; i++)
    masked_merge_sse41_impl<MASK444>(
      dstp8[i], ovrp8[i], maskp8,
      dst_pitch, overlay_pitch, mask_pitch,
      width, height, opacity_i, bits_per_pixel);
}

static void layer_planarrgb_add_sse41_4plane(
  BYTE** dstp8, const BYTE** ovrp8, const BYTE* maskp8,
  int dst_pitch, int overlay_pitch, int mask_pitch,
  int width, int height, int opacity_i, int bits_per_pixel)
{
  for (int i = 0; i < 4; i++)
    masked_merge_sse41_impl<MASK444>(
      dstp8[i], ovrp8[i], maskp8,
      dst_pitch, overlay_pitch, mask_pitch,
      width, height, opacity_i, bits_per_pixel);
}

void get_layer_planarrgb_add_functions_sse41(
  bool chroma, bool hasAlpha, bool blendAlpha, int bits_per_pixel,
  layer_planarrgb_add_c_t** layer_fn,
  layer_planarrgb_add_f_c_t** layer_f_fn)
{
  // Integer + hasAlpha + chroma=true: dispatch per-plane to masked_merge_avx2_impl (MASK444).
  // chroma=false (blend toward luma) has a different formula — keep C template.
  // float: keep C template (float perf is usually fine; could add later).
  if (hasAlpha && chroma && bits_per_pixel != 32) {
    *layer_fn = blendAlpha ? layer_planarrgb_add_sse41_4plane : layer_planarrgb_add_sse41_3plane;
    return;
  }

  // chroma is true: Layer can use the unified masked and weighted blend routines
  // chroma is false: Layer-specific extension
  // Integer + hasAlpha + chroma=true: dispatch per-plane to masked_merge_avx2_impl (MASK444).
  // chroma=false (blend toward luma) has a different formula — keep C template.
  // float: keep C template (float perf is usually fine; could add later).
  if (bits_per_pixel != 32 && chroma) {
    if (hasAlpha) {
      // standard masked merge
      *layer_fn = blendAlpha ? layer_planarrgb_add_sse41_4plane : layer_planarrgb_add_sse41_3plane;
      return;
    }
    // no alpha: standard weighted merge, to be added later.
  }
  get_layer_planarrgb_add_functions(chroma, hasAlpha, blendAlpha, bits_per_pixel, layer_fn, layer_f_fn);
}

// ---------------------------------------------------------------------------
// Packed RGBA (RGB32) magic-div blend — SSE4.1, 8-bit only.
//
// Processes 4 BGRA pixels (16 bytes) per iteration.
// Arithmetic is identical to the AVX2 version; see that function's comment
// for derivations and overflow proofs.
//
// has_separate_mask=false → alpha from ovrp8[x*4+3]  (Add)
// has_separate_mask=true  → alpha from maskp8[x]      (Subtract, overlay pre-inverted)
// ---------------------------------------------------------------------------
template<bool has_separate_mask>
#if defined(GCC) || defined(CLANG)
__attribute__((__target__("sse4.1")))
#endif
static void masked_blend_packedrgba_sse41_u8(
  BYTE* dstp8, const BYTE* ovrp8, const BYTE* maskp8,
  int dst_pitch, int ovr_pitch, int mask_pitch,
  int width, int height, int opacity_i)
{
  // Shuffle: replicate byte 3 (A) of each BGRA pixel to all 4 bytes.
  // {3,3,3,3, 7,7,7,7, 11,11,11,11, 15,15,15,15}
  const __m128i shuf_alpha_bgra = _mm_set_epi8(
    15,15,15,15, 11,11,11,11, 7,7,7,7, 3,3,3,3);
  // Shuffle for separate mask: 4 bytes m0..m3 → each byte broadcast to 4 positions.
  // {0,0,0,0, 1,1,1,1, 2,2,2,2, 3,3,3,3}
  const __m128i shuf_alpha_mask = _mm_set_epi8(
    3,3,3,3, 2,2,2,2, 1,1,1,1, 0,0,0,0);

  const __m128i v_opacity = _mm_set1_epi16((short)opacity_i);
  const __m128i v_half    = _mm_set1_epi16(127);
  const __m128i v_max     = _mm_set1_epi16(255);
  const __m128i v_magic   = _mm_set1_epi16((short)0x8081u); // magic for ÷255

  // magic ÷255: mulhi_epu16(x, 0x8081) >> 7
  auto div255 = [&](__m128i x) -> __m128i {
    return _mm_srli_epi16(_mm_mulhi_epu16(x, v_magic), 7);
  };

  const int mod4_width = width / 4 * 4;

  for (int y = 0; y < height; ++y) {
    int x = 0;
    for (; x < mod4_width; x += 4) {
      __m128i dst16 = _mm_loadu_si128((const __m128i*)(dstp8 + x * 4));
      __m128i ovr16 = _mm_loadu_si128((const __m128i*)(ovrp8 + x * 4));

      // Broadcast per-pixel alpha to all 4 channel bytes.
      __m128i alpha_bcast;
      if constexpr (has_separate_mask) {
        // Load 4 mask bytes (one per pixel) and broadcast each to 4 bytes.
        __m128i mask4 = _mm_cvtsi32_si128(*(const int*)(maskp8 + x));
        alpha_bcast = _mm_shuffle_epi8(mask4, shuf_alpha_mask);
      } else {
        alpha_bcast = _mm_shuffle_epi8(ovr16, shuf_alpha_bgra);
      }

      // Expand to 16-bit in two halves (lo=px0-1, hi=px2-3).
      __m128i dst_lo = _mm_cvtepu8_epi16(dst16);
      __m128i dst_hi = _mm_cvtepu8_epi16(_mm_srli_si128(dst16, 8));
      __m128i ovr_lo = _mm_cvtepu8_epi16(ovr16);
      __m128i ovr_hi = _mm_cvtepu8_epi16(_mm_srli_si128(ovr16, 8));
      __m128i a_lo   = _mm_cvtepu8_epi16(alpha_bcast);
      __m128i a_hi   = _mm_cvtepu8_epi16(_mm_srli_si128(alpha_bcast, 8));

      // alpha_eff = (alpha * opacity_i + 127) / 255
      __m128i aeff_lo = div255(_mm_add_epi16(_mm_mullo_epi16(a_lo, v_opacity), v_half));
      __m128i aeff_hi = div255(_mm_add_epi16(_mm_mullo_epi16(a_hi, v_opacity), v_half));
      __m128i inv_lo  = _mm_sub_epi16(v_max, aeff_lo);
      __m128i inv_hi  = _mm_sub_epi16(v_max, aeff_hi);

      // b = ovr (overlay pre-inverted for Subtract; plain for Add)
      const __m128i b_lo = ovr_lo;
      const __m128i b_hi = ovr_hi;

      // result = (dst * inv + b * aeff + 127) / 255
      __m128i res_lo = div255(_mm_add_epi16(
        _mm_add_epi16(_mm_mullo_epi16(dst_lo, inv_lo),
                      _mm_mullo_epi16(b_lo,   aeff_lo)), v_half));
      __m128i res_hi = div255(_mm_add_epi16(
        _mm_add_epi16(_mm_mullo_epi16(dst_hi, inv_hi),
                      _mm_mullo_epi16(b_hi,   aeff_hi)), v_half));

      // Pack 16→8-bit; _mm_packus_epi16 preserves pixel order (no permute needed).
      _mm_storeu_si128((__m128i*)(dstp8 + x * 4), _mm_packus_epi16(res_lo, res_hi));
    }

    // Scalar tail.
    constexpr MagicDiv magic8 = get_magic_div(8);
    for (; x < width; ++x) {
      const uint32_t alpha_src = has_separate_mask
        ? (uint32_t)maskp8[x]
        : (uint32_t)ovrp8[x * 4 + 3];
      const uint32_t ae = (uint32_t)magic_div_rt<uint8_t>(
        alpha_src * (uint32_t)opacity_i + 127u, magic8);
      const uint32_t iv = 255u - ae;
      for (int ch = 0; ch < 4; ++ch) {
        dstp8[x * 4 + ch] = (BYTE)magic_div_rt<uint8_t>(
          (uint32_t)dstp8[x * 4 + ch] * iv + (uint32_t)ovrp8[x * 4 + ch] * ae + 127u, magic8);
      }
    }

    dstp8 += dst_pitch;
    ovrp8 += ovr_pitch;
    if constexpr (has_separate_mask) maskp8 += mask_pitch;
  }
}

// Dispatcher: 8-bit only; 16-bit falls back to C reference.
// has_separate_mask=false → Add  (alpha from ovrp8[x*4+3])
// has_separate_mask=true  → Subtract (alpha from maskp8[x], overlay pre-inverted)
void get_layer_packedrgb_blend_functions_sse41(
  bool has_separate_mask, int bits_per_pixel,
  layer_packedrgb_blend_c_t** fn)
{
  if (bits_per_pixel == 8) {
    *fn = has_separate_mask ? masked_blend_packedrgba_sse41_u8<true>
                            : masked_blend_packedrgba_sse41_u8<false>;
    return;
  }
  get_layer_packedrgb_blend_functions(has_separate_mask, bits_per_pixel, fn);
}

// ---------------------------------------------------------------------------
// SSE4.1 Layer add dispatcher.
// ---------------------------------------------------------------------------
void get_layer_yuv_masked_add_functions_sse41(
  bool is_chroma,
  int placement, VideoInfo& vi, int bits_per_pixel,
  /*out*/masked_merge_fn_t** layer_fn,
  /*out*/masked_merge_float_fn_t** layer_f_fn)
{
  // only masked merge is left here, simple weighted blend is already handled

  // Use the unified (Layer,Overlay) masked merge functions
  MaskMode maskMode = is_chroma ? resolveChromaMaskMode(placement, vi) : MASK444;

  *layer_fn = get_overlay_blend_masked_fn_sse41(is_chroma, maskMode);
  *layer_f_fn = get_overlay_blend_masked_float_fn_sse41(is_chroma, maskMode);

}
