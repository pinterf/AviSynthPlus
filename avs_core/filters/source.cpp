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


#include "../core/internal.h"
#include "../convert/convert_matrix.h"
#include "../convert/convert_helper.h"
#include "colorbars_const.h"
#include "transform.h"
#ifdef AVS_WINDOWS
#include "AviSource/avi_source.h"
#endif
#include "../convert/convert_planar.h"

#define PI 3.1415926535897932384626433832795
#include <ctime>
#include <cmath>
#include <new>
#include <cassert>
#include <stdint.h>
#include <algorithm>

#define XP_LAMBDA_CAPTURE_FIX(x) (void)(x)

/********************************************************************
********************************************************************/

enum {
    COLOR_MODE_RGB = 0,
    COLOR_MODE_YUV
};

class StaticImage : public IClip {
  const VideoInfo vi;
  const PVideoFrame frame;
  bool parity;

public:
  StaticImage(const VideoInfo& _vi, const PVideoFrame& _frame, bool _parity)
    : vi(_vi), frame(_frame), parity(_parity) {}
  PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env) {
    AVS_UNUSED(n);
    AVS_UNUSED(env);
    return frame;
  }
  void __stdcall GetAudio(void* buf, int64_t start, int64_t count, IScriptEnvironment* env) {
    AVS_UNUSED(start);
    AVS_UNUSED(env);
    memset(buf, 0, (size_t)vi.BytesFromAudioSamples(count));
  }
  const VideoInfo& __stdcall GetVideoInfo() { return vi; }
  bool __stdcall GetParity(int n) { return (vi.IsFieldBased() ? (n&1) : false) ^ parity; }
  int __stdcall SetCacheHints(int cachehints,int frame_range)
  {
    AVS_UNUSED(frame_range);
    switch (cachehints)
    {
    case CACHE_DONT_CACHE_ME:
      return 1;
    case CACHE_GET_MTMODE:
      return MT_NICE_FILTER;
    case CACHE_GET_DEV_TYPE:
       return DEV_TYPE_CPU;
    case CACHE_GET_CHILD_DEV_TYPE:
       return DEV_TYPE_ANY; // any type is ok because this clip does not require child's frames.
    default:
      return 0;
    }
  }
};

// For any frame number, this clip returns the first frame of a child clip .
// This clip makes cache effective and reduce unnecessary frame transfer.
class SingleFrame : public GenericVideoFilter {
public:
  SingleFrame(PClip _child) : GenericVideoFilter(_child) {}
  PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env) { return child->GetFrame(0, env); }
  int __stdcall SetCacheHints(int cachehints, int frame_range)
  {
    switch (cachehints)
    {
    case CACHE_DONT_CACHE_ME:
      return 1;
    case CACHE_GET_MTMODE:
      return MT_NICE_FILTER;
    case CACHE_GET_DEV_TYPE:
      return (child->GetVersion() >= 5) ? child->SetCacheHints(CACHE_GET_DEV_TYPE, 0) : 0;
    default:
      return 0;
    }
  }
};


static PVideoFrame CreateBlankFrame(const VideoInfo& vi, int color, int mode, const int *colors, const float *colors_f, bool color_is_array, IScriptEnvironment* env) {

  if (!vi.HasVideo()) return 0;

  PVideoFrame frame = env->NewVideoFrame(vi);
  // no frame property origin

  // but we set Rec601 (ST170) if YUV
  auto props = env->getFramePropsRW(frame);
  int theMatrix = vi.IsRGB() ? Matrix_e::AVS_MATRIX_RGB : Matrix_e::AVS_MATRIX_ST170_M;
  int theColorRange = vi.IsRGB() ? ColorRange_Compat_e::AVS_COLORRANGE_FULL : ColorRange_Compat_e::AVS_COLORRANGE_LIMITED;
  update_Matrix_and_ColorRange(props, theMatrix, theColorRange, env);

  // RGB 8->16 bit: not << 8 like YUV but 0..255 -> 0..65535 or 0..1023 for 10 bit
  int pixelsize = vi.ComponentSize();
  int bits_per_pixel = vi.BitsPerComponent();
  int max_pixel_value = (1 << bits_per_pixel) - 1;
  auto rgbcolor8to16 = [](uint8_t color8, int max_pixel_value) { return (uint16_t)(color8 * max_pixel_value / 255); };

  // int color holds the "old" 8 bit color values that are scaled automatically to the right bitmap
  // new in avs+: if color_is_array, color values are filled as-is, no conversion or any shift occurs

  if (vi.IsPlanar()) {

    bool isyuvlike = vi.IsYUV() || vi.IsYUVA();

    if (color_is_array) {
          // works from colors or colors_f: as-is
          // color order in the array: RGBA or YUVA
      int planes_y[4] = { PLANAR_Y, PLANAR_U, PLANAR_V, PLANAR_A };
      int planes_r[4] = { PLANAR_R, PLANAR_G, PLANAR_B, PLANAR_A };
      int *planes = isyuvlike ? planes_y : planes_r;

      for (int p = 0; p < vi.NumComponents(); p++)
      {
        int plane = planes[p];
        BYTE *dstp = frame->GetWritePtr(plane);
        int rowsize = frame->GetRowSize(plane);
        int pitch = frame->GetPitch(plane);
        int height = frame->GetHeight(plane);
        switch (pixelsize) {
        case 1: fill_plane<uint8_t>(dstp, height, rowsize, pitch, clamp(colors[p], 0, 0xFF)); break;
        case 2: fill_plane<uint16_t>(dstp, height, rowsize, pitch, clamp(colors[p], 0, (1 << vi.BitsPerComponent()) - 1)); break;
        case 4: fill_plane<float>(dstp, height, rowsize, pitch, colors_f[p]); break;
        }
      }
    }
    else {
      int color_yuv = (mode == COLOR_MODE_YUV) ? color : RGB2YUV_Rec601(color);

      int val_i = 0;

      int planes_y[4] = { PLANAR_Y, PLANAR_U, PLANAR_V, PLANAR_A };
      int planes_r[4] = { PLANAR_G, PLANAR_B, PLANAR_R, PLANAR_A };
      int *planes = isyuvlike ? planes_y : planes_r;

      for (int p = 0; p < vi.NumComponents(); p++)
      {
        int plane = planes[p];
        // color order: ARGB or AYUV
        // (8)-8-8-8 bit color from int parameter
        if (isyuvlike) {
          switch (plane) {
          case PLANAR_A: val_i = (color >> 24) & 0xff; break;
          case PLANAR_Y: val_i = (color_yuv >> 16) & 0xff; break;
          case PLANAR_U: val_i = (color_yuv >> 8) & 0xff; break;
          case PLANAR_V: val_i = color_yuv & 0xff; break;
          }
          if (bits_per_pixel != 32)
            val_i = val_i << (bits_per_pixel - 8);
        }
        else {
         // planar RGB
          switch (plane) {
          case PLANAR_A: val_i = (color >> 24) & 0xff; break;
          case PLANAR_R: val_i = (color >> 16) & 0xff; break;
          case PLANAR_G: val_i = (color >> 8) & 0xff; break;
          case PLANAR_B: val_i = color & 0xff; break;
          }
          if (bits_per_pixel != 32)
            val_i = rgbcolor8to16(val_i, max_pixel_value);
        }


        BYTE *dstp = frame->GetWritePtr(plane);
        int size = frame->GetPitch(plane) * frame->GetHeight(plane);

        switch (pixelsize) {
        case 1:
          memset(dstp, val_i, size);
          break;
        case 2:
          val_i = clamp(val_i, 0, (1 << vi.BitsPerComponent()) - 1);
          std::fill_n((uint16_t *)dstp, size / sizeof(uint16_t), (uint16_t)val_i);
          break; // 2 pixels at a time
        default: // case 4:
          float val_f;
          if (plane == PLANAR_U || plane == PLANAR_V) {
            float shift = 0.0f;
            val_f = float(val_i - 128) / 255.0f + shift; // 32 bit float chroma 128=0.5
          }
          else {
            val_f = float(val_i) / 255.0f;
          }
          std::fill_n((float *)dstp, size / sizeof(float), val_f);
        }
      }
    }
    return frame;
  } // if planar

  BYTE* p = frame->GetWritePtr();
  int size = frame->GetPitch() * frame->GetHeight();

  if (vi.IsYUY2()) {
    int color_yuv =(mode == COLOR_MODE_YUV) ? color : RGB2YUV_Rec601(color);
    if (color_is_array) {
      color_yuv = (clamp(colors[0], 0, max_pixel_value) << 16) | (clamp(colors[1], 0, max_pixel_value) << 8) | (clamp(colors[2], 0, max_pixel_value));
    }
    uint32_t d = ((color_yuv>>16)&255) * 0x010001 + ((color_yuv>>8)&255) * 0x0100 + (color_yuv&255) * 0x01000000;
    for (int i=0; i<size; i+=4)
      *(uint32_t *)(p+i) = d;
  } else if (vi.IsRGB24()) {
    const uint8_t color_b = color_is_array ? clamp(colors[2], 0, max_pixel_value) : (uint8_t)(color & 0xFF);
    const uint8_t color_g = color_is_array ? clamp(colors[1], 0, max_pixel_value) : (uint8_t)(color >> 8);
    const uint8_t color_r = color_is_array ? clamp(colors[0], 0, max_pixel_value) : (uint8_t)(color >> 16);
    const int rowsize = frame->GetRowSize();
    const int pitch = frame->GetPitch();
    for (int y=frame->GetHeight();y>0;y--) {
      for (int i=0; i<rowsize; i+=3) {
        p[i] = color_b;
        p[i+1] = color_g;
        p[i+2] = color_r;
      }
      p+=pitch;
    }
  } else if (vi.IsRGB32()) {
    uint32_t c;
    c = color;
    if (color_is_array) {
      uint32_t r = clamp(colors[0], 0, max_pixel_value);
      uint32_t g = clamp(colors[1], 0, max_pixel_value);
      uint32_t b = clamp(colors[2], 0, max_pixel_value);
      uint32_t a = clamp(colors[3], 0, max_pixel_value);
      c = (a << 24) + (r << 16) + (g << 8) + (b);
    }
    std::fill_n((uint32_t *)p, size / 4, c);
    //for (int i=0; i<size; i+=4)
    //  *(unsigned*)(p+i) = color;
  } else if (vi.IsRGB48()) {
      const uint16_t color_b  = color_is_array ? clamp(colors[2], 0, max_pixel_value) : rgbcolor8to16(color & 0xFF, max_pixel_value);
      uint16_t r = color_is_array ? clamp(colors[0], 0, max_pixel_value) : rgbcolor8to16((color >> 16) & 0xFF, max_pixel_value);
      uint16_t g = color_is_array ? clamp(colors[1], 0, max_pixel_value) : rgbcolor8to16((color >> 8 ) & 0xFF, max_pixel_value);
      const uint32_t color_rg = (r << 16) + (g);
      const int rowsize = frame->GetRowSize() / sizeof(uint16_t);
      const int pitch = frame->GetPitch() / sizeof(uint16_t);
      uint16_t* p16 = reinterpret_cast<uint16_t*>(p);
      for (int y=frame->GetHeight();y>0;y--) {
          for (int i=0; i<rowsize; i+=3) {
              p16[i] = color_b;   // b
              *reinterpret_cast<uint32_t*>(p16+i+1) = color_rg; // gr
          }
          p16 += pitch;
      }
  } else if (vi.IsRGB64()) {
    uint64_t r, g, b, a;
    r = color_is_array ? clamp(colors[0], 0, max_pixel_value) : rgbcolor8to16((color >> 16) & 0xFF, max_pixel_value);
    g = color_is_array ? clamp(colors[1], 0, max_pixel_value) : rgbcolor8to16((color >> 8 ) & 0xFF, max_pixel_value);
    b = color_is_array ? clamp(colors[2], 0, max_pixel_value) : rgbcolor8to16((color      ) & 0xFF, max_pixel_value);
    a = color_is_array ? clamp(colors[3], 0, max_pixel_value) : rgbcolor8to16((color >> 24) & 0xFF, max_pixel_value);
    uint64_t color64 = (a << 48) + (r << 32) + (g << 16) + (b);
    std::fill_n(reinterpret_cast<uint64_t*>(p), size / sizeof(uint64_t), color64);
  }
  return frame;
}


