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

#ifndef __Overlay_helpers_h
#define __Overlay_helpers_h

#include <avisynth.h>
#include <avs/minmax.h>
#include <avs/alignment.h>
#include "444convert.h"
#include "blend_common.h"

class ImageOverlayInternal {
private:
  IScriptEnvironment* Env;

  PVideoFrame &frame;

  BYTE* origPlanes[4];
  BYTE* fakePlanes[4];

  int fake_w;
  int fake_h;
  int fake_x_accum; // cumulative luma x offset from all SubFrame calls
  int fake_y_accum; // cumulative luma y offset from all SubFrame calls
  const int _w;
  const int _h;
  const int _bits_per_pixel;
  const bool grey;

  bool return_original;

  const int planes_yuva[4] = { PLANAR_Y, PLANAR_U, PLANAR_V, PLANAR_A };
  const int planes_ya[2]   = { PLANAR_Y, PLANAR_A };
  const int planes_r[4]    = { PLANAR_G, PLANAR_B, PLANAR_R, PLANAR_A };
  const int *planes;

  int planeCount;
  int pitches[4];

  BYTE *maskChroma;
public:
  int xSubSamplingShifts[4];
  int ySubSamplingShifts[4];
  int pitch;
  int pitchUV;
  int pitchA;

  ImageOverlayInternal(
    PVideoFrame &_frame,
    int _inw, int _inh, VideoInfo &_workingVI, bool _hasAlpha, bool _grey, IScriptEnvironment* env) :
    Env(env),
    frame(_frame),
    _w(_inw), _h(_inh), _bits_per_pixel(_workingVI.BitsPerComponent()), grey(_grey), maskChroma(nullptr) {

    planeCount = _workingVI.NumComponents();

    planes = _workingVI.IsYA() ? planes_ya : (_workingVI.IsYUV() || _workingVI.IsYUVA()) ? planes_yuva : planes_r;
    for (int p = 0; p < 4; p++) {
      xSubSamplingShifts[p] = ySubSamplingShifts[p] = 0;
      pitches[p] = 0;
    }

    for (int p = 0; p < planeCount; ++p) {
      const int plane = planes[p];
      xSubSamplingShifts[p] = _workingVI.GetPlaneWidthSubsampling(plane);
      ySubSamplingShifts[p] = _workingVI.GetPlaneHeightSubsampling(plane);
      pitches[p] = frame->GetPitch(plane);
    }

    // Set chroma pitches for greymask mode.
    // All paths now use the luma pitch for chroma planes — no separate chroma buffer.
    if (grey) {
      pitches[1] = pitches[2] = pitches[0];
    }

    // temporarily. so far only blend is ported to arrays
    pitch = pitches[0];
    pitchUV = pitches[1];
    pitchA = pitches[3];

    for (int p = 0; p < planeCount; p++)
      origPlanes[p] = (BYTE*)frame->GetReadPtr(planes[p]);
    if (grey) {
      if (_workingVI.Is420() || _workingVI.Is422()) {
        // Chroma planes alias the luma plane.  BlendImageMask (Overlay Blend/Luma/Chroma —
        // the only modes that reach this branch with use444=false for subsampled YUV)
        // computes placement-correct spatial averages per row into a per-row scratch buffer.
        // xSubSamplingShifts[1/2] remain at their actual values (1) so callers can derive
        // chroma dimensions and advance the luma-resolution mask row pointer correctly.
        origPlanes[1] = origPlanes[2] = origPlanes[0];

        // -------------------------------------------------------------------
        // Historical: former per-frame chroma-mask pre-resampling (removed).
        // ConvertYToYV12Chroma / ConvertYToYV16Chroma were called here once per
        // frame to produce a pre-downsampled chroma-resolution mask plane stored
        // in the maskChroma allocation.  Removed due to several limitations:
        //
        //   1. MPEG1 (centre) chroma placement only.  The converters averaged
        //      horizontally centred pairs of luma pixels, which corresponds to
        //      MPEG1/JPEG placement.  MPEG2 (left-aligned / co-sited) placement
        //      — the default for H.262, H.264, and most broadcast content — was
        //      never supported; callers used the result regardless.
        //
        //   2. SIMD path was not bit-exact with the C path.  The SSE2 variant
        //      used the  avg_epu8( avg_epu8(a,b), avg_epu8(a^1,b^1) )  trick
        //      to approximate unbiased rounding, while the C fallback used plain
        //      (a+b+1)>>1.  Results could differ by ±1 LSB.
        //
        //   3. No YV411 (4:1:1) support.  No ConvertYToYV411Chroma function
        //      existed, so a greymask + YV411 blend in subsampled mode would
        //      have silently produced wrong output (chroma planes pointing at
        //      luma data without any horizontal averaging applied).
        //
        //   4. Only Overlay Blend, Luma, and Chroma with subsampled (420/422)
        //      YUV input and use444=false ever reached this code.  All other
        //      modes are blocked from use444=false with YUV input:
        //        - Multiply, Darken, Lighten, SoftLight, HardLight, Difference,
        //          Exclusion: overlay.cpp throws "cannot specify use444=false
        //          for this overlay mode" at construction time.
        //        - Add, Subtract: use444=false is auto-set only for RGB input
        //          (lines 203-208 of overlay.cpp); for YUV these modes stay on
        //          the use444=true path and their ImageOverlayInternal is always
        //          created with a 4:4:4 working format.
        //      maskChroma (the BYTE* member) and its ~ImageOverlayInternal check
        //      are preserved below so the historical code compiles.
        // -------------------------------------------------------------------
#if 0
        // 4:2:0 former path
        {
          int pixelsize;
          if (_bits_per_pixel == 8) pixelsize = 1;
          else if (_bits_per_pixel <= 16) pixelsize = 2;
          else pixelsize = 4;

          int tmppitch = AlignNumber((_w >> xSubSamplingShifts[1]) * pixelsize, FRAME_ALIGN);
          maskChroma = static_cast<BYTE*>(Env->Allocate(
            tmppitch * (_h >> ySubSamplingShifts[1]), 64, AVS_POOLED_ALLOC));
          pitches[1] = pitches[2] = tmppitch;
          ConvertYToYV12Chroma(maskChroma, origPlanes[0], pitches[1], pitches[0], pixelsize,
            _w >> xSubSamplingShifts[1], _h >> ySubSamplingShifts[1], Env);
          origPlanes[1] = origPlanes[2] = maskChroma;
        }
        // 4:2:2 former path
        {
          int tmppitch = AlignNumber((_w >> xSubSamplingShifts[1]) * pixelsize, FRAME_ALIGN);
          maskChroma = static_cast<BYTE*>(Env->Allocate(
            tmppitch * (_h >> ySubSamplingShifts[1]), 64, AVS_POOLED_ALLOC));
          pitches[1] = pitches[2] = tmppitch;
          ConvertYToYV16Chroma(maskChroma, origPlanes[0], pitches[1], pitches[0], pixelsize,
            _w >> xSubSamplingShifts[1], _h >> ySubSamplingShifts[1], Env);
          origPlanes[1] = origPlanes[2] = maskChroma;
        }
#endif
      }
      else {
        // 444 / RGB / YV411 (no dedicated converter): UV = Y plane pointer.
        origPlanes[1] = origPlanes[2] = origPlanes[0];
        xSubSamplingShifts[1] = xSubSamplingShifts[2] = 0;
        ySubSamplingShifts[1] = ySubSamplingShifts[2] = 0;
      }
    }

    ResetFake();
  }

