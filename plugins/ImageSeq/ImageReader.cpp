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



#include <avisynth.h>
#include "ImageSeq.h"
#include <algorithm>
#include <sstream>
#include <avs/config.h>
#include <avs/filesystem.h>

#define TEXT_COLOR 0xf0f080

using namespace std;


ImageReader::ImageReader(const char * _base_name, const int _start, const int _end,
                         const double _fps, bool _use_DevIL, bool _info, const char * _pixel,
                         bool _animation, IScriptEnvironment* env)
 : start(_start), use_DevIL(_use_DevIL), info(_info), animation(_animation), framecopies(0)
{
  if (DevIL_Version == 0) // Init the DevIL.dll version
    DevIL_Version = ilGetInteger(IL_VERSION_NUM);

#ifdef AVS_WINDOWS
  // treat empty input as current directory
  const char *base_name_good = (*_base_name == 0) ? ".\\" : _base_name;
  // Make sure we have an absolute path.
  DWORD len = GetFullPathName(base_name_good, 0, base_name, NULL);
  if (len == 0)
    env->ThrowError("ImageReader: GetFullPathName failed. Error code: %d.", GetLastError());
  if (len > sizeof(base_name))
    env->ThrowError("ImageReader: Path to %s too long.", _base_name);
  (void)GetFullPathName(base_name_good, len, base_name, NULL);
  _snprintf(filename, (sizeof filename) - 1, base_name, start);

#else
  std::string base_name_good = (*_base_name == 0) ? "./" : _base_name;
  auto path = fs::path(base_name_good);
  std::error_code ec;
  auto fullpath = fs::absolute(path, ec);
  // to-do: check ec
  //auto fullpath = fs::canonical(path, ec); // canonical removes e.g. dot. Path must exist.
  std::string fullpaths = fullpath.string();
  // cout << fullpaths.c_str() << "\n"; // debug
  if (fullpaths.size() > sizeof(base_name) - 1)
    env->ThrowError("ImageReader: Path to %s too long.", _base_name);
  strcpy(base_name, fullpath.c_str());

  snprintf(filename, (sizeof filename) - 1, base_name, start);

#endif


  memset(&vi, 0, sizeof(vi));

  // Invariants
  vi.num_frames = -start + _end + 1;  // make sure each frame can be requested
  vi.audio_samples_per_second = 0;
  double num = _fps;  // calculate fps as num/denom for vi
  unsigned denom = 1;
  while (num < 16777216 && denom < 16777216) { num*=2; denom*=2; } // float mantissa is only 24 bits
  vi.SetFPS(unsigned(num+0.5), denom); // And normalize

  if (use_DevIL == false)
  {
    fileHeader.bfType = 0;
    // Try to parse as bmp/ebmp
    ifstream file(filename, ios::binary);
    file.read( reinterpret_cast<char *> (&fileHeader), sizeof(fileHeader) );
    file.read( reinterpret_cast<char *> (&infoHeader), sizeof(infoHeader) );
    file.close();

    if ( fileHeader.bfType == ('M' << 8) + 'B')
    {
      if (infoHeader.biCompression == 0) {
        vi.width = infoHeader.biWidth;
        vi.height = infoHeader.biHeight;

        if (infoHeader.biPlanes == 1) {
          if (infoHeader.biBitCount == 32)
            vi.pixel_type = VideoInfo::CS_BGR32;
          else if (infoHeader.biBitCount == 24)
            vi.pixel_type = VideoInfo::CS_BGR24;
          else if (infoHeader.biBitCount == 16)
            vi.pixel_type = VideoInfo::CS_YUY2;
          else if (infoHeader.biBitCount == 8)
            vi.pixel_type = VideoInfo::CS_Y8;
          else if (DevIL_Version <= 166)
            // DevIL 1.6.6 has a major coronary with palletted BMP files so don't fail thru to it
            env->ThrowError("ImageReader: %d bit BMP is unsupported.", infoHeader.biBitCount);
          else
            use_DevIL = true; // give it to DevIL (for example: biBitCount == 1 or 4 bit)
        }
        else if (infoHeader.biPlanes == 3) {
          if (infoHeader.biBitCount == 24)
            vi.pixel_type = VideoInfo::CS_YV24;
          else if (infoHeader.biBitCount == 16) {
            // YV16 (4:2:2) and YUV440 (4:4:0) both occupy 16 bits/pixel
            // YV16 keeps precedence, before YUV440 it was the only one at 16bpp
            if (!lstrcmpi(_pixel, "rgb24")) // Hack - the default text is "rgb24"
              vi.pixel_type = VideoInfo::CS_YV16;
            else if (!lstrcmpi(_pixel, "yv16"))
              vi.pixel_type = VideoInfo::CS_YV16;
            else if (!lstrcmpi(_pixel, "yuv440"))
              vi.pixel_type = VideoInfo::CS_YUV440;
            else
              env->ThrowError("ImageReader: 16 bit, 3 plane EBMP: Pixel_type must be \"YV16\" or \"YUV440\".");
          }
          else if (infoHeader.biBitCount == 12) {
            if (!lstrcmpi(_pixel, "rgb24")) // Hack - the default text is "rgb24"
              vi.pixel_type = VideoInfo::CS_YV12;
            else if (!lstrcmpi(_pixel, "yv12"))
              vi.pixel_type = VideoInfo::CS_YV12;
            else if (!lstrcmpi(_pixel, "yv411"))
              vi.pixel_type = VideoInfo::CS_YV411;
            else
              env->ThrowError("ImageReader: 12 bit, 3 plane EBMP: Pixel_type must be \"YV12\" or \"YV411\".");
          }
          else if (infoHeader.biBitCount == 9)
            vi.pixel_type = VideoInfo::CS_YUV410; // 4:1:0 specific, no other 3-plane formats have this bpp
          else
            env->ThrowError("ImageReader: %d bit, 3 plane EBMP is unsupported.", infoHeader.biBitCount);
        }
        else
          env->ThrowError("ImageReader: %d plane BMP is unsupported.", infoHeader.biPlanes);

        if (DevIL_Version > 166 && (infoHeader.biWidth <= 0 || infoHeader.biHeight <= 0)) {
            use_DevIL = true; // Not values we know, give it to DevIL
        }
        else {
          if (infoHeader.biWidth <= 0)
            // use_DevIL = true; // Not a type we know, give it to DevIL
            env->ThrowError("ImageReader: Unsupported width %d", infoHeader.biWidth);
          if (infoHeader.biHeight <= 0)
            // use_DevIL = true; // Not a type we know, give it to DevIL
            env->ThrowError("ImageReader: Unsupported height %d", infoHeader.biHeight);
        }
      }
      else {
        // DevIL 1.6.6 has a major coronary with compressed BMP files so don't fail thru to it
        if (DevIL_Version <= 166)
          env->ThrowError("ImageReader: EBMP reader cannot handle compressed images.");

        use_DevIL = true; // give it to DevIL (image is compressed)
      }
    }
    else {
      use_DevIL = true; // Not a BMP, give it to DevIL
    }
  }

  if (use_DevIL == true) {  // attempt to open via DevIL

    std::unique_lock<std::mutex> lock(DevIL_mutex);

    ilInit();

    ILuint myImage=0;
    ilGenImages(1, &myImage);
    ilBindImage(myImage);

    ilLoadImage(filename);

    vi.width = ilGetInteger(IL_IMAGE_WIDTH);
    vi.height = ilGetInteger(IL_IMAGE_HEIGHT);

    if (!lstrcmpi(_pixel, "rgb")) {
      vi.pixel_type = VideoInfo::CS_BGR32;
    }
    else if (!lstrcmpi(_pixel, "rgb32")) {
      vi.pixel_type = VideoInfo::CS_BGR32;
    }
    else if (!lstrcmpi(_pixel, "rgb24")) {
      vi.pixel_type = VideoInfo::CS_BGR24;
    }
    else if (!lstrcmpi(_pixel, "y8")) {
      vi.pixel_type = VideoInfo::CS_Y8;
    }
    else if (!lstrcmpi(_pixel, "y16")) {
      vi.pixel_type = VideoInfo::CS_Y16;
    }
    else if (!lstrcmpi(_pixel, "rgb48")) {
      vi.pixel_type = VideoInfo::CS_BGR48;
    }
    else if (!lstrcmpi(_pixel, "rgb64")) {
      vi.pixel_type = VideoInfo::CS_BGR64;
    }
    /*
    else if (!lstrcmpi(_pixel, "rgbp8") || !lstrcmpi(_pixel, "rgbp")) {
      vi.pixel_type = VideoInfo::CS_RGBP;
    }
    else if (!lstrcmpi(_pixel, "rgbap8") || !lstrcmpi(_pixel, "rgbap")) {
      vi.pixel_type = VideoInfo::CS_RGBAP;
    }
    else if (!lstrcmpi(_pixel, "rgbp16")) {
      vi.pixel_type = VideoInfo::CS_RGBP16;
    }
    else if (!lstrcmpi(_pixel, "rgbap16")) {
      vi.pixel_type = VideoInfo::CS_RGBAP16;
    }
    // these would need on-the-fly conversion, todo
    */
    else {
      lock.unlock();
      //env->ThrowError("ImageReader: supports the following pixel types: RGB24/32/48/64, Y8/16 or RGB(A)P8/16");
      env->ThrowError("ImageReader: supports the following pixel types: RGB24/32/48/64 or Y8/16");
    }

    if (animation) {
      vi.num_frames = ilGetInteger(IL_NUM_IMAGES) + 1; // bug in DevIL (ilGetInteger is one off in 166 en 178)
      if (vi.num_frames <= 0) {
        lock.unlock();
        env->ThrowError("ImageSourceAnim: DevIL can't detect the number of images in the animation");
      }

      unsigned duration_ms = (unsigned)ilGetInteger(IL_IMAGE_DURATION);
      if (duration_ms != 0 && _fps == 24.0) { // overwrite framerate in case of non-zero duration
          vi.SetFPS(1000, duration_ms);
      }
    }

    // Get errors if any
    // (note: inability to parse an (e)bmp will show up here as a DevIL error)
    ILenum err = ilGetError();

    ilDeleteImages(1, &myImage);

    lock.unlock();

    if (err != IL_NO_ERROR) {
      env->ThrowError("ImageReader: error '%s' in DevIL library.\nreading file \"%s\"\nDevIL version %d.", getErrStr(err), filename, DevIL_Version);
    }
    // work around DevIL upside-down bug with compressed images
    should_flip = false;
    const char * ext = strrchr(_base_name, '.') + 1;
    if (  !lstrcmpi(ext, "jpeg") || !lstrcmpi(ext, "jpg") || !lstrcmpi(ext, "jpe") || !lstrcmpi(ext, "dds") ||
          !lstrcmpi(ext, "pal") || !lstrcmpi(ext, "psd") || !lstrcmpi(ext, "pcx") || !lstrcmpi(ext, "png") ||
          !lstrcmpi(ext, "pbm") || !lstrcmpi(ext, "pgm") || !lstrcmpi(ext, "ppm") || !lstrcmpi(ext, "gif") ||
          !lstrcmpi(ext, "exr") || !lstrcmpi(ext, "jp2") || !lstrcmpi(ext, "hdr") )
    {
      should_flip = true;
    }
    else if ((DevIL_Version > 166) && (!lstrcmpi(ext, "tif") || !lstrcmpi(ext, "tiff")))
    {
      should_flip = true;
    }
    // flip back for Y8 or Y16
    if (vi.IsY()) {
        should_flip = !should_flip;
    }
  }

  // undecorated filename means they want a single, static image or an animation
  if (strcmp(filename, base_name) == 0) {
    if (animation)
      framecopies = 1;
    else
      framecopies = vi.num_frames;
  }
}