static AVSValue __cdecl Create_BlankClip(AVSValue args, void*, IScriptEnvironment* env) {
  VideoInfo vi_default;
  memset(&vi_default, 0, sizeof(VideoInfo));

  VideoInfo vi = vi_default;

  vi_default.fps_denominator=1;
  vi_default.fps_numerator=24;
  vi_default.height=480;
  vi_default.pixel_type=VideoInfo::CS_BGR32;
  vi_default.num_frames=240;
  vi_default.width=640;
  vi_default.audio_samples_per_second=44100;
  vi_default.nchannels=1;
  vi_default.num_audio_samples=44100*10;
  vi_default.sample_type=SAMPLE_INT16;
  vi_default.SetFieldBased(false);
  bool parity=false;

  AVSValue args0 = args[0];

  // param#12: "clip" overrides
  if (args0.Defined() && args0.ArraySize() == 1 && !args[12].Defined()) {
    vi_default = args0[0].AsClip()->GetVideoInfo();
    parity = args0[0].AsClip()->GetParity(0);
  }
  else if (args0.Defined() && args0.ArraySize() != 0) {
    // when "clip" is defined then beginning clip parameter is forbidden
    env->ThrowError("BlankClip: Only 1 Template clip allowed.");
  }
  else if (args[12].Defined()) {
    // supplied "clip" parameter
    vi_default = args[12].AsClip()->GetVideoInfo();
    parity = args[12].AsClip()->GetParity(0);
  }

  bool defHasVideo = vi_default.HasVideo();
  bool defHasAudio = vi_default.HasAudio();

  // If no default video
  if ( !defHasVideo ) {
    vi_default.fps_numerator=24;
    vi_default.fps_denominator=1;

    vi_default.num_frames = 240;

    // If specify Width    or Height            or Pixel_Type
    if ( args[2].Defined() || args[3].Defined() || args[4].Defined() ) {
      vi_default.width=640;
      vi_default.height=480;
      vi_default.pixel_type=VideoInfo::CS_BGR32;

      vi_default.SetFieldBased(false);
      parity=false;
    }
  }

  // If no default audio but specify Audio_rate or Channels     or Sample_Type
  if ( !defHasAudio && ( args[7].Defined() || args[8].Defined() || args[9].Defined() ) ) {
    vi_default.audio_samples_per_second=44100;
    vi_default.nchannels=1;
    vi_default.sample_type=SAMPLE_INT16;
  }

  vi.width = args[2].AsInt(vi_default.width);
  vi.height = args[3].AsInt(vi_default.height);

  if (args[4].Defined()) {
      int pixel_type = GetPixelTypeFromName(args[4].AsString());
      if(pixel_type == VideoInfo::CS_UNKNOWN)
      {
          env->ThrowError("BlankClip: pixel_type must be \"RGB32\", \"RGB24\", \"YV12\", \"YV24\", \"YV16\", \"Y8\", \n"\
              "\"YUV420P?\",\"YUV422P?\",\"YUV444P?\",\"Y?\",\n"\
              "\"RGB48\",\"RGB64\",\"RGBP\",\"RGBP?\",\n"\
              "\"YV411\" or \"YUY2\"");
      }
      vi.pixel_type = pixel_type;
  }
  else {
    vi.pixel_type = vi_default.pixel_type;
  }

  if (!vi.pixel_type)
    vi.pixel_type = VideoInfo::CS_BGR32;


  double n = args[5].AsDblDef(double(vi_default.fps_numerator));

  if (args[5].Defined() && !args[6].Defined()) {
    unsigned d = 1;
    while (n < 16777216 && d < 16777216) { n*=2; d*=2; }
    vi.SetFPS(int(n+0.5), d);
  } else {
    vi.SetFPS(int(n+0.5), args[6].AsInt(vi_default.fps_denominator));
  }

  vi.image_type = vi_default.image_type; // Copy any field sense from template

  vi.audio_samples_per_second = args[7].AsInt(vi_default.audio_samples_per_second);

  if (args[8].IsBool())
    vi.nchannels = args[8].AsBool() ? 2 : 1; // stereo=True
  else if (args[8].IsInt())
    vi.nchannels = args[8].AsInt();          // channels=2
  else
    vi.nchannels = vi_default.nchannels;

  if (args[9].IsBool())
    vi.sample_type = args[9].AsBool() ? SAMPLE_INT16 : SAMPLE_FLOAT; // sixteen_bit=True
  else if (args[9].IsString()) {
    const char* sample_type_string = args[9].AsString();
    if        (!lstrcmpi(sample_type_string, "8bit" )) {  // sample_type="8Bit"
      vi.sample_type = SAMPLE_INT8;
    } else if (!lstrcmpi(sample_type_string, "16bit")) {  // sample_type="16Bit"
      vi.sample_type = SAMPLE_INT16;
    } else if (!lstrcmpi(sample_type_string, "24bit")) {  // sample_type="24Bit"
      vi.sample_type = SAMPLE_INT24;
    } else if (!lstrcmpi(sample_type_string, "32bit")) {  // sample_type="32Bit"
      vi.sample_type = SAMPLE_INT32;
    } else if (!lstrcmpi(sample_type_string, "float")) {  // sample_type="Float"
      vi.sample_type = SAMPLE_FLOAT;
    } else {
      env->ThrowError("BlankClip: sample_type must be \"8bit\", \"16bit\", \"24bit\", \"32bit\" or \"float\"");
    }
  } else
    vi.sample_type = vi_default.sample_type;

  // If we got an Audio only default clip make the default duration the same
  if (!defHasVideo && defHasAudio) {
    const int64_t denom = Int32x32To64(vi.fps_denominator, vi_default.audio_samples_per_second);
    vi_default.num_frames = int((vi_default.num_audio_samples * vi.fps_numerator + denom - 1) / denom); // ceiling
  }

  vi.num_frames = args[1].AsInt(vi_default.num_frames);

  vi.width++; // cheat HasVideo() call for Audio Only clips
  vi.num_audio_samples = vi.AudioSamplesFromFrames(vi.num_frames);
  vi.width--;

  int color = args[10].AsInt(0);
  int mode = COLOR_MODE_RGB;
  if (args[11].Defined()) {
    if (color != 0) // Not quite 100% test
      env->ThrowError("BlankClip: color and color_yuv are mutually exclusive");
    if (!vi.IsYUV() && !vi.IsYUVA())
      env->ThrowError("BlankClip: color_yuv only valid for YUV color spaces");
    color = args[11].AsInt();
    mode=COLOR_MODE_YUV;
  }

  int colors[4] = { 0 };
  float colors_f[4] = { 0.0 };
  bool color_is_array = false;

  if (args.ArraySize() >= 14) {
    // new colors parameter
    if (args[13].Defined()) // colors
    {
      if (!args[13].IsArray())
        env->ThrowError("BlankClip: colors must be an array");
      int color_count = args[13].ArraySize();
      if (color_count < vi.NumComponents())
        env->ThrowError("BlankClip: 'colors' size %d is less than component count %d", color_count, vi.NumComponents());
      int pixelsize = vi.ComponentSize();
      int bits_per_pixel = vi.BitsPerComponent();
      for (int i = 0; i < color_count; i++) {
        const float c = args[13][i].AsFloatf(0.0);
        if (pixelsize == 4)
          colors_f[i] = c;
        else {
          const int color = (int)(c + 0.5f);
          if (color >= (1 << bits_per_pixel) || color < 0)
            env->ThrowError("BlankClip: invalid color value (%d) for %d-bit video format", color, bits_per_pixel);
          colors[i] = color;
        }
      }
      color_is_array = true;
    }
  }

  PClip clip = new StaticImage(vi, CreateBlankFrame(vi, color, mode, colors, colors_f, color_is_array, env), parity);

  // wrap in OnCPU to support multi devices
  AVSValue arg[2]{ clip, 1 }; // prefetch=1: enable cache but not thread
  return new SingleFrame(env->Invoke("OnCPU", AVSValue(arg, 2)).AsClip());
}


/********************************************************************
********************************************************************/

#if defined(AVS_WINDOWS) && !defined(NO_WIN_GDI)
// in text-overlay.cpp
extern bool GetTextBoundingBox(const char* text, const char* fontname,
  int size, bool bold, bool italic, int align, int* width, int* height, bool utf8);
#endif

extern bool GetTextBoundingBoxFixed(const char* text, const char* fontname, int size, bool bold,
  bool italic, int align, int& width, int& height, bool utf8);


PClip Create_MessageClip(const char* message, int width, int height, int pixel_type, bool shrink,
                         int textcolor, int halocolor, int bgcolor,
                         int fps_numerator, int fps_denominator, int num_frames,
                         bool utf8,
                         IScriptEnvironment* env) {
  int size;
#if defined(AVS_WINDOWS) && !defined(NO_WIN_GDI)
  // MessageClip produces a clip containing a text message.Used internally for error reporting.
  // The font face is "Arial".
  // The font size is between 24 points and 9 points - chosen to fit, if possible,
  // in the width by height clip.
  // The pixeltype is RGB32.

  for (size = 24*8; /*size>=9*8*/; size-=4) {
    int text_width, text_height;
    GetTextBoundingBox(message, "Arial", size, true, false, TA_TOP | TA_CENTER, &text_width, &text_height, utf8);
    text_width = ((text_width>>3)+8+7) & ~7; // mod 8
    text_height = ((text_height>>3)+8+1) & ~1; // mod 2
    if (size <= 9 * 8 || ((width <= 0 || text_width <= width) && (height <= 0 || text_height <= height))) {
      if (width <= 0 || (shrink && width > text_width))
        width = text_width;
      if (height <= 0 || (shrink && height > text_height))
        height = text_height;
      break;
    }
  }
#else
  constexpr int MAX_SIZE = 24; // Terminus 12,14,16,18,20,22,24,28,32
  constexpr int MIN_SIZE = 12;
  for (size = MAX_SIZE; /*size>=9*/; size -= 2) {
    int text_width, text_height;
#ifdef AVS_POSIX
    bool utf8 = true;
#endif
    GetTextBoundingBoxFixed(message, "Terminus", size, true, false, 0 /* align */, text_width, text_height, utf8);
    text_width = (text_width + 8 + 7) & ~7; // mod 8
    text_height = (text_height + 8 + 1) & ~1; // mod 2
    if (size <= MIN_SIZE || ((width <= 0 || text_width <= width) && (height <= 0 || text_height <= height))) {
      if (width <= 0 || (shrink && width > text_width))
        width = text_width;
      if (height <= 0 || (shrink && height > text_height))
        height = text_height;
      break;
    }
  }

  size *= 8; // back to GDI units
#endif

  VideoInfo vi;
  memset(&vi, 0, sizeof(vi));
  vi.width = width;
  vi.height = height;
  vi.pixel_type = pixel_type;
  vi.fps_numerator = fps_numerator > 0 ? fps_numerator : 24;
  vi.fps_denominator = fps_denominator > 0 ? fps_denominator : 1;
  vi.num_frames = num_frames > 0 ? num_frames : 240;

  PVideoFrame frame = CreateBlankFrame(vi, bgcolor, COLOR_MODE_RGB, nullptr, nullptr, false, env);
  env->ApplyMessageEx(&frame, vi, message, size, textcolor, halocolor, bgcolor, utf8);
  PClip clip = new StaticImage(vi, frame, false);

  // wrap in OnCPU to support multi devices
  AVSValue args[2]{ clip, 1 }; // prefetch=1: enable cache but not thread
  return new SingleFrame(env->Invoke("OnCPU", AVSValue(args, 2)).AsClip());
};

AVSValue __cdecl Create_MessageClip(AVSValue args, void*, IScriptEnvironment* env) {
  const bool utf8_default =
#ifdef AVS_POSIX
    true
#else
    false
#endif
    ;

  return Create_MessageClip(args[0].AsString(), args[1].AsInt(-1),
    args[2].AsInt(-1), VideoInfo::CS_BGR32, args[3].AsBool(false),
    args[4].AsInt(0xFFFFFF), args[5].AsInt(0), args[6].AsInt(0),
    -1, -1, -1, // fps_numerator, fps_denominator, num_frames: auto
    args[7].AsBool(utf8_default), // utf8
    env);
}


/*******************************************************************
*
* ColorBarsHD for YUV 444 formats (Rec.709)
*
*********************************************************************/

static void GetYUVRec709fromRGB(double R, double G, double B, double& dY, double& dU, double& dV)
{
  // See 3.2 from https://www.itu.int/dms_pubrec/itu-r/rec/bt/R-REC-BT.709-6-201506-I!!PDF-E.pdf
  double Kr, Kb;
  GetKrKb(AVS_MATRIX_BT709, Kr, Kb);
  dY = Kr * R + (1.0 - Kr - Kb) * G + Kb * B;
  dU = (B - dY) / (2.0 * (1.0 - Kb));
  dV = (R - dY) / (2.0 * (1.0 - Kr));
}

