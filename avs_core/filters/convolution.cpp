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


#include <cstdint>
#include <cstdlib>

// Avisynth filter: general convolution
// by Richard Berg (avisynth-dev@richardberg.net)
// adapted from General Convolution 3D for VDub by Gunnar Thalin (guth@home.se)

#include "convolution.h"
#include "../core/internal.h"


/********************************************************************
***** Declare index of new filters for Avisynth's filter engine *****
********************************************************************/

extern const AVSFunction Convolution_filters[] = {
// Please when adding parameters try not to break the legacy order - IanB July 2004
	{ "GeneralConvolution", BUILTIN_FUNC_PREFIX, "c[bias]i[matrix]s[divisor]f[auto]b", GeneralConvolution::Create },
    /**
      * GeneralConvolution(PClip clip, int divisor=1, int bias=0, string matrix)
      * clip     =  input video
      * bias     =  additive bias to adjust the total output intensity
      * matrix   =  the kernel (3x3 or 5x5).  any kind of whitespace is ok, see example
      * divisor  =  divides the output of the convolution (calculated before adding bias)
      * auto     =  automaticly scale the result based on the sum of the matrix elements
      *
      * clip.GeneralConvolution(matrix = "1 2 3
      *                                   4 5 6
      *                                   7 8 9")
     **/

  { 0 }
};


/*****************************************
****** General Convolution 2D filter *****
*****************************************/


/***** Setup stuff ****/

GeneralConvolution::GeneralConvolution(PClip _child, double _divisor, int _nBias, const char * _matrix,
                                       bool _autoscale, IScriptEnvironment* _env)
  : GenericVideoFilter(_child), divisor(_divisor), nBias(_nBias), autoscale(_autoscale)
{
  if (!child->GetVideoInfo().IsRGB32())
    _env->ThrowError("GeneralConvolution requires RGBA input");
  if (divisor == 0.0)
    _env->ThrowError("GeneralConvolution: divisor cannot be zero");
  setMatrix(_matrix, _env);
}


AVSValue __cdecl GeneralConvolution::Create(AVSValue args, void* user_data, IScriptEnvironment* env)
{
  return new GeneralConvolution( args[0].AsClip(), args[3].AsFloat(1.0f), args[1].AsInt(0),
                                   args[2].AsString("0 0 0 0 1 0 0 0 0" ), args[4].AsBool(true), env);
}


void GeneralConvolution::setMatrix(const char * _matrix, IScriptEnvironment* env)
{
  char * copymatrix = _strdup (_matrix); // strtok mangles the input string
  char * ch = strtok (copymatrix, " \n\t\r");
  int matrix[26];
  nSize = 0;
  while (ch)
  {
    matrix[nSize++] = atoi(ch);
	if (nSize > 25) break;
    ch = strtok (NULL, " \n\t\r");
  }
  free(copymatrix);

  if (nSize < 9)
    env->ThrowError("GeneralConvolution sez: matrix too small");
  else if (nSize > 9 && nSize < 25)
    env->ThrowError("GeneralConvolution sez: invalid matrix");
  else if (nSize > 25)
    env->ThrowError("GeneralConvolution sez: matrix too big");


  // Uglify matrix storage in return for fast lookups
  if (25 == nSize)
  {
    i00 = matrix[ 0]; i10 = matrix[ 1]; i20 = matrix[ 2]; i30 = matrix[ 3]; i40 = matrix[ 4];
    i01 = matrix[ 5]; i11 = matrix[ 6]; i21 = matrix[ 7]; i31 = matrix[ 8]; i41 = matrix[ 9];
    i02 = matrix[10]; i12 = matrix[11]; i22 = matrix[12]; i32 = matrix[13]; i42 = matrix[14];
    i03 = matrix[15]; i13 = matrix[16]; i23 = matrix[17]; i33 = matrix[18]; i43 = matrix[19];
    i04 = matrix[20]; i14 = matrix[21]; i24 = matrix[22]; i34 = matrix[23]; i44 = matrix[24];
  }
  else if (9 == nSize)
  {
    i11 = matrix[0]; i21 = matrix[1]; i31 = matrix[2];
    i12 = matrix[3]; i22 = matrix[4]; i32 = matrix[5];
    i13 = matrix[6]; i23 = matrix[7]; i33 = matrix[8];

    i00 = i10 = i20 = i30 = i40 = 0;
    i01 =                   i41 = 0;
    i02 =                   i42 = 0;
    i03 =                   i43 = 0;
    i04 = i14 = i24 = i34 = i44 = 0;
  }
}

