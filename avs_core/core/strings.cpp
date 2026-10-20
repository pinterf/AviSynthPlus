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

#include <avs/config.h>
#ifdef AVS_WINDOWS
#include <io.h>
#include <avs/win.h>
#else
#include <avs/posix.h>
#include "parser/os/win32_string_compat.h"
#endif
#include <avs/filesystem.h>

#include "strings.h"
#include <cassert>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <algorithm>
#include <codecvt>
#include <locale>
#include <vector>


bool streqi(const char* s1, const char* s2)
{
  // Why we dont use Windows's lstrcmpi? It is by multiple orders of magnitude slower and non-portable.
  // lstrcmpi handles locales and UTF and whatnot, but because variable and function names in Avisynth
  // are limited to ASCII, we don't need that funcationality.

  while(1)
  {
    if ((*s1 == 0) && (*s2 == 0))
      return true;

    if (asciiToLower(*s1) != asciiToLower(*s2))
      return false;

    ++s1;
    ++s2;
  }

  assert(0);
  return false;
}

std::string concat(const std::string &s1, const std::string &s2)
{
  std::string ret(s1);
  return ret.append(s2);
}

bool replace(std::string &haystack, const std::string &needle, const std::string &newStr)
{
  bool replaced = false;
  for(size_t pos = 0; ; pos += newStr.length())
  {
    // Locate the substring to replace
    pos = haystack.find(needle, pos);
    if(pos == std::string::npos) break;
    // Replace by erasing and inserting
    haystack.erase(pos, needle.length());
    haystack.insert(pos, newStr);
    replaced = true;
  }

  return replaced;
}

bool replace_beginning(std::string &haystack, const std::string &needle, const std::string &newStr)
{
  // Locate the substring to replace
  size_t pos = haystack.find(needle);
  if(pos == std::string::npos) return false;
  if(pos != 0) return false;

  // Replace by erasing and inserting
  haystack.erase( pos, needle.length() );
  haystack.insert( pos, newStr );

  return true;
}

bool replace(std::string &haystack, char needle, char newChar)
{
  std::string haystack_bck = haystack;
  std::replace(haystack.begin(),haystack.end(), needle, newChar);
  return haystack.compare(haystack_bck) != 0;
}

const char* ws = " \t\n\r\f\v";

std::string trim(const std::string& s)
{
  auto wsfront = std::find_if_not(s.begin(), s.end(), [](char c) {return isAsciiSpace(c); });
  auto wsback = std::find_if_not(s.rbegin(), s.rend(), [](char c) {return isAsciiSpace(c); }).base();
  return (wsback <= wsfront ? std::string() : std::string(wsfront, wsback));
}

/***********************************
 *******   wchar_t-utf-ansi   ******
 **********************************/

#ifdef AVS_WINDOWS
std::unique_ptr<char[]> AnsiToUtf8(const char* input)
{
  int wlen = MultiByteToWideChar(AreFileApisANSI() ? CP_ACP : CP_OEMCP, 0, input, -1, NULL, 0);
  wchar_t* wstr = new wchar_t[wlen];
  MultiByteToWideChar(AreFileApisANSI() ? CP_ACP : CP_OEMCP, 0, input, -1, wstr, wlen);
  const auto utf8len = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, NULL, 0, NULL, NULL);
  // The length comes with the \0 terminator
  // Allocate a buffer for the UTF-8 string
  auto s_utf8 = std::make_unique<char[]>(utf8len);
  // Convert the UTF-16 string from UTF-8
  WideCharToMultiByte(CP_UTF8, 0, wstr, -1, s_utf8.get(), (int)utf8len, NULL, NULL);
  delete[] wstr;
  return s_utf8;
}

std::unique_ptr<char[]> WideCharToUtf8(const wchar_t* w_string)
{
  const auto utf8len = WideCharToMultiByte(CP_UTF8, 0, w_string, -1, NULL, 0, 0, 0) - 1; // w/o the \0 terminator
  auto s_utf8 = std::make_unique<char[]>(utf8len + 1);
  WideCharToMultiByte(CP_UTF8, 0, w_string, -1, s_utf8.get(), (int)utf8len + 1, 0, 0);
  return s_utf8;
}

std::unique_ptr<char[]> WideCharToAnsi(const wchar_t* w_string)
{
  const auto len = wcslen(w_string);
  auto s_ansi = std::make_unique<char[]>(len + 1);
  WideCharToMultiByte(AreFileApisANSI() ? CP_ACP : CP_OEMCP, 0, w_string, -1, s_ansi.get(), (int)len + 1, NULL, NULL); // replaces out-of-CP chars by ?
  // int succ = wcstombs(s_ansi, w_string, len +1);
  // no good, stops at non-replacable unicode chars. If wcstombs encounters a wide character it cannot convert to a multibyte character, it returns 1 cast to type size_t and sets errno to EILSEQ.
  return s_ansi;
}