template<typename pixel_t>
static void draw_colorbarsHD_444(uint8_t *pY8, uint8_t *pU8, uint8_t *pV8, int pitchY, int pitchUV, int w, int h, int bits_per_pixel)
{
  pixel_t *pY = reinterpret_cast<pixel_t *>(pY8);
  pixel_t *pU = reinterpret_cast<pixel_t *>(pU8);
  pixel_t *pV = reinterpret_cast<pixel_t *>(pV8);
  pitchY /= sizeof(pixel_t);
  pitchUV /= sizeof(pixel_t);

  const int shift = sizeof(pixel_t) == 4 ? 0 : (bits_per_pixel - 8);

  // Also for float target we make "limited" range
  bits_conv_constants luma, chroma;
  // RGB is source, YUV is destination
  // For RGB source / Y destination (both luma-like):
  const bool full_scale_s = true; // full scale reference
  const bool full_scale_d = false; // narrow range reference
  get_bits_conv_constants(luma, false, full_scale_s, full_scale_d, 32, 32);
  // For UV destination (chroma behavior):
  // Note: we only need dst_span for UV, so we use full_scale_d for both params
  get_bits_conv_constants(chroma, true, full_scale_s, full_scale_d, 32, 32);

  double float_offset = luma.dst_offset;
  double float_scale = luma.mul_factor; // 219.0 / 255.0;
  double float_uv_scale = chroma.mul_factor;

//		Nearest 16:9 pixel exact sizes
//		56*X x 12*Y
//		 728 x  480  ntsc anamorphic
//		 728 x  576  pal anamorphic
//		 840 x  480
//		1008 x  576
//		1288 x  720 <- default
//		1456 x 1080  hd anamorphic
//		1904 x 1080
/*
  ARIB STD-B28  Version 1.0-E1

  *1: 75W/100W/I+: Choice from 75% white, 100% white and +I signal. Avisynth: I+.
  *2: can be changed to any value other than the standard values in accordance with the operation purpose by the user

              |<-------------------------------------------------------- a -------------------------------------------------->|
              |             |<------------------------------------------3/4 a --------------------------------->|             |
              |<---- d ---->|<--- c --->|<--- c --->|<--- c --->|<--- c --->|<--- c --->|<--- c --->|<--- c --->|<---- d ---->|

              +-------------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+-------------+  -----------
              |             |           |           |           |           |           |           |           |             |      ^   ^
              |             |           |           |           |           |           |           |           |             |      |   |
Pattern 1     |     40%     |    75%    |    75%    |    75%    |    75%    |    75%    |    75%    |    75%    |    40%      |      |   |
              |    Grey     |   White   |   Yellow  |   Cyan    |   Green   |  Magenta  |   Red     |   Blue    |    Grey     | 7/12b|   |
              |     *2      |           |           |           |           |           |           |           |    *2       |      |   |
              |             |           |           |           |           |           |           |           |             |      V   |
              +-------------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+-------------+ -------  |
Pattern 2     | 100% Cyan   |75W/100W/I+|                          75% white (chroma set signal)                | 100% blue   | 1/12b|   | b
              +-------------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+-------------+ ------   |
Pattern 3     | 100% Yellow |                                       Y ramp                                      | 100% red    | 1/12b|   |
              +-------------+-----------------+-----------+-----------+-------+----+----+---+---+---+-----------+-------------+ -------  |
              |     *2      |                 |                       |       |    |    |   |   |   |           |      *2     |      ^   |
Pattern 4     |     15%     |        0%       |          100%         |   0%  |-2% | 0  |+2%| 0 |+4%|     0%    |     15%     | 3/12 |   |
              |    Grey     |       Black     |         White         | Black |    |    |   |   |   |   Black   |    Grey     |  b   V   V
              +-------------+-----------------+-----------+-----------+-------+----+----+---+---+---+-----------+-------------+  -----------

              |<---- d ---->|<---- 3/2 c ---->|<--------- 2c -------->|<5/6c->|1/3c|1/3c|1/3|1/3|1/3|<--- c --->|<---- d ---->|

              a:b = 16:9


  2021: SMPTE RP 219-1:2014
  *1: can be changed to any value other than the standard values in accordance with the operation purpose by the user
  *2: 75W/100W/+I/-I: Choice from 75% white, 100% white and +I or -I signal. Avisynth: 100% White.
  *3: Choice from 0% Black or +Q (Left from Y ramp) Avisynth: 0% Black.
  *4: can be changed to any value other than the standard values in accordance with the operation purpose by the user
  *5: Choice from 0% Black, Sub-black valley. Avisynth: 0% Black.
      The sub-black valley signal shall begin at the 0% black level, shall decrease in a linear ramp to the minimum permitted level at the mid-point,
      and shall increase in a linear ramp to the 0% black level at the end of the black bar.
  *6: Choice from 100% White, Super-white Peak. Avisynth: 100% White.
      The super-white peak signal shall begin at the 100% white level, shall increase in a linear ramp to the maximum permitted level at the midpoint, 
      and shall decrease in a linear ramp to the 100% white level at the end of the white bar. 

                            |<-------------------------------------------------------- a -------------------------------------------------->|
              |             |<------------------------------------------3/4 a --------------------------------->|             |
              |<---- d ---->|<--- c --->|<--- c --->|<--- c --->|<--- c --->|<--- c --->|<--- c --->|<--- c --->|<---- d ---->|

              +-------------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+-------------+  -----------
              |             |           |           |           |           |           |           |           |             |      ^   ^
              |             |           |           |           |           |           |           |           |             |      |   |
Pattern 1     |     40%     |    75%    |    75%    |    75%    |    75%    |    75%    |    75%    |    75%    |    40%      |      |   |
              |    Grey     |   White   |   Yellow  |   Cyan    |   Green   |  Magenta  |   Red     |   Blue    |    Grey     | 7/12b|   |
              |     *1      |           |           |           |           |           |           |           |    *1       |      |   |
              |             |           |           |           |           |           |           |           |             |      V   |
              +-------------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+-------------+ -------  |
Pattern 2     | 100% Cyan   |75/100W/I-+|                          75% white (chroma set signal)                | 100% blue   | 1/12b|   | b
              +-------------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+-------------+ ------   |
Pattern 3     | 100% Yellow |0%Blk or +Q|                           Y ramp                          | 100% White| 100% red    | 1/12b|   |
              +-------------+-----------+-----+-----------+-----------+-------+----+----+---+---+---+-----------+-------------+ -------  |
              |     *4      |   0% Black    *5|      100% White     *6|       |    |    |   |   |   |           |      *4     |      ^   |
Pattern 4     |     15%     |0% Blk or SubBlck|100%White/SuperWhtePeak|   0%  |-2% | 0  |+2%| 0 |+4%|     0%    |     15%     | 3/12 |   |
              |    Grey     |   0%  Black     |      100% White       | Black |    |    |   |   |   |   Black   |    Grey     |  b   V   V
              +-------------+-----------------+-----------+-----------+-------+----+----+---+---+---+-----------+-------------+  -----------

              |<---- d ---->|<---- 3/2 c ---->|<--------- 2c -------->|<5/6c->|1/3c|1/3c|1/3|1/3|1/3|<--- c --->|<---- d ---->|

              a:b = 16:9

*/

  int y = 0;

  const int c = (w * 3 + 14) / 28; // 1/7th of 3/4 of width
  const int d = (w - c * 7 + 1) / 2; // remaining 1/8th of width

  const int p4 = (3 * h + 6) / 12; // 3/12th of height
  const int p23 = (h + 6) / 12;  // 1/12th of height
  const int p1 = h - p23 * 2 - p4; // remaining 7/12th of height

  /*
  //               75%  Rec709 -- Grey40 Grey75 Yellow  Cyan   Green Magenta  Red   Blue
  static const BYTE pattern1Y[] = { 104,   180,   168,   145,   133,    63,    51,    28 };
  static const BYTE pattern1U[] = { 128,   128,    44,   147,    63,   193,   109,   212 };
  static const BYTE pattern1V[] = { 128,   128,   136,    44,    52,   204,   212,   120 };
  // 3.7.6: replaced the 8 bit table (inaccurate base for higher bitdepths) with accurate 
  // RGB values with double precision RGB to YUV conversion.
  */

  // Define as double-precision gamma-encoded signal levels (E'R, E'G, E'B).
  // 0.75 = 75% of encoded signal swing, not a linear-light value.
  // Convert to Rec.709 YUV for target bitdepth and limited/full range later.
  static const double pattern1R[] = { 0.4, 0.75, 0.75, 0.00, 0.00, 0.75, 0.75, 0.00 };
  static const double pattern1G[] = { 0.4, 0.75, 0.75, 0.75, 0.75, 0.00, 0.00, 0.00 };
  static const double pattern1B[] = { 0.4, 0.75, 0.00, 0.75, 0.00, 0.75, 0.00, 0.75 };

  // Helper to process and write a pixel based on RGB input
  auto ProcessPixel = [&](double r, double g, double b, int targetX) {
    XP_LAMBDA_CAPTURE_FIX(float_scale);
    XP_LAMBDA_CAPTURE_FIX(float_offset);
    XP_LAMBDA_CAPTURE_FIX(float_uv_scale);
    XP_LAMBDA_CAPTURE_FIX(shift);
    double dY, dU, dV;
    GetYUVRec709fromRGB(r, g, b, dY, dU, dV);

    if constexpr (std::is_same<pixel_t, float>::value) {
      pY[targetX] = (pixel_t)(dY * float_scale + float_offset);
      pU[targetX] = (pixel_t)(dU * float_uv_scale);
      pV[targetX] = (pixel_t)(dV * float_uv_scale);
    }
    else {
      // High-precision calculation for 10/12/16-bit
      pY[targetX] = (pixel_t)(((dY * 219.0 + 16.0) * (1 << shift)) + 0.5);
      pU[targetX] = (pixel_t)(((dU * 224.0 + 128.0) * (1 << shift)) + 0.5);
      pV[targetX] = (pixel_t)(((dV * 224.0 + 128.0) * (1 << shift)) + 0.5);
    }
    };

  // ColorbarsHD produces "limited", and since Avisynth handles "limited" 32 bit float, so we adjust it as well.

  // Pattern 1

  for (; y < p1; ++y) {
    int x = 0;
    // 40% grey
    for (; x < d; ++x) {
      ProcessPixel(pattern1R[0], pattern1G[0], pattern1B[0], x);
    }
    // 75% White, Yellow, Cyan, Green, Magenta, Red, Blue
    for (int i = 1; i < 8; i++) {
      for (int j = 0; j < c; ++j, ++x) {
        ProcessPixel(pattern1R[i], pattern1G[i], pattern1B[i], x);
      }
    }
    for (; x < w; ++x) {
      ProcessPixel(pattern1R[0], pattern1G[0], pattern1B[0], x);
    }
    pY += pitchY; pU += pitchUV; pV += pitchUV;
  }

  /*
  SMPTE RP 219 / EG 1: +I Signal Reference (Rec. 709 / HD)
  * The +I (In-phase) signal is defined by its analog IRE levels:
    R = 41.2545 IRE, G = 16.6946 IRE, B = 0 IRE.
  * Normalized Linear RGB (IRE/100): R: 0.412545, G: 0.166946, B: 0.000000
  -----------------------------------------------------------
  Bit-Depth | Y (Luma)      | U (Cb)        | V (Cr)        |
  -----------------------------------------------------------
  8-bit     | 61   (3D)     | 103  (67)     | 157  (9D)     |
  10-bit    | 245  (0F5)    | 412  (19C)    | 629  (275)    |
  16-bit    | 15707 (3D5B)  | 26368 (6700)  | 40249 (9D39)  |
  -----------------------------------------------------------
  See also https://www.arib.or.jp/english/html/overview/doc/6-STD-B28v1_0-E1.pdf
  and
  Wikipedia (https://en.wikipedia.org/wiki/SMPTE_color_bars) (2026)

  Pre 3.7.6 old table, containing precalculated 8 bit values
  //              100% Rec709       Cyan  Blue Yellow  Red    +I Grey75  White
  static const BYTE pattern23Y[] = { 188,   32,  219,   63,   61,  180,  235 };
  static const BYTE pattern23U[] = { 154,  240,   16,  102,  103,  128,  128 };
  static const BYTE pattern23V[] = {  16,  118,  138,  240,  157,  128,  128 };

  */
  // Pattern 2

  // 0: Cyan100, 1: Blue100, 2: Yellow100, 3: Red100, 4: +I, 5: Grey75, 6: White100
  static const double pattern23R[] = { 0.0, 0.0, 1.0, 1.0, PLUS_I_R_YUV, 0.75, 1.0 };
  static const double pattern23G[] = { 1.0, 0.0, 1.0, 0.0, PLUS_I_G_YUV, 0.75, 1.0 };
  static const double pattern23B[] = { 1.0, 1.0, 0.0, 0.0, PLUS_I_B_YUV, 0.75, 1.0 };

  // For pattern 2 and 3
  auto ProcessBar = [&](int index, int endX, int& currentX) {
    XP_LAMBDA_CAPTURE_FIX(float_scale);
    XP_LAMBDA_CAPTURE_FIX(float_offset);
    XP_LAMBDA_CAPTURE_FIX(float_uv_scale);
    XP_LAMBDA_CAPTURE_FIX(shift);
    double dY, dU, dV;
    GetYUVRec709fromRGB(pattern23R[index], pattern23G[index], pattern23B[index], dY, dU, dV);

    for (; currentX < endX && currentX < w; ++currentX) {
      if constexpr(std::is_same<pixel_t, float>::value) {
        pY[currentX] = (pixel_t)(dY * float_scale + float_offset);
        pU[currentX] = (pixel_t)(dU * float_uv_scale);
        pV[currentX] = (pixel_t)(dV * float_uv_scale);
      }
      else {
        pY[currentX] = (pixel_t)(((dY * 219.0 + 16.0) * (1 << shift)) + 0.5);
        pU[currentX] = (pixel_t)(((dU * 224.0 + 128.0) * (1 << shift)) + 0.5);
        pV[currentX] = (pixel_t)(((dV * 224.0 + 128.0) * (1 << shift)) + 0.5);
      }
    }
    };

  for (; y < p1 + p23; ++y) {
    int x = 0;

    // 1. Left Padding (100% Cyan) - Index 0
    ProcessBar(0, d, x);
    // 2. The +I Bar - Index 4 (+I or Grey75 or White)
    ProcessBar(4, c + d, x);
    // 3. 75% White (Grey75) - Index 5
    ProcessBar(5, c * 7 + d, x);
    // 4. Remaining width (100% Blue) - Index 1
    ProcessBar(1, w, x);

    pY += pitchY; pU += pitchUV; pV += pitchUV;
  }

  // Pattern 3

  for (; y < p1 + p23 * 2; ++y) {
    int x = 0;

    ProcessBar(2, d, x); // 100% Yellow

    // Y ramp section: 0% Black, Ramp, 100% White
    // FIXED in 3.7.6: Y-ramp to conform SMPTE RP 219-1:2014, put 0% Black before and 100% White after
    // Divide the c * 7 area: 
    // - 1x - 0% Black (or optional +Q)
    // - 5x - Ramp
    // - 1x - 100% White

    int rampStartX = x + c;         // End of Black (+Q) step
    int rampEndX = x + (c * 6);     // Start of White step
    int sectionEndX = x + (c * 7);  // End of this whole middle section

    // A. 0% black (or optional +Q) step
    for (; x < rampStartX; ++x) {
      ProcessPixel(0.0, 0.0, 0.0, x);
      // ProcessPixel(PLUS_Q_R_YUV, PLUS_Q_G_YUV, PLUS_Q_B_YUV, x); // For +Q instead of 0% Black
    }

    // B. Y-ramp (0.0 to 1.0) - 5 units wide
    int rampWidth = rampEndX - rampStartX;
    for (int j = 0; x < rampEndX; ++x, ++j) {
      double v = (double)j / (rampWidth - 1);
      ProcessPixel(v, v, v, x); // For a grayscale ramp, R=G=B
    }

    // C. 100% White
    for (; x < sectionEndX; ++x) {
      ProcessPixel(1.0, 1.0, 1.0, x);
    }
    // end of Ramp section

    ProcessBar(3, w, x); // 100% Red
    pY += pitchY; pU += pitchUV; pV += pitchUV;
  }

  // Pattern 4

  // Normalized RGB for Pattern 4: 15% Grey, Black, White, Black, -2%, Black, +2%, Black, +4%, Black
  /* old table, precalculated 8 bit values
  //                             Grey15 Black White Black   -2% Black   +2% Black   +4% Black
  static const BYTE pattern4Y[] = { 49,   16,  235,   16,   12,   16,   20,   16,   25,   16 };
  U and V are 128
  */

  static const double pattern4RGB[] = { 0.15, 0.0, 1.0, 0.0, -0.02, 0.0, 0.02, 0.0, 0.04, 0.0 };
  static const BYTE pattern4W[]     = { 0,    9,   21,  26,  28,    30,  32,   34,  36,   42 }; // in 6th's
  for (; y < h; ++y) {
    int x = 0;

    // 1. Left Padding (15% Grey)
    for (; x < d; ++x) {
      ProcessPixel(pattern4RGB[0], pattern4RGB[0], pattern4RGB[0], x);
    }

    // 2. PLUGE and Bars (Indices 1 through 9)
    for (int i = 1; i <= 9; i++) {
      int endX = d + (pattern4W[i] * c + 3) / 6;
      for (; x < endX && x < w; ++x) {
        ProcessPixel(pattern4RGB[i], pattern4RGB[i], pattern4RGB[i], x);
      }
    }

    // 3. Right Padding (15% Grey)
    for (; x < w; ++x) {
      ProcessPixel(pattern4RGB[0], pattern4RGB[0], pattern4RGB[0], x);
    }

    pY += pitchY; pU += pitchUV; pV += pitchUV;
  }
} // ColorBarsHD

