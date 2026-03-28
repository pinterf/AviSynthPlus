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
#ifndef __Histogram_H__
#define __Histogram_H__

#include <avisynth.h>
#include <vector>
#include "stdint.h"
#include "../convert/convert_matrix.h"
#include <string>

/********************************************************************
********************************************************************/

struct histogram_color2_params {
  enum GraticuleType {
    GRATICULE_OFF=0,
    GRATICULE_ON,
    GRATICULE_AUTO
  };
  GraticuleType graticule_type; // paint graticule on histogram on off auto
  bool targets; // the 6 color bar boxes
  bool axes; // paint axes horizontal + vertical cross
  bool iq; // paint +-I/+Q boxes on histogram
  bool iq_lines; // paint I/Q diagonal lines on histogram
  bool circle; // paint circle on histogram, originally only at "color2"
  bool targets100; // the 6 color bar boxes at 100%
};

class Histogram : public GenericVideoFilter
/**
  * Class to display histogram based on video input
 **/
{
public:
  enum Mode {
    ModeClassic=0,
	ModeLevels,
	ModeColor,
	ModeColor2,
	ModeLuma,
	ModeStereoY8,
	ModeStereo,
	ModeOverlay,
	ModeAudioLevels
  };

  Histogram(PClip _child, Mode _mode, AVSValue _option, int _show_bits, bool _keepsource, bool _markers, const char* _matrix_name, histogram_color2_params _color2_params, IScriptEnvironment* env);
  PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env) override;
  PVideoFrame DrawModeClassic    (int n, IScriptEnvironment* env);
  PVideoFrame DrawModeLevels     (int n, IScriptEnvironment* env);
  PVideoFrame DrawModeColor      (int n, IScriptEnvironment* env);
  PVideoFrame DrawModeColor2     (int n, IScriptEnvironment* env);
  PVideoFrame DrawModeLuma       (int n, IScriptEnvironment* env);
  PVideoFrame DrawModeStereo     (int n, IScriptEnvironment* env);
  PVideoFrame DrawModeOverlay    (int n, IScriptEnvironment* env);
  PVideoFrame DrawModeAudioLevels(int n, IScriptEnvironment* env);

  int __stdcall SetCacheHints(int cachehints, int frame_range) override {
    return cachehints == CACHE_GET_MTMODE ? MT_NICE_FILTER : 0;
  }

  static AVSValue __cdecl Create(AVSValue args, void*, IScriptEnvironment* env);
private:
  Mode mode;
  int deg15c[24], deg15s[24];
  double color2_innerF;
  PClip aud_clip;
  AVSValue option;
  int pixelsize;
  int bits_per_pixel;
  int show_bits; // e.g. levels for 10 bits
  bool keepsource; // return only the Histogram drawing
  bool markers; // paint hazardous YUV area "levels": draw unsafe zone, "classic"

  histogram_color2_params color2_params;

  int origwidth;
  int origheight;

  // read from defaults/props/matrix overrides
  int theMatrix;
  int theColorRange;
  bool full_range;
  // separate out set for rgb target
  int theOutColorRange;
  ConversionMatrix matrix;

  int E167;
  std::vector<uint16_t> exptab;
  void ClassicLUTInit();
  PVideoFrame VectorscopePrelude(
    int n, IScriptEnvironment* env,
    PVideoFrame& src,
    // out:
    int& dst_pitch, int& dst_height,
    int& dst_pitchUV, int& dst_heightUV,
    int& dst_pitchA, int& dst_heightA,
    BYTE*& panel, BYTE*& panelU, BYTE*& panelV, BYTE*& panelA);
};


#endif  // __Histogram_H__
