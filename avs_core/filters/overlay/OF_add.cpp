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

// Overlay (c) 2003, 2004 by Klaus Post

#include "overlayfunctions.h"
#include "blend_common.h"

#include <stdint.h>
#include <string.h>
#include <type_traits>

// Tricky optimizer workaround
// cond ? a : b on the bit patterns. GCC turns a ternary that feeds a multiplication back into a
// conditional multiplication, which its default -ftrapping-math keeps as control flow: no vectorizing.
static AVS_FORCEINLINE float select_float_bits(bool cond, float a, float b) {
  uint32_t ia, ib;
  memcpy(&ia, &a, sizeof(float));
  memcpy(&ib, &b, sizeof(float));
  const uint32_t m = 0u - (uint32_t)cond;
  const uint32_t r = (ia & m) | (ib & ~m);
  float result;
  memcpy(&result, &r, sizeof(float));
  return result;
}

void OL_AddImage::DoBlendImageMask(ImageOverlayInternal* base, ImageOverlayInternal* overlay, ImageOverlayInternal* mask) {
  if (rgb) {
    if (of_mode == OF_Add) {
      if (bits_per_pixel == 8)
        BlendImageMask_RGB<uint8_t, true, true>(base, overlay, mask);
      else if (bits_per_pixel <= 16)
        BlendImageMask_RGB<uint16_t, true, true>(base, overlay, mask);
      else if (bits_per_pixel == 32)
        BlendImageMask_RGB_float<true, true>(base, overlay, mask);
    } else {
      // OF_Subtract
      if (bits_per_pixel == 8)
        BlendImageMask_RGB<uint8_t, true, false>(base, overlay, mask);
      else if (bits_per_pixel <= 16)
        BlendImageMask_RGB<uint16_t, true, false>(base, overlay, mask);
      else if (bits_per_pixel == 32)
        BlendImageMask_RGB_float<true, false>(base, overlay, mask);
    }
    return;
  }
  // existing YUV logic
  const bool fullOpacity = (opacity_f == 1.0f); // becomes a template switch
  if(of_mode == OF_Add) {
    if (bits_per_pixel == 8) {
      if (fullOpacity) BlendImageMask<uint8_t, true, true, true>(base, overlay, mask);
      else             BlendImageMask<uint8_t, true, true, false>(base, overlay, mask);
    } else if(bits_per_pixel <= 16) {
      if (fullOpacity) BlendImageMask<uint16_t, true, true, true>(base, overlay, mask);
      else             BlendImageMask<uint16_t, true, true, false>(base, overlay, mask);
    } else if(bits_per_pixel == 32) {
      if (fullOpacity) BlendImageMask_float<true, true, true>(base, overlay, mask);
      else             BlendImageMask_float<true, true, false>(base, overlay, mask);
    }
  }
  else {
    // OF_Subtract
    if (bits_per_pixel == 8) {
      if (fullOpacity) BlendImageMask<uint8_t, true, false, true>(base, overlay, mask);
      else             BlendImageMask<uint8_t, true, false, false>(base, overlay, mask);
    } else if(bits_per_pixel <= 16) {
      if (fullOpacity) BlendImageMask<uint16_t, true, false, true>(base, overlay, mask);
      else             BlendImageMask<uint16_t, true, false, false>(base, overlay, mask);
    } else if(bits_per_pixel == 32) {
      if (fullOpacity) BlendImageMask_float<true, false, true>(base, overlay, mask);
      else             BlendImageMask_float<true, false, false>(base, overlay, mask);
    }
  }
}

