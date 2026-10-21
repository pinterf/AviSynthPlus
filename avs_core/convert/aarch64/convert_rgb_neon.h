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

#ifndef __Convert_RGB_NEON_H__
#define __Convert_RGB_NEON_H__

#include <avs/types.h>
#include <cstddef>

void convert_rgb24_to_rgb32_neon(const BYTE* srcp, BYTE* dstp, size_t src_pitch, size_t dst_pitch, size_t width, size_t height);
void convert_rgb48_to_rgb64_neon(const BYTE* srcp, BYTE* dstp, size_t src_pitch, size_t dst_pitch, size_t width, size_t height);
void convert_rgb32_to_rgb24_neon(const BYTE* srcp, BYTE* dstp, size_t src_pitch, size_t dst_pitch, size_t width, size_t height);
void convert_rgb64_to_rgb48_neon(const BYTE* srcp, BYTE* dstp, size_t src_pitch, size_t dst_pitch, size_t width, size_t height);

#endif // __Convert_RGB_NEON_H__
