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

#ifndef __Focus_NEON_H__
#define __Focus_NEON_H__

#include <avs/types.h>
#include <cstddef>
#include <cstdint>

// Blur/Sharpen horizontal, planar, in-place. Same arithmetic as af_horizontal_planar_c / _float_c.
void af_horizontal_planar_neon(BYTE* dstp, size_t height, size_t pitch, size_t row_size, size_t half_amount);
void af_horizontal_planar_uint16_t_neon(BYTE* dstp, size_t height, size_t pitch, size_t row_size, size_t half_amount, int bits_per_pixel);
void af_horizontal_planar_float_neon(BYTE* dstp, size_t height, size_t pitch, size_t row_size, float amount);

// TemporalSoften: one row of the center frame averaged with the other frames, in-place. Same arithmetic as accumulate_line_c.
template<typename pixel_t, bool maxThreshold>
void accumulate_line_neon(BYTE* c_plane, const BYTE** planeP, int planes, size_t rowsize, BYTE threshold, int div, int bits_per_pixel);

// Sum of absolute differences, 8 / 16 bit, rowsize in bytes; packedRGB3264: alpha skipped.
// Same result as calculate_sad_c / get_sad_c / get_sad_rgb_c and calculate_sad_8_or_16_sse2.
template<typename pixel_t, bool packedRGB3264>
int64_t calculate_sad_8_or_16_neon(const BYTE* cur_ptr, const BYTE* other_ptr, int cur_pitch, int other_pitch, size_t rowsize, size_t height);

#endif // __Focus_NEON_H__
