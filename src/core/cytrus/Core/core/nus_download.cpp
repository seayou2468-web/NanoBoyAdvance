// Copyright 2020 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <CoreFoundation/CoreFoundation.h>
#include <array>
#include <CFNetwork/CFNetwork.h>
#include <vector>
#include "common/logging/log.h"
#include "core/nus_download.h"

namespace Core::NUS {

std::optional<std::vector<u8>> Download(const std::string& path) {
    constexpr auto HOST = "http://nus.cdn.c.shop.nintendowifi.net";

    const std::string url_string = std::string{HOST} + path;
    CFStringRef url_cf_string =
        CFStringCreateWithCString(kCFAllocatorDefault, url_string.c_str(), kCFStringEncodingUTF8);
    if (!url_cf_string) {
        LOG_ERROR(WebService, "Invalid URL {}{}", HOST, path);
        return {};
    }

    CFURLRef url = CFURLCreateWithString(kCFAllocatorDefault, url_cf_string, nullptr);
    CFRelease(url_cf_string);
    if (!url) {
        LOG_ERROR(WebService, "Invalid URL {}{}", HOST, path);
        return {};
    }

    CFHTTPMessageRef request =
        CFHTTPMessageCreateRequest(kCFAllocatorDefault, CFSTR("GET"), url, kCFHTTPVersion1_1);
    CFRelease(url);
    if (!request) {
        LOG_ERROR(WebService, "Failed to create HTTP request for {}{}", HOST, path);
        return {};
    }

    CFReadStreamRef stream = CFReadStreamCreateForHTTPRequest(kCFAllocatorDefault, request);
    CFRelease(request);
    if (!stream) {
        LOG_ERROR(WebService, "Failed to create HTTP stream for {}{}", HOST, path);
        return {};
    }

    CFReadStreamSetProperty(stream, kCFStreamPropertyHTTPShouldAutoredirect, kCFBooleanTrue);

    if (!CFReadStreamOpen(stream)) {
        LOG_ERROR(WebService, "GET to {}{} failed to open stream", HOST, path);
        CFRelease(stream);
        return {};
    }

    std::vector<u8> response_body;
    std::array<u8, 16 * 1024> buffer{};
    while (true) {
        const CFIndex read_bytes =
            CFReadStreamRead(stream, reinterpret_cast<UInt8*>(buffer.data()), buffer.size());
        if (read_bytes > 0) {
            response_body.insert(response_body.end(), buffer.begin(), buffer.begin() + read_bytes);
            continue;
        }
        if (read_bytes == 0) {
            break;
        }

        LOG_ERROR(WebService, "GET to {}{} failed during read", HOST, path);
        CFReadStreamClose(stream);
        CFRelease(stream);
        return {};
    }

    CFTypeRef response_header_prop =
        CFReadStreamCopyProperty(stream, kCFStreamPropertyHTTPResponseHeader);
    CFReadStreamClose(stream);
    CFRelease(stream);

    if (!response_header_prop) {
        LOG_ERROR(WebService, "GET to {}{} returned no response header", HOST, path);
        return {};
    }

    CFHTTPMessageRef response_header = static_cast<CFHTTPMessageRef>(response_header_prop);
    const CFIndex status = CFHTTPMessageGetResponseStatusCode(response_header);
    if (status >= 400) {
        LOG_ERROR(WebService, "GET to {}{} returned error status code: {}", HOST, path, status);
        CFRelease(response_header);
        return {};
    }

    CFDictionaryRef headers = CFHTTPMessageCopyAllHeaderFields(response_header);
    CFRelease(response_header);
    if (!headers) {
        LOG_ERROR(WebService, "GET to {}{} returned no content", HOST, path);
        return {};
    }

    const bool has_content_type =
        CFDictionaryContainsKey(headers, CFSTR("Content-Type")) ||
        CFDictionaryContainsKey(headers, CFSTR("content-type"));
    CFRelease(headers);

    if (!has_content_type) {
        LOG_ERROR(WebService, "GET to {}{} returned no content", HOST, path);
        return {};
    }

    return response_body;
}

} // namespace Core::NUS