void OL_AddImage::DoBlendImage(ImageOverlayInternal* base, ImageOverlayInternal* overlay) {
  if (rgb) {
    if (of_mode == OF_Add) {
      if (bits_per_pixel == 8)
        BlendImageMask_RGB<uint8_t, false, true>(base, overlay, nullptr);
      else if (bits_per_pixel <= 16)
        BlendImageMask_RGB<uint16_t, false, true>(base, overlay, nullptr);
      else if (bits_per_pixel == 32)
        BlendImageMask_RGB_float<false, true>(base, overlay, nullptr);
    }
    else {
      // OF_Subtract
      if (bits_per_pixel == 8)
        BlendImageMask_RGB<uint8_t, false, false>(base, overlay, nullptr);
      else if (bits_per_pixel <= 16)
        BlendImageMask_RGB<uint16_t, false, false>(base, overlay, nullptr);
      else if (bits_per_pixel == 32)
        BlendImageMask_RGB_float<false, false>(base, overlay, nullptr);
    }
    return;
  }
  // existing YUV logic
  const bool fullOpacity = (opacity_f == 1.0f); // becomes a template switch
  if(of_mode == OF_Add) {
    if (bits_per_pixel == 8) {
      if (fullOpacity) BlendImageMask<uint8_t, false, true, true>(base, overlay, nullptr);
      else             BlendImageMask<uint8_t, false, true, false>(base, overlay, nullptr);
    } else if(bits_per_pixel <= 16) {
      if (fullOpacity) BlendImageMask<uint16_t, false, true, true>(base, overlay, nullptr);
      else             BlendImageMask<uint16_t, false, true, false>(base, overlay, nullptr);
    } else if(bits_per_pixel == 32) {
      if (fullOpacity) BlendImageMask_float<false, true, true>(base, overlay, nullptr);
      else             BlendImageMask_float<false, true, false>(base, overlay, nullptr);
    }
  }
  else {
    // OF_Subtract
    if (bits_per_pixel == 8) {
      if (fullOpacity) BlendImageMask<uint8_t, false, false, true>(base, overlay, nullptr);
      else             BlendImageMask<uint8_t, false, false, false>(base, overlay, nullptr);
    } else if(bits_per_pixel <= 16) {
      if (fullOpacity) BlendImageMask<uint16_t, false, false, true>(base, overlay, nullptr);
      else             BlendImageMask<uint16_t, false, false, false>(base, overlay, nullptr);
    } else if(bits_per_pixel == 32) {
      if (fullOpacity) BlendImageMask_float<false, false, true>(base, overlay, nullptr);
      else             BlendImageMask_float<false, false, false>(base, overlay, nullptr);
    }
  }
}

