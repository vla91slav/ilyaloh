// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "string_utils.h"

#include <array>
#include <cctype>
#include <codecvt>
#include <locale>
#include <regex>
#include <sstream>

#include "third_party/dart/runtime/third_party/double-conversion/src/double-conversion.h"

#if defined(_WIN32)
#include "base/win/string_conversion.h"
#endif

#include "no_destructor.h"

namespace base {

using double_conversion::DoubleToStringConverter;
using double_conversion::StringBuilder;

namespace {
constexpr char kExponentChar = 'e';
constexpr char kInfinitySymbol[] = "Infinity";
constexpr char kNaNSymbol[] = "NaN";

// The number of digits after the decimal we allow before switching to
// exponential representation.
constexpr int kDecimalInShortestLow = -6;
// The number of digits before the decimal we allow before switching to
// exponential representation.
constexpr int kDecimalInShortestHigh = 12;
constexpr int kConversionFlags =
    DoubleToStringConverter::EMIT_POSITIVE_EXPONENT_SIGN;

const DoubleToStringConverter& GetDoubleToStringConverter() {
  static DoubleToStringConverter converter(
      kConversionFlags, kInfinitySymbol, kNaNSymbol, kExponentChar,
      kDecimalInShortestLow, kDecimalInShortestHigh, 0, 0);
  return converter;
}

std::string NumberToStringImpl(double number, bool is_single_precision) {
  if (number == 0.0) {
    return "0";
  }

  constexpr int kBufferSize = 128;
  std::array<char, kBufferSize> char_buffer;
  StringBuilder builder(char_buffer.data(), char_buffer.size());
  if (is_single_precision) {
    GetDoubleToStringConverter().ToShortestSingle(static_cast<float>(number),
                                                  &builder);
  } else {
    GetDoubleToStringConverter().ToShortest(number, &builder);
  }
  return std::string(char_buffer.data(), builder.position());
}
}  // namespace

std::u16string ASCIIToUTF16(std::string src) {
  return std::u16string(src.begin(), src.end());
}

std::u16string UTF8ToUTF16(std::string src) {
#if defined(_WIN32)
  return WideToUTF16(win::Utf16FromUtf8(src));
#else
  std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> convert;
  return convert.from_bytes(src);
#endif
}

std::string UTF16ToUTF8(std::u16string src) {
#if defined(_WIN32)
  return win::Utf8FromUtf16(UTF16ToWide(src));
#else
  std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> convert;
  return convert.to_bytes(src);
#endif
}

std::u16string WideToUTF16(const std::wstring& src) {
  return std::u16string(src.begin(), src.end());
}

std::wstring UTF16ToWide(const std::u16string& src) {
  return std::wstring(src.begin(), src.end());
}

std::u16string NumberToString16(float number) {
  return ASCIIToUTF16(NumberToString(number));
}

std::u16string NumberToString16(int32_t number) {
  return ASCIIToUTF16(NumberToString(number));
}

std::u16string NumberToString16(unsigned int number) {
  return ASCIIToUTF16(NumberToString(number));
}

std::u16string NumberToString16(double number) {
  return ASCIIToUTF16(NumberToString(number));
}

std::string NumberToString(int32_t number) {
  return std::to_string(number);
}

std::string NumberToString(unsigned int number) {
  return std::to_string(number);
}

std::string NumberToString(float number) {
  return NumberToStringImpl(number, true);
}

std::string NumberToString(double number) {
  return NumberToStringImpl(number, false);
}

std::string JoinString(std::vector<std::string> tokens, std::string delimiter) {
  std::ostringstream imploded;
  for (size_t i = 0; i < tokens.size(); i++) {
    if (i == tokens.size() - 1) {
      imploded << tokens[i];
    } else {
      imploded << tokens[i] << delimiter;
    }
  }
  return imploded.str();
}

std::u16string JoinString(std::vector<std::u16string> tokens,
                          std::u16string delimiter) {
  std::u16string result;
  for (size_t i = 0; i < tokens.size(); i++) {
    if (i == tokens.size() - 1) {
      result.append(tokens[i]);
    } else {
      result.append(tokens[i]);
      result.append(delimiter);
    }
  }
  return result;
}

void ReplaceChars(std::string in,
                  std::string from,
                  std::string to,
                  std::string* out) {
  size_t pos = in.find(from);
  while (pos != std::string::npos) {
    in.replace(pos, from.size(), to);
    pos = in.find(from, pos + to.size());
  }
  *out = in;
}

void ReplaceChars(std::u16string in,
                  std::u16string from,
                  std::u16string to,
                  std::u16string* out) {
  size_t pos = in.find(from);
  while (pos != std::u16string::npos) {
    in.replace(pos, from.size(), to);
    pos = in.find(from, pos + to.size());
  }
  *out = in;
}

const std::string& EmptyString() {
  static const base::NoDestructor<std::string> s;
  return *s;
}

std::string ToUpperASCII(std::string str) {
  std::string ret;
  ret.reserve(str.size());
  for (size_t i = 0; i < str.size(); i++)
    ret.push_back(std::toupper(str[i]));
  return ret;
}

std::string ToLowerASCII(std::string str) {
  std::string ret;
  ret.reserve(str.size());
  for (size_t i = 0; i < str.size(); i++)
    ret.push_back(std::tolower(str[i]));
  return ret;
}

bool LowerCaseEqualsASCII(std::string a, std::string b) {
  std::string lower_a = ToLowerASCII(a);
  return lower_a.compare(ToLowerASCII(b)) == 0;
}

bool ContainsOnlyChars(std::u16string str, char16_t ch) {
  return std::find_if(str.begin(), str.end(),
                      [ch](char16_t c) { return c != ch; }) == str.end();
}

}  // namespace base