std::unique_ptr<char[]> WideCharToAnsiACP(const wchar_t* w_string)
{
  const auto len = wcslen(w_string);
  auto s_ansi = std::make_unique<char[]>(len + 1);
  WideCharToMultiByte(CP_ACP, 0, w_string, -1, s_ansi.get(), (int)len + 1, NULL, NULL); // replaces out-of-CP chars by ?
  // int succ = wcstombs(s_ansi, w_string, len +1);
  // no good, stops at non-replacable unicode chars. If wcstombs encounters a wide character it cannot convert to a multibyte character, it returns 1 cast to type size_t and sets errno to EILSEQ.
  return s_ansi;
}

std::unique_ptr<char[]> WideCharToUtf8_maxn(const wchar_t* w_string, size_t maxn)
{
  const auto utf8len = WideCharToMultiByte(CP_UTF8, 0, w_string, (int)maxn, NULL, 0, 0, 0); // no \0 terminator check requested here
  auto s_utf8 = std::make_unique<char[]>(utf8len + 1);
  WideCharToMultiByte(CP_UTF8, 0, w_string, -1, s_utf8.get(), utf8len, 0, 0);
  s_utf8[utf8len] = 0;
  return s_utf8;
}

std::unique_ptr<char[]> WideCharToAnsi_maxn(const wchar_t* w_string, size_t maxn)
{
  auto s_ansi = std::make_unique<char[]>(maxn + 1);
  WideCharToMultiByte(AreFileApisANSI() ? CP_ACP : CP_OEMCP, 0, w_string, -1, s_ansi.get(), (int)maxn, NULL, NULL); // replaces out-of-CP chars by ?
  s_ansi[maxn] = 0;
  return s_ansi;
}

std::unique_ptr<wchar_t[]> AnsiToWideChar(const char* s_ansi)
{
  const size_t bufsize = strlen(s_ansi) + 1;
  auto w_string = std::make_unique<wchar_t[]>(bufsize);
  MultiByteToWideChar(AreFileApisANSI() ? CP_ACP : CP_OEMCP, 0, s_ansi, -1, w_string.get(), (int)bufsize);
  //mbstowcs(script_name_w, script_name, len); // ansi to wchar_t, does not convert properly out-of-the box
  return w_string;
}

std::unique_ptr<wchar_t[]> AnsiToWideCharACP(const char* s_ansi)
{
  const size_t bufsize = strlen(s_ansi) + 1;
  auto w_string = std::make_unique<wchar_t[]>(bufsize);
  MultiByteToWideChar(CP_ACP, 0, s_ansi, -1, w_string.get(), (int)bufsize);
  //mbstowcs(script_name_w, script_name, len); // ansi to wchar_t, does not convert properly out-of-the box
  return w_string;
}

std::unique_ptr<wchar_t[]> Utf8ToWideChar(const char* s_utf8)
{
  const size_t wchars_count = MultiByteToWideChar(CP_UTF8, 0, s_utf8, -1, NULL, 0);
  const size_t bufsize = wchars_count + 1;
  auto w_string = std::make_unique<wchar_t[]>(bufsize);
  MultiByteToWideChar(CP_UTF8, 0, s_utf8, -1, w_string.get(), (int)bufsize);
  return w_string;
}

std::string Utf8ToAnsi(const char* s_utf8)
{
  std::string s;
  auto w = Utf8ToWideChar(s_utf8);
  auto ansi = WideCharToAnsi(w.get()); // replaces out-of-CP chars by ?
  s = ansi.get();
  return s;
}

#endif

// Shortest decimal digits which read back to the same value, laid out like Python's repr().
// min_digits: 15 for double (DBL_DIG), 6 for float (FLT_DIG); max_digits: 17 for double, 9 for float;
// is_float: round-trip check in 32 bit float precision.
// Starting at DBL_DIG/FLT_DIG is enough: when the shortest form has k <= DBL_DIG digits, printing with
// DBL_DIG digits gives the same k digits followed by zeros (the value is closer to its shortest decimal
// than half a unit in the DBL_DIG-th digit), and the trailing zeros are stripped below.
// So at most 3 (float: 4) print + read back rounds instead of up to 17.
// Not for subnormals (below DBL_MIN / FLT_MIN): they have fewer significant bits, so they start from 1 digit.
static std::string shortest_to_string(double d, int min_digits, int max_digits, bool is_float)
{
  if (std::isnan(d))
    return "nan";
  if (std::isinf(d))
    return d < 0 ? "-inf" : "inf";
  if (d == 0)
    return std::signbit(d) ? "-0.0" : "0.0";

  // find the shortest precision which round-trips
  std::string digits; // significant digits, without the decimal point
  int exponent = 0; // decimal exponent of the first digit
  const bool subnormal = std::fabs(d) < (is_float ? (double)FLT_MIN : DBL_MIN);
  for (int precision = subnormal ? 1 : min_digits; precision <= max_digits; precision++) {
    char buf[40];
    snprintf(buf, sizeof(buf), "%.*e", precision - 1, std::fabs(d)); // e.g. "1.5e-05"
    // collect the digits, skipping the decimal point (which can be locale dependent)
    digits.clear();
    const char* p = buf;
    for (; *p && *p != 'e' && *p != 'E'; p++)
      if (isAsciiDigit(*p))
        digits += *p;
    exponent = (int)strtol(p + 1, nullptr, 10);
    // read back without a decimal point, so locale independent
    const std::string number = digits + "e" + std::to_string(exponent - (int)(digits.size() - 1));
    const bool same = is_float ?
      std::strtof(number.c_str(), nullptr) == (float)std::fabs(d) :
      std::strtod(number.c_str(), nullptr) == std::fabs(d);
    if (same)
      break;
  }
  while (digits.size() > 1 && digits.back() == '0')
    digits.pop_back();

  std::string s = d < 0 ? "-" : "";
  const int ndigits = (int)digits.size();
  if (exponent < -4 || exponent >= 16) {
    // 1e-05, 1.5e+16
    s += digits[0];
    if (ndigits > 1) {
      s += '.';
      s += digits.substr(1);
    }
    char e[8];
    snprintf(e, sizeof(e), "e%c%02d", exponent < 0 ? '-' : '+', std::abs(exponent));
    s += e;
  }
  else if (exponent < 0) {
    // 0.0001, 0.015
    s += "0.";
    s += std::string(-exponent - 1, '0');
    s += digits;
  }
  else {
    // 1.0, 1.5, 100.0, 1234567890123456.0
    if (ndigits > exponent + 1) {
      s += digits.substr(0, exponent + 1);
      s += '.';
      s += digits.substr(exponent + 1);
    }
    else {
      s += digits;
      s += std::string(exponent + 1 - ndigits, '0');
      s += ".0";
    }
  }
  return s;
}

