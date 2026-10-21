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

#ifndef AVSCORE_STRINGS_H
#define AVSCORE_STRINGS_H

#include <string>
#include <memory>
#include <cstdint>
#include <cstdarg>

bool streqi(const char* s1, const char* s2);
std::string concat(const std::string &s1, const std::string &s2);
bool replace_beginning(std::string &_haystack, const std::string &needle, const std::string &newStr);
bool replace(std::string &haystack, const std::string &needle, const std::string &newStr);
bool replace(std::string &haystack, char needle, char newChar);
std::string trim(const std::string& s);

// ASCII-only character classes for parsers, instead of <cctype>: no locale dependency,
// and no undefined behavior for negative (signed char) values, e.g. UTF-8 bytes >= 0x80.
static inline bool isAsciiDigit(char c) {
  return c >= '0' && c <= '9';
}

static inline bool isAsciiAlpha(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static inline bool isAsciiAlnum(char c) {
  return isAsciiAlpha(c) || isAsciiDigit(c);
}

static inline bool isAsciiSpace(char c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

static inline char asciiToLower(char c) {
  return (c >= 'A' && c <= 'Z') ? c + ('a' - 'A') : c;
}

static inline char asciiToUpper(char c) {
  return (c >= 'a' && c <= 'z') ? c - ('a' - 'A') : c;
}

// identifier start and continuation characters
static inline bool isAlphaUnderscore(char c) {
  return isAsciiAlpha(c) || c == '_';
}

static inline bool isAlphaNumUnderscore(char c) {
  return isAsciiAlnum(c) || c == '_';
}

#ifdef AVS_WINDOWS
std::unique_ptr<char[]> AnsiToUtf8(const char* input);
std::unique_ptr<char[]> WideCharToUtf8(const wchar_t* w_string);
std::unique_ptr<char[]> WideCharToAnsi(const wchar_t* w_string);
std::unique_ptr<char[]> WideCharToAnsiACP(const wchar_t* w_string);
std::unique_ptr<char[]> WideCharToUtf8_maxn(const wchar_t* w_string, size_t maxn);
std::unique_ptr<char[]> WideCharToAnsi_maxn(const wchar_t* w_string, size_t maxn);
std::unique_ptr<wchar_t[]> AnsiToWideChar(const char* s_ansi);
std::unique_ptr<wchar_t[]> AnsiToWideCharACP(const char* s_ansi);
std::unique_ptr<wchar_t[]> Utf8ToWideChar(const char* s_utf8);
std::string Utf8ToAnsi(const char* s_utf8);
#endif

std::string GetFullPathNameWrapUtf8(const std::string& f);

// Shortest representation which reads back to the same value, like Python's repr():
// 1.5, 1.0, 0.0001, 1e-05, 1.5e+16, nan, inf. Locale independent ('.' as decimal separator).
// The float version gives the shortest digits for the 32 bit value (0.1f -> "0.1").
std::string double_to_string(double d);
std::string float_to_string(float f);

// Decimal number: digits [. digits] [e|E [+|-] digits], correctly rounded, '.' as the decimal point
// whatever the process locale. No whitespace, sign, hex, inf, nan (script parser, strtod_c).
// Returns the end of the number; p itself if there were no digits (value 0).
const char* scan_decimal(const char* p, double* value);

// printf-like formatting with '.' as the decimal point, whatever the process locale
// (a host or plugin may call setlocale)
std::string vsprintf_c(const char* fmt, va_list args);
std::string sprintf_c(const char* fmt, ...);

// strtod with '.' as the decimal point, whatever the process locale (a host or plugin may call
// setlocale). Leading ASCII whitespace, sign, digits, '.', digits, e/E exponent; hex, inf, nan as strtod.
// end (optional): the first character not used; s itself if no number was found (result 0).
double strtod_c(const char* s, const char** end = nullptr);

size_t str_utf8_size(const std::string& s);
std::string U32_to_utf8(uint32_t cp); // empty for surrogates and above 10FFFF
std::string charToUtf8(const char* text, bool utf8);

#endif // AVSCORE_STRINGS_H
