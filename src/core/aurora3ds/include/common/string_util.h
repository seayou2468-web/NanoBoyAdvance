// Copyright Aurora3DS Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <algorithm>
#include <codecvt>
#include <locale>
#include <string>
#include <array>

namespace Common {

inline std::string UTF16ToUTF8(const std::u16string& input) {
    std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> convert;
    return convert.to_bytes(input);
}

inline std::string UTF16BufferToUTF8(const std::u16string& input) {
    auto end = std::find(input.begin(), input.end(), u'\0');
    return UTF16ToUTF8(std::u16string(input.begin(), end));
}

template <std::size_t N>
inline std::string UTF16BufferToUTF8(const std::array<char16_t, N>& input) {
    auto end = std::find(input.begin(), input.end(), u'\0');
    return UTF16ToUTF8(std::u16string(input.begin(), end));
}

inline std::u16string UTF8ToUTF16(const std::string& input) {
    std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> convert;
    return convert.from_bytes(input);
}

inline void TruncateString(std::string& value) {
    auto nul = value.find('\0');
    if (nul != std::string::npos) {
        value.resize(nul);
    }
}

inline void SplitPath(const std::string& full_path, std::string* out_dir, std::string* out_name,
                      std::string* out_ext) {
    const auto slash = full_path.find_last_of("/\\");
    const std::string filename = slash == std::string::npos ? full_path : full_path.substr(slash + 1);

    if (out_dir) {
        *out_dir = slash == std::string::npos ? std::string{} : full_path.substr(0, slash);
    }

    const auto dot = filename.find_last_of('.');
    if (out_name) {
        *out_name = dot == std::string::npos ? filename : filename.substr(0, dot);
    }
    if (out_ext) {
        *out_ext = dot == std::string::npos ? std::string{} : filename.substr(dot);
    }
}

} // namespace Common
