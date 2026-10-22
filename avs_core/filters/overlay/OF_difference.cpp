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
#include <cmath>
#include <avs/minmax.h>

#include <stdint.h>
#include <type_traits>

void OL_DifferenceImage::DoBlendImageMask(ImageOverlayInternal* base, ImageOverlayInternal* overlay, ImageOverlayInternal* mask) {
  const bool fullOpacity = (opacity_f == 1.0f); // becomes a template switch
  if (bits_per_pixel == 8) {
    if (fullOpacity) BlendImageMask<uint8_t, true, true>(base, overlay, mask);
    else             BlendImageMask<uint8_t, true, false>(base, overlay, mask);
  } else if(bits_per_pixel <= 16) {
    if (fullOpacity) BlendImageMask<uint16_t, true, true>(base, overlay, mask);
    else             BlendImageMask<uint16_t, true, false>(base, overlay, mask);
  }
  //else if(bits_per_pixel == 32)
  //  BlendImageMask<float>(base, overlay, mask);
}

void OL_DifferenceImage::DoBlendImage(ImageOverlayInternal* base, ImageOverlayInternal* overlay) {
  const bool fullOpacity = (opacity_f == 1.0f); // becomes a template switch
  if (bits_per_pixel == 8) {
    if (fullOpacity) BlendImageMask<uint8_t, false, true>(base, overlay, nullptr);
    else             BlendImageMask<uint8_t, false, false>(base, overlay, nullptr);
  } else if(bits_per_pixel <= 16) {
    if (fullOpacity) BlendImageMask<uint16_t, false, true>(base, overlay, nullptr);
    else             BlendImageMask<uint16_t, false, false>(base, overlay, nullptr);
  }
  //else if(bits_per_pixel == 32)
  //  BlendImage<float>(base, overlay);
}

// no float support
template<typename pixel_t, bool maskMode, bool fullOpacity>
void OL_DifferenceImage::BlendImageMask(ImageOverlayInternal* base, ImageOverlayInternal* overlay, ImageOverlayInternal* mask) {
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

  // "Difference" formula: base+ov-2*min(base,ov) == abs(base-ov)+half_pixel_value.
  const MagicDiv magic = get_magic_div(bits_per_pixel);

  // fullOpacity is a template parameter (based on opacity_f==1.0f check)
  // Formulas below containing opacity_i are hopefully compiler optimized.
  // opacity_i on the max_pixel_value scale gives finer opacity granularity at >8-bit
  // than the old fixed 0..256 scale.
  const int opacity_i = fullOpacity ? max_pixel_value : (int)(opacity_f * max_pixel_value + 0.5f);
  const int rounder = max_pixel_value / 2;

  int w = base->w();
  int h = base->h();

  for (int y = 0; y < h; y++) {
    // With mask: 3 base + 3 overlay + 3 mask planes: too many pointer pairs for GCC's runtime alias checks, the loop
    // stayed scalar on aarch64. Safe: x reads and writes element x of each plane only.
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC ivdep
#endif
    for (int x = 0; x < w; x++) {
      int Y = abs((int)baseY[x] - (int)ovY[x]) + half_pixel_value;
      int U = abs((int)baseU[x] - (int)ovU[x]) + half_pixel_value;
      int V = abs((int)baseV[x] - (int)ovV[x]) + half_pixel_value;
      if constexpr (!(fullOpacity && !maskMode)) {
        // eff on the max_pixel_value scale; mask == max_pixel_value when absent.
        int64_t effY, effU, effV; // effective masks/opacity/their combinations
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
        // Y/U/V can exceed max_pixel_value (abs(diff)+half, up to ~1.5*max_pixel_value)
        // Must stay at int64_t, but no sign problems here unlike at OF_softhardlight
        Y = (int)(((int64_t)Y * effY + (max_pixel_value - effY) * (int64_t)baseY[x] + rounder) / max_pixel_value);
        U = (int)(((int64_t)U * effU + (max_pixel_value - effU) * (int64_t)baseU[x] + rounder) / max_pixel_value);
        V = (int)(((int64_t)V * effV + (max_pixel_value - effV) * (int64_t)baseV[x] + rounder) / max_pixel_value);
      }
      if (Y > max_pixel_value) {  // Apply overbrightness to UV
        int multiplier = max(0, pixel_range + over32 - Y);  // 0 to 32
        U = ((U * (multiplier)) + (half_pixel_value * (over32 - multiplier))) >> SHIFT;
        V = ((V * (multiplier)) + (half_pixel_value * (over32 - multiplier))) >> SHIFT;
        Y = max_pixel_value;
      }
      else if (Y < 0) {  // Apply superdark to UV
        int multiplier = min(-Y, over32);  // 0 to 32
        U = ((U * (over32 - multiplier)) + (half_pixel_value * (multiplier))) >> SHIFT;
        V = ((V * (over32 - multiplier)) + (half_pixel_value * (multiplier))) >> SHIFT;
        Y = 0;
      }
      baseY[x] = (pixel_t)Y;
      baseU[x] = (pixel_t)clamp(U, 0, max_pixel_value);
      baseV[x] = (pixel_t)clamp(V, 0, max_pixel_value);
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