ImageReader::~ImageReader()
{
  if (use_DevIL) {
      std::lock_guard<std::mutex> lock(DevIL_mutex);
      ilShutDown();
  }
}

/*  Notes to clear thinking!

  vi.num_frames = end-start+1
  0 <= n < vi.num_frames

  for animation=true: vi.num_frames = ilGetInteger(IL_NUM_IMAGES)
*/

PVideoFrame ImageReader::GetFrame(int n, IScriptEnvironment* env)
{
  ILenum err = IL_NO_ERROR;

  PVideoFrame frame = env->NewVideoFrame(vi);
  BYTE * dstPtr = frame->GetWritePtr();
  BYTE * const WritePtr = dstPtr;

  const int pitch = frame->GetPitch();
  const int row_size = frame->GetRowSize();
  const int height = frame->GetHeight();
  const int width = vi.width;

#ifdef AVS_WINDOWS
  _snprintf(filename, (sizeof filename)-1, base_name, n+start);
#else
  snprintf(filename, (sizeof filename) - 1, base_name, n+start);
#endif

  // do not lock right now
  std::unique_lock<std::mutex> lock(DevIL_mutex, std::defer_lock);

  if (use_DevIL)  /* read using DevIL */
  {
    lock.lock();

    // Setup
    ILuint myImage = 0;
    ilGenImages(1, &myImage);
    ilBindImage(myImage);

    if (ilLoadImage(filename) == IL_FALSE) {
      // Get errors if any
      err = ilGetError();

      // Cleanup
      ilDeleteImages(1, &myImage);

      lock.unlock();

      memset(WritePtr, 0, pitch * height);  // Black frame
      if ((info) || (err != IL_COULD_NOT_OPEN_FILE)) {
        ostringstream ss;
        ss << "ImageReader: error '" << getErrStr(err) << "' in DevIL library\n"
          "opening file \"" << filename << "\"\n"
          "DevIL version " << DevIL_Version << ".";
        env->ApplyMessage(&frame, vi, ss.str().c_str(), vi.width / 4, TEXT_COLOR, 0, 0);
      }
      return frame;
    }

    if (animation) {
        if (ilActiveImage(n) == IL_FALSE) { // load image N from file
          // Get errors if any
          err = ilGetError();

          // Cleanup
          ilDeleteImages(1, &myImage);

          lock.unlock();

          memset(WritePtr, 0, pitch * height);  // Black frame
          if (info) {
            ostringstream ss;
            ss << "ImageSourceAnim: error '" << getErrStr(err) << "' in DevIL library\n"
              "processing image " << n << " from file \"" << filename << "\"\n"
              "DevIL version " << DevIL_Version << ".";
            env->ApplyMessage(&frame, vi, ss.str().c_str(), vi.width / 4, TEXT_COLOR, 0, 0);
          }
          return frame;
        }
      }
    else {
      // Check some parameters
      if (ilGetInteger(IL_IMAGE_HEIGHT) != height)
      {
        // Cleanup
        ilDeleteImages(1, &myImage);

        lock.unlock();

        memset(WritePtr, 0, pitch * height);
        env->ApplyMessage(&frame, vi, "ImageReader: images must have identical heights", vi.width / 4, TEXT_COLOR, 0, 0);
        return frame;
      }

      if (ilGetInteger(IL_IMAGE_WIDTH) != width)
      {
        // Cleanup
        ilDeleteImages(1, &myImage);

        lock.unlock();

        memset(WritePtr, 0, pitch * height);
        env->ApplyMessage(&frame, vi, "ImageReader: images must have identical widths", vi.width / 4, TEXT_COLOR, 0, 0);
        return frame;
      }
    }

    const ILenum il_format = vi.IsY() ? IL_LUMINANCE : ( (vi.IsRGB32() || vi.IsRGB64()) ? IL_BGRA : IL_BGR );
    const ILenum linesize = width * ( vi.IsY() ? 1 : ( (vi.IsRGB32() || vi.IsRGB64()) ? 4 : 3 ) ) * vi.ComponentSize();
    const int height_image = min(height, ilGetInteger(IL_IMAGE_HEIGHT));
    const int width_image = min(width, ilGetInteger(IL_IMAGE_WIDTH));
    const ILenum linesize_image = width_image * ( vi.IsY() ? 1 : ( (vi.IsRGB32() || vi.IsRGB64()) ? 4 : 3 ) ) * vi.ComponentSize();

    if (!vi.IsY()) {
      // fill bottom with black pixels
      memset(dstPtr, 0, pitch * (height-height_image));
      dstPtr += pitch * (height-height_image);
    }

    int ilPixelType = vi.ComponentSize() == 1 ? IL_UNSIGNED_BYTE : IL_UNSIGNED_SHORT;

    // Copy raster to AVS frame
////if (ilGetInteger(IL_ORIGIN_MODE) == IL_ORIGIN_UPPER_LEFT, IL_ORIGIN_LOWER_LEFT ???
    if (should_flip)
    {
      // Copy upside down
      for (int y=height_image-1; y>=0; --y)
      {
        if (ilCopyPixels(0, y, 0, width_image, 1, 1, il_format, ilPixelType, dstPtr) > linesize)
          break; // Try not to spew all over memory
        memset(dstPtr+linesize_image, 0, linesize-linesize_image);
        dstPtr += pitch;
      }
    }
    else {
      // Copy right side up
      for (int y=0; y<height_image; ++y)
      {
        if (ilCopyPixels(0, y, 0, width_image, 1, 1, il_format, ilPixelType, dstPtr) > linesize)
          break; // Try not to spew all over memory
        memset(dstPtr+linesize_image, 0, linesize-linesize_image);
        dstPtr += pitch;
      }
    }

    if (vi.IsY()) {
      // fill bottom with black pixels
      memset(dstPtr, 0, pitch * (height-height_image));
    }

    // Get errors if any
    err = ilGetError();

    // Cleanup
    ilDeleteImages(1, &myImage);

    lock.unlock();

    if (err != IL_NO_ERROR)
    {
      memset(WritePtr, 0, pitch * height);
      ostringstream ss;
      ss << "ImageReader: error '" << getErrStr(err) << "' in DevIL library\n"
            "reading file \"" << filename << "\"\n"
            "DevIL version " << DevIL_Version << ".";
      env->ApplyMessage(&frame, vi, ss.str().c_str(), vi.width/4, TEXT_COLOR, 0, 0);
      return frame;
    }
  }
  else {  /* treat as ebmp  */
    // Open file, ensure it has the expected properties
    ifstream file(filename, ios::binary);
    if (!checkProperties(file, frame, env)) {
      file.close();
      return frame;
    }

    // Seek past padding
    file.seekg (fileHeader.bfOffBits, ios::beg);

    // Read in raster
    if (vi.IsY())
    {
      // read upside down
      BYTE * endPtr = dstPtr + pitch * (height-1);
      fileRead(file, endPtr, -pitch, row_size, height);
    }
    else
    {
      fileRead(file, dstPtr, pitch, row_size, height);

      if (vi.IsPlanar())
      {
        dstPtr = frame->GetWritePtr(PLANAR_U);
        const int pitchUV = frame->GetPitch(PLANAR_U);
        const int row_sizeUV = frame->GetRowSize(PLANAR_U);
        const int heightUV = frame->GetHeight(PLANAR_U);
        fileRead(file, dstPtr, pitchUV, row_sizeUV, heightUV);

        dstPtr = frame->GetWritePtr(PLANAR_V);
        fileRead(file, dstPtr, pitchUV, row_sizeUV, heightUV);
      }
    }

    file.close();
  }

  if (info) {
    // overlay on video output: progress indicator
    ostringstream text;
    text << "Frame " << n << ".\n"
            "Read from \"" << filename << "\"\n"
            "DevIL version " << DevIL_Version << ".";
    env->ApplyMessage(&frame, vi, text.str().c_str(), vi.width/4, TEXT_COLOR, 0, 0);
  }

  return frame;
}


