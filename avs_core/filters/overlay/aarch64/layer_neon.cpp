// AviSynth+  Copyright 2026- AviSynth+ Project
// SPDX-License-Identifier: GPL-2.0-or-later
//
// NEON Layer add/subtract dispatcher.
// Named *_neon.cpp so the CMake handle_arch_flags(NEON) glob assigns
// per-file -march=armv8-a (GCC/Clang) flags to this translation unit.
// Subtract is handled upstream by pre-inverting the overlay in Layer::Create;
// GetFrame never sees Op="Subtract" for paths dispatched here.

#include "../../layer.h"       // layer_yuv_add_c_t/f, PLACEMENT_*, pulls in blend_common.h
#include "layer_neon.h"        // declarations

#include <arm_neon.h>
#include <cstdint>
#include <type_traits>
#include <vector>

#include "blend_common_neon.h"  // masked_merge_neon_dispatch

#include "../../../core/internal.h"  // DISABLE_WARNING_PUSH/POP — needed by layer.hpp

// C template dispatcher: get_layer_yuv_add_functions / get_layer_planarrgb_add_functions.
// static functions — this TU's copy gets -march=armv8-a auto-vectorization (GCC/Clang).
#include "../../layer.hpp"

// ---------------------------------------------------------------------------
// Planar RGB add — NEON per-plane wrappers (mirrors SSE4.1/AVX2 counterparts).
// All planar RGB planes use MASK444. maskp8 is the per-pixel weight.
// chroma=false and float fall back to C templates.

static void layer_planarrgb_add_neon_3plane(
  BYTE** dstp8, const BYTE** ovrp8, const BYTE* maskp8,
  int dst_pitch, int overlay_pitch, int mask_pitch,
  int width, int height, int opacity_i, int bits_per_pixel)
{
  for (int i = 0; i < 3; i++)
    masked_merge_neon_dispatch<MASK444>(
      dstp8[i], ovrp8[i], maskp8,
      dst_pitch, overlay_pitch, mask_pitch,
      width, height, opacity_i, bits_per_pixel);
}

static void layer_planarrgb_add_neon_4plane(
  BYTE** dstp8, const BYTE** ovrp8, const BYTE* maskp8,
  int dst_pitch, int overlay_pitch, int mask_pitch,
  int width, int height, int opacity_i, int bits_per_pixel)
{
  for (int i = 0; i < 4; i++)
    masked_merge_neon_dispatch<MASK444>(
      dstp8[i], ovrp8[i], maskp8,
      dst_pitch, overlay_pitch, mask_pitch,
      width, height, opacity_i, bits_per_pixel);
}

void get_layer_planarrgb_add_functions_neon(
  bool chroma, bool hasAlpha, bool blendAlpha, int bits_per_pixel,
  layer_planarrgb_add_c_t** layer_fn,
  layer_planarrgb_add_f_c_t** layer_f_fn)
{
  if (hasAlpha && chroma && bits_per_pixel != 32) {
    // standard unified masked_merge
    *layer_fn = blendAlpha ? layer_planarrgb_add_neon_4plane : layer_planarrgb_add_neon_3plane;
    return;
  }
  get_layer_planarrgb_add_functions(chroma, hasAlpha, blendAlpha, bits_per_pixel, layer_fn, layer_f_fn);
}

// ---------------------------------------------------------------------------
// NEON Layer YUV masked add dispatcher — mirrors get_layer_yuv_masked_add_functions_sse41.
// Determines MaskMode from format and placement, then delegates to the unified
// get_overlay_blend_masked_fn_neon / get_overlay_blend_masked_float_fn_neon getters.
// Both integer and float paths are covered.
// Subtract is handled by pre-inverting the overlay in Layer::Create.
// ---------------------------------------------------------------------------
void get_layer_yuv_masked_add_functions_neon(
  bool is_chroma,
  int placement, VideoInfo& vi, int bits_per_pixel,
  masked_merge_fn_t**       layer_fn,
  masked_merge_float_fn_t** layer_f_fn)
{
  MaskMode maskMode = is_chroma ? resolveChromaMaskMode(placement, vi) : MASK444;

  *layer_fn   = get_overlay_blend_masked_fn_neon(is_chroma, maskMode);
  *layer_f_fn = get_overlay_blend_masked_float_fn_neon(is_chroma, maskMode);
}