template<int mi, int ma>
__forceinline int static_clip(int value) {
  if (value < mi) {
    return mi;
  }
  if (value > ma) {
    return ma;
  }
  return value;
}

PVideoFrame __stdcall GeneralConvolution::GetFrame(int n, IScriptEnvironment* env)
{
  auto env2 = static_cast<IScriptEnvironment2*>(env);
  auto pbyA = static_cast<uint8_t*>(env2->Allocate(vi.width*vi.height, 8, AVS_POOLED_ALLOC));
  auto pbyR = static_cast<uint8_t*>(env2->Allocate(vi.width*vi.height, 8, AVS_POOLED_ALLOC));
  auto pbyG = static_cast<uint8_t*>(env2->Allocate(vi.width*vi.height, 8, AVS_POOLED_ALLOC));
  auto pbyB = static_cast<uint8_t*>(env2->Allocate(vi.width*vi.height, 8, AVS_POOLED_ALLOC));

  if ((pbyA == nullptr) || (pbyR == nullptr) || (pbyG == nullptr) || (pbyB == nullptr)) {
    env2->Free(pbyA);
    env2->Free(pbyR);
    env2->Free(pbyG);
    env2->Free(pbyB);
    env->ThrowError("GeneralConvolution: out of memory");
  }

  int h = vi.height;
  int w = vi.width;

  PVideoFrame src = child->GetFrame(n, env);
  PVideoFrame dst = env->NewVideoFrame(vi);
  char *dstStart = (char *) dst->GetWritePtr();

  const int pitch = dst->GetPitch();
  const int modulo = src->GetPitch() - src->GetRowSize();

  Pixel32* srcp = (Pixel32*)src->GetReadPtr();
  uint8_t *pbyA0 = pbyA;
  uint8_t *pbyR0 = pbyR;
  uint8_t *pbyG0 = pbyG;
  uint8_t *pbyB0 = pbyB;

  for(int y = 0; y < h; y++)
  {
    for(int x = 0; x < w; x++)
    {
      *pbyA0++ = (uint8_t)((*srcp &  0xff000000) >> 24);
      *pbyR0++ = (uint8_t)((*srcp &  0x00ff0000) >> 16);
      *pbyG0++ = (uint8_t)((*srcp &  0x0000ff00) >> 8);
      *pbyB0++ = (uint8_t)(*srcp++ & 0x000000ff);
    }
    srcp = (Pixel32 *)((char *)srcp + modulo);
  }

  int iA, iR, iG, iB, x0, x1, x2, x3, x4;

  int iCountT;
  if (autoscale) {
    iCountT = i00 + i01 + i02 + i03 + i04 +
              i10 + i11 + i12 + i13 + i14 +
              i20 + i21 + i22 + i23 + i24 +
              i30 + i31 + i32 + i33 + i34 +
              i40 + i41 + i42 + i43 + i44;
  } else {
    iCountT = 0;
  }
    
  // Truncate instead of round - keep in the spirit of the original code
  int iCountDiv = (int)(0x100000 / (iCountT == 0 ? divisor : iCountT * divisor));

  for(int y = 0; y < h; y++)
  {
    uint8_t *pbyR1, *pbyG1, *pbyB1, *pbyR2, *pbyG2, *pbyB2,
      *pbyR3, *pbyG3, *pbyB3, *pbyR4, *pbyG4, *pbyB4;

    pbyA0                                 = pbyA + y * w;
    pbyR0 = pbyR1 = pbyR2 = pbyR3 = pbyR4 = pbyR + y * w;
    pbyG0 = pbyG1 = pbyG2 = pbyG3 = pbyG4 = pbyG + y * w;
    pbyB0 = pbyB1 = pbyB2 = pbyB3 = pbyB4 = pbyB + y * w;

    if(y > 0)
    {
      pbyR4 -= w;  pbyG4 -= w;  pbyB4 -= w;
      pbyR3 -= w;  pbyG3 -= w;  pbyB3 -= w;

      if(y > 1)
      {
        pbyR4 -= w;  pbyG4 -= w;  pbyB4 -= w;
      }
    }

    if(y < h - 1)
    {
      pbyR1 += w;  pbyG1 += w;  pbyB1 += w;
      pbyR0 += w;  pbyG0 += w;  pbyB0 += w;

      if(y < h - 2)
      {
        pbyR0 += w;  pbyG0 += w;  pbyB0 += w;
      }
    }

    Pixel32* dstp = (Pixel32 *)(dstStart + y * pitch);
    for(x2 = 0; x2 < w; x2++)
    {
      x0 = x2 > 2 ? x2 - 2 : 0;
      x1 = x2 > 1 ? x2 - 1 : 0;
      x3 = x2 < w - 2 ? x2 + 1 : w - 1;
      x4 = x2 < w - 3 ? x2 + 2 : w - 2;

      iA = pbyA0[x2];
      // Always do 3x3 ring of pixel
      iR = i11 * pbyR1[x1] + i21 * pbyR1[x2] + i31 * pbyR1[x3] +
           i12 * pbyR2[x1] + i22 * pbyR2[x2] + i32 * pbyR2[x3] +
           i13 * pbyR3[x1] + i23 * pbyR3[x2] + i33 * pbyR3[x3];

      iG = i11 * pbyG1[x1] + i21 * pbyG1[x2] + i31 * pbyG1[x3] +
           i12 * pbyG2[x1] + i22 * pbyG2[x2] + i32 * pbyG2[x3] +
           i13 * pbyG3[x1] + i23 * pbyG3[x2] + i33 * pbyG3[x3];

      iB = i11 * pbyB1[x1] + i21 * pbyB1[x2] + i31 * pbyB1[x3] +
           i12 * pbyB2[x1] + i22 * pbyB2[x2] + i32 * pbyB2[x3] +
           i13 * pbyB3[x1] + i23 * pbyB3[x2] + i33 * pbyB3[x3];
      // Only do 5x5 ring of pixel if needed
      if(nSize == 25)
      {
        iR += i00 * pbyR0[x0] + i10 * pbyR0[x1] + i20 * pbyR0[x2] + i30 * pbyR0[x3] + i40 * pbyR0[x4] +
              i01 * pbyR1[x0] +                                                       i41 * pbyR1[x4] +
              i02 * pbyR2[x0] +                                                       i42 * pbyR2[x4] +
              i03 * pbyR3[x0] +                                                       i43 * pbyR3[x4] +
              i04 * pbyR4[x0] + i14 * pbyR4[x1] + i24 * pbyR4[x2] + i34 * pbyR4[x3] + i44 * pbyR4[x4];

        iG += i00 * pbyG0[x0] + i10 * pbyG0[x1] + i20 * pbyG0[x2] + i30 * pbyG0[x3] + i40 * pbyG0[x4] +
              i01 * pbyG1[x0] +                                                       i41 * pbyG1[x4] +
              i02 * pbyG2[x0] +                                                       i42 * pbyG2[x4] +
              i03 * pbyG3[x0] +                                                       i43 * pbyG3[x4] +
              i04 * pbyG4[x0] + i14 * pbyG4[x1] + i24 * pbyG4[x2] + i34 * pbyG4[x3] + i44 * pbyG4[x4];

        iB += i00 * pbyB0[x0] + i10 * pbyB0[x1] + i20 * pbyB0[x2] + i30 * pbyB0[x3] + i40 * pbyB0[x4] +
              i01 * pbyB1[x0] +                                                       i41 * pbyB1[x4] +
              i02 * pbyB2[x0] +                                                       i42 * pbyB2[x4] +
              i03 * pbyB3[x0] +                                                       i43 * pbyB3[x4] +
              i04 * pbyB4[x0] + i14 * pbyB4[x1] + i24 * pbyB4[x2] + i34 * pbyB4[x3] + i44 * pbyB4[x4];
      }

      iR = ((iR * iCountDiv) >> 20) + nBias;
      iG = ((iG * iCountDiv) >> 20) + nBias;
      iB = ((iB * iCountDiv) >> 20) + nBias;

      iR = static_clip<0, 255>(iR);
      iG = static_clip<0, 255>(iG);
      iB = static_clip<0, 255>(iB);

      *dstp++ = (iA << 24) + (iR << 16) + (iG << 8) + iB;
    }
  }

  env2->Free(pbyA);
  env2->Free(pbyR);
  env2->Free(pbyG);
  env2->Free(pbyB);

  return dst;
}