  __inline int w() { return (return_original) ? _w : fake_w; }
  __inline int h() { return (return_original) ? _h : fake_h; }

  BYTE* GetPtr(int plane) {
    if (!(_w && _h)) {
      _RPT0(1,"Image444: Height or Width is 0");
    }
    switch (plane) {
      case PLANAR_Y:
        return (return_original) ? origPlanes[0] : fakePlanes[0];
      case PLANAR_U:
        return (return_original) ? origPlanes[1] : fakePlanes[1];
      case PLANAR_V:
        return (return_original) ? origPlanes[2] : fakePlanes[2];
      case PLANAR_A:
        return (return_original) ? origPlanes[3] : fakePlanes[3];
    }
    return origPlanes[0];
  }

  int GetPitchByIndex(int planeIndex) {
    if (planeIndex>=0 && planeIndex <=3)
      return pitches[planeIndex];
    return pitches[0]; // Y
  }


  BYTE* GetPtrByIndex(int planeIndex) {
    if (!(_w && _h)) {
      _RPT0(1,"Image444: Height or Width is 0");
    }
    if (planeIndex>=0 && planeIndex <=3)
      return (return_original) ? origPlanes[planeIndex] : fakePlanes[planeIndex];
    return origPlanes[0]; // Y
  }

  /*
  void SetPtr(BYTE* ptr, int plane) {
    if (!(_w && _h)) {
      _RPT0(1,"Image444: Height or Width is 0");
    }
    switch (plane) {
      case PLANAR_Y:
        fake_Y_plane = Y_plane = ptr;
        break;
      case PLANAR_U:
        fake_Y_plane = U_plane = ptr;
        break;
      case PLANAR_V:
        fake_Y_plane = V_plane = ptr;
        break;
      case PLANAR_A:
        fake_A_plane = A_plane = ptr;
        break;
    }
  }
  */
  void SetPtrByIndex(BYTE* ptr, int planeIndex) {
    if (!(_w && _h)) {
      _RPT0(1,"Image444: Height or Width is 0");
    }
    if (planeIndex >= 0 && planeIndex <= 3)
      origPlanes[planeIndex] = fakePlanes[planeIndex] = ptr;
  }