// integer 8-16 bit add/subtract with YUV overshoot handling
template<typename pixel_t, bool maskMode, bool of_add, bool fullOpacity>
void OL_AddImage::BlendImageMask(ImageOverlayInternal* base, ImageOverlayInternal* overlay, ImageOverlayInternal* mask) {

  pixel_t* baseY = reinterpret_cast<pixel_t*>(base->GetPtr(PLANAR_Y));
  pixel_t* baseU = reinterpret_cast<pixel_t*>(base->GetPtr(PLANAR_U));
  pixel_t* baseV = reinterpret_cast<pixel_t*>(base->GetPtr(PLANAR_V));

  pixel_t* ovY = reinterpret_cast<pixel_t*>(overlay->GetPtr(PLANAR_Y));
  pixel_t* ovU = reinterpret_cast<pixel_t*>(overlay->GetPtr(PLANAR_U));
  pixel_t* ovV = reinterpret_cast<pixel_t*>(overlay->GetPtr(PLANAR_V));

  pixel_t* maskY;
  pixel_t* maskU;
  pixel_t* maskV;
  if constexpr (maskMode) {
    maskY = reinterpret_cast<pixel_t*>(mask->GetPtr(PLANAR_Y));
    maskU = reinterpret_cast<pixel_t*>(mask->GetPtr(PLANAR_U));
    maskV = reinterpret_cast<pixel_t*>(mask->GetPtr(PLANAR_V));
  }
  else {
    maskY = nullptr;
    maskU = nullptr;
    maskV = nullptr;
  }

  const int half_pixel_value = (sizeof(pixel_t) == 1) ? 128 : (1 << (bits_per_pixel - 1));
  const int max_pixel_value = (sizeof(pixel_t) == 1) ? 255 : (1 << bits_per_pixel) - 1;
  const int pixel_range = max_pixel_value + 1;
  const int SHIFT = (sizeof(pixel_t) == 1) ? 5 : 5 + (bits_per_pixel - 8);
  const int over32 = (1 << SHIFT); // 32
  const int basepitch = (base->pitch) / sizeof(pixel_t);
  const int overlaypitch = (overlay->pitch) / sizeof(pixel_t);
  int maskpitch;
  if constexpr (maskMode) {
    maskpitch = (mask->pitch) / sizeof(pixel_t);
  }
  else {
    maskpitch = 0;
  }

  const MagicDiv magic = get_magic_div(bits_per_pixel);

  // fullOpacity is a template parameter (based on opacity_f==1.0f check).
  // eff on the max_pixel_value scale, finer opacity granularity at >8-bit than the old
  // fixed 0..256 scale. Unused when fullOpacity+!maskMode.
  const int opacity_i = fullOpacity ? max_pixel_value : (int)(opacity_f * max_pixel_value + 0.5f);
  const int rounder = max_pixel_value / 2;

  /*
    In YUV, "add" and "subtract" are not just per-channel math. The luma (Y) is added/subtracted, but if the result
    overflows (Y > max) or underflows (Y < 0), the chroma (U/V) is "pulled" toward neutral (gray/white) to mimic
    how RGB overbright/underbright behaves visually.

    In RGB, adding two bright colors can result in "white" (all channels maxed). In YUV, if you just add Y, U, and V,
    we can get weird color shifts. The code compensates by blending U/V toward neutral when Y is out of range,
    making the result look more like RGB addition.

    For RGB, a simple per-channel add/subtract (with clamping for 8/16-bit, or no clamping for float) is done.
    The "magic" is only needed for YUV to avoid odd color artifacts. In RGB, overbright naturally becomes white,
    so no special handling is needed.
  */

  int w = base->w();
  int h = base->h();
  for (int y = 0; y < h; y++) {
    // With mask: 3 base + 3 overlay + 3 mask planes: too many pointer pairs for GCC's runtime alias checks, the loop
    // stayed scalar on aarch64. Safe: x reads and writes element x of each plane only.
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC ivdep
#endif
    for (int x = 0; x < w; x++) {
      int Y, U, V;
      // U/V "blend toward neutral" chroma trick.
      // Original: base + (half*(max-eff)+eff*ov)/max - half
      // New: base+eff*(ov-half)/max
      // (ov-half) is signed, but magic_div_rt is uint32_t only, so we split by sign.
      // The split is bit-identical to the unsigned formula's rounding since max_pixel_value is
      // always odd, so rounder (max_pixel_value/2, integer division) is never an exact
      // 0.5-like value on the integer scale, so mask*abs(dU)/max_pixel_value can never be
      // exactly at a tie, where the rounding half addition would to break the equivalence.
      // (Nevertheless, this is an artistic effect, so we would even accept that the
      // original formula differently rounded and yielded an LSB difference, but it's
      // not even the case here).
      int dU = (int)ovU[x] - half_pixel_value;
      int dV = (int)ovV[x] - half_pixel_value;
      int TU, TV;
      if constexpr (fullOpacity && !maskMode) {
        // eff == max_pixel_value
        TU = dU;
        TV = dV;
        if constexpr (of_add) Y = baseY[x] + ovY[x];
        else                  Y = baseY[x] - ovY[x];
      }
      else {
        int effY, effU, effV; // effective masks/opacity/their combinations
        if constexpr (maskMode) {
          if constexpr (fullOpacity) {
            effY = maskY[x];
            effU = maskU[x];
            effV = maskV[x];
          }
          else {
            effY = magic_div_rt<pixel_t>((uint32_t)maskY[x] * (uint32_t)opacity_i + rounder, magic);
            effU = magic_div_rt<pixel_t>((uint32_t)maskU[x] * (uint32_t)opacity_i + rounder, magic);
            effV = magic_div_rt<pixel_t>((uint32_t)maskV[x] * (uint32_t)opacity_i + rounder, magic);
          }
        }
        else {
          effY = opacity_i;
          effU = opacity_i;
          effV = opacity_i;
        }
        TU = (dU >= 0) ? (int)magic_div_rt<pixel_t>((uint32_t)effU * (uint32_t)dU + rounder, magic)
          : -(int)magic_div_rt<pixel_t>((uint32_t)effU * (uint32_t)(-dU) + rounder, magic);
        TV = (dV >= 0) ? (int)magic_div_rt<pixel_t>((uint32_t)effV * (uint32_t)dV + rounder, magic)
          : -(int)magic_div_rt<pixel_t>((uint32_t)effV * (uint32_t)(-dV) + rounder, magic);
        if constexpr (of_add)
          Y = baseY[x] + magic_div_rt<pixel_t>((uint32_t)ovY[x] * (uint32_t)effY + rounder, magic);
        else
          Y = baseY[x] - magic_div_rt<pixel_t>((uint32_t)ovY[x] * (uint32_t)effY + rounder, magic);
      }
      if constexpr (of_add) {
        U = baseU[x] + TU;
        V = baseV[x] + TV;
        // When Y is too high, U and V are blended toward half_pixel_value (neutral chroma),
        // making the color "whiter".
        if (Y > max_pixel_value) {  // Apply overbrightness to UV
          int multiplier = max(0, pixel_range + over32 - Y);  // 0 to 32
          U = ((U * (multiplier)) + (half_pixel_value * (over32 - multiplier))) >> SHIFT;
          V = ((V * (multiplier)) + (half_pixel_value * (over32 - multiplier))) >> SHIFT;
          Y = max_pixel_value;
        }
      }
      else {
        // of_subtract
        U = baseU[x] - TU;
        V = baseV[x] - TV;
        if (Y < 0) {  // Apply superdark to UV
          int multiplier = min(-Y, over32);  // 0 to 32
          U = ((U * (over32 - multiplier)) + (half_pixel_value * (multiplier))) >> SHIFT;
          V = ((V * (over32 - multiplier)) + (half_pixel_value * (multiplier))) >> SHIFT;
          Y = 0;
        }
      }
      baseU[x] = (pixel_t)clamp(U, 0, max_pixel_value);
      baseV[x] = (pixel_t)clamp(V, 0, max_pixel_value);
      baseY[x] = (pixel_t)Y;
    }
    baseY += basepitch;
    baseU += basepitch;
    baseV += basepitch;

    ovY += overlaypitch;
    ovU += overlaypitch;
    ovV += overlaypitch;

    if constexpr (maskMode) {
      maskY += maskpitch;
      maskU += maskpitch;
      maskV += maskpitch;
    }
  }
}

