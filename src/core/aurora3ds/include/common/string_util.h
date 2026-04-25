// Copyright Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>
#include "common/common_types.h"
#include "common/swap.h"

namespace Common {

[[nodiscard]] char ToLower(char c);
[[nodiscard]] char ToUpper(char c);
[[nodiscard]] std::string ToLower(std::string str);
[[nodiscard]] std::string ToUpper(std::string str);

[[nodiscard]] std::string StripSpaces(const std::string& s);
[[nodiscard]] std::string StripQuotes(const std::string& s);
[[nodiscard]] std::string StringFromBool(bool value);
[[nodiscard]] std::string TabsToSpaces(int tab_size, std::string in);
[[nodiscard]] bool EndsWith(const std::string& value, const std::string& ending);
[[nodiscard]] std::vector<std::string> SplitString(const std::string& str, char delim);

// "C:/Windows/winhelp.exe" to "C:/Windows/", "winhelp", ".exe"
bool SplitPath(const std::string& full_path, std::string* path, std::string* filename,
               std::string* extension);
void BuildCompleteFilename(std::string& complete_filename, const std::string& path,
                           const std::string& filename);
[[nodiscard]] std::string ReplaceAll(std::string result, const std::string& src,
                                     const std::string& dest);

[[nodiscard]] std::string UTF16ToUTF8(std::u16string_view input);
[[nodiscard]] std::u16string UTF8ToUTF16(std::string_view input);

template <typename InIt>
[[nodiscard]] bool ComparePartialString(InIt begin, InIt end, const char* other) {
    for (; begin != end && *other != '\0'; ++begin, ++other) {
        if (*begin != *other) {
            return false;
        }
    }
    return (begin == end) == (*other == '\0');
}

template <typename T>
std::string UTF16BufferToUTF8(const T& text) {
    const auto text_end = std::find(text.begin(), text.end(), u'\0');
    const std::size_t text_size = std::distance(text.begin(), text_end);
    std::u16string buffer(text_size, 0);
    std::transform(text.begin(), text_end, buffer.begin(), [](u16_le character) {
        return static_cast<char16_t>(static_cast<u16>(character));
    });
    return UTF16ToUTF8(buffer);
}

inline void TruncateString(std::string& str) {
    while (!str.empty() && str.back() == '\0') {
        str.pop_back();
    }
}

[[nodiscard]] std::string StringFromFixedZeroTerminatedBuffer(const char* buffer,
                                                              std::size_t max_len);

}  // namespace Common
