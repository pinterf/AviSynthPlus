// AviSynth+  Copyright 2026- AviSynth+ Project
// SPDX-License-Identifier: GPL-2.0-or-later

// NEON Layer add dispatcher — mirrors layer_sse41.h / layer_avx2.h.
// Delegates to get_overlay_blend_masked_fn_neon / get_overlay_blend_masked_float_fn_neon
// for both integer and float masked merge paths.
// Subtract is handled by pre-inverting the overlay in Layer::Create.

#ifndef __Layer_NEON_H__
#define __Layer_NEON_H__

#include <avisynth.h>
#include "../../layer.h"          // layer_yuv_add_c_t, layer_yuv_add_f_c_t, PLACEMENT_*
#include "../blend_common.h"      // masked_merge_fn_t, masked_merge_float_fn_t

void get_layer_yuv_masked_add_functions_neon(
  bool is_chroma,
  int placement, VideoInfo& vi, int bits_per_pixel,
  masked_merge_fn_t** layer_fn,
  masked_merge_float_fn_t** layer_f_fn);

void get_layer_planarrgb_add_functions_neon(
  bool chroma, bool hasAlpha, bool blendAlpha, int bits_per_pixel,
  layer_planarrgb_add_c_t** layer_fn,
  layer_planarrgb_add_f_c_t** layer_f_fn);

// Mul / mulovr / lighten / darken: NEON row kernels in this module's copies of the layer.hpp C templates.
void get_layer_yuv_mul_functions_neon(
  bool is_chroma, bool hasAlpha,
  int placement, VideoInfo& vi, int bits_per_pixel,
  layer_yuv_mul_c_t** layer_fn,
  layer_yuv_mul_f_c_t** layer_f_fn);

void get_layer_yuv_lighten_darken_functions_neon(bool isLighten, int placement, VideoInfo& vi, int bits_per_pixel,
  layer_yuv_lighten_darken_c_t** layer_fn,
  layer_yuv_lighten_darken_f_c_t** layer_f_fn);

void get_layer_planarrgb_mul_functions_neon(
  bool chroma, bool hasAlpha, bool blendAlpha, int bits_per_pixel,
  layer_planarrgb_mul_c_t** layer_fn,
  layer_planarrgb_mul_f_c_t** layer_f_fn);

void get_layer_planarrgb_lighten_darken_functions_neon(bool isLighten, bool hasAlpha, bool blendAlpha, int bits_per_pixel,
  layer_planarrgb_lighten_darken_c_t** layer_fn,
  layer_planarrgb_lighten_darken_f_c_t** layer_f_fn);

void get_layer_yuv_mulovr_functions_neon(
  bool has_alpha, int placement, VideoInfo& vi, int bits_per_pixel,
  layer_yuv_mulovr_c_t** layer_fn,
  layer_yuv_mulovr_f_c_t** layer_f_fn);

#endif  // __Layer_NEON_H__
