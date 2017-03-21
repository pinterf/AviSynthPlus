// Avisynth v2.5.  Copyright 2002 Ben Rudiak-Gould et al.
// http://www.avisynth.org

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
#include <cmath>
#include <avs/minmax.h>

#include <stdint.h>
#include <type_traits>

void OL_DifferenceImage::DoBlendImageMask(ImageOverlayInternal* base, ImageOverlayInternal* overlay, ImageOverlayInternal* mask) {
  if (bits_per_pixel == 8)
    BlendImageMask<uint8_t, true>(base, overlay, mask);
  else if(bits_per_pixel <= 16)
    BlendImageMask<uint16_t, true>(base, overlay, mask);
  //else if(bits_per_pixel == 32)
  //  BlendImageMask<float>(base, overlay, mask);
}

void OL_DifferenceImage::DoBlendImage(ImageOverlayInternal* base, ImageOverlayInternal* overlay) {
  if (bits_per_pixel == 8)
    BlendImageMask<uint8_t, false>(base, overlay, nullptr);
  else if(bits_per_pixel <= 16)
    BlendImageMask<uint16_t, false>(base, overlay, nullptr);
  //else if(bits_per_pixel == 32)
  //  BlendImage<float>(base, overlay);
}


template<typename pixel_t, bool maskMode>
void OL_DifferenceImage::BlendImageMask(ImageOverlayInternal* base, ImageOverlayInternal* overlay, ImageOverlayInternal* mask) {
  pixel_t* baseY = reinterpret_cast<pixel_t *>(base->GetPtr(PLANAR_Y));
  pixel_t* baseU = reinterpret_cast<pixel_t *>(base->GetPtr(PLANAR_U));
  pixel_t* baseV = reinterpret_cast<pixel_t *>(base->GetPtr(PLANAR_V));

  pixel_t* ovY = reinterpret_cast<pixel_t *>(overlay->GetPtr(PLANAR_Y));
  pixel_t* ovU = reinterpret_cast<pixel_t *>(overlay->GetPtr(PLANAR_U));
  pixel_t* ovV = reinterpret_cast<pixel_t *>(overlay->GetPtr(PLANAR_V));

  pixel_t* maskY = maskMode ? reinterpret_cast<pixel_t *>(mask->GetPtr(PLANAR_Y)) : nullptr;
  pixel_t* maskU = maskMode ? reinterpret_cast<pixel_t *>(mask->GetPtr(PLANAR_U)) : nullptr;
  pixel_t* maskV = maskMode ? reinterpret_cast<pixel_t *>(mask->GetPtr(PLANAR_V)) : nullptr;

  const int half_pixel_value = (sizeof(pixel_t) == 1) ? 128 : (1 << (bits_per_pixel - 1));
  const int max_pixel_value = (sizeof(pixel_t) == 1) ? 255 : (1 << bits_per_pixel) - 1;
  const int pixel_range = max_pixel_value + 1;
  const int SHIFT  = (sizeof(pixel_t) == 1) ? 5 : 5 + (bits_per_pixel - 8);
  const int MASK_CORR_SHIFT = (sizeof(pixel_t) == 1) ? 8 : bits_per_pixel;
  const int OPACITY_SHIFT  = 8; // opacity always max 0..256
  const int over32 = (1 << SHIFT); // 32
  const int basepitch = (base->pitch) / sizeof(pixel_t);
  const int overlaypitch = (overlay->pitch) / sizeof(pixel_t);
  const int maskpitch = maskMode ? (mask->pitch) / sizeof(pixel_t) : 0;

  // avoid "uint16*uint16 can't get into int32" overflows
  typedef typename std::conditional < sizeof(pixel_t) == 1, int, typename std::conditional < sizeof(pixel_t) == 2, __int64, float>::type >::type result_t;

  int w = base->w();
  int h = base->h();

  if (opacity == 256) {
    for (int y = 0; y < h; y++) {
      for (int x = 0; x < w; x++) {
        int Y = abs((int)baseY[x] - (int)ovY[x]) + half_pixel_value;
        int U = abs((int)baseU[x] - (int)ovU[x]) + half_pixel_value;
        int V = abs((int)baseV[x] - (int)ovV[x]) + half_pixel_value;
        if(maskMode) {
          result_t mY = maskY[x];
          result_t mU = maskU[x];
          result_t mV = maskV[x];
          Y = (int)(((Y*mY) + ((pixel_range - mY)*baseY[x])) >> MASK_CORR_SHIFT);
          U = (int)(((U*mU) + ((pixel_range - mU)*baseU[x])) >> MASK_CORR_SHIFT);
          V = (int)(((V*mV) + ((pixel_range - mV)*baseV[x])) >> MASK_CORR_SHIFT);
        }
        if (Y>max_pixel_value) {  // Apply overbrightness to UV
          int multiplier = max(0,pixel_range + over32 -Y);  // 0 to 32
          U = ((U*(         multiplier)) + (half_pixel_value*(over32-multiplier)))>>SHIFT;
          V = ((V*(         multiplier)) + (half_pixel_value*(over32-multiplier)))>>SHIFT;
          Y = max_pixel_value;
        } else if (Y<0) {  // Apply superdark to UV
          int multiplier = min(-Y,over32);  // 0 to 32
          U = ((U*(over32 - multiplier)) + (half_pixel_value*(       multiplier)))>>SHIFT;
          V = ((V*(over32 - multiplier)) + (half_pixel_value*(       multiplier)))>>SHIFT;
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

      if(maskMode) {
        maskY += maskpitch;
        maskU += maskpitch;
        maskV += maskpitch;
      }
    } // for y
  } else {
    for (int y = 0; y < h; y++) {
      for (int x = 0; x < w; x++) {
        int Y = abs((int)baseY[x] - (int)ovY[x]) + half_pixel_value;
        int U = abs((int)baseU[x] - (int)ovU[x]) + half_pixel_value;
        int V = abs((int)baseV[x] - (int)ovV[x]) + half_pixel_value;
        if(maskMode) {
          result_t  mY = (maskY[x] * opacity) >> OPACITY_SHIFT;
          result_t  mU = (maskU[x] * opacity) >> OPACITY_SHIFT;
          result_t  mV = (maskV[x] * opacity) >> OPACITY_SHIFT;
          Y = (int)(((Y*mY) + ((pixel_range - mY)*baseY[x])) >> MASK_CORR_SHIFT);
          U = (int)(((U*mU) + ((pixel_range - mU)*baseU[x])) >> MASK_CORR_SHIFT);
          V = (int)(((V*mV) + ((pixel_range - mV)*baseV[x])) >> MASK_CORR_SHIFT);
        }
        else {
          Y = ((Y*opacity) + (inv_opacity*baseY[x])) >> OPACITY_SHIFT;
          U = ((U*opacity) + (inv_opacity*baseU[x])) >> OPACITY_SHIFT;
          V = ((V*opacity) + (inv_opacity*baseV[x])) >> OPACITY_SHIFT;
        }
        if (Y>max_pixel_value) {  // Apply overbrightness to UV
          int multiplier = max(0,pixel_range + over32 -Y);  // 0 to 32
          U = ((U*(         multiplier)) + (half_pixel_value*(over32-multiplier)))>>SHIFT;
          V = ((V*(         multiplier)) + (half_pixel_value*(over32-multiplier)))>>SHIFT;
          Y = max_pixel_value;
        } else if (Y<0) {  // Apply superdark to UV
          int multiplier = min(-Y,over32);  // 0 to 32
          U = ((U*(over32 - multiplier)) + (half_pixel_value*(       multiplier)))>>SHIFT;
          V = ((V*(over32 - multiplier)) + (half_pixel_value*(       multiplier)))>>SHIFT;
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

      if(maskMode) {
        maskY += maskpitch;
        maskU += maskpitch;
        maskV += maskpitch;
      }
    } // for x
  } // for y
}

