// Copyright Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/string_util.h"

#include <algorithm>
#include <cctype>
#include <codecvt>
#include <locale>

namespace Common {

char ToLower(char c) {
    return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
}

char ToUpper(char c) {
    return static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
}

std::string ToLower(std::string str) {
    std::transform(str.begin(), str.end(), str.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return str;
}

std::string ToUpper(std::string str) {
    std::transform(str.begin(), str.end(), str.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return str;
}

std::string StripSpaces(const std::string& str) {
    const std::size_t start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return {};
    }
    return str.substr(start, str.find_last_not_of(" \t\r\n") - start + 1);
}

std::string StripQuotes(const std::string& s) {
    if (s.size() >= 2 && s.front() == '"' && s.back() == '"') {
        return s.substr(1, s.size() - 2);
    }
    return s;
}

std::string StringFromBool(bool value) {
    return value ? "True" : "False";
}

std::string TabsToSpaces(int tab_size, std::string in) {
    std::string spaces(static_cast<std::size_t>(std::max(tab_size, 0)), ' ');
    std::size_t pos = 0;
    while ((pos = in.find('\t', pos)) != std::string::npos) {
        in.replace(pos, 1, spaces);
        pos += spaces.size();
    }
    return in;
}

bool EndsWith(const std::string& value, const std::string& ending) {
    if (ending.size() > value.size()) {
        return false;
    }
    return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
}

std::vector<std::string> SplitString(const std::string& str, char delim) {
    std::vector<std::string> out;
    std::string current;
    for (char c : str) {
        if (c == delim) {
            out.push_back(current);
            current.clear();
        } else {
            current.push_back(c);
        }
    }
    out.push_back(current);
    return out;
}

bool SplitPath(const std::string& full_path, std::string* path, std::string* filename,
               std::string* extension) {
    if (full_path.empty()) {
        return false;
    }

    const std::size_t dir_end = full_path.find_last_of("/\\");
    const std::size_t fname_start = (dir_end == std::string::npos) ? 0 : dir_end + 1;
    const std::size_t ext_start = full_path.find_last_of('.');

    if (path) {
        *path = (dir_end == std::string::npos) ? "" : full_path.substr(0, dir_end + 1);
    }
    if (filename) {
        const std::size_t len =
            (ext_start != std::string::npos && ext_start >= fname_start) ? ext_start - fname_start
                                                                          : std::string::npos;
        *filename = full_path.substr(fname_start, len);
    }
    if (extension) {
        *extension = (ext_start == std::string::npos || ext_start < fname_start)
                         ? ""
                         : full_path.substr(ext_start);
    }
    return true;
}

void BuildCompleteFilename(std::string& complete_filename, const std::string& path,
                           const std::string& filename) {
    complete_filename = path;
    if (!complete_filename.empty() && complete_filename.back() != '/' &&
        complete_filename.back() != '\\') {
        complete_filename.push_back('/');
    }
    complete_filename += filename;
}

std::string ReplaceAll(std::string result, const std::string& src, const std::string& dest) {
    if (src.empty()) {
        return result;
    }
    std::size_t pos = 0;
    while ((pos = result.find(src, pos)) != std::string::npos) {
        result.replace(pos, src.length(), dest);
        pos += dest.length();
    }
    return result;
}

std::string UTF16ToUTF8(std::u16string_view input) {
    if (input.empty()) {
        return {};
    }
    std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> conv;
    return conv.to_bytes(input.data(), input.data() + input.size());
}

std::u16string UTF8ToUTF16(std::string_view input) {
    if (input.empty()) {
        return {};
    }
    std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> conv;
    return conv.from_bytes(input.data(), input.data() + input.size());
}

std::string StringFromFixedZeroTerminatedBuffer(const char* buffer, std::size_t max_len) {
    std::size_t length = 0;
    while (length < max_len && buffer[length] != '\0') {
        ++length;
    }
    return {buffer, length};
}

}  // namespace Common