/*******************************************************************
*
* ColorBars for YUV formats (BT.601) and and RGB
*
*********************************************************************/
// See also: http://avisynth.nl/index.php/ColorBars_theory
// and https://avisynthplus.readthedocs.io/en/latest/avisynthdoc/corefilters/colorbars.html

/*
* Integer 8 bit tables not used anymore, replaced by double-precision RGB tables with
* accurate RGB and RGB->YUV conversion for better precision and correctness, especially
* for higher bitdepths.

// Studio RGB constants for ColorBars
static const uint32_t bottom_quarter[] =
// RGB[16..235]     -I     white        +Q     Black     -4% Black     Black     +4% Black     Black
               { 0x10466a, 0xebebeb, 0x481076, 0x101010,  0x070707, 0x101010, 0x191919,  0x101010 }; // Qlum=Ilum=13.4%
static const int two_thirds_to_three_quarters[] =
// RGB[16..235]   Blue     Black  Magenta      Black      Cyan     Black    LtGrey
               { 0x1010b4, 0x101010, 0xb410b4, 0x101010, 0x10b4b4, 0x101010, 0xb4b4b4 };
static const int top_two_thirds[] =
// RGB[16..235] LtGrey    Yellow      Cyan     Green   Magenta       Red      Blue
               { 0xb4b4b4, 0xb4b410, 0x10b4b4, 0x10b410, 0xb410b4, 0xb41010, 0x1010b4 };
*/

// Ground truth gamma-encoded RGB for ColorBars (Rec. ITU-R BT.801-1).
// Normalised limited-range signal levels [0.0..1.0]: 0.0 = code 16, 1.0 = code 235.
// 0.75 = 75% encoded signal swing (E'R/G'B, not linear light).
// 8-bit studio value: (int)(R * 219.0 + 16.0 + 0.5)

// Top 2/3: LtGrey, Yellow, Cyan, Green, Magenta, Red, Blue
static const double top_two_thirdsR[] = { 0.75, 0.75, 0.0,  0.0,  0.75, 0.75, 0.0 };
static const double top_two_thirdsG[] = { 0.75, 0.75, 0.75, 0.75, 0.0,  0.0,  0.0 };
static const double top_two_thirdsB[] = { 0.75, 0.0,  0.75, 0.0,  0.75, 0.0,  0.75 };

// 2/3 to 3/4: Blue, Black, Magenta, Black, Cyan, Black, LtGrey
static const double two_thirds_to_three_quartersR[] = { 0.0,  0.0, 0.75, 0.0, 0.0,  0.0, 0.75 };
static const double two_thirds_to_three_quartersG[] = { 0.0,  0.0, 0.0,  0.0, 0.75, 0.0, 0.75 };
static const double two_thirds_to_three_quartersB[] = { 0.75, 0.0, 0.75, 0.0, 0.75, 0.0, 0.75 };

/*
   BOTTOM QUARTER BAR DEFINITIONS (-I, White, +Q, Black, -4%, Black, +4%, Black)

   ============================================================================
   DERIVATION OF -I AND +Q VALUES
   ============================================================================

   The -I and +Q signals are defined in the YIQ colorspace as pure chroma-axis
   signals with zero luma and 20 IRE saturation (0.2162 normalized):

     -I:  I = -0.2162,  Q = 0
     +Q:  I = 0,        Q = +0.2162

   Converting via the BT.601 UV rotation (Poynton eq. 33, with UV swap):

     -I raw RGB (Y=0):  R = -0.2067,  G = +0.0588,  B = +0.2394
     +Q raw RGB (Y=0):  R = +0.1343,  G = -0.1400,  B = +0.3685

   Both signals contain out-of-range (negative) RGB components. Three
   interpretations exist in the literature:

   ---------------------------------------------------------------------------
   OPTION 1 — Zero-luma, lift to studio black (legacy YUV implementation)
   ---------------------------------------------------------------------------
   Y = 16 (studio black). The most negative RGB component is left negative
   and will encode to a super-black code (0-15 range), or must be clipped
   when rendering as RGB, distorting the color.

   This is a HACK that had to be applied differently for RGB vs YUV:

   For YUV output (bottom_quarterR/G/B_for_YUV):
     No lift applied - use raw zero-luma RGB values
     Result: Y = 16, perfectly preserved chroma-axis definition
     Consequence: RGB contains super-black components (codes < 16)

   For RGB output (legacy AviSynth approach, NOT current implementation):
     Each component individually clamped/adjusted to avoid codes < 16
     Result: RGB codes all ≥ 16, but YUV back-conversion doesn't match
     Consequence: Loss of theoretical purity, inconsistent RGB↔YUV round-trip

   This was the legacy AviSynth ColorBars implementation:
     RGB path used bitmap-derived values:  -I = RGB(16, 70, 106)
     YUV path used zero-luma calculation:  -I = Y16 Cb158 Cr95
     These two specifications are fundamentally incompatible.

   ---------------------------------------------------------------------------
   OPTION 2 — Luma-corrected to studio black (CURRENT RGB IMPLEMENTATION)
   ---------------------------------------------------------------------------
   Luma is raised until the most negative component reaches code 16
   (studio black). The lift calculation:

     For -I: Y_lift = 0.2067 - 16/219 = 0.13364
     For +Q: Y_lift = 0.1400 - 16/219 = 0.06694

   After lifting (bottom_quarterR/G/B arrays, RGB-native):
     -I: R = 16 (studio black), G = 90, B = 130, Y ≈ 77
     +Q: R = 92, G = 16 (studio black), B = 143, Y ≈ 63

   This gives a consistent, broadcast-safe signal for RGB output with all
   components within valid studio range (16-235), but produces different
   YUV values than the legacy zero-luma specification (Y=77/63 vs Y=16).

   We maintain TWO separate ground truth tables to preserve legacy compatibility:

   1. bottom_quarterR/G/B (Option 2):
      - Used for RGB output formats
      - all I and Q codes >= 16
      - Colorimetrically consistent RGB↔YUV conversion

   2. bottom_quarterR/G/B_for_YUV (Option 1 YUV side):
      - Used for YUV output formats
      - Produces exact legacy values: -I Y=16, +Q Y=16
      - Contains out-of-range RGB components (will clip if rendered as RGB)

   This dual-table approach acknowledges the historical reality: the original
   AviSynth ColorBars had two independent specifications that don't convert
   to each other via standard matrix math. The -I and +Q signals were analog
   broadcast test signals (voltage levels), not digital RGB/YUV values, and
   their digital representation requires compromises.

   So we use different linear RGB tables for -I and +Q bars, depending on whether
   the target is RGB or YUV, to best match legacy values in each domain.
   Note that moving I and Q values to provide legal RGB and YUV values is a "hack".
   These values match the legacy Avisynth.
*/

// ===== RGB-NATIVE GROUND TRUTH =====
// For RGB output formats only.
// Luma-corrected: most negative component lifted to studio black (code 16).
// -I: R lifted to code 16  ->  RGB(16, 90, 130) at 8-bit
// +Q: G lifted to code 16  ->  RGB(92, 16, 143) at 8-bit
// Convention: 0.0 = code 16 (studio black), 1.0 = code 235 (studio white)
static const double bottom_quarterR[] = { MINUS_I_R, 1.0, PLUS_Q_R, 0.0, -0.04, 0.0, 0.04, 0.0 };
static const double bottom_quarterG[] = { MINUS_I_G, 1.0, PLUS_Q_G, 0.0, -0.04, 0.0, 0.04, 0.0 };
static const double bottom_quarterB[] = { MINUS_I_B, 1.0, PLUS_Q_B, 0.0, -0.04, 0.0, 0.04, 0.0 };