void ImageReader::fileRead(istream & file, BYTE * dstPtr, const int pitch, const int row_size, const int height)
{
  int padding = (4 - (row_size % 4)) % 4;
  for (int y=0; y<height; ++y)
  {
    file.read( reinterpret_cast<char *> (dstPtr), row_size);
    file.seekg(padding, ios_base::cur);
    dstPtr += pitch;
  }
}


void ImageReader::BlankFrame(PVideoFrame & frame)
{
  const int size = frame->GetPitch() * frame->GetHeight();

  if (vi.IsPlanarRGB() || vi.IsPlanarRGBA()) {
    bool hasAlpha = vi.IsPlanarRGBA();
    int pixelsize = vi.ComponentSize();
    if (pixelsize == 1) {
      memset(frame->GetWritePtr(PLANAR_G), 128, size); // Grey frame
      memset(frame->GetWritePtr(PLANAR_B), 128, size);
      memset(frame->GetWritePtr(PLANAR_R), 128, size);
      if(hasAlpha)
        memset(frame->GetWritePtr(PLANAR_A), 255, size); // transparent alpha
    }
    else if (pixelsize == 2) {
      uint16_t half = 1 << (vi.BitsPerComponent() - 1);
      uint16_t max = (1 << vi.BitsPerComponent()) - 1;
      std::fill_n((uint16_t *)frame->GetWritePtr(PLANAR_G), size / 2, half); // Grey frame
      std::fill_n((uint16_t *)frame->GetWritePtr(PLANAR_B), size / 2, half); // Grey frame
      std::fill_n((uint16_t *)frame->GetWritePtr(PLANAR_R), size / 2, half); // Grey frame
      if (hasAlpha)
        std::fill_n((uint16_t *)frame->GetWritePtr(PLANAR_A), size / 2, max); // transparent alpha
    }
    else if (pixelsize == 4) { // float
      const float half = 0.5f;
      std::fill_n((float *)frame->GetWritePtr(PLANAR_G), size / 4, half); // Grey frame
      std::fill_n((float *)frame->GetWritePtr(PLANAR_B), size / 4, half); // Grey frame
      std::fill_n((float *)frame->GetWritePtr(PLANAR_R), size / 4, half); // Grey frame
      if (hasAlpha)
        std::fill_n((float *)frame->GetWritePtr(PLANAR_A), size / 4, 1.0f); // Grey frame
    }
  } else if (vi.IsRGB() || vi.IsY()) {
    memset(frame->GetWritePtr(), 0, size); // Black frame
  }
  else {
    int pixelsize = vi.ComponentSize();
    const int UVpitch = frame->GetPitch(PLANAR_U);
    if (pixelsize == 1) {
      memset(frame->GetWritePtr(), 128, size); // Grey frame

      if (UVpitch) {
        const int UVsize = UVpitch * frame->GetHeight(PLANAR_U);

        memset(frame->GetWritePtr(PLANAR_U), 128, UVsize);
        memset(frame->GetWritePtr(PLANAR_V), 128, UVsize);
      }
    }
    else if (pixelsize == 2) {
      uint16_t half = 1 << (vi.BitsPerComponent() - 1);
      std::fill_n((uint16_t *)frame->GetWritePtr(), size / 2, half); // Grey frame

      if (UVpitch) {
        const int UVsize = UVpitch * frame->GetHeight(PLANAR_U);
        std::fill_n((uint16_t *)frame->GetWritePtr(PLANAR_U), UVsize / 2, half); // Grey frame
        std::fill_n((uint16_t *)frame->GetWritePtr(PLANAR_V), UVsize / 2, half); // Grey frame
      }
    }
    else if (pixelsize == 4) { // float
      const float half = 0.5f;
#ifdef FLOAT_CHROMA_IS_HALF_CENTERED
      const float halfUV = 0.5f;
#else
      const float halfUV = 0.0f;
#endif
      std::fill_n((float *)frame->GetWritePtr(), size / 4, half); // Grey frame

      if (UVpitch) {
        const int UVsize = UVpitch * frame->GetHeight(PLANAR_U);
        std::fill_n((float *)frame->GetWritePtr(PLANAR_U), UVsize / 4, halfUV); // Grey frame
        std::fill_n((float *)frame->GetWritePtr(PLANAR_V), UVsize / 4, halfUV); // Grey frame
      }
    }
  }
}