std::string double_to_string(double d)
{
  return shortest_to_string(d, DBL_DIG, 17, false);
}

std::string float_to_string(float f)
{
  return shortest_to_string(f, FLT_DIG, 9, true);
}

std::string GetFullPathNameWrapUtf8(const std::string& f)
{
#ifdef AVS_WINDOWS
  // assume f is utf-8
  auto f_wide = Utf8ToWideChar(f.c_str());
  auto p = fs::absolute(fs::path(f_wide.get()).lexically_normal());
  auto w = p.wstring(); // UTF-16 on Windows
  auto utf8 = WideCharToUtf8(w.c_str());
  return std::string(utf8.get());
#else
  return fs::absolute(fs::path(f).lexically_normal()).generic_string();
#endif
}

size_t str_utf8_size(const std::string& s) {
  // Does not handle combined codepoints, e.g. diacritic mark modifications
  // Leading UTF8 byte:
  // 1 byte  0xxxxxxx
  // 2 bytes 110xxxxx
  // 3 bytes 1110xxxx
  // 4 bytes 11110xxx
  size_t len = 0;
  for (char c : s) {
    if ((c & 0xc0) != 0x80) // Not a trailing byte 10xxxxxx
      len++;
  }
  return len;
}

// converts a unicode codepoint to its utf8 string representation
// empty string for a value which is not a valid codepoint: surrogates D800-DFFF, above 10FFFF
std::string U32_to_utf8(uint32_t cp)
{
  if ((cp >= 0xD800 && cp <= 0xDFFF) || cp > 0x10FFFF)
    return std::string();
  uint8_t bytes[4];
  int size = 0;
  if (cp <= 0x007F) {
    // one byte, 0xxxxxxx
    bytes[0] = static_cast<uint8_t>(cp);
    size = 1;
  }
  else if (cp <= 0x07FF) {
    // two bytes, 110xxxxx 10xxxxxx
    bytes[0] = static_cast<uint8_t>(0xC0 | ((cp >> 6) & 0x1F));
    bytes[1] = static_cast<uint8_t>(0x80 | (cp & 0x3F));
    size = 2;
  }
  else if (cp <= 0xFFFF) {
    // three bytes, 1110xxxx 10xxxxxx 10xxxxxx
    bytes[0] = static_cast<uint8_t>(0xE0 | ((cp >> 12) & 0x0F));
    bytes[1] = static_cast<uint8_t>(0x80 | ((cp >> 6) & 0x3F));
    bytes[2] = static_cast<uint8_t>(0x80 | (cp & 0x3F));
    size = 3;
  }
  else {
    // four bytes, 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
    bytes[0] = static_cast<uint8_t>(0xF0 | ((cp >> 18) & 0x07));
    bytes[1] = static_cast<uint8_t>(0x80 | ((cp >> 12) & 0x3F));
    bytes[2] = static_cast<uint8_t>(0x80 | ((cp >> 6) & 0x3F));
    bytes[3] = static_cast<uint8_t>(0x80 | (cp & 0x3F));
    size = 4;
  }
  // convert the array of bytes to a string
  std::string u8str(bytes, bytes + size);
  return u8str;
}

// for posix: always assume utf8
std::string charToUtf8(const char* text, bool utf8)
{
  std::string s;
  // AVS_POSIX: utf8 is always true, no ANSI here
#ifdef AVS_POSIX
  utf8 = true;
#endif
  if (utf8) {
    s = text; // no change
  }
#ifdef AVS_WINDOWS
  else {
    // ANSI (or system local Active CP, since Win10 it can be UTF8), Windows
    auto source = AnsiToUtf8(text);
    s = source.get();
  }
#endif
  return s;
}