// ===== YUV-TARGETED RGB GROUND TRUTH =====
// For YUV output formats only.
// When converted via GetYUVBT601fromRGB, produces legacy YUV values:
// -I: Y=16, Cb=158, Cr=95   (zero-luma, pure chroma definition)
// +Q: Y=16, Cb=174, Cr=149  (zero-luma, pure chroma definition)
// Note: Contains out-of-range values (R < 0 for -I, G < 0 for +Q)
//       which will be clamped when rendering RGB formats.
// Convention: 0.0 = code 16 (studio black), 1.0 = code 235 (studio white)
static const double bottom_quarterR_for_YUV[] = { MINUS_I_R_YUV, 1.0, PLUS_Q_R_YUV, 0.0, -0.04, 0.0, 0.04, 0.0 };
static const double bottom_quarterG_for_YUV[] = { MINUS_I_G_YUV, 1.0, PLUS_Q_G_YUV, 0.0, -0.04, 0.0, 0.04, 0.0 };
static const double bottom_quarterB_for_YUV[] = { MINUS_I_B_YUV, 1.0, PLUS_Q_B_YUV, 0.0, -0.04, 0.0, 0.04, 0.0 };

/*******************************************************************
* ColorBars for YUV 
*********************************************************************/

static void GetYUVBT601fromRGB(double R, double G, double B, double& dY, double& dU, double& dV)
{
  // See https://www.itu.int/rec/R-REC-BT.601/en
  double Kr, Kb;
  GetKrKb(AVS_MATRIX_ST170_M, Kr, Kb); // BT601: Kr=0.299, Kb=0.114
  dY = Kr * R + (1.0 - Kr - Kb) * G + Kb * B;
  dU = (B - dY) / (2.0 * (1.0 - Kb));
  dV = (R - dY) / (2.0 * (1.0 - Kr));
}

// BT.601 YUV conversion constants for ColorBars (Rec. ITU-R BT.801-1)
// Ground truth linear RGB -> BT.601 YUV, integer and float limited range output.
// Replaces the old hardcoded 8-bit tables.

// Bar boundaries are computed in chroma coordinates so that color transitions
// always fall on chroma-aligned luma positions (a multiple of the horizontal
// subsampling factor). This satisfies the requirement from Rec. ITU-R BT.801-1
// that transitions occur on chroma-aligned boundaries.
// Note: due to integer rounding, boundary positions may differ by +/-1 luma pixel
// compared to a 4:4:4 or RGB rendering of the same width, which is unavoidable
// when 7 bars do not divide evenly into the frame width.
template<typename pixel_t, bool is420, bool is422, bool is411>
static void draw_colorbars_yuv(uint8_t* pY8, uint8_t* pU8, uint8_t* pV8, int pitchY, int pitchUV, int w, int h, int bits_per_pixel)
{
  pixel_t* pY = reinterpret_cast<pixel_t*>(pY8);
  pixel_t* pU = reinterpret_cast<pixel_t*>(pU8);
  pixel_t* pV = reinterpret_cast<pixel_t*>(pV8);
  pitchY /= sizeof(pixel_t);
  pitchUV /= sizeof(pixel_t);

  const int shift = sizeof(pixel_t) == 4 ? 0 : (bits_per_pixel - 8);

  // Pre-compute conversion constants for float limited range,
  // using the same centralized function as ColorbarsHD.

  // Also for float target we make "limited" range
  bits_conv_constants luma, chroma;
  // RGB is source, YUV is destination
  // For RGB source / Y destination (both luma-like):
  const bool full_scale_s = true; // full scale reference
  const bool full_scale_d = false; // narrow range reference
  get_bits_conv_constants(luma, false, full_scale_s, full_scale_d, 32, 32);
  // For UV destination (chroma behavior):
  // Note: we only need dst_span for UV, so we use full_scale_d for both params
  get_bits_conv_constants(chroma, true, full_scale_s, full_scale_d, 32, 32);

  double float_offset = luma.dst_offset;
  double float_scale = luma.mul_factor; // 219.0 / 255.0;
  double float_uv_scale = chroma.mul_factor;

  // Convert one RGB triplet to a YUV pixel triplet at target bit depth.
  struct YUV3 { pixel_t y, u, v; };

  // Helper to process and write a pixel based on RGB input
  // Convention: encoded signal levels; 0.0 = code 16 (studio black), 1.0 = code 235 (studio white)

  auto make_yuv = [&](double r, double g, double b) -> YUV3 {
    XP_LAMBDA_CAPTURE_FIX(float_scale);
    XP_LAMBDA_CAPTURE_FIX(float_offset);
    XP_LAMBDA_CAPTURE_FIX(float_uv_scale);
    XP_LAMBDA_CAPTURE_FIX(shift);
    double dY, dU, dV;
    GetYUVBT601fromRGB(r, g, b, dY, dU, dV);

    if constexpr (std::is_same<pixel_t, float>::value) {
      return {
        (pixel_t)(dY * float_scale + float_offset),
        (pixel_t)(dU * float_uv_scale),
        (pixel_t)(dV * float_uv_scale)
      };
    }
    else {
      // High-precision calculation for 10/12/16-bit
      return {
        (pixel_t)(((dY * 219.0 + 16.0) * (1 << shift)) + 0.5),
        (pixel_t)(((dU * 224.0 + 128.0) * (1 << shift)) + 0.5),
        (pixel_t)(((dV * 224.0 + 128.0) * (1 << shift)) + 0.5)
      };
    }
    };

  // Pre-compute all bar entries from ground truth RGB tables.
  YUV3 bq[8], ttq[7], ttt[7];
  for (int i = 0; i < 8; ++i)
    bq[i] = make_yuv(bottom_quarterR_for_YUV[i], bottom_quarterG_for_YUV[i], bottom_quarterB_for_YUV[i]);
  for (int i = 0; i < 7; ++i)
    ttq[i] = make_yuv(two_thirds_to_three_quartersR[i], two_thirds_to_three_quartersG[i], two_thirds_to_three_quartersB[i]);
  for (int i = 0; i < 7; ++i)
    ttt[i] = make_yuv(top_two_thirdsR[i], top_two_thirdsG[i], top_two_thirdsB[i]);

  // Write luma for one chroma-sample position x.
  // For subsampled formats, each chroma x covers multiple luma pixels.
  // For 444, chromaX == lumaX directly.
  auto write_luma = [&](int x, pixel_t yval) {
    XP_LAMBDA_CAPTURE_FIX(pitchY);
    if constexpr (is420)
      pY[x * 2 + 0] = pY[x * 2 + 1] = pY[x * 2 + pitchY] = pY[x * 2 + 1 + pitchY] = yval;
    else if constexpr (is422)
      pY[x * 2 + 0] = pY[x * 2 + 1] = yval;
    else if constexpr (is411)
      pY[x * 4 + 0] = pY[x * 4 + 1] = pY[x * 4 + 2] = pY[x * 4 + 3] = yval;
    else // 444
      pY[x] = yval;
    };

  auto write_yuv = [&](int x, const YUV3& c) {
    write_luma(x, c.y);
    pU[x] = c.u;
    pV[x] = c.v;
    };

  // For subsampled formats the chroma plane is narrower/shorter.
  // We iterate in chroma coordinates; write_luma expands to luma coordinates.
  int wUV = w;
  int hUV = h;
  if constexpr (is420 || is422) wUV >>= 1;
  if constexpr (is411)          wUV >>= 2;
  if constexpr (is420)          hUV >>= 1;

  int y = 0;

  // Top 2/3
  for (; y * 3 < hUV * 2; ++y) {
    int x = 0;
    for (int i = 0; i < 7; ++i)
      for (; x < (wUV * (i + 1) + 3) / 7; ++x)
        write_yuv(x, ttt[i]);
    if constexpr (is420)
      pY += pitchY * 2;
    else
      pY += pitchY;
    pU += pitchUV; pV += pitchUV;
  }

  // Middle band (2/3 to 3/4)
  for (; y * 4 < hUV * 3; ++y) {
    int x = 0;
    for (int i = 0; i < 7; ++i)
      for (; x < (wUV * (i + 1) + 3) / 7; ++x)
        write_yuv(x, ttq[i]);
    if constexpr (is420)
      pY += pitchY * 2;
    else
      pY += pitchY;
    pU += pitchUV; pV += pitchUV;
  }

  // Bottom quarter
  for (; y < hUV; ++y) {
    int x = 0;
    for (int i = 0; i < 4; ++i)
      for (; x < (wUV * (i + 1) * 5 + 14) / 28; ++x)
        write_yuv(x, bq[i]);
    for (int j = 4; j < 7; ++j)
      for (; x < (wUV * (j + 12) + 10) / 21; ++x)
        write_yuv(x, bq[j]);
    for (; x < wUV; ++x)
      write_yuv(x, bq[7]);
    if constexpr (is420)
      pY += pitchY * 2;
    else
      pY += pitchY;
    pU += pitchUV; pV += pitchUV;
  }
}

/*******************************************************************
* ColorBars for RGB (packed 32/64, 24/48, planar RGB)
*********************************************************************/

// Convert normalised linear RGB [0.0..1.0] to integer studio/limited RGB at any bit depth.
// Limited range: black = 16 << (bpp-8), white = 235 << (bpp-8)
// Values outside [0.0..1.0] (e.g. PLUGE -4%, +4%) are handled naturally.
static int studio_rgb_to_integer(double value, int bits_per_pixel)
{
  const int offset = 16 << (bits_per_pixel - 8);
  const int range = 219 << (bits_per_pixel - 8);
  return (int)(value * range + offset + 0.5);
}

template<typename pixel_t>
static void draw_colorbars_rgb3264(uint8_t* p8, int pitch, int w, int h)
{
  typedef typename std::conditional<sizeof(pixel_t) == 2, uint64_t, uint32_t>::type internal_pixel_t;
  internal_pixel_t* p = reinterpret_cast<internal_pixel_t*>(p8);
  pitch /= sizeof(pixel_t);

  // Pre-compute packed pixel values from ground truth double tables at target bit depth.
  // Pack order in uint32: 0x00RRGGBB, in uint64: RR(16)GG(16)BB(16) (alpha/padding zero)
  // RGB32/64 pixel layout (bottom byte = B, then G, then R, then pad/alpha)
  auto make_pixel = [&](double r, double g, double b) -> internal_pixel_t {
    if constexpr (sizeof(pixel_t) == 1) {
      // RGB32: 8-bit per channel, packed as 0x00RRGGBB
      uint32_t ri = (uint32_t)studio_rgb_to_integer(r, 8);
      uint32_t gi = (uint32_t)studio_rgb_to_integer(g, 8);
      uint32_t bi = (uint32_t)studio_rgb_to_integer(b, 8);
      return (internal_pixel_t)((ri << 16) | (gi << 8) | bi);
    }
    else {
      // RGB64: 16-bit per channel
      uint64_t ri = (uint64_t)studio_rgb_to_integer(r, 16);
      uint64_t gi = (uint64_t)studio_rgb_to_integer(g, 16);
      uint64_t bi = (uint64_t)studio_rgb_to_integer(b, 16);
      return (internal_pixel_t)((ri << 32) | (gi << 16) | bi);
    }
    };

  // Pre-compute all entries (bottom->top scan order matches original)
  internal_pixel_t bq[8], ttq[7], ttt[7];
  for (int i = 0; i < 8; ++i)
    bq[i] = make_pixel(bottom_quarterR[i], bottom_quarterG[i], bottom_quarterB[i]);
  for (int i = 0; i < 7; ++i)
    ttq[i] = make_pixel(two_thirds_to_three_quartersR[i], two_thirds_to_three_quartersG[i], two_thirds_to_three_quartersB[i]);
  for (int i = 0; i < 7; ++i)
    ttt[i] = make_pixel(top_two_thirdsR[i], top_two_thirdsG[i], top_two_thirdsB[i]);

  // note we go bottom->top
  int y = 0;
  for (; y < h / 4; ++y) {
    int x = 0;
    for (int i = 0; i < 4; ++i)
      for (; x < (w * (i + 1) * 5 + 14) / 28; ++x)
        p[x] = bq[i];
    for (int j = 4; j < 7; ++j)
      for (; x < (w * (j + 12) + 10) / 21; ++x)
        p[x] = bq[j];
    for (; x < w; ++x)
      p[x] = bq[7];
    p += pitch;
  }
  for (; y < h / 3; ++y) {
    int x = 0;
    for (int i = 0; i < 7; ++i)
      for (; x < (w * (i + 1) + 3) / 7; ++x)
        p[x] = ttq[i];
    p += pitch;
  }
  for (; y < h; ++y) {
    int x = 0;
    for (int i = 0; i < 7; ++i)
      for (; x < (w * (i + 1) + 3) / 7; ++x)
        p[x] = ttt[i];
    p += pitch;
  }
}