void ImageReader::BlankApplyMessage(PVideoFrame & frame, const char * text, IScriptEnvironment * env)
{
  BlankFrame(frame);
  env->ApplyMessage(&frame, vi, text, vi.width/4, TEXT_COLOR, 0, 0);
}


bool ImageReader::checkProperties(ifstream & file, PVideoFrame & frame, IScriptEnvironment * env)
{
  if (!file.is_open())
  {
    if (info)
      BlankApplyMessage(frame, "ImageReader: cannot open file", env);
    else
      BlankFrame(frame);

    return false;
  }

  BITMAPFILEHEADER tempFileHeader;
  BITMAPINFOHEADER tempInfoHeader;

  file.read( reinterpret_cast<char *> (&tempFileHeader), sizeof(tempFileHeader) );
  file.read( reinterpret_cast<char *> (&tempInfoHeader), sizeof(tempInfoHeader) );

  if (tempFileHeader.bfType != fileHeader.bfType)
  {
    BlankApplyMessage(frame, "ImageReader: invalid (E)BMP file", env);
    return false;
  }

  if (tempInfoHeader.biWidth != infoHeader.biWidth)
  {
    BlankApplyMessage(frame, "ImageReader: image widths must be identical", env);
    return false;
  }

  if (tempInfoHeader.biHeight != infoHeader.biHeight)
  {
    BlankApplyMessage(frame, "ImageReader: image heights must be identical", env);
    return false;
  }

  if (tempInfoHeader.biPlanes != infoHeader.biPlanes)
  {
    BlankApplyMessage(frame, "ImageReader: images must have the same number of planes", env);
    return false;
  }

  if (tempInfoHeader.biBitCount != infoHeader.biBitCount)
  {
    BlankApplyMessage(frame, "ImageReader: images must have identical bits per pixel", env);
    return false;
  }

  if (tempFileHeader.bfSize != fileHeader.bfSize)
  {
    BlankApplyMessage(frame, "ImageReader: raster sizes must be identical", env);
    return false;
  }

  if (tempInfoHeader.biCompression != 0)
  {
    BlankApplyMessage(frame, "ImageReader: EBMP reader cannot handle compressed images", env);
    return false;
  }

  return true;
}