// A float-inside variant of BlendImageMask was benchmarked here.
// Verdict: not adopted. 20-50% slower, only special most complex case
// was quicker by 10%, not worth the code complexity.

// float add/subtract with YUV overshoot handling
template<bool maskMode, bool of_add, bool fullOpacity>
void OL_AddImage::BlendImageMask_float(ImageOverlayInternal* base, ImageOverlayInternal* overlay, ImageOverlayInternal* mask) {
  // specialized pixel_t float images
  // No clamping needed.
  // float range here is supposed to be [0.0f .. 1.0f] for Y, [-0.5f .. 0.5f] for U/V
  // mask is [0.0f .. 1.0f]
  float* baseY = reinterpret_cast<float*>(base->GetPtr(PLANAR_Y));
  float* baseU = reinterpret_cast<float*>(base->GetPtr(PLANAR_U));
  float* baseV = reinterpret_cast<float*>(base->GetPtr(PLANAR_V));

  float* ovY = reinterpret_cast<float*>(overlay->GetPtr(PLANAR_Y));
  float* ovU = reinterpret_cast<float*>(overlay->GetPtr(PLANAR_U));
  float* ovV = reinterpret_cast<float*>(overlay->GetPtr(PLANAR_V));

  float* maskY = maskMode ? reinterpret_cast<float*>(mask->GetPtr(PLANAR_Y)) : nullptr;
  float* maskU = maskMode ? reinterpret_cast<float*>(mask->GetPtr(PLANAR_U)) : nullptr;
  float* maskV = maskMode ? reinterpret_cast<float*>(mask->GetPtr(PLANAR_V)) : nullptr;

  // For float, half_pixel_value is 0.0f, max_pixel_value is 1.0f for Y
  constexpr float half_pixel_value = 0.0f; // intentionally keep it 0.0f for U/V calculation, compiler will optimize away
  constexpr float max_pixel_value = 1.0f; // for Y overshoot check
  constexpr float pixel_range = 1.0f; // mask must be in [0.0f, 1.0f]

  const int basepitch = (base->pitch) / sizeof(float);
  const int overlaypitch = (overlay->pitch) / sizeof(float);
  const int maskpitch = maskMode ? (mask->pitch) / sizeof(float) : 0;

  int w = base->w();
  int h = base->h();

  for (int y = 0; y < h; y++) {
    // With mask: too many pointer pairs for GCC's runtime alias checks, see BlendImageMask.
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC ivdep
#endif
    for (int x = 0; x < w; ++x) {
      float Y, U, V;
      // eff on the 0.0f..1.0f opacity scale (mask == 1.0f when absent)
      float effY, effU, effV;
      if constexpr (maskMode) {
        if constexpr (fullOpacity) {
          effY = maskY[x]; // *1.0f
          effU = maskU[x];
          effV = maskV[x];
        }
        else {
          effY = maskY[x] * opacity_f;
          effU = maskU[x] * opacity_f;
          effV = maskV[x] * opacity_f;
        }
      }
      else {
        effY = fullOpacity ? 1.0f : opacity_f;
        effU = effY;
        effV = effY;
      }

      if constexpr (of_add) {
        Y = baseY[x] + ovY[x] * effY;
        U = baseU[x] + (half_pixel_value * (pixel_range - effU) + effU * ovU[x]) - half_pixel_value;
        V = baseV[x] + (half_pixel_value * (pixel_range - effV) + effV * ovV[x]) - half_pixel_value;
      }
      else {
        Y = baseY[x] - ovY[x] * effY;
        U = baseU[x] - (half_pixel_value * (pixel_range - effU) + effU * ovU[x]) + half_pixel_value;
        V = baseV[x] - (half_pixel_value * (pixel_range - effV) + effV * ovV[x]) + half_pixel_value;
      }

      constexpr float over32 = 32.0f / 255.0f; // ~0.12549f

      // Overshoot handling without a branch: the operands are selected, U * 1.0f / 1.0f is exactly U.
      // GCC did not vectorize the if blocks ("control flow in loop"), see select_float_bits.
      if constexpr (of_add) {
        const bool over = Y > max_pixel_value; // Y > 1.0f
        const float multiplier = max(0.0f, 1.0f + over32 - Y); // 1.12549 - Y, clamp to >=0
        const float mul = select_float_bits(over, multiplier, 1.0f);
        const float div = select_float_bits(over, over32, 1.0f);
        // Blend U/V toward neutral (0.0f)
        U = U * mul / div;
        V = V * mul / div;
        Y = over ? max_pixel_value : Y; // 1.0f
      }
      else {
        const bool under = Y < 0.0f;
        // over32 - min(-Y, over32), written in the form GCC keeps branchless; same value: x - (-y) == x + y,
        // and over32 - over32 == +0.0f == max(0.0f, negative or +0.0f)
        const float factor = max(0.0f, over32 + Y); // over32 to 0
        const float mul = select_float_bits(under, factor, 1.0f);
        const float div = select_float_bits(under, over32, 1.0f);
        U = U * mul / div;
        V = V * mul / div;
        Y = under ? 0.0f : Y;
      }

      // No other clamping for float
      baseU[x] = U;
      baseV[x] = V;
      baseY[x] = Y;
    }
    baseY += basepitch;
    baseU += basepitch;
    baseV += basepitch;

    ovY += overlaypitch;
    ovU += overlaypitch;
    ovV += overlaypitch;

    if constexpr (maskMode) {
      maskY += maskpitch;
      maskU += maskpitch;
      maskV += maskpitch;
    }
  }
}