template<typename pixel_t>
static void draw_colorbars_rgb2448(uint8_t* p8, int pitch, int w, int h)
{
  pixel_t* p = reinterpret_cast<pixel_t*>(p8);
  pitch /= sizeof(pixel_t);

  // Pre-computed triplets from ground truth double tables
  struct RGB3 { pixel_t r, g, b; };

  auto make_rgb3 = [&](double r, double g, double b) -> RGB3 {
    return {
      (pixel_t)studio_rgb_to_integer(r, sizeof(pixel_t) == 1 ? 8 : 16),
      (pixel_t)studio_rgb_to_integer(g, sizeof(pixel_t) == 1 ? 8 : 16),
      (pixel_t)studio_rgb_to_integer(b, sizeof(pixel_t) == 1 ? 8 : 16)
    };
    };

  // Pre-compute all entries (bottom->top scan order matches original)
  RGB3 bq[8], ttq[7], ttt[7];
  for (int i = 0; i < 8; ++i)
    bq[i] = make_rgb3(bottom_quarterR[i], bottom_quarterG[i], bottom_quarterB[i]);
  for (int i = 0; i < 7; ++i)
    ttq[i] = make_rgb3(two_thirds_to_three_quartersR[i], two_thirds_to_three_quartersG[i], two_thirds_to_three_quartersB[i]);
  for (int i = 0; i < 7; ++i)
    ttt[i] = make_rgb3(top_two_thirdsR[i], top_two_thirdsG[i], top_two_thirdsB[i]);

  auto write_pixel = [&](int x, const RGB3& c) {
    p[x * 3 + 0] = c.b;  // RGB24/48 memory layout: B, G, R
    p[x * 3 + 1] = c.g;
    p[x * 3 + 2] = c.r;
    };

  // note we go bottom->top
  int y = 0;
  for (; y < h / 4; ++y) {
    int x = 0;
    for (int i = 0; i < 4; ++i)
      for (; x < (w * (i + 1) * 5 + 14) / 28; ++x)
        write_pixel(x, bq[i]);
    for (int j = 4; j < 7; ++j)
      for (; x < (w * (j + 12) + 10) / 21; ++x)
        write_pixel(x, bq[j]);
    for (; x < w; ++x)
      write_pixel(x, bq[7]);
    p += pitch;
  }
  for (; y < h / 3; ++y) {
    int x = 0;
    for (int i = 0; i < 7; ++i)
      for (; x < (w * (i + 1) + 3) / 7; ++x)
        write_pixel(x, ttq[i]);
    p += pitch;
  }
  for (; y < h; ++y) {
    int x = 0;
    for (int i = 0; i < 7; ++i)
      for (; x < (w * (i + 1) + 3) / 7; ++x)
        write_pixel(x, ttt[i]);
    p += pitch;
  }
}

template<typename pixel_t>
static void draw_colorbars_rgbp(uint8_t* pR8, uint8_t* pG8, uint8_t* pB8, int pitch, int w, int h, int bits_per_pixel)
{
  pixel_t* pR = reinterpret_cast<pixel_t*>(pR8);
  pixel_t* pG = reinterpret_cast<pixel_t*>(pG8);
  pixel_t* pB = reinterpret_cast<pixel_t*>(pB8);
  pitch /= sizeof(pixel_t);

  struct RGB3 { pixel_t r, g, b; };

  bits_conv_constants rgb_luma_f;
  // source: full range [0..1] RGB, destination: limited range float
  // rgb_luma_f.mul_factor = 219.0/255.0
  // rgb_luma_f.dst_offset = 16.0/255.0
  get_bits_conv_constants(rgb_luma_f, false /*use_chroma*/, true /*fulls*/, false/*fulld*/, 32, 32);

  auto make_rgb3 = [&](double r, double g, double b) -> RGB3 {

    if constexpr(std::is_same<pixel_t, float>::value) {
      return {
        (float)(r * rgb_luma_f.mul_factor + rgb_luma_f.dst_offset),
        (float)(g * rgb_luma_f.mul_factor + rgb_luma_f.dst_offset),
        (float)(b * rgb_luma_f.mul_factor + rgb_luma_f.dst_offset)
      };
    }
    else {
      return {
        (pixel_t)studio_rgb_to_integer(r, bits_per_pixel),
        (pixel_t)studio_rgb_to_integer(g, bits_per_pixel),
        (pixel_t)studio_rgb_to_integer(b, bits_per_pixel)
      };
    }
    };

  RGB3 bq[8], ttq[7], ttt[7];
  for (int i = 0; i < 8; ++i)
    bq[i] = make_rgb3(bottom_quarterR[i], bottom_quarterG[i], bottom_quarterB[i]);
  for (int i = 0; i < 7; ++i)
    ttq[i] = make_rgb3(two_thirds_to_three_quartersR[i], two_thirds_to_three_quartersG[i], two_thirds_to_three_quartersB[i]);
  for (int i = 0; i < 7; ++i)
    ttt[i] = make_rgb3(top_two_thirdsR[i], top_two_thirdsG[i], top_two_thirdsB[i]);

  auto write_pixel = [&](int x, const RGB3& c) {
    pR[x] = c.r;
    pG[x] = c.g;
    pB[x] = c.b;
    };

  // Planar RGB is top-to-bottom natively, no bottom-up workaround needed.
  // layout top->bottom: top_two_thirds, two_thirds_to_three_quarters, bottom_quarter
  int y = 0;

  // Top 2/3
  for (; y * 3 < h * 2; ++y) {
    int x = 0;
    for (int i = 0; i < 7; ++i)
      for (; x < (w * (i + 1) + 3) / 7; ++x)
        write_pixel(x, ttt[i]);
    pR += pitch; pG += pitch; pB += pitch;
  }

  // Middle band (2/3 to 3/4)
  for (; y * 4 < h * 3; ++y) {
    int x = 0;
    for (int i = 0; i < 7; ++i)
      for (; x < (w * (i + 1) + 3) / 7; ++x)
        write_pixel(x, ttq[i]);
    pR += pitch; pG += pitch; pB += pitch;
  }

  // Bottom quarter
  for (; y < h; ++y) {
    int x = 0;
    for (int i = 0; i < 4; ++i)
      for (; x < (w * (i + 1) * 5 + 14) / 28; ++x)
        write_pixel(x, bq[i]);
    for (int j = 4; j < 7; ++j)
      for (; x < (w * (j + 12) + 10) / 21; ++x)
        write_pixel(x, bq[j]);
    for (; x < w; ++x)
      write_pixel(x, bq[7]);
    pR += pitch; pG += pitch; pB += pitch;
  }
}

class ColorBars : public IClip {
  VideoInfo vi;
  PVideoFrame frame;
  SFLOAT *audio;
  unsigned nsamples;
  bool staticframes; // P.F. false: re-draw each frame. Defaults to true (one pre-computed static frame is served).

  enum { Hz = 440 } ;

public:

  ~ColorBars() {
    delete audio;
  }

