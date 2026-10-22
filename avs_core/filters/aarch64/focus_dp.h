// AviSynth+.  Copyright 2026 AviSynth+ Project
// https://avs-plus.net
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

#ifndef __Focus_DP_H__
#define __Focus_DP_H__

#include <avs/types.h>
#include <cstddef>
#include <cstdint>

// 8 bit SAD with UDOT (FEAT_DotProd, Armv8.2+): compiled with -march=armv8.2-a+dotprod (*_dp.cpp),
// called only when CPUF_ARM_DOTPROD is set. Same result as calculate_sad_8_or_16_neon<uint8_t, packedRGB3264>.
template<bool packedRGB3264>
int64_t calculate_sad_8_dp(const BYTE* cur_ptr, const BYTE* other_ptr, int cur_pitch, int other_pitch, size_t rowsize, size_t height);

#endif // __Focus_DP_H__