// integer 8-16-bit RGB add/subtract
template<typename pixel_t, bool maskMode, bool of_add>
void OL_AddImage::BlendImageMask_RGB(ImageOverlayInternal* base, ImageOverlayInternal* overlay, ImageOverlayInternal* mask) {
  int w = base->w();
  int h = base->h();
  const int pixelsize = sizeof(pixel_t);
  const int max_pixel_value = (sizeof(pixel_t) == 1) ? 255 : (1 << bits_per_pixel) - 1;
  auto factor = maskMode ? opacity_f / max_pixel_value : opacity_f;

  for (int p = 0; p < 3; ++p) {
    pixel_t* baseP = reinterpret_cast<pixel_t*>(base->GetPtrByIndex(p));
    pixel_t* ovP = reinterpret_cast<pixel_t*>(overlay->GetPtrByIndex(p));
    pixel_t* maskP = maskMode ? reinterpret_cast<pixel_t*>(mask->GetPtrByIndex(p)) : nullptr;
    int basePitch = base->GetPitchByIndex(p) / pixelsize;
    int overlayPitch = overlay->GetPitchByIndex(p) / pixelsize;
    int maskPitch = maskMode ? (mask->GetPitchByIndex(p) / pixelsize) : 0;

    for (int y = 0; y < h; ++y) {
      for (int x = 0; x < w; ++x) {
        int baseVal = baseP[x];
        int ovVal = ovP[x];

        const float new_mask = maskMode ? (float)reinterpret_cast<const pixel_t*>(maskP)[x] * factor : factor;
        float result;

        if constexpr (of_add)
          result = baseVal + ovVal * new_mask;
        else
          result = baseVal - ovVal * new_mask;

        baseP[x] = (pixel_t)(min(max((int)(result + 0.5f), 0), max_pixel_value));
      }
      baseP += basePitch;
      ovP += overlayPitch;
      if constexpr (maskMode) maskP += maskPitch;
    }
  }
}