  ColorBars(int w, int h, const char* pixel_type, bool _staticframes, int type, IScriptEnvironment* env) {
    memset(&vi, 0, sizeof(VideoInfo));
    staticframes = _staticframes;
    vi.width = w;
    vi.height = h;
    vi.fps_numerator = 30000;
    vi.fps_denominator = 1001;
    vi.num_frames = 107892;   // 1 hour
    int i_pixel_type = GetPixelTypeFromName(pixel_type);
    vi.pixel_type = i_pixel_type;
    int bits_per_pixel = vi.BitsPerComponent();

    const bool IsColorbars = (type == 0);
    const bool IsColorbarsHD = (type == 1);

    if (IsColorbarsHD) { // ColorbarsHD
        if (!vi.Is444())
          env->ThrowError("ColorBarsHD: pixel_type must be \"YV24\" or other 4:4:4 video format");
    }
    else if (vi.IsRGB32() || vi.IsRGB64() || vi.IsRGB24() || vi.IsRGB48()) {
      // no special check
    }
    else if (vi.IsRGB() && vi.IsPlanar()) { // planar RGB
      // no special check
    }
    else if (vi.IsYUY2()) { // YUY2
        if (w & 1)
          env->ThrowError("ColorBars: YUY2 width must be even!");
    }
    else if (vi.Is420()) { // 4:2:0
        if ((w & 1) || (h & 1))
          env->ThrowError("ColorBars: for 4:2:0 both height and width must be even!");
    }
    else if (vi.Is422()) { // 4:2:2
      if (w & 1)
        env->ThrowError("ColorBars: for 4:2:2 width must be even!");
    }
    else if (vi.IsYV411()) { // 4:1:1
      if (w & 3)
        env->ThrowError("ColorBars: for 4:1:1 width must be divisible by 4!");
    }
    else if (vi.Is444()) { // 4:4:4
        // no special check
    }
    else {
      env->ThrowError("ColorBars: this pixel_type not supported");
    }
    vi.sample_type = SAMPLE_FLOAT;
    vi.nchannels = 2;
    vi.audio_samples_per_second = 48000;
    vi.num_audio_samples=vi.AudioSamplesFromFrames(vi.num_frames);

    frame = env->NewVideoFrame(vi);

    uint32_t* p = (uint32_t *)frame->GetWritePtr();

    // set basic frame properties
    auto props = env->getFramePropsRW(frame);
    int theMatrix;
    int theColorRange;
    if (IsColorbarsHD) {
      // ColorBarsHD 444 only
      theMatrix = Matrix_e::AVS_MATRIX_BT709;
      theColorRange = ColorRange_Compat_e::AVS_COLORRANGE_LIMITED;
    }
    else {
      // IsColorBars
      // ColorBars can be rgb or yuv
      theMatrix = vi.IsRGB() ? Matrix_e::AVS_MATRIX_RGB : Matrix_e::AVS_MATRIX_ST170_M;
      // Studio RGB: limited!
      theColorRange = vi.IsRGB() ? ColorRange_Compat_e::AVS_COLORRANGE_LIMITED : ColorRange_Compat_e::AVS_COLORRANGE_LIMITED;
    }
    update_Matrix_and_ColorRange(props, theMatrix, theColorRange, env);

  // HD colorbars arib_std_b28
  // Rec709 yuv values
  if (IsColorbarsHD) { // ColorbarsHD
    BYTE* pY = (BYTE*)frame->GetWritePtr(PLANAR_Y);
    BYTE* pU = (BYTE*)frame->GetWritePtr(PLANAR_U);
    BYTE* pV = (BYTE*)frame->GetWritePtr(PLANAR_V);
    const int pitchY  = frame->GetPitch(PLANAR_Y);
    const int pitchUV = frame->GetPitch(PLANAR_U);

    if (bits_per_pixel == 8)
      draw_colorbarsHD_444<uint8_t>(pY, pU, pV, pitchY, pitchUV, w, h, 8);
    else if (bits_per_pixel <= 16)
      draw_colorbarsHD_444<uint16_t>(pY, pU, pV, pitchY, pitchUV, w, h, bits_per_pixel);
    else if (bits_per_pixel == 32)
      draw_colorbarsHD_444<float>(pY, pU, pV, pitchY, pitchUV, w, h,32);
  }
  else if (IsColorbars) {
    // Rec. ITU-R BT.801-1
      // "ColorBars" pattern is defined in Rec. ITU-R BT.801-1, with studio RGB values that are then converted to YUV for YUV formats.
    // Optional YUV output calculation uses 170_ST (601) independent from the actual frame dimensions.
    if (vi.IsRGB() && vi.IsPlanar()) {
      BYTE* pG = (BYTE*)frame->GetWritePtr(PLANAR_G);
      BYTE* pB = (BYTE*)frame->GetWritePtr(PLANAR_B);
      BYTE* pR = (BYTE*)frame->GetWritePtr(PLANAR_R);
      const int pitch = frame->GetPitch(PLANAR_G);

      if (bits_per_pixel == 8)
        draw_colorbars_rgbp<uint8_t>(pR, pG, pB, pitch, w, h, 8);
      else if (bits_per_pixel <= 16)
        draw_colorbars_rgbp<uint16_t>(pR, pG, pB, pitch, w, h, bits_per_pixel);
      else if (bits_per_pixel == 32)
        draw_colorbars_rgbp<float>(pR, pG, pB, pitch, w, h, 32);
    }
    else if (vi.IsRGB32() || vi.IsRGB64()) {
      const int pitch = frame->GetPitch() / 4;
      switch (bits_per_pixel) {
      case 8: draw_colorbars_rgb3264<uint8_t>((uint8_t*)p, pitch, w, h); break;
      case 16: draw_colorbars_rgb3264<uint16_t>((uint8_t*)p, pitch, w, h); break;
      }
    }
    else if (vi.IsRGB24() || vi.IsRGB48()) {
      const int pitch = frame->GetPitch();
      switch (bits_per_pixel) {
      case 8: draw_colorbars_rgb2448<uint8_t>((uint8_t*)p, pitch, w, h); break;
      case 16: draw_colorbars_rgb2448<uint16_t>((uint8_t*)p, pitch, w, h); break;
      }
    }
    else if (vi.IsYUY2()) {
      // YUY2 is a packed 4:2:2 format: Y0 U0 Y1 V0 (alternating luma/chroma samples)
      // We treat it as planar internally, then pack the results
      const int pitch = frame->GetPitch();
      uint8_t* dst = frame->GetWritePtr();

      // Allocate temporary planar buffers for 8-bit YUV
      std::vector<uint8_t> tempY(w * h);
      std::vector<uint8_t> tempU((w >> 1) * h);
      std::vector<uint8_t> tempV((w >> 1) * h);

      // Use the unified YUV drawing function to generate planar data
      draw_colorbars_yuv<uint8_t, false, true, false>(
        tempY.data(), tempU.data(), tempV.data(),
        w, w >> 1, w, h, 8
      );

      // Pack planar YUV 4:2:2 into YUY2 format (Y0 U0 Y1 V0)
      for (int y = 0; y < h; ++y) {
        const uint8_t* srcY = tempY.data() + y * w;
        const uint8_t* srcU = tempU.data() + y * (w >> 1);
        const uint8_t* srcV = tempV.data() + y * (w >> 1);
        uint8_t* dstRow = dst + y * pitch;

        for (int x = 0; x < w >> 1; ++x) {
          dstRow[x * 4 + 0] = srcY[x * 2 + 0];  // Y0
          dstRow[x * 4 + 1] = srcU[x];          // U0
          dstRow[x * 4 + 2] = srcY[x * 2 + 1];  // Y1
          dstRow[x * 4 + 3] = srcV[x];          // V0
        }
      }
    }
    if (vi.Is444()) {
      BYTE* pY = (BYTE*)frame->GetWritePtr(PLANAR_Y);
      BYTE* pU = (BYTE*)frame->GetWritePtr(PLANAR_U);
      BYTE* pV = (BYTE*)frame->GetWritePtr(PLANAR_V);
      const int pitchY = frame->GetPitch(PLANAR_Y);
      const int pitchUV = frame->GetPitch(PLANAR_U);
      if (bits_per_pixel == 8)
        draw_colorbars_yuv<uint8_t, false, false, false>(pY, pU, pV, pitchY, pitchUV, w, h, 8);
      else if (bits_per_pixel <= 16)
        draw_colorbars_yuv<uint16_t, false, false, false>(pY, pU, pV, pitchY, pitchUV, w, h, bits_per_pixel);
      else
        draw_colorbars_yuv<float, false, false, false>(pY, pU, pV, pitchY, pitchUV, w, h, 32);
    }
    else if (vi.Is420()) {
      BYTE* pY = (BYTE*)frame->GetWritePtr(PLANAR_Y);
      BYTE* pU = (BYTE*)frame->GetWritePtr(PLANAR_U);
      BYTE* pV = (BYTE*)frame->GetWritePtr(PLANAR_V);
      const int pitchY = frame->GetPitch(PLANAR_Y);
      const int pitchUV = frame->GetPitch(PLANAR_U);
      if (bits_per_pixel == 8)
        draw_colorbars_yuv<uint8_t, true, false, false>(pY, pU, pV, pitchY, pitchUV, w, h, 8);
      else if (bits_per_pixel <= 16)
        draw_colorbars_yuv<uint16_t, true, false, false>(pY, pU, pV, pitchY, pitchUV, w, h, bits_per_pixel);
      else
        draw_colorbars_yuv<float, true, false, false>(pY, pU, pV, pitchY, pitchUV, w, h, 32);
    }
    else if (vi.Is422()) {
      BYTE* pY = (BYTE*)frame->GetWritePtr(PLANAR_Y);
      BYTE* pU = (BYTE*)frame->GetWritePtr(PLANAR_U);
      BYTE* pV = (BYTE*)frame->GetWritePtr(PLANAR_V);
      const int pitchY = frame->GetPitch(PLANAR_Y);
      const int pitchUV = frame->GetPitch(PLANAR_U);
      if (bits_per_pixel == 8)
        draw_colorbars_yuv<uint8_t, false, true, false>(pY, pU, pV, pitchY, pitchUV, w, h, 8);
      else if (bits_per_pixel <= 16)
        draw_colorbars_yuv<uint16_t, false, true, false>(pY, pU, pV, pitchY, pitchUV, w, h, bits_per_pixel);
      else
        draw_colorbars_yuv<float, false, true, false>(pY, pU, pV, pitchY, pitchUV, w, h, 32);
    }
    else if (vi.IsYV411()) {
      BYTE* pY = (BYTE*)frame->GetWritePtr(PLANAR_Y);
      BYTE* pU = (BYTE*)frame->GetWritePtr(PLANAR_U);
      BYTE* pV = (BYTE*)frame->GetWritePtr(PLANAR_V);
      const int pitchY = frame->GetPitch(PLANAR_Y);
      const int pitchUV = frame->GetPitch(PLANAR_U);
      // YV411 is 8-bit only
      draw_colorbars_yuv<uint8_t, false, false, true>(pY, pU, pV, pitchY, pitchUV, w, h, 8);
    }
  } // "ColorBars" pattern generation
  else {
    // future other ColorBars types can be added here
  }

  // Alpha cnannel - if any - is common. RGB32/64 has already filled alpha.
  if (vi.IsYUVA() || vi.IsPlanarRGBA()) {
    // initialize planar alpha planes with zero (no transparency), like RGB32 does
    BYTE* dstp = frame->GetWritePtr(PLANAR_A);
    int rowsize = frame->GetRowSize(PLANAR_A);
    int pitch = frame->GetPitch(PLANAR_A);
    int height = frame->GetHeight(PLANAR_A);
    switch (bits_per_pixel) {
    case 8: fill_plane<uint8_t>(dstp, height, rowsize, pitch, 0); break;
    case 10: case 12: case 14: case 16: fill_plane<uint16_t>(dstp, height, rowsize, pitch, 0); break;
    case 32: fill_plane<float>(dstp, height, rowsize, pitch, 0.0f); break;
    }
  }

  // Generate Audio buffer
  {
    unsigned x = vi.audio_samples_per_second, y = Hz;
    while (y) {     // find gcd
      unsigned t = x%y; x = y; y = t;
    }
    nsamples = vi.audio_samples_per_second / x; // 1200
    const unsigned ncycles = Hz / x; // 11

    audio = new(std::nothrow) SFLOAT[nsamples];
    if (!audio)
      env->ThrowError("ColorBars: insufficient memory");

    const double add_per_sample = ncycles / (double)nsamples;
    double second_offset = 0.0;
    for (unsigned i = 0; i < nsamples; i++) {
      audio[i] = (SFLOAT)sin(PI * 2.0 * second_offset);
      second_offset += add_per_sample;
    }
  }
  }

  // By the new "staticframes" parameter: colorbars we generate (copy) real new frames instead of a ready-to-use static one
  PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env)
  {
    AVS_UNUSED(n);
    if (staticframes)
      return frame; // original default method returns precomputed static frame.
    else {
      PVideoFrame result = env->NewVideoFrameP(vi, &frame);
      // BitBlts are safe to call on all planes, planes with zero-size height or row-size are ignored.
      env->BitBlt(result->GetWritePtr(), result->GetPitch(), frame->GetReadPtr(), frame->GetPitch(), frame->GetRowSize(), frame->GetHeight());
      env->BitBlt(result->GetWritePtr(PLANAR_V), result->GetPitch(PLANAR_V), frame->GetReadPtr(PLANAR_V), frame->GetPitch(PLANAR_V), frame->GetRowSize(PLANAR_V), frame->GetHeight(PLANAR_V));
      env->BitBlt(result->GetWritePtr(PLANAR_U), result->GetPitch(PLANAR_U), frame->GetReadPtr(PLANAR_U), frame->GetPitch(PLANAR_U), frame->GetRowSize(PLANAR_U), frame->GetHeight(PLANAR_U));
      env->BitBlt(result->GetWritePtr(PLANAR_A), result->GetPitch(PLANAR_A), frame->GetReadPtr(PLANAR_A), frame->GetPitch(PLANAR_A), frame->GetRowSize(PLANAR_A), frame->GetHeight(PLANAR_A));
      return result;
    }
  }

  bool __stdcall GetParity(int n) {
    AVS_UNUSED(n);
    return false;
  }
  const VideoInfo& __stdcall GetVideoInfo() { return vi; }
  int __stdcall SetCacheHints(int cachehints,int frame_range)
  {
    AVS_UNUSED(frame_range);
    switch (cachehints)
      {
      case CACHE_GET_MTMODE:
          return MT_NICE_FILTER;
      case CACHE_DONT_CACHE_ME:
          return 1;
      default:
          return 0;
      }
  };

  void FillAudioZeros(void* buf, int start_offset, int count) {
    const int bps = vi.BytesPerAudioSample();
    unsigned char* byte_buf = (unsigned char*)buf;
    memset(byte_buf + start_offset * bps, 0, count * bps);
  }

  void __stdcall GetAudio(void* buf, int64_t start, int64_t count, IScriptEnvironment* env) {
    AVS_UNUSED(env);
#if 1
    // This filter is non-cached so we guard against negative start and overread, like in Cache::GetAudio
    if ((start + count <= 0) || (start >= vi.num_audio_samples)) {
      // Completely skip.
      FillAudioZeros(buf, 0, (int)count);
      count = 0;
      return;
    }

    if (start < 0) {  // Partial initial skip
      FillAudioZeros(buf, 0, (int)-start);  // Fill all samples before 0 with silence.
      count += start;  // Subtract start bytes from count.
      buf = ((BYTE*)buf) - (int)(start * vi.BytesPerAudioSample());
      start = 0;
    }

    if (start + count > vi.num_audio_samples) {  // Partial ending skip
      FillAudioZeros(buf, (int)(vi.num_audio_samples - start), (int)(count - (vi.num_audio_samples - start)));  // Fill end samples
      count = (vi.num_audio_samples - start);
    }

  const int d_mod = vi.audio_samples_per_second*2;
  float* samples = (float*)buf;

  unsigned j = (unsigned)(start % nsamples);
  for (int i=0;i<count;i++) {
    samples[i*2]=audio[j];
    if (((start+i)%d_mod)>vi.audio_samples_per_second) {
    samples[i*2+1]=audio[j];
    } else {
    samples[i*2+1]=0;
    }
    if (++j >= nsamples) j = 0;
  }
#else
    int64_t Hz=440;
    // Calculate what start equates in cycles.
    // This is the number of cycles (rounded down) that has already been taken.
    int64_t startcycle = (start*Hz) /  vi.audio_samples_per_second;

    // Move offset down - this is to avoid float rounding errors
    int start_offset = (int)(start - ((startcycle * vi.audio_samples_per_second) / Hz));

    double add_per_sample=Hz/(double)vi.audio_samples_per_second;
    double second_offset=((double)start_offset*add_per_sample);
    int d_mod=vi.audio_samples_per_second*2;
    float* samples = (float*)buf;

    for (int i=0;i<count;i++) {
        samples[i*2]=(SFLOAT)sin(PI * 2.0 * second_offset);
        if (((start+i)%d_mod)>vi.audio_samples_per_second) {
          samples[i*2+1]=samples[i*2];
        } else {
          samples[i*2+1]=0;
        }
        second_offset+=add_per_sample;
    }
#endif
  }

  static AVSValue __cdecl Create(AVSValue args, void* _type, IScriptEnvironment* env) {
    const int type = (int)(size_t)_type;
    // 0: ColorBars, 1: ColorBarsHD
    bool staticframes = args[3].AsBool(true);

    const int default_width = type ? 1288 : 640;
    const int default_height = type ? 720 : 480;
    const char* default_pixel_type = type ? "YV24" : "RGB32";

    PClip clip = new ColorBars(args[0].AsInt(default_width),
                         args[1].AsInt(default_height),
                         args[2].AsString(default_pixel_type),
                         staticframes,
                         type, env);
    // wrap in OnCPU to support multi devices
    AVSValue arg[2]{ clip, 1 }; // prefetch=1: enable cache but not thread
    AVSValue ret = env->Invoke("OnCPU", AVSValue(arg, 2));
    if (staticframes) {
      return new SingleFrame(ret.AsClip());
    }
    return ret;
  }
};

/********************************************************************
********************************************************************/

