// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <array>
#include <string_view>
#include "common/logging/log.h"
#include "web_service/verify_user_jwt.h"

namespace WebService {
namespace {

std::string Base64UrlDecode(std::string_view input) {
    std::string normalized(input);
    std::replace(normalized.begin(), normalized.end(), '-', '+');
    std::replace(normalized.begin(), normalized.end(), '_', '/');
    while (normalized.size() % 4 != 0) {
        normalized.push_back('=');
    }

    std::array<unsigned char, 256> table;
    table.fill(0xFF);
    for (int i = 0; i < 26; ++i) {
        table[static_cast<unsigned char>('A' + i)] = static_cast<unsigned char>(i);
        table[static_cast<unsigned char>('a' + i)] = static_cast<unsigned char>(26 + i);
    }
    for (int i = 0; i < 10; ++i) {
        table[static_cast<unsigned char>('0' + i)] = static_cast<unsigned char>(52 + i);
    }
    table[static_cast<unsigned char>('+')] = 62;
    table[static_cast<unsigned char>('/')] = 63;

    std::string out;
    out.reserve((normalized.size() / 4) * 3);
    for (std::size_t i = 0; i < normalized.size(); i += 4) {
        const unsigned char c0 = table[static_cast<unsigned char>(normalized[i + 0])];
        const unsigned char c1 = table[static_cast<unsigned char>(normalized[i + 1])];
        const unsigned char c2 = normalized[i + 2] == '='
                                     ? 0
                                     : table[static_cast<unsigned char>(normalized[i + 2])];
        const unsigned char c3 = normalized[i + 3] == '='
                                     ? 0
                                     : table[static_cast<unsigned char>(normalized[i + 3])];
        if (c0 == 0xFF || c1 == 0xFF || (normalized[i + 2] != '=' && c2 == 0xFF) ||
            (normalized[i + 3] != '=' && c3 == 0xFF)) {
            return {};
        }

        const unsigned int value =
            (static_cast<unsigned int>(c0) << 18) | (static_cast<unsigned int>(c1) << 12) |
            (static_cast<unsigned int>(c2) << 6) | static_cast<unsigned int>(c3);
        out.push_back(static_cast<char>((value >> 16) & 0xFF));
        if (normalized[i + 2] != '=') {
            out.push_back(static_cast<char>((value >> 8) & 0xFF));
        }
        if (normalized[i + 3] != '=') {
            out.push_back(static_cast<char>(value & 0xFF));
        }
    }
    return out;
}

std::string ExtractJSONString(std::string_view json, std::string_view key) {
    const std::string token = "\"" + std::string(key) + "\"";
    const std::size_t k = json.find(token);
    if (k == std::string_view::npos) {
        return {};
    }
    const std::size_t colon = json.find(':', k + token.size());
    if (colon == std::string_view::npos) {
        return {};
    }
    const std::size_t start = json.find('"', colon + 1);
    if (start == std::string_view::npos) {
        return {};
    }
    const std::size_t end = json.find('"', start + 1);
    if (end == std::string_view::npos || end <= start) {
        return {};
    }
    return std::string(json.substr(start + 1, end - start - 1));
}

bool ContainsStringInArray(std::string_view json, std::string_view key, std::string_view value) {
    const std::string token = "\"" + std::string(key) + "\"";
    const std::size_t k = json.find(token);
    if (k == std::string_view::npos) {
        return false;
    }
    const std::size_t array_start = json.find('[', k + token.size());
    const std::size_t array_end = json.find(']', k + token.size());
    if (array_start == std::string_view::npos || array_end == std::string_view::npos ||
        array_end <= array_start) {
        return false;
    }
    const std::string entry = "\"" + std::string(value) + "\"";
    return json.substr(array_start, array_end - array_start).find(entry) != std::string_view::npos;
}

} // namespace

VerifyUserJWT::VerifyUserJWT(const std::string& host) : pub_key(host) {}

Network::VerifyUser::UserData VerifyUserJWT::LoadUserData(const std::string& verify_UID,
                                                          const std::string& token) {
    const std::size_t first_dot = token.find('.');
    const std::size_t second_dot = token.find('.', first_dot == std::string::npos ? 0 : first_dot + 1);
    if (first_dot == std::string::npos || second_dot == std::string::npos) {
        LOG_INFO(WebService, "Verification failed: malformed JWT");
        return {};
    }

    const std::string payload = Base64UrlDecode(
        std::string_view(token).substr(first_dot + 1, second_dot - first_dot - 1));
    if (payload.empty()) {
        LOG_INFO(WebService, "Verification failed: invalid base64 payload");
        return {};
    }

    const std::string expected_aud = "external-" + verify_UID;
    const std::string iss = ExtractJSONString(payload, "iss");
    const std::string aud = ExtractJSONString(payload, "aud");
    if (iss != "citra-core" || (aud != expected_aud && payload.find(expected_aud) == std::string::npos)) {
        LOG_INFO(WebService, "Verification failed: issuer/audience mismatch");
        return {};
    }

    Network::VerifyUser::UserData user_data{};
    user_data.username = ExtractJSONString(payload, "username");
    user_data.display_name = ExtractJSONString(payload, "displayName");
    user_data.avatar_url = ExtractJSONString(payload, "avatarUrl");
    user_data.moderator = ContainsStringInArray(payload, "roles", "moderator");
    return user_data;
}

} // namespace WebService