// 32-bit float RGB add/subtract
template<bool maskMode, bool of_add>
void OL_AddImage::BlendImageMask_RGB_float(ImageOverlayInternal* base, ImageOverlayInternal* overlay, ImageOverlayInternal* mask) {
  int w = base->w();
  int h = base->h();

  auto factor = maskMode ? opacity_f / 1.0f : opacity_f; // for float, max_pixel_value is 1.0f for masks

  for (int p = 0; p < 3; ++p) {
    float* baseP = reinterpret_cast<float*>(base->GetPtrByIndex(p));
    float* ovP = reinterpret_cast<float*>(overlay->GetPtrByIndex(p));
    float* maskP = maskMode ? reinterpret_cast<float*>(mask->GetPtrByIndex(p)) : nullptr;
    int basePitch = base->GetPitchByIndex(p) / sizeof(float);
    int overlayPitch = overlay->GetPitchByIndex(p) / sizeof(float);
    int maskPitch = maskMode ? (mask->GetPitchByIndex(p) / sizeof(float)) : 0;

    for (int y = 0; y < h; ++y) {
      for (int x = 0; x < w; ++x) {
        float baseVal = baseP[x];
        float ovVal = ovP[x];

        const float new_mask = maskMode ? (float)reinterpret_cast<const float*>(maskP)[x] * factor : factor;
        float result;

        if constexpr (of_add)
          result = baseVal + ovVal * new_mask;
        else
          result = baseVal - ovVal * new_mask;
        baseP[x] = result; // no clamping for float
      }
      baseP += basePitch;
      ovP += overlayPitch;
      if constexpr (maskMode) maskP += maskPitch;
    }
  }
}