#ifdef AVS_WINDOWS
// AviSource is Windows-only, because it explicitly relies on Video for Windows
AVSValue __cdecl Create_SegmentedSource(AVSValue args, void* use_directshow, IScriptEnvironment* env) {
  bool bAudio = !use_directshow && args[1].AsBool(true);
  const char* pixel_type = 0;
  const char* fourCC = 0;
  int vtrack = 0;
  int atrack = 0;
  bool utf8;
  const int inv_args_count = args.ArraySize();
  AVSValue inv_args[9];
  if (!use_directshow) {
    pixel_type = args[2].AsString("");
    fourCC = args[3].AsString("");
    vtrack = args[4].AsInt(0);
    atrack = args[5].AsInt(0);
    utf8 = args[6].AsBool(false);
  }
  else {
    for (int i=1; i<inv_args_count ;i++)
      inv_args[i] = args[i];
  }
  args = args[0];
  PClip result = 0;
  const char* error_msg=0;
  for (int i = 0; i < args.ArraySize(); ++i) {
    char basename[260];
    strcpy(basename, args[i].AsString());
    char* extension = strrchr(basename, '.');
    if (extension)
      *extension++ = 0;
    else
      extension[0] = 0;
    for (int j = 0; j < 100; ++j) {
      char filename[260];
      wsprintf(filename, "%s.%02d.%s", basename, j, extension);
      if (GetFileAttributes(filename) != (DWORD)-1) {   // check if file exists
          PClip clip;
        try {
          if (use_directshow) {
            inv_args[0] = filename;
            clip = env->Invoke("DirectShowSource",AVSValue(inv_args, inv_args_count)).AsClip(); // no utf8 yet
          } else {
            clip =  (IClip*)(new AVISource(filename, bAudio, pixel_type, fourCC, vtrack, atrack, AVISource::MODE_NORMAL, utf8, env));
          }
          AVSValue arg[3] = { result, clip, 0 };
          result = !result ? clip : env->Invoke("UnalignedSplice", AVSValue(arg, 3)).AsClip();
        } catch (const AvisynthError &e) {
          error_msg=e.msg;
        }
      }
    }
  }
  if (!result) {
    if (!error_msg) {
      env->ThrowError("Segmented%sSource: no files found!", use_directshow ? "DirectShow" : "AVI");
    } else {
      env->ThrowError("Segmented%sSource: decompressor returned error:\n%s!", use_directshow ? "DirectShow" : "AVI",error_msg);
    }
  }
  return result;
}
#endif

/**********************************************************
 *                         TONE                           *
 **********************************************************/
class SampleGenerator {
public:
  SampleGenerator() {}
  virtual SFLOAT getValueAt(double where) {
    AVS_UNUSED(where);
    return 0.0f;}
};

class SineGenerator : public SampleGenerator {
public:
  SineGenerator() {}
  SFLOAT getValueAt(double where) {return (SFLOAT)sin(PI * where * 2.0);}
};


class NoiseGenerator : public SampleGenerator {
public:
  NoiseGenerator() {
    srand( (unsigned)time( NULL ) );
  }

  SFLOAT getValueAt(double where) {
    AVS_UNUSED(where);
    return (float) rand()*(2.0f/RAND_MAX) -1.0f;}
};

class SquareGenerator : public SampleGenerator {
public:
  SquareGenerator() {}

  SFLOAT getValueAt(double where) {
    if (where<=0.5) {
      return 1.0f;
    } else {
      return -1.0f;
    }
  }
};

class TriangleGenerator : public SampleGenerator {
public:
  TriangleGenerator() {}

  SFLOAT getValueAt(double where) {
    if (where<=0.25) {
      return (SFLOAT)(where*4.0);
    } else if (where<=0.75) {
      return (SFLOAT)((-4.0*(where-0.50)));
    } else {
      return (SFLOAT)((4.0*(where-1.00)));
    }
  }
};

class SawtoothGenerator : public SampleGenerator {
public:
  SawtoothGenerator() {}

  SFLOAT getValueAt(double where) {
    return (SFLOAT)(2.0*(where-0.5));
  }
};


class Tone : public IClip {
  VideoInfo vi;
  SampleGenerator *s;
  const double freq;            // Frequency in Hz
  const double samplerate;      // Samples per second
  const int ch;                 // Number of channels
  const double add_per_sample;  // How much should we add per sample in seconds
  const float level;

public:

  Tone(double _length, double _freq, int _samplerate, int _ch, const char* _type, float _level, IScriptEnvironment* env):
             freq(_freq), samplerate(_samplerate), ch(_ch), add_per_sample(_freq/_samplerate), level(_level) {
    memset(&vi, 0, sizeof(VideoInfo));
    vi.sample_type = SAMPLE_FLOAT;
    vi.nchannels = _ch;
    vi.audio_samples_per_second = _samplerate;
    vi.num_audio_samples=(int64_t)(_length*vi.audio_samples_per_second+0.5);

    if (!lstrcmpi(_type, "Sine"))
      s = new SineGenerator();
    else if (!lstrcmpi(_type, "Noise"))
      s = new NoiseGenerator();
    else if (!lstrcmpi(_type, "Square"))
      s = new SquareGenerator();
    else if (!lstrcmpi(_type, "Triangle"))
      s = new TriangleGenerator();
    else if (!lstrcmpi(_type, "Sawtooth"))
      s = new SawtoothGenerator();
    else if (!lstrcmpi(_type, "Silence"))
      s = new SampleGenerator();
    else
      env->ThrowError("Tone: Type was not recognized!");
  }

  void __stdcall GetAudio(void* buf, int64_t start, int64_t count, IScriptEnvironment* env) {
    AVS_UNUSED(env);
    // Where in the cycle are we in?
    const double cycle = (freq * start) / samplerate;
    double period_place = cycle - floor(cycle);

    SFLOAT* samples = (SFLOAT*)buf;

    for (int i=0;i<count;i++) {
      SFLOAT v = s->getValueAt(period_place) * level;
      for (int o=0;o<ch;o++) {
        samples[o+i*ch] = v;
      }
      period_place += add_per_sample;
      if (period_place >= 1.0)
        period_place -= floor(period_place);
    }
  }

  static AVSValue __cdecl Create(AVSValue args, void*, IScriptEnvironment* env)
  {
    return new Tone(args[0].AsFloat(10.0f), args[1].AsFloat(440.0f), args[2].AsInt(48000),
          args[3].AsInt(2), args[4].AsString("Sine"), args[5].AsFloatf(1.0f), env);
  }

  PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env) {
    AVS_UNUSED(n);
    AVS_UNUSED(env);
    return NULL; }
  const VideoInfo& __stdcall GetVideoInfo() { return vi; }
  bool __stdcall GetParity(int n) {
    AVS_UNUSED(n);
    return false; }
  int __stdcall SetCacheHints(int cachehints,int frame_range) {
    AVS_UNUSED(cachehints);
    AVS_UNUSED(frame_range);
    return 0; };

};


AVSValue __cdecl Create_Version(AVSValue args, void*, IScriptEnvironment* env) {
  //     0       1       2           3         4
  // [length]i[width]i[height]i[pixel_type]s[clip]c
  VideoInfo vi_default;

  int i_pixel_type = VideoInfo::CS_BGR24;

  const bool has_clip = args[4].Defined();
  if (has_clip) {
    // clip overrides
    vi_default = args[4].AsClip()->GetVideoInfo();
    i_pixel_type = vi_default.pixel_type;
  }

  if (args[3].Defined()) {
    i_pixel_type = GetPixelTypeFromName(args[3].AsString());
    if (i_pixel_type == VideoInfo::CS_UNKNOWN)
      env->ThrowError("Version: invalid 'pixel_type'");
  }

  int num_frames = args[0].AsInt(has_clip ? vi_default.num_frames : -1); // auto (240)
  int w = args[1].AsInt(has_clip ? vi_default.width : -1); // auto
  int h = args[2].AsInt(has_clip ? vi_default.height : -1); // auto
  const bool shrink = false;
  const int textcolor = 0xECF2BF;
  const int halocolor = 0;
  const int bgcolor = 0x404040;

  const int fps_numerator = has_clip ? vi_default.fps_numerator :-1; // auto
  const int fps_denominator = has_clip ? vi_default.fps_denominator : -1; // auto

  return Create_MessageClip(
    AVS_FULLVERSION AVS_DEVELOPMENT_BUILD AVS_DEVELOPMENT_BUILD_GIT AVS_COPYRIGHT_UTF8,
    w, h, i_pixel_type, shrink, textcolor, halocolor, bgcolor, fps_numerator, fps_denominator, num_frames,
    true, // utf8
    env);
}


extern const AVSFunction Source_filters[] = {
#ifdef AVS_WINDOWS
  { "AVISource",     BUILTIN_FUNC_PREFIX, "s+[audio]b[pixel_type]s[fourCC]s[vtrack]i[atrack]i[utf8]b", AVISource::Create, (void*) AVISource::MODE_NORMAL },
  { "AVIFileSource", BUILTIN_FUNC_PREFIX, "s+[audio]b[pixel_type]s[fourCC]s[vtrack]i[atrack]i[utf8]b", AVISource::Create, (void*) AVISource::MODE_AVIFILE },
  { "WAVSource",     BUILTIN_FUNC_PREFIX, "s+[utf8]b", AVISource::Create, (void*) AVISource::MODE_WAV },
  { "OpenDMLSource", BUILTIN_FUNC_PREFIX, "s+[audio]b[pixel_type]s[fourCC]s[vtrack]i[atrack]i[utf8]b", AVISource::Create, (void*) AVISource::MODE_OPENDML },
  { "SegmentedAVISource", BUILTIN_FUNC_PREFIX, "s+[audio]b[pixel_type]s[fourCC]s[vtrack]i[atrack]i[utf8]b", Create_SegmentedSource, (void*)0 },
  { "SegmentedDirectShowSource", BUILTIN_FUNC_PREFIX,
// args               0      1      2       3       4            5          6         7            8
                     "s+[fps]f[seek]b[audio]b[video]b[convertfps]b[seekzero]b[timeout]i[pixel_type]s",
                     Create_SegmentedSource, (void*)1 },
// args             0         1       2        3            4     5                 6            7        8             9       10          11     12
#endif
  { "BlankClip", BUILTIN_FUNC_PREFIX, "[]c*[length]i[width]i[height]i[pixel_type]s[fps]f[fps_denominator]i[audio_rate]i[stereo]b[sixteen_bit]b[color]i[color_yuv]i[clip]c", Create_BlankClip },
  { "BlankClip", BUILTIN_FUNC_PREFIX, "[]c*[length]i[width]i[height]i[pixel_type]s[fps]f[fps_denominator]i[audio_rate]i[channels]i[sample_type]s[color]i[color_yuv]i[clip]c", Create_BlankClip },
  { "BlankClip", BUILTIN_FUNC_PREFIX, "[]c*[length]i[width]i[height]i[pixel_type]s[fps]f[fps_denominator]i[audio_rate]i[stereo]b[sixteen_bit]b[color]i[color_yuv]i[clip]c[colors]f+", Create_BlankClip },
  { "BlankClip", BUILTIN_FUNC_PREFIX, "[]c*[length]i[width]i[height]i[pixel_type]s[fps]f[fps_denominator]i[audio_rate]i[channels]i[sample_type]s[color]i[color_yuv]i[clip]c[colors]f+", Create_BlankClip },
  { "Blackness", BUILTIN_FUNC_PREFIX, "[]c*[length]i[width]i[height]i[pixel_type]s[fps]f[fps_denominator]i[audio_rate]i[stereo]b[sixteen_bit]b[color]i[color_yuv]i[clip]c", Create_BlankClip },
  { "Blackness", BUILTIN_FUNC_PREFIX, "[]c*[length]i[width]i[height]i[pixel_type]s[fps]f[fps_denominator]i[audio_rate]i[channels]i[sample_type]s[color]i[color_yuv]i[clip]c", Create_BlankClip },
  { "MessageClip", BUILTIN_FUNC_PREFIX, "s[width]i[height]i[shrink]b[text_color]i[halo_color]i[bg_color]i[utf8]b", Create_MessageClip },
  { "ColorBars", BUILTIN_FUNC_PREFIX, "[width]i[height]i[pixel_type]s[staticframes]b", ColorBars::Create, (void*)0 },
  { "ColorBarsHD", BUILTIN_FUNC_PREFIX, "[width]i[height]i[pixel_type]s[staticframes]b", ColorBars::Create, (void*)1 },
  { "Tone", BUILTIN_FUNC_PREFIX, "[length]f[frequency]f[samplerate]i[channels]i[type]s[level]f", Tone::Create },

  { "Version", BUILTIN_FUNC_PREFIX, "[length]i[width]i[height]i[pixel_type]s[clip]c", Create_Version },

  { NULL }
};

#undef XP_LAMBDA_CAPTURE_FIX