  int GetPitch(int plane) {
    if (!(_w && _h)) {
      _RPT0(1,"Image444: Height or Width is 0");
    }
    switch (plane) {
    case PLANAR_Y:
    case PLANAR_G:
    case PLANAR_B:
    case PLANAR_R:
      return pitch;
    case PLANAR_U:
    case PLANAR_V:
      return pitchUV;
    case PLANAR_A:
      return pitchA;
    }
    return pitch;
  }

  void SubFrame(int x, int y, int new_w, int new_h) {
    new_w = min(new_w, w()-x);
    new_h = min(new_h, h()-y);

    int pixelsize;
    switch(_bits_per_pixel) {
    case 8: pixelsize = 1; break;
    case 32: pixelsize = 4; break;
    default: pixelsize = 2;
    }

    for (int p = 0; p < 3; p++)
    {
      fakePlanes[p] = GetPtrByIndex(p) + (x >> xSubSamplingShifts[p]) * pixelsize + (y >> ySubSamplingShifts[p]) * pitches[p];
    }
    fakePlanes[3] = pitches[3] > 0 ? (GetPtrByIndex(3) + (x >> xSubSamplingShifts[3])*pixelsize + (y >> ySubSamplingShifts[3])*pitches[3]) : nullptr;

    fake_w = new_w;
    fake_h = new_h;
    fake_x_accum += x;
    fake_y_accum += y;
  }

  // Accumulated luma x/y offset from all SubFrame calls since last ResetFake.
  // Used by blend functions to compute the correct chroma column/row count via
  // ceiling division when the starting position is not chroma-grid-aligned.
  int xAccum() const { return return_original ? 0 : fake_x_accum; }
  int yAccum() const { return return_original ? 0 : fake_y_accum; }

  bool IsSizeZero() {
    if (w()<=0) return true;
    if (h()<=0) return true;
    if (!(pitch && origPlanes[0])) return true;
    return false;
  }

  void ReturnOriginal(bool shouldI) {
    return_original = shouldI;
  }

  void ResetFake() {
    return_original = true;
    for (int i = 0; i < 4; i++)
      fakePlanes[i] = origPlanes[i];
    fake_w = _w;
    fake_h = _h;
    fake_x_accum = 0;
    fake_y_accum = 0;
  }

  ~ImageOverlayInternal() {
    if(maskChroma)
      Env->Free(maskChroma);
  }

};


#endif
