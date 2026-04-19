// Copyright Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <atomic>
#if defined(__APPLE__)
#include <CFNetwork/CFNetwork.h>
#include <CoreFoundation/CoreFoundation.h>
#else
#error "Mikage HTTP backend now requires Apple CFNetwork/CoreFoundation"
#endif
#include <tuple>
#include <cstring>
#include <string_view>
#include <unordered_map>
#include "../../../../common/archives.h"
#include "../../../../common/assert.h"
#include "../../../../common/commoncrypto_aes.h"
#include "../../../../common/secure_random.h"
#include "../../../../common/string_util.h"
#include "../../../core.h"
#include "../../../file_sys/archive_ncch.h"
#include "../../../file_sys/file_backend.h"
#include "../../ipc_helpers.h"
#include "../../kernel/ipc.h"
#include "../../romfs.h"
#include "../fs/archive.h"
#include "http_c.h"
#include "../../../hw/aes/key.h"

SERIALIZE_EXPORT_IMPL(Service::HTTP::HTTP_C)
SERIALIZE_EXPORT_IMPL(Service::HTTP::SessionData)

namespace Service::HTTP {

namespace {
void ReplaceAllInPlace(std::string& target, std::string_view from, std::string_view to) {
    if (from.empty()) {
        return;
    }

    std::size_t pos = 0;
    while ((pos = target.find(from, pos)) != std::string::npos) {
        target.replace(pos, from.size(), to);
        pos += to.size();
    }
}
} // namespace

#include "ctr-common-1-cert.h"
#include "ctr-common-1-key.h"

namespace ErrCodes {
enum {
    InvalidRequestState = 22,
    TooManyContexts = 26,
    InvalidRequestMethod = 32,
    HeaderNotFound = 40,
    BufferTooSmall = 43,

    /// This error is returned in multiple situations: when trying to add Post data that is
    /// incompatible with the one that is used in the session, or when trying to use chunked
    /// requests with Post data already set
    IncompatibleAddPostData = 50,

    InvalidPostDataEncoding = 53,
    IncompatibleSendPostData = 54,
    WrongCertID = 57,
    CertAlreadySet = 61,
    ContextNotFound = 100,
    Timeout = 105,

    /// This error is returned in multiple situations: when trying to initialize an
    /// already-initialized session, or when using the wrong context handle in a context-bound
    /// session
    SessionStateError = 102,

    WrongCertHandle = 201,
    TooManyClientCerts = 203,
    NotImplemented = 1012,
};
}

constexpr Result ErrorStateError = // 0xD8A0A066
    Result(ErrCodes::SessionStateError, ErrorModule::HTTP, ErrorSummary::InvalidState,
           ErrorLevel::Permanent);
constexpr Result ErrorNotImplemented = // 0xD960A3F4
    Result(ErrCodes::NotImplemented, ErrorModule::HTTP, ErrorSummary::Internal,
           ErrorLevel::Permanent);
constexpr Result ErrorTooManyClientCerts = // 0xD8A0A0CB
    Result(ErrCodes::TooManyClientCerts, ErrorModule::HTTP, ErrorSummary::InvalidState,
           ErrorLevel::Permanent);
constexpr Result ErrorHeaderNotFound = // 0xD8A0A028
    Result(ErrCodes::HeaderNotFound, ErrorModule::HTTP, ErrorSummary::InvalidState,
           ErrorLevel::Permanent);
constexpr Result ErrorBufferSmall = // 0xD840A02B
    Result(ErrCodes::BufferTooSmall, ErrorModule::HTTP, ErrorSummary::WouldBlock,
           ErrorLevel::Permanent);
constexpr Result ErrorWrongCertID = // 0xD8E0B839
    Result(ErrCodes::WrongCertID, ErrorModule::SSL, ErrorSummary::InvalidArgument,
           ErrorLevel::Permanent);
constexpr Result ErrorWrongCertHandle = // 0xD8A0A0C9
    Result(ErrCodes::WrongCertHandle, ErrorModule::HTTP, ErrorSummary::InvalidState,
           ErrorLevel::Permanent);
constexpr Result ErrorCertAlreadySet = // 0xD8A0A03D
    Result(ErrCodes::CertAlreadySet, ErrorModule::HTTP, ErrorSummary::InvalidState,
           ErrorLevel::Permanent);
constexpr Result ErrorIncompatibleAddPostData = // 0xD8A0A032
    Result(ErrCodes::IncompatibleAddPostData, ErrorModule::HTTP, ErrorSummary::InvalidState,
           ErrorLevel::Permanent);
constexpr Result ErrorContextNotFound = // 0xD8A0A064
    Result(ErrCodes::ContextNotFound, ErrorModule::HTTP, ErrorSummary::InvalidState,
           ErrorLevel::Permanent);
constexpr Result ErrorTimeout = // 0xD820A069
    Result(ErrCodes::Timeout, ErrorModule::HTTP, ErrorSummary::NothingHappened,
           ErrorLevel::Permanent);
constexpr Result ErrorTooManyContexts = // 0xD8A0A01A
    Result(ErrCodes::TooManyContexts, ErrorModule::HTTP, ErrorSummary::InvalidState,
           ErrorLevel::Permanent);
constexpr Result ErrorInvalidRequestMethod = // 0xD8A0A020
    Result(ErrCodes::InvalidRequestMethod, ErrorModule::HTTP, ErrorSummary::InvalidState,
           ErrorLevel::Permanent);
constexpr Result ErrorInvalidRequestState = // 0xD8A0A016
    Result(ErrCodes::InvalidRequestState, ErrorModule::HTTP, ErrorSummary::InvalidState,
           ErrorLevel::Permanent);
constexpr Result ErrorInvalidPostDataEncoding = // 0xD8A0A035
    Result(ErrCodes::InvalidPostDataEncoding, ErrorModule::HTTP, ErrorSummary::InvalidState,
           ErrorLevel::Permanent);
constexpr Result ErrorIncompatibleSendPostData = // 0xD8A0A036
    Result(ErrCodes::IncompatibleSendPostData, ErrorModule::HTTP, ErrorSummary::InvalidState,
           ErrorLevel::Permanent);

// Splits URL into its components. Example: https://citra-emu.org:443/index.html
// is_https: true; host: citra-emu.org; port: 443; path: /index.html
static URLInfo SplitUrl(const std::string& url) {
    const std::string prefix = "://";
    constexpr int default_http_port = 80;
    constexpr int default_https_port = 443;

    std::string host;
    int port = -1;
    std::string path;

    const auto scheme_end = url.find(prefix);
    const auto prefix_end = scheme_end == std::string::npos ? 0 : scheme_end + prefix.length();
    bool is_https = scheme_end != std::string::npos && url.starts_with("https");
    const auto path_index = url.find("/", prefix_end);

    if (path_index == std::string::npos) {
        // If no path is specified after the host, set it to "/"
        host = url.substr(prefix_end);
        path = "/";
    } else {
        host = url.substr(prefix_end, path_index - prefix_end);
        path = url.substr(path_index);
    }

    const auto port_start = host.find(":");
    if (port_start != std::string::npos) {
        std::string port_str = host.substr(port_start + 1);
        host = host.substr(0, port_start);
        char* p_end = nullptr;
        port = std::strtol(port_str.c_str(), &p_end, 10);
        if (*p_end) {
            port = -1;
        }
    }

    if (port == -1) {
        port = is_https ? default_https_port : default_http_port;
    }
    return URLInfo{
        .is_https = is_https,
        .host = host,
        .port = port,
        .path = path,
    };
}

static std::string PercentEncode(std::string_view input) {
    static constexpr char kHex[] = "0123456789ABCDEF";
    std::string out;
    out.reserve(input.size() * 3);
    for (unsigned char c : input) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '~') {
            out.push_back(static_cast<char>(c));
        } else {
            out.push_back('%');
            out.push_back(kHex[c >> 4]);
            out.push_back(kHex[c & 0x0F]);
        }
    }
    return out;
}

static std::string BuildMultipartBody(const Context::Params& params, const std::string& boundary) {
    std::string out;
    for (const auto& param : params) {
        out += "--" + boundary + "\r\n";
        out += "Content-Disposition: form-data; name=\"" + param.second.name + "\"\r\n";
        if (param.second.is_binary) {
            out += "Content-Type: application/octet-stream\r\n";
        }
        out += "\r\n";
        out += param.second.value;
        out += "\r\n";
    }
    out += "--" + boundary + "--\r\n";
    return out;
}

static std::string MakeMultipartBoundary() {
    return StringFromFormat("mikage-boundary-%08x", Common::Random::GenerateValue<u32>());
}

static std::string CFStringToStdString(CFStringRef str) {
    if (!str) {
        return {};
    }
    const CFIndex max_size = CFStringGetMaximumSizeForEncoding(CFStringGetLength(str), kCFStringEncodingUTF8) + 1;
    std::string out(static_cast<std::size_t>(max_size), '\0');
    if (CFStringGetCString(str, out.data(), max_size, kCFStringEncodingUTF8)) {
        out.resize(std::strlen(out.c_str()));
        return out;
    }
    return {};
}

static bool PerformRequestCFNetwork(const std::string& full_url, const std::string& method,
                                    const std::vector<Context::RequestHeader>& headers,
                                    const std::string& body, bool disable_cert_verify,
                                    Context::Response& out_response, std::string& out_error,
                                    std::atomic<u64>& current_download_size_bytes,
                                    std::atomic<u64>& total_download_size_bytes) {
#ifndef __APPLE__
    out_error = "CFNetwork backend is only available on Apple platforms";
    return false;
#else
    CFURLRef url = CFURLCreateWithBytes(kCFAllocatorDefault,
                                        reinterpret_cast<const UInt8*>(full_url.data()),
                                        static_cast<CFIndex>(full_url.size()),
                                        kCFStringEncodingUTF8, nullptr);
    if (!url) {
        out_error = "Failed to create URL";
        return false;
    }

    CFStringRef method_cf = CFStringCreateWithCString(kCFAllocatorDefault, method.c_str(),
                                                       kCFStringEncodingUTF8);
    if (!method_cf) {
        CFRelease(url);
        out_error = "Failed to create HTTP method";
        return false;
    }

    CFHTTPMessageRef request = CFHTTPMessageCreateRequest(kCFAllocatorDefault, method_cf, url,
                                                          kCFHTTPVersion1_1);
    CFRelease(method_cf);
    CFRelease(url);
    if (!request) {
        out_error = "Failed to create HTTP request";
        return false;
    }

    for (const auto& header : headers) {
        CFStringRef key = CFStringCreateWithCString(kCFAllocatorDefault, header.name.c_str(),
                                                    kCFStringEncodingUTF8);
        CFStringRef val = CFStringCreateWithCString(kCFAllocatorDefault, header.value.c_str(),
                                                    kCFStringEncodingUTF8);
        if (key && val) {
            CFHTTPMessageSetHeaderFieldValue(request, key, val);
        }
        if (key) CFRelease(key);
        if (val) CFRelease(val);
    }

    if (!body.empty()) {
        CFDataRef body_data = CFDataCreate(kCFAllocatorDefault,
                                           reinterpret_cast<const UInt8*>(body.data()),
                                           static_cast<CFIndex>(body.size()));
        if (body_data) {
            CFHTTPMessageSetBody(request, body_data);
            CFRelease(body_data);
        }
    }

    CFReadStreamRef stream = CFReadStreamCreateForHTTPRequest(kCFAllocatorDefault, request);
    CFRelease(request);
    if (!stream) {
        out_error = "Failed to create read stream";
        return false;
    }

    if (disable_cert_verify) {
        const void* keys[] = {kCFStreamSSLValidatesCertificateChain};
        const void* vals[] = {kCFBooleanFalse};
        CFDictionaryRef ssl = CFDictionaryCreate(kCFAllocatorDefault, keys, vals, 1,
                                                 &kCFTypeDictionaryKeyCallBacks,
                                                 &kCFTypeDictionaryValueCallBacks);
        if (ssl) {
            CFReadStreamSetProperty(stream, kCFStreamPropertySSLSettings, ssl);
            CFRelease(ssl);
        }
    }

    if (!CFReadStreamOpen(stream)) {
        CFRelease(stream);
        out_error = "Failed to open read stream";
        return false;
    }

    std::string body_out;
    constexpr CFIndex kBufSize = 4096;
    UInt8 buffer[kBufSize];
    while (true) {
        const CFIndex n = CFReadStreamRead(stream, buffer, kBufSize);
        if (n > 0) {
            body_out.append(reinterpret_cast<const char*>(buffer), static_cast<std::size_t>(n));
            current_download_size_bytes = static_cast<u64>(body_out.size());
        } else if (n == 0) {
            break;
        } else {
            out_error = "Read stream error";
            CFReadStreamClose(stream);
            CFRelease(stream);
            return false;
        }
    }
    CFReadStreamClose(stream);

    CFTypeRef response_header = CFReadStreamCopyProperty(stream, kCFStreamPropertyHTTPResponseHeader);
    if (!response_header) {
        CFRelease(stream);
        out_error = "Missing HTTP response headers";
        return false;
    }

    CFHTTPMessageRef response = reinterpret_cast<CFHTTPMessageRef>(response_header);
    out_response.status = static_cast<int>(CFHTTPMessageGetResponseStatusCode(response));
    out_response.reason.clear();
    out_response.version = "HTTP/1.1";
    out_response.body = std::move(body_out);
    total_download_size_bytes = static_cast<u64>(out_response.body.size());

    CFDictionaryRef header_dict = CFHTTPMessageCopyAllHeaderFields(response);
    if (header_dict) {
        CFIndex count = CFDictionaryGetCount(header_dict);
        std::vector<const void*> keys(static_cast<std::size_t>(count));
        std::vector<const void*> vals(static_cast<std::size_t>(count));
        CFDictionaryGetKeysAndValues(header_dict, keys.data(), vals.data());
        out_response.headers.clear();
        for (CFIndex i = 0; i < count; ++i) {
            auto* k = reinterpret_cast<CFStringRef>(const_cast<void*>(keys[static_cast<std::size_t>(i)]));
            auto* v = reinterpret_cast<CFStringRef>(const_cast<void*>(vals[static_cast<std::size_t>(i)]));
            out_response.headers.emplace(CFStringToStdString(k), CFStringToStdString(v));
        }
        CFRelease(header_dict);
    }

    CFRelease(response_header);
    CFRelease(stream);
    return true;
#endif
}

void Context::ParseAsciiPostData() {
    std::string query;
    for (auto it = post_data.begin(); it != post_data.end(); ++it) {
        if (it != post_data.begin()) {
            query += "&";
        }
        query += PercentEncode(it->first);
        query += "=";
        query += PercentEncode(it->second.value);
    }
    post_data_raw = std::move(query);
    ReplaceAllInPlace(post_data_raw, "*", "%2A");
}

std::string Context::ParseMultipartFormData() {
    multipart_boundary = MakeMultipartBoundary();
    post_data_raw = BuildMultipartBody(post_data, multipart_boundary);
    return "multipart/form-data; boundary=" + multipart_boundary;
}

void Context::MakeRequest() {
    ASSERT(state == RequestState::NotStarted);
    state = RequestState::ConnectingToServer;

    static const std::unordered_map<RequestMethod, std::string> request_method_strings{
        {RequestMethod::Get, "GET"},       {RequestMethod::Post, "POST"},
        {RequestMethod::Head, "HEAD"},     {RequestMethod::Put, "PUT"},
        {RequestMethod::Delete, "DELETE"}, {RequestMethod::PostEmpty, "POST"},
        {RequestMethod::PutEmpty, "PUT"},
    };

    URLInfo url_info = SplitUrl(url);
    std::vector<Context::RequestHeader> pending_headers = headers;

    if (method == RequestMethod::Post || method == RequestMethod::Put) {
        switch (post_data_encoding) {
        case PostDataEncoding::AsciiForm:
            ParseAsciiPostData();
            pending_headers.emplace_back("Content-Type", "application/x-www-form-urlencoded");
            break;
        case PostDataEncoding::MultipartForm:
            pending_headers.emplace_back("Content-Type", ParseMultipartFormData());
            break;
        case PostDataEncoding::Auto:
            if (!post_data.empty()) {
                if (force_multipart) {
                    pending_headers.emplace_back("Content-Type", ParseMultipartFormData());
                } else {
                    ParseAsciiPostData();
                    pending_headers.emplace_back("Content-Type", "application/x-www-form-urlencoded");
                }
            }
            break;
        }
    }

    if (chunked_request && !post_data.empty()) {
        finish_post_data.Wait();
        if (post_data_type == PostDataType::Raw) {
            post_data_raw.clear();
            for (const auto& data : post_data) {
                post_data_raw += data.second.value;
            }
        } else if (post_data_type == PostDataType::AsciiForm) {
            ParseAsciiPostData();
            pending_headers.emplace_back("Content-Type", "application/x-www-form-urlencoded");
        } else if (post_data_type == PostDataType::MultipartForm) {
            pending_headers.emplace_back("Content-Type", ParseMultipartFormData());
        }
    }

    if (url_info.is_https) {
        MakeRequestSSL(url_info, pending_headers);
    } else {
        MakeRequestNonSSL(url_info, pending_headers);
    }
}

void Context::MakeRequestNonSSL(const URLInfo& url_info,
                                const std::vector<Context::RequestHeader>& pending_headers) {
    const std::string method_string = (method == RequestMethod::Head) ? "HEAD" :
                                      (method == RequestMethod::Put || method == RequestMethod::PutEmpty) ? "PUT" :
                                      (method == RequestMethod::Delete) ? "DELETE" :
                                      (method == RequestMethod::Post || method == RequestMethod::PostEmpty) ? "POST" : "GET";
    std::string request_url = StringFromFormat("http://%s:%d%s", url_info.host.c_str(), url_info.port,
                                               url_info.path.c_str());
    std::string error;
    if (!PerformRequestCFNetwork(request_url, method_string, pending_headers, post_data_raw, false,
                                 response, error, current_download_size_bytes,
                                 total_download_size_bytes)) {
        LOG_ERROR(Service_HTTP, "Request failed: {}", error);
        state = RequestState::Completed;
        return;
    }
    state = RequestState::ReceivingBody;
}

void Context::MakeRequestSSL(const URLInfo& url_info,
                             const std::vector<Context::RequestHeader>& pending_headers) {
    const std::string method_string = (method == RequestMethod::Head) ? "HEAD" :
                                      (method == RequestMethod::Put || method == RequestMethod::PutEmpty) ? "PUT" :
                                      (method == RequestMethod::Delete) ? "DELETE" :
                                      (method == RequestMethod::Post || method == RequestMethod::PostEmpty) ? "POST" : "GET";
    std::string request_url = StringFromFormat("https://%s:%d%s", url_info.host.c_str(), url_info.port,
                                               url_info.path.c_str());
    std::string error;
    if (!PerformRequestCFNetwork(request_url, method_string, pending_headers, post_data_raw, true,
                                 response, error, current_download_size_bytes,
                                 total_download_size_bytes)) {
        LOG_ERROR(Service_HTTP, "Request failed: {}", error);
        state = RequestState::Completed;
        return;
    }
    state = RequestState::ReceivingBody;
}

void HTTP_C::Initialize(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 shmem_size = rp.Pop<u32>();
    u32 pid = rp.PopPID();
    shared_memory = rp.PopObject<Kernel::SharedMemory>();
    if (shared_memory) {
        shared_memory->SetName("HTTP_C:shared_memory");
    }

    LOG_DEBUG(Service_HTTP, "called, shared memory size: {} pid: {}", shmem_size, pid);

    auto* session_data = GetSessionData(ctx.Session());
    ASSERT(session_data);

    if (session_data->initialized) {
        LOG_ERROR(Service_HTTP, "Tried to initialize an already initialized session");
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ErrorStateError);
        return;
    }

    session_data->initialized = true;
    session_data->session_id = ++session_counter;

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    // This returns 0xd8a0a046 if no network connection is available.
    // Just assume we are always connected.
    rb.Push(ResultSuccess);
}

void HTTP_C::InitializeConnectionSession(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const Context::Handle context_handle = rp.Pop<u32>();
    u32 pid = rp.PopPID();

    LOG_DEBUG(Service_HTTP, "called, context_id={} pid={}", context_handle, pid);

    auto* session_data = GetSessionData(ctx.Session());
    ASSERT(session_data);

    if (session_data->initialized) {
        LOG_ERROR(Service_HTTP, "Tried to initialize an already initialized session");
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ErrorStateError);
        return;
    }

    // TODO(Subv): Check that the input PID matches the PID that created the context.
    auto itr = contexts.find(context_handle);
    if (itr == contexts.end()) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ErrorContextNotFound);
        return;
    }

    session_data->initialized = true;
    session_data->session_id = ++session_counter;
    // Bind the context to the current session.
    session_data->current_http_context = context_handle;

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);
}

void HTTP_C::BeginRequest(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const Context::Handle context_handle = rp.Pop<u32>();

    LOG_DEBUG(Service_HTTP, "called, context_id={}", context_handle);

    if (!PerformStateChecks(ctx, rp, context_handle)) {
        return;
    }

    Context& http_context = GetContext(context_handle);

    // This should never happen in real hardware, but can happen on citra.
    if (http_context.uses_default_client_cert && !http_context.clcert_data->init) {
        LOG_ERROR(Service_HTTP, "Failed to begin HTTP request: client cert not found.");
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ErrorStateError);
        return;
    }

    // On a 3DS BeginRequest and BeginRequestAsync will push the Request to a worker queue.
    // You can only enqueue 8 requests at the same time.
    // trying to enqueue any more will either fail (BeginRequestAsync), or block (BeginRequest)
    // Note that you only can have 8 Contexts at a time. So this difference shouldn't matter
    // Then there are 3? worker threads that pop the requests from the queue and send them
    // For now make every request async in it's own thread.

    // This always returns success, but the request is only performed when it hasn't started

    if (http_context.state == RequestState::NotStarted) {
        if (http_context.method == RequestMethod::Post && !http_context.post_data_added) {
            http_context.post_pending_request = true;
        } else {
            http_context.current_copied_data = 0;
            http_context.request_future =
                std::async(std::launch::async, &Context::MakeRequest, std::ref(http_context));
        }
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);
}

void HTTP_C::BeginRequestAsync(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const Context::Handle context_handle = rp.Pop<u32>();

    LOG_DEBUG(Service_HTTP, "called, context_id={}", context_handle);

    if (!PerformStateChecks(ctx, rp, context_handle)) {
        return;
    }

    Context& http_context = GetContext(context_handle);

    // This should never happen in real hardware, but can happen on citra.
    if (http_context.uses_default_client_cert && !http_context.clcert_data->init) {
        LOG_ERROR(Service_HTTP, "Failed to begin HTTP request: client cert not found.");
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ErrorStateError);
        return;
    }

    // On a 3DS BeginRequest and BeginRequestAsync will push the Request to a worker queue.
    // You can only enqueue 8 requests at the same time.
    // trying to enqueue any more will either fail (BeginRequestAsync), or block (BeginRequest)
    // Note that you only can have 8 Contexts at a time. So this difference shouldn't matter
    // Then there are 3? worker threads that pop the requests from the queue and send them
    // For now make every request async in it's own thread.

    // This always returns success, but the request is only performed when it hasn't started
    if (http_context.state == RequestState::NotStarted) {
        if (http_context.method == RequestMethod::Post && !http_context.post_data_added) {
            http_context.post_pending_request = true;
        } else {
            http_context.current_copied_data = 0;
            http_context.request_future =
                std::async(std::launch::async, &Context::MakeRequest, std::ref(http_context));
        }
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);
}

void HTTP_C::ReceiveData(Kernel::HLERequestContext& ctx) {
    ReceiveDataImpl(ctx, false);
}

void HTTP_C::ReceiveDataTimeout(Kernel::HLERequestContext& ctx) {
    ReceiveDataImpl(ctx, true);
}

void HTTP_C::ReceiveDataImpl(Kernel::HLERequestContext& ctx, bool timeout) {
    IPC::RequestParser rp(ctx);

    struct AsyncData {
        // Input
        u64 timeout_nanos = 0;
        bool timeout;
        Context::Handle context_handle;
        u32 buffer_size;
        Kernel::MappedBuffer* buffer;
        bool is_complete;
        // Output
        Result async_res = ResultSuccess;
    };
    std::shared_ptr<AsyncData> async_data = std::make_shared<AsyncData>();
    async_data->timeout = timeout;
    async_data->context_handle = rp.Pop<u32>();
    async_data->buffer_size = rp.Pop<u32>();

    if (timeout) {
        async_data->timeout_nanos = rp.Pop<u64>();
        LOG_DEBUG(Service_HTTP, "called, timeout={}", async_data->timeout_nanos);
    } else {
        LOG_DEBUG(Service_HTTP, "called");
    }
    async_data->buffer = &rp.PopMappedBuffer();

    if (!PerformStateChecks(ctx, rp, async_data->context_handle)) {
        return;
    }

    ctx.RunAsync(
        [this, async_data](Kernel::HLERequestContext& ctx) {
            Context& http_context = GetContext(async_data->context_handle);

            if (async_data->timeout) {
                const auto wait_res = http_context.request_future.wait_for(
                    std::chrono::nanoseconds(async_data->timeout_nanos));
                if (wait_res == std::future_status::timeout) {
                    async_data->async_res = ErrorTimeout;
                }
            } else {
                http_context.request_future.wait();
            }
            // Simulate small delay from HTTP receive.
            return 1'000'000;
        },
        [this, async_data](Kernel::HLERequestContext& ctx) {
            IPC::RequestBuilder rb(ctx, static_cast<u16>(ctx.CommandHeader().command_id.Value()), 1,
                                   0);
            if (async_data->async_res != ResultSuccess) {
                rb.Push(async_data->async_res);
                return;
            }
            Context& http_context = GetContext(async_data->context_handle);

            const std::size_t remaining_data =
                http_context.response.body.size() - http_context.current_copied_data;

            if (async_data->buffer_size >= remaining_data) {
                async_data->buffer->Write(http_context.response.body.data() +
                                              http_context.current_copied_data,
                                          0, remaining_data);
                http_context.current_copied_data += remaining_data;
                http_context.state = RequestState::Completed;
                rb.Push(ResultSuccess);
            } else {
                async_data->buffer->Write(http_context.response.body.data() +
                                              http_context.current_copied_data,
                                          0, async_data->buffer_size);
                http_context.current_copied_data += async_data->buffer_size;
                rb.Push(ErrorBufferSmall);
            }
            LOG_DEBUG(Service_HTTP, "Receive: buffer_size= {}, total_copied={}, total_body={}",
                      async_data->buffer_size, http_context.current_copied_data,
                      http_context.response.body.size());
        });
}

void HTTP_C::SetProxyDefault(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const Context::Handle context_handle = rp.Pop<u32>();

    LOG_WARNING(Service_HTTP, "(STUBBED) called, handle={}", context_handle);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);
}

void HTTP_C::CreateContext(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 url_size = rp.Pop<u32>();
    RequestMethod method = rp.PopEnum<RequestMethod>();
    Kernel::MappedBuffer& buffer = rp.PopMappedBuffer();

    // Copy the buffer into a string without the \0 at the end of the buffer
    std::string url(url_size - 1, '\0');
    buffer.Read(url.data(), 0, url_size - 1);

    LOG_DEBUG(Service_HTTP, "called, url_size={}, url={}, method={}", url_size, url, method);

    auto* session_data = EnsureSessionInitialized(ctx, rp);
    if (!session_data) {
        return;
    }

    // This command can only be called without a bound session.
    if (session_data->current_http_context) {
        LOG_ERROR(Service_HTTP, "Command called with a bound context");

        IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
        rb.Push(ErrorNotImplemented);
        rb.PushMappedBuffer(buffer);
        return;
    }

    static constexpr std::size_t MaxConcurrentHTTPContexts = 8;
    if (session_data->num_http_contexts >= MaxConcurrentHTTPContexts) {
        // There can only be 8 HTTP contexts open at the same time for any particular session.
        LOG_ERROR(Service_HTTP, "Tried to open too many HTTP contexts");

        IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
        rb.Push(ErrorTooManyContexts);
        rb.PushMappedBuffer(buffer);
        return;
    }

    if (method == RequestMethod::None || static_cast<u32>(method) >= TotalRequestMethods) {
        LOG_ERROR(Service_HTTP, "invalid request method={}", method);

        IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
        rb.Push(ErrorInvalidRequestMethod);
        rb.PushMappedBuffer(buffer);
        return;
    }

    contexts.try_emplace(++context_counter);
    contexts[context_counter].url = std::move(url);
    contexts[context_counter].method = method;
    contexts[context_counter].state = RequestState::NotStarted;
    // TODO(Subv): Find a correct default value for this field.
    contexts[context_counter].socket_buffer_size = 0;
    contexts[context_counter].handle = context_counter;
    contexts[context_counter].session_id = session_data->session_id;

    session_data->num_http_contexts++;

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 2);
    rb.Push(ResultSuccess);
    rb.Push<u32>(context_counter);
    rb.PushMappedBuffer(buffer);
}

void HTTP_C::CloseContext(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    u32 context_handle = rp.Pop<u32>();

    LOG_DEBUG(Service_HTTP, "called, handle={}", context_handle);

    auto* session_data = EnsureSessionInitialized(ctx, rp);
    if (!session_data) {
        return;
    }

    ASSERT_MSG(!session_data->current_http_context,
               "Unimplemented CloseContext on context-bound session");

    auto itr = contexts.find(context_handle);
    if (itr == contexts.end()) {
        // The real HTTP module just silently fails in this case.
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ResultSuccess);
        LOG_ERROR(Service_HTTP, "called, context {} not found", context_handle);
        return;
    }

    // TODO(Subv): What happens if you try to close a context that's currently being used?
    // TODO(Subv): Make sure that only the session that created the context can close it.

    // Note that this will block if a request is still in progress
    contexts.erase(itr);
    session_data->num_http_contexts--;

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);
}

void HTTP_C::CancelConnection(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 context_handle = rp.Pop<u32>();

    LOG_WARNING(Service_HTTP, "(STUBBED) called, handle={}", context_handle);

    const auto* session_data = EnsureSessionInitialized(ctx, rp);
    if (!session_data) {
        return;
    }

    [[maybe_unused]] Context& http_context = GetContext(context_handle);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);
}

void HTTP_C::GetRequestState(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 context_handle = rp.Pop<u32>();

    const auto* session_data = EnsureSessionInitialized(ctx, rp);
    if (!session_data) {
        return;
    }

    LOG_DEBUG(Service_HTTP, "called, context_handle={}", context_handle);

    Context& http_context = GetContext(context_handle);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(ResultSuccess);
    rb.PushEnum<RequestState>(http_context.state);
}

void HTTP_C::AddRequestHeader(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 context_handle = rp.Pop<u32>();
    [[maybe_unused]] const u32 name_size = rp.Pop<u32>();
    const u32 value_size = rp.Pop<u32>();
    const std::vector<u8> name_buffer = rp.PopStaticBuffer();
    Kernel::MappedBuffer& value_buffer = rp.PopMappedBuffer();

    // Copy the name_buffer into a string without the \0 at the end
    const std::string name(name_buffer.begin(), name_buffer.end() - 1);

    // Copy the value_buffer into a string without the \0 at the end
    std::string value(value_size - 1, '\0');
    value_buffer.Read(&value[0], 0, value_size - 1);

    LOG_DEBUG(Service_HTTP, "called, name={}, value={}, context_handle={}", name, value,
              context_handle);

    if (!PerformStateChecks(ctx, rp, context_handle)) {
        return;
    }

    Context& http_context = GetContext(context_handle);

    if (http_context.state != RequestState::NotStarted) {
        LOG_ERROR(Service_HTTP,
                  "Tried to add a request header on a context that has already been started.");
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
        rb.Push(ErrorInvalidRequestState);
        rb.PushMappedBuffer(value_buffer);
        return;
    }

    http_context.headers.emplace_back(name, value);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(ResultSuccess);
    rb.PushMappedBuffer(value_buffer);
}

void HTTP_C::AddPostDataAscii(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 context_handle = rp.Pop<u32>();
    [[maybe_unused]] const u32 name_size = rp.Pop<u32>();
    const u32 value_size = rp.Pop<u32>();
    const std::vector<u8> name_buffer = rp.PopStaticBuffer();
    Kernel::MappedBuffer& value_buffer = rp.PopMappedBuffer();

    // Copy the name_buffer into a string without the \0 at the end
    const std::string name(name_buffer.begin(), name_buffer.end() - 1);

    // Copy the value_buffer into a string without the \0 at the end
    std::string value(value_size - 1, '\0');
    value_buffer.Read(value.data(), 0, value_size - 1);

    LOG_DEBUG(Service_HTTP, "called, name={}, value={}, context_handle={}", name, value,
              context_handle);

    if (!PerformStateChecks(ctx, rp, context_handle)) {
        return;
    }

    Context& http_context = GetContext(context_handle);

    if (http_context.state != RequestState::NotStarted) {
        LOG_ERROR(Service_HTTP,
                  "Tried to add Post data on a context that has already been started");
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
        rb.Push(ErrorInvalidRequestState);
        rb.PushMappedBuffer(value_buffer);
        return;
    }

    if (!http_context.post_data_raw.empty()) {
        LOG_ERROR(Service_HTTP, "Cannot add ASCII Post data to context with raw Post data");
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
        rb.Push(ErrorIncompatibleAddPostData);
        rb.PushMappedBuffer(value_buffer);
        return;
    }

    if (http_context.chunked_request) {
        LOG_ERROR(Service_HTTP, "Cannot add ASCII Post data to context in chunked request mode");
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
        rb.Push(ErrorIncompatibleAddPostData);
        rb.PushMappedBuffer(value_buffer);
        return;
    }

    Context::Param param_value(name, value);
    http_context.post_data.emplace(name, param_value);
    http_context.post_data_added = true;

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(ResultSuccess);
    rb.PushMappedBuffer(value_buffer);
}

void HTTP_C::AddPostDataBinary(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 context_handle = rp.Pop<u32>();
    [[maybe_unused]] const u32 name_size = rp.Pop<u32>();
    const u32 value_size = rp.Pop<u32>();
    const std::vector<u8> name_buffer = rp.PopStaticBuffer();
    Kernel::MappedBuffer& value_buffer = rp.PopMappedBuffer();

    // Copy the name_buffer into a string without the \0 at the end
    const std::string name(name_buffer.begin(), name_buffer.end() - 1);

    // Copy the value_buffer into a vector
    std::vector<u8> value(value_size);
    value_buffer.Read(value.data(), 0, value_size);

    LOG_DEBUG(Service_HTTP, "called, name={}, value_size={}, context_handle={}", name, value_size,
              context_handle);

    if (!PerformStateChecks(ctx, rp, context_handle)) {
        return;
    }

    Context& http_context = GetContext(context_handle);

    if (http_context.state != RequestState::NotStarted) {
        LOG_ERROR(Service_HTTP,
                  "Tried to add Post data on a context that has already been started");
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
        rb.Push(ErrorInvalidRequestState);
        rb.PushMappedBuffer(value_buffer);
        return;
    }

    if (!http_context.post_data_raw.empty()) {
        LOG_ERROR(Service_HTTP, "Cannot add Binary Post data to context with raw Post data");
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
        rb.Push(ErrorIncompatibleAddPostData);
        rb.PushMappedBuffer(value_buffer);
        return;
    }

    if (http_context.chunked_request) {
        LOG_ERROR(Service_HTTP, "Cannot add Binary Post data to context in chunked request mode");
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
        rb.Push(ErrorIncompatibleAddPostData);
        rb.PushMappedBuffer(value_buffer);
        return;
    }

    Context::Param param_value(name, value);
    http_context.post_data.emplace(name, param_value);
    http_context.force_multipart = true;
    http_context.post_data_added = true;

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(ResultSuccess);
    rb.PushMappedBuffer(value_buffer);
}

void HTTP_C::AddPostDataRaw(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 context_handle = rp.Pop<u32>();
    const u32 post_data_len = rp.Pop<u32>();
    auto buffer = rp.PopMappedBuffer();

    LOG_DEBUG(Service_HTTP, "called, context_handle={}, post_data_len={}", context_handle,
              post_data_len);

    if (!PerformStateChecks(ctx, rp, context_handle)) {
        return;
    }

    Context& http_context = GetContext(context_handle);

    if (http_context.state != RequestState::NotStarted) {
        LOG_ERROR(Service_HTTP,
                  "Tried to add Post data on a context that has already been started");
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
        rb.Push(ErrorInvalidRequestState);
        rb.PushMappedBuffer(buffer);
        return;
    }

    if (!http_context.post_data.empty()) {
        LOG_ERROR(Service_HTTP,
                  "Cannot add raw Post data to context with ASCII or Binary Post data");
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
        rb.Push(ErrorIncompatibleAddPostData);
        rb.PushMappedBuffer(buffer);
        return;
    }

    if (http_context.chunked_request) {
        LOG_ERROR(Service_HTTP, "Cannot add raw Post data to context in chunked request mode");
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
        rb.Push(ErrorIncompatibleAddPostData);
        rb.PushMappedBuffer(buffer);
        return;
    }

    http_context.post_data_raw.resize(buffer.GetSize());
    buffer.Read(http_context.post_data_raw.data(), 0, buffer.GetSize());
    http_context.post_data_added = true;

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(ResultSuccess);
    rb.PushMappedBuffer(buffer);
}

void HTTP_C::SetPostDataType(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 context_handle = rp.Pop<u32>();
    const PostDataType type = rp.PopEnum<PostDataType>();

    LOG_DEBUG(Service_HTTP, "called, context_handle={}, type={}", context_handle, type);

    if (!PerformStateChecks(ctx, rp, context_handle)) {
        return;
    }

    Context& http_context = GetContext(context_handle);

    if (http_context.state != RequestState::NotStarted) {
        LOG_ERROR(Service_HTTP,
                  "Tried to set chunked mode on a context that has already been started");
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ErrorInvalidRequestState);
        return;
    }

    if (!http_context.post_data.empty() || !http_context.post_data_raw.empty()) {
        LOG_ERROR(Service_HTTP, "Tried to set chunked mode on a context that has Post data");
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ErrorIncompatibleSendPostData);
        return;
    }

    switch (type) {
    case PostDataType::AsciiForm:
    case PostDataType::MultipartForm:
    case PostDataType::Raw:
        http_context.post_data_type = type;
        break;
    // Use ASCII form by default
    default:
        http_context.post_data_type = PostDataType::AsciiForm;
        break;
    }

    http_context.chunked_request = true;

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);
}

void HTTP_C::SendPostDataAscii(Kernel::HLERequestContext& ctx) {
    SendPostDataAsciiImpl(ctx, false);
}

void HTTP_C::SendPostDataAsciiTimeout(Kernel::HLERequestContext& ctx) {
    SendPostDataAsciiImpl(ctx, true);
}

void HTTP_C::SendPostDataAsciiImpl(Kernel::HLERequestContext& ctx, bool timeout) {
    IPC::RequestParser rp(ctx);
    const u32 context_handle = rp.Pop<u32>();
    [[maybe_unused]] const u32 name_size = rp.Pop<u32>();
    const u32 value_size = rp.Pop<u32>();
    // TODO(DaniElectra): The original module waits until a connection with the server is made
    const u64 timeout_nanos = timeout ? rp.Pop<u64>() : 0;
    const std::vector<u8> name_buffer = rp.PopStaticBuffer();
    Kernel::MappedBuffer& value_buffer = rp.PopMappedBuffer();

    // Copy the name_buffer into a string without the \0 at the end
    const std::string name(name_buffer.begin(), name_buffer.end() - 1);

    // Copy the value_buffer into a string without the \0 at the end
    std::string value(value_size - 1, '\0');
    value_buffer.Read(value.data(), 0, value_size - 1);

    if (timeout) {
        LOG_DEBUG(Service_HTTP, "called, name={}, value={}, context_handle={}, timeout={}", name,
                  value, context_handle, timeout_nanos);
    } else {
        LOG_DEBUG(Service_HTTP, "called, name={}, value={}, context_handle={}", name, value,
                  context_handle);
    }

    if (!PerformStateChecks(ctx, rp, context_handle)) {
        return;
    }

    Context& http_context = GetContext(context_handle);

    if (http_context.state != RequestState::NotStarted) {
        LOG_ERROR(Service_HTTP, "Tried to send Post data on a context that has been started");
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
        rb.Push(ErrorInvalidRequestState);
        rb.PushMappedBuffer(value_buffer);
        return;
    }

    if (!http_context.chunked_request) {
        LOG_ERROR(Service_HTTP,
                  "Cannot send ASCII Post data to context not in chunked request mode");
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
        rb.Push(ErrorInvalidRequestState);
        rb.PushMappedBuffer(value_buffer);
        return;
    }

    Context::Param param_value(name, value);
    http_context.post_data.emplace(name, param_value);
    http_context.post_data_added = true;

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(ResultSuccess);
    rb.PushMappedBuffer(value_buffer);
}

void HTTP_C::SendPostDataBinary(Kernel::HLERequestContext& ctx) {
    SendPostDataBinaryImpl(ctx, false);
}

void HTTP_C::SendPostDataBinaryTimeout(Kernel::HLERequestContext& ctx) {
    SendPostDataBinaryImpl(ctx, true);
}

void HTTP_C::SendPostDataBinaryImpl(Kernel::HLERequestContext& ctx, bool timeout) {
    IPC::RequestParser rp(ctx);
    const u32 context_handle = rp.Pop<u32>();
    [[maybe_unused]] const u32 name_size = rp.Pop<u32>();
    const u32 value_size = rp.Pop<u32>();
    // TODO(DaniElectra): The original module waits until a connection with the server is made
    const u64 timeout_nanos = timeout ? rp.Pop<u64>() : 0;
    const std::vector<u8> name_buffer = rp.PopStaticBuffer();
    Kernel::MappedBuffer& value_buffer = rp.PopMappedBuffer();

    // Copy the name_buffer into a string without the \0 at the end
    const std::string name(name_buffer.begin(), name_buffer.end() - 1);

    // Copy the value_buffer into a vector
    std::vector<u8> value(value_size);
    value_buffer.Read(value.data(), 0, value_size);

    if (timeout) {
        LOG_DEBUG(Service_HTTP, "called, name={}, value_size={}, context_handle={}, timeout={}",
                  name, value_size, context_handle, timeout_nanos);
    } else {
        LOG_DEBUG(Service_HTTP, "called, name={}, value_size={}, context_handle={}", name,
                  value_size, context_handle);
    }

    if (!PerformStateChecks(ctx, rp, context_handle)) {
        return;
    }

    Context& http_context = GetContext(context_handle);

    if (http_context.state == RequestState::NotStarted) {
        LOG_ERROR(Service_HTTP, "Tried to add Post data on a context that has not been started");
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
        rb.Push(ErrorInvalidRequestState);
        rb.PushMappedBuffer(value_buffer);
        return;
    }

    if (!http_context.chunked_request) {
        LOG_ERROR(Service_HTTP,
                  "Cannot send Binary Post data to context not in chunked request mode");
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
        rb.Push(ErrorInvalidRequestState);
        rb.PushMappedBuffer(value_buffer);
        return;
    }

    Context::Param param_value(name, value);
    http_context.post_data.emplace(name, param_value);
    http_context.post_data_added = true;

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(ResultSuccess);
    rb.PushMappedBuffer(value_buffer);
}

void HTTP_C::SendPostDataRaw(Kernel::HLERequestContext& ctx) {
    SendPostDataRawImpl(ctx, false);
}

void HTTP_C::SendPostDataRawTimeout(Kernel::HLERequestContext& ctx) {
    SendPostDataRawImpl(ctx, true);
}

void HTTP_C::SendPostDataRawImpl(Kernel::HLERequestContext& ctx, bool timeout) {
    IPC::RequestParser rp(ctx);
    const u32 context_handle = rp.Pop<u32>();
    const u32 post_data_len = rp.Pop<u32>();
    // TODO(DaniElectra): The original module waits until a connection with the server is made
    const u64 timeout_nanos = timeout ? rp.Pop<u64>() : 0;
    auto buffer = rp.PopMappedBuffer();

    if (timeout) {
        LOG_DEBUG(Service_HTTP, "called, context_handle={}, post_data_len={}, timeout={}",
                  context_handle, post_data_len, timeout_nanos);
    } else {
        LOG_DEBUG(Service_HTTP, "called, context_handle={}, post_data_len={}", context_handle,
                  post_data_len);
    }

    if (!PerformStateChecks(ctx, rp, context_handle)) {
        return;
    }

    Context& http_context = GetContext(context_handle);

    if (http_context.state != RequestState::NotStarted) {
        LOG_ERROR(Service_HTTP,
                  "Tried to add Post data on a context that has already been started");
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
        rb.Push(ErrorInvalidRequestState);
        rb.PushMappedBuffer(buffer);
        return;
    }

    if (!http_context.chunked_request) {
        LOG_ERROR(Service_HTTP, "Cannot send raw Post data to context not in chunked request mode");
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
        rb.Push(ErrorInvalidRequestState);
        rb.PushMappedBuffer(buffer);
        return;
    }

    if (http_context.post_data_type != PostDataType::Raw) {
        LOG_ERROR(Service_HTTP, "Cannot send raw Post data to context not in raw mode");
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
        rb.Push(ErrorIncompatibleSendPostData);
        rb.PushMappedBuffer(buffer);
        return;
    }

    std::vector<u8> value(buffer.GetSize());
    buffer.Read(value.data(), 0, value.size());

    // Workaround for sending the raw data in combination of other data in chunked requests
    Context::Param raw_param(value);
    std::string value_string(value.begin(), value.end());
    http_context.post_data.emplace(value_string, raw_param);
    http_context.post_data_added = true;

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(ResultSuccess);
    rb.PushMappedBuffer(buffer);
}

void HTTP_C::SetPostDataEncoding(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 context_handle = rp.Pop<u32>();
    const PostDataEncoding encoding = rp.PopEnum<PostDataEncoding>();

    LOG_DEBUG(Service_HTTP, "called, context_handle={}, encoding={}", context_handle, encoding);

    if (!PerformStateChecks(ctx, rp, context_handle)) {
        return;
    }

    Context& http_context = GetContext(context_handle);

    if (http_context.state != RequestState::NotStarted) {
        LOG_ERROR(Service_HTTP,
                  "Tried to set Post encoding on a context that has already been started");
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ErrorInvalidRequestState);
        return;
    }

    switch (encoding) {
    case PostDataEncoding::Auto:
        http_context.post_data_encoding = encoding;
        break;
    case PostDataEncoding::AsciiForm:
    case PostDataEncoding::MultipartForm:
        if (!http_context.post_data_raw.empty()) {
            LOG_ERROR(Service_HTTP, "Cannot set Post data encoding to context with raw Post data");
            IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
            rb.Push(ErrorIncompatibleAddPostData);
            return;
        }

        http_context.post_data_encoding = encoding;
        break;
    default:
        LOG_ERROR(Service_HTTP, "Invalid Post data encoding: {}", encoding);
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ErrorInvalidPostDataEncoding);
        return;
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);
}

void HTTP_C::NotifyFinishSendPostData(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 context_handle = rp.Pop<u32>();

    LOG_DEBUG(Service_HTTP, "called, context_handle={}", context_handle);

    if (!PerformStateChecks(ctx, rp, context_handle)) {
        return;
    }

    Context& http_context = GetContext(context_handle);

    if (http_context.state != RequestState::NotStarted) {
        LOG_ERROR(Service_HTTP, "Tried to notfy finish Post on a context that has been started");
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ErrorInvalidRequestState);
        return;
    }

    if (!http_context.post_pending_request) {
        LOG_ERROR(Service_HTTP, "Tried to notfy finish Post on a context that has not begun");
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ErrorInvalidRequestState);
        return;
    }

    if (!http_context.post_data_added) {
        LOG_ERROR(Service_HTTP, "Tried to notfy finish Post on a context that has no post data");
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ErrorInvalidRequestState);
        return;
    }

    http_context.finish_post_data.Set();
    http_context.post_pending_request = false;

    http_context.current_copied_data = 0;
    http_context.request_future =
        std::async(std::launch::async, &Context::MakeRequest, std::ref(http_context));

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);
}

void HTTP_C::GetResponseData(Kernel::HLERequestContext& ctx) {
    GetResponseDataImpl(ctx, false);
}

void HTTP_C::GetResponseDataTimeout(Kernel::HLERequestContext& ctx) {
    GetResponseDataImpl(ctx, true);
}

void HTTP_C::GetResponseDataImpl(Kernel::HLERequestContext& ctx, bool timeout) {
    IPC::RequestParser rp(ctx);

    struct AsyncData {
        // Input
        u32 context_handle;
        bool timeout;
        u64 timeout_nanos;
        u32 data_max_len;
        Kernel::MappedBuffer* data_buffer;
        // Output
        Result async_res = ResultSuccess;
    };
    std::shared_ptr<AsyncData> async_data = std::make_shared<AsyncData>();

    async_data->timeout = timeout;
    async_data->context_handle = rp.Pop<u32>();
    async_data->data_max_len = rp.Pop<u32>();
    if (timeout) {
        async_data->timeout_nanos = rp.Pop<u64>();
    }
    async_data->data_buffer = &rp.PopMappedBuffer();

    if (!PerformStateChecks(ctx, rp, async_data->context_handle)) {
        return;
    }

    ctx.RunAsync(
        [this, async_data](Kernel::HLERequestContext& ctx) {
            Context& http_context = GetContext(async_data->context_handle);

            if (async_data->timeout) {
                const auto wait_res = http_context.request_future.wait_for(
                    std::chrono::nanoseconds(async_data->timeout_nanos));
                if (wait_res == std::future_status::timeout) {
                    async_data->async_res = ErrorTimeout;
                }
            } else {
                http_context.request_future.wait();
            }

            return 0;
        },
        [this, async_data](Kernel::HLERequestContext& ctx) {
            IPC::RequestBuilder rb(ctx, 2, 0);
            if (async_data->async_res != ResultSuccess) {
                rb.Push(async_data->async_res);
                return;
            }

            Context& http_context = GetContext(async_data->context_handle);
            auto& headers = http_context.response.headers;
            std::vector<u8> out;

            if (async_data->timeout) {
                LOG_DEBUG(Service_HTTP, "timeout={}", async_data->timeout_nanos);
            } else {
                LOG_DEBUG(Service_HTTP, "");
            }

            // Native backend does not keep the raw HTTP header bytes, so we reconstruct them.
            // Sadly, the order of headers is lost, but for now it's good enough.
            std::string hdr =
                StringFromFormat("%s %d %s\r\n", http_context.response.version.c_str(),
                                 http_context.response.status, http_context.response.reason.c_str());
            out.insert(out.end(), hdr.begin(), hdr.end());

            for (auto& h : headers) {
                hdr = StringFromFormat("%s: %s\r\n", h.first.c_str(), h.second.c_str());
                out.insert(out.end(), hdr.begin(), hdr.end());
            }

            hdr = "\r\n";
            out.insert(out.end(), hdr.begin(), hdr.end());

            size_t write_size = std::min(out.size(), async_data->data_buffer->GetSize());
            async_data->data_buffer->Write(out.data(), 0, write_size);

            rb.Push(ResultSuccess);
            rb.Push(static_cast<u32>(write_size));
        });
}

void HTTP_C::GetResponseHeader(Kernel::HLERequestContext& ctx) {
    GetResponseHeaderImpl(ctx, false);
}

void HTTP_C::GetResponseHeaderTimeout(Kernel::HLERequestContext& ctx) {
    GetResponseHeaderImpl(ctx, true);
}

void HTTP_C::GetResponseHeaderImpl(Kernel::HLERequestContext& ctx, bool timeout) {
    IPC::RequestParser rp(ctx);

    struct AsyncData {
        // Input
        u32 context_handle;
        u32 name_len;
        u32 value_max_len;
        bool timeout;
        u64 timeout_nanos;
        std::span<const u8> header_name;
        Kernel::MappedBuffer* value_buffer;
        // Output
        Result async_res = ResultSuccess;
    };
    std::shared_ptr<AsyncData> async_data = std::make_shared<AsyncData>();

    async_data->timeout = timeout;
    async_data->context_handle = rp.Pop<u32>();
    async_data->name_len = rp.Pop<u32>();
    async_data->value_max_len = rp.Pop<u32>();
    if (timeout) {
        async_data->timeout_nanos = rp.Pop<u64>();
    }
    async_data->header_name = rp.PopStaticBuffer();
    async_data->value_buffer = &rp.PopMappedBuffer();

    if (!PerformStateChecks(ctx, rp, async_data->context_handle)) {
        return;
    }

    ctx.RunAsync(
        [this, async_data](Kernel::HLERequestContext& ctx) {
            Context& http_context = GetContext(async_data->context_handle);

            if (async_data->timeout) {
                const auto wait_res = http_context.request_future.wait_for(
                    std::chrono::nanoseconds(async_data->timeout_nanos));
                if (wait_res == std::future_status::timeout) {
                    async_data->async_res = ErrorTimeout;
                }
            } else {
                http_context.request_future.wait();
            }

            return 0;
        },
        [this, async_data](Kernel::HLERequestContext& ctx) {
            IPC::RequestBuilder rb(ctx, static_cast<u16>(ctx.CommandHeader().command_id.Value()), 2,
                                   2);
            if (async_data->async_res != ResultSuccess) {
                rb.Push(async_data->async_res);
                return;
            }

            std::string header_name_str(
                reinterpret_cast<const char*>(async_data->header_name.data()),
                async_data->name_len);
            Common::TruncateString(header_name_str);

            Context& http_context = GetContext(async_data->context_handle);

            auto& headers = http_context.response.headers;
            u32 copied_size = 0;

            if (async_data->timeout) {
                LOG_DEBUG(Service_HTTP, "header={}, max_len={}, timeout={}", header_name_str,
                          async_data->value_buffer->GetSize(), async_data->timeout_nanos);
            } else {
                LOG_DEBUG(Service_HTTP, "header={}, max_len={}", header_name_str,
                          async_data->value_buffer->GetSize());
            }

            auto header = headers.find(header_name_str);
            if (header != headers.end()) {
                std::string header_value = header->second;
                copied_size = static_cast<u32>(header_value.size());
                if (header_value.size() >= async_data->value_buffer->GetSize()) {
                    header_value.resize(async_data->value_buffer->GetSize() - 1);
                }
                header_value.push_back('\0');
                async_data->value_buffer->Write(header_value.data(), 0, header_value.size());
            } else {
                LOG_DEBUG(Service_HTTP, "header={} not found", header_name_str);
                rb.Push(ErrorHeaderNotFound);
                rb.Push(0);
                rb.PushMappedBuffer(*async_data->value_buffer);
                return;
            }

            rb.Push(ResultSuccess);
            rb.Push(copied_size);
            rb.PushMappedBuffer(*async_data->value_buffer);
        });
}

void HTTP_C::GetResponseStatusCode(Kernel::HLERequestContext& ctx) {
    GetResponseStatusCodeImpl(ctx, false);
}

void HTTP_C::GetResponseStatusCodeTimeout(Kernel::HLERequestContext& ctx) {
    GetResponseStatusCodeImpl(ctx, true);
}

void HTTP_C::GetResponseStatusCodeImpl(Kernel::HLERequestContext& ctx, bool timeout) {
    IPC::RequestParser rp(ctx);

    struct AsyncData {
        // Input
        Context::Handle context_handle;
        bool timeout;
        u64 timeout_nanos = 0;
        // Output
        Result async_res = ResultSuccess;
    };
    std::shared_ptr<AsyncData> async_data = std::make_shared<AsyncData>();

    async_data->context_handle = rp.Pop<u32>();
    async_data->timeout = timeout;

    if (timeout) {
        async_data->timeout_nanos = rp.Pop<u64>();
        LOG_INFO(Service_HTTP, "called, timeout={}", async_data->timeout_nanos);
    } else {
        LOG_INFO(Service_HTTP, "called");
    }

    if (!PerformStateChecks(ctx, rp, async_data->context_handle)) {
        return;
    }

    ctx.RunAsync(
        [this, async_data](Kernel::HLERequestContext& ctx) {
            Context& http_context = GetContext(async_data->context_handle);

            if (async_data->timeout) {
                const auto wait_res = http_context.request_future.wait_for(
                    std::chrono::nanoseconds(async_data->timeout_nanos));
                if (wait_res == std::future_status::timeout) {
                    LOG_DEBUG(Service_HTTP, "Status code: {}", "timeout");
                    async_data->async_res = ErrorTimeout;
                }
            } else {
                http_context.request_future.wait();
            }
            return 0;
        },
        [this, async_data](Kernel::HLERequestContext& ctx) {
            if (async_data->async_res != ResultSuccess) {
                IPC::RequestBuilder rb(
                    ctx, static_cast<u16>(ctx.CommandHeader().command_id.Value()), 1, 0);
                rb.Push(async_data->async_res);
                return;
            }

            Context& http_context = GetContext(async_data->context_handle);

            const u32 response_code = http_context.response.status;
            LOG_DEBUG(Service_HTTP, "Status code: {}, response_code={}", "good", response_code);

            IPC::RequestBuilder rb(ctx, static_cast<u16>(ctx.CommandHeader().command_id.Value()), 2,
                                   0);
            rb.Push(ResultSuccess);
            rb.Push(response_code);
        });
}

void HTTP_C::AddTrustedRootCA(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const Context::Handle context_handle = rp.Pop<u32>();
    [[maybe_unused]] const u32 root_ca_len = rp.Pop<u32>();
    auto root_ca_data = rp.PopMappedBuffer();

    LOG_WARNING(Service_HTTP, "(STUBBED) called, handle={}", context_handle);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(ResultSuccess);
    rb.PushMappedBuffer(root_ca_data);
}

void HTTP_C::AddDefaultCert(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const Context::Handle context_handle = rp.Pop<u32>();
    const u32 certificate_id = rp.Pop<u32>();

    LOG_WARNING(Service_HTTP, "(STUBBED) called, handle={}, certificate_id={}", context_handle,
                certificate_id);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);
}

void HTTP_C::SetDefaultClientCert(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const Context::Handle context_handle = rp.Pop<u32>();
    const ClientCertID client_cert_id = static_cast<ClientCertID>(rp.Pop<u32>());

    LOG_DEBUG(Service_HTTP, "client_cert_id={}", client_cert_id);

    if (!PerformStateChecks(ctx, rp, context_handle)) {
        return;
    }

    Context& http_context = GetContext(context_handle);

    if (client_cert_id != ClientCertID::Default) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ErrorWrongCertID);
        return;
    }

    http_context.uses_default_client_cert = true;
    http_context.clcert_data = &GetClCertA();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);
}

void HTTP_C::SetClientCertContext(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 context_handle = rp.Pop<u32>();
    const u32 client_cert_handle = rp.Pop<u32>();

    LOG_DEBUG(Service_HTTP, "called with context_handle={} client_cert_handle={}", context_handle,
              client_cert_handle);

    if (!PerformStateChecks(ctx, rp, context_handle)) {
        return;
    }

    Context& http_context = GetContext(context_handle);

    auto cert_context_itr = client_certs.find(client_cert_handle);
    if (cert_context_itr == client_certs.end()) {
        LOG_ERROR(Service_HTTP, "called with wrong client_cert_handle {}", client_cert_handle);
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ErrorWrongCertHandle);
        return;
    }

    if (http_context.ssl_config.client_cert_ctx.lock()) {
        LOG_ERROR(Service_HTTP,
                  "Tried to set a client cert to a context that already has a client cert");
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ErrorCertAlreadySet);
        return;
    }

    if (http_context.state != RequestState::NotStarted) {
        LOG_ERROR(Service_HTTP,
                  "Tried to set a client cert on a context that has already been started.");
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ErrorInvalidRequestState);
        return;
    }

    http_context.ssl_config.client_cert_ctx = cert_context_itr->second;
    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);
}

void HTTP_C::GetSSLError(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 context_handle = rp.Pop<u32>();

    LOG_WARNING(Service_HTTP, "(STUBBED) called, context_handle={}", context_handle);

    [[maybe_unused]] Context& http_context = GetContext(context_handle);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(ResultSuccess);
    // Since we create the actual http/ssl context only when the request is submitted we can't check
    // for SSL Errors here. Just submit no error.
    rb.Push<u32>(0);
}

void HTTP_C::SetSSLOpt(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 context_handle = rp.Pop<u32>();
    const u32 opts = rp.Pop<u32>();

    LOG_WARNING(Service_HTTP, "(STUBBED) called, context_handle={}, opts={}", context_handle, opts);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);
}

void HTTP_C::OpenClientCertContext(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    u32 cert_size = rp.Pop<u32>();
    u32 key_size = rp.Pop<u32>();
    Kernel::MappedBuffer& cert_buffer = rp.PopMappedBuffer();
    Kernel::MappedBuffer& key_buffer = rp.PopMappedBuffer();

    LOG_DEBUG(Service_HTTP, "called, cert_size {}, key_size {}", cert_size, key_size);

    auto* session_data = GetSessionData(ctx.Session());
    ASSERT(session_data);

    Result result(ResultSuccess);

    if (!session_data->initialized) {
        LOG_ERROR(Service_HTTP, "Command called without Initialize");
        result = ErrorStateError;
    } else if (session_data->current_http_context) {
        LOG_ERROR(Service_HTTP, "Command called with a bound context");
        result = ErrorNotImplemented;
    } else if (session_data->num_client_certs >= 2) {
        LOG_ERROR(Service_HTTP, "tried to load more then 2 client certs");
        result = ErrorTooManyClientCerts;
    } else {
        client_certs[++client_certs_counter] = std::make_shared<ClientCertContext>();
        client_certs[client_certs_counter]->handle = client_certs_counter;
        client_certs[client_certs_counter]->certificate.resize(cert_size);
        cert_buffer.Read(&client_certs[client_certs_counter]->certificate[0], 0, cert_size);
        client_certs[client_certs_counter]->private_key.resize(key_size);
        cert_buffer.Read(&client_certs[client_certs_counter]->private_key[0], 0, key_size);
        client_certs[client_certs_counter]->session_id = session_data->session_id;

        ++session_data->num_client_certs;
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 4);
    rb.Push(result);
    rb.PushMappedBuffer(cert_buffer);
    rb.PushMappedBuffer(key_buffer);
}

void HTTP_C::OpenDefaultClientCertContext(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    u8 cert_id = rp.Pop<u8>();

    LOG_DEBUG(Service_HTTP, "called, cert_id={} cert_handle={}", cert_id, client_certs_counter);

    auto* session_data = EnsureSessionInitialized(ctx, rp);
    if (!session_data) {
        return;
    }

    if (session_data->current_http_context) {
        LOG_ERROR(Service_HTTP, "Command called with a bound context");
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ErrorNotImplemented);
        return;
    }

    if (session_data->num_client_certs >= 2) {
        LOG_ERROR(Service_HTTP, "tried to load more then 2 client certs");
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ErrorTooManyClientCerts);
        return;
    }

    constexpr u8 default_cert_id = static_cast<u8>(ClientCertID::Default);
    if (cert_id != default_cert_id) {
        LOG_ERROR(Service_HTTP, "called with invalid cert_id {}", cert_id);
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ErrorWrongCertID);
        return;
    }

    if (!ClCertA.init) {
        LOG_ERROR(Service_HTTP, "called but ClCertA is missing");
    }

    const auto& it = std::find_if(client_certs.begin(), client_certs.end(),
                                  [default_cert_id, &session_data](const auto& i) {
                                      return default_cert_id == i.second->cert_id &&
                                             session_data->session_id == i.second->session_id;
                                  });

    if (it != client_certs.end()) {
        IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
        rb.Push(ResultSuccess);
        rb.Push<u32>(it->first);

        LOG_DEBUG(Service_HTTP, "called, with an already loaded cert_id={}", cert_id);
        return;
    }

    client_certs[++client_certs_counter] = std::make_shared<ClientCertContext>();
    client_certs[client_certs_counter]->handle = client_certs_counter;
    client_certs[client_certs_counter]->certificate = ClCertA.certificate;
    client_certs[client_certs_counter]->private_key = ClCertA.private_key;
    client_certs[client_certs_counter]->session_id = session_data->session_id;
    ++session_data->num_client_certs;

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(ResultSuccess);
    rb.Push<u32>(client_certs_counter);
}

void HTTP_C::CloseClientCertContext(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    ClientCertContext::Handle cert_handle = rp.Pop<u32>();

    LOG_DEBUG(Service_HTTP, "called, cert_handle={}", cert_handle);

    auto* session_data = GetSessionData(ctx.Session());
    ASSERT(session_data);

    if (client_certs.find(cert_handle) == client_certs.end()) {
        LOG_ERROR(Service_HTTP, "Command called with a unkown client cert handle {}", cert_handle);
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        // This just return success without doing anything
        rb.Push(ResultSuccess);
        return;
    }

    if (client_certs[cert_handle]->session_id != session_data->session_id) {
        LOG_ERROR(Service_HTTP, "called from another main session");
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        // This just return success without doing anything
        rb.Push(ResultSuccess);
        return;
    }

    client_certs.erase(cert_handle);
    session_data->num_client_certs--;

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);
}

void HTTP_C::SetKeepAlive(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 context_handle = rp.Pop<u32>();
    const u32 option = rp.Pop<u32>();

    LOG_WARNING(Service_HTTP, "(STUBBED) called, handle={}, option={}", context_handle, option);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);
}

void HTTP_C::SetPostDataTypeSize(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 context_handle = rp.Pop<u32>();
    const PostDataType type = rp.PopEnum<PostDataType>();
    const u32 size = rp.Pop<u32>();

    LOG_DEBUG(Service_HTTP, "called, context_handle={}, type={}, size={}", context_handle, type,
              size);

    if (!PerformStateChecks(ctx, rp, context_handle)) {
        return;
    }

    Context& http_context = GetContext(context_handle);

    if (http_context.state != RequestState::NotStarted) {
        LOG_ERROR(Service_HTTP,
                  "Tried to set chunked mode on a context that has already been started");
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ErrorInvalidRequestState);
        return;
    }

    if (!http_context.post_data.empty() || !http_context.post_data_raw.empty()) {
        LOG_ERROR(Service_HTTP, "Tried to set chunked mode on a context that has Post data");
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ErrorIncompatibleSendPostData);
        return;
    }

    switch (type) {
    case PostDataType::AsciiForm:
    case PostDataType::MultipartForm:
    case PostDataType::Raw:
        http_context.post_data_type = type;
        break;
    default:
        http_context.post_data_type = PostDataType::AsciiForm;
        break;
    }

    http_context.chunked_request = true;
    http_context.chunked_content_length = size;

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);
}

void HTTP_C::Finalize(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    shared_memory = nullptr;

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);

    LOG_WARNING(Service_HTTP, "(STUBBED) called");
}

void HTTP_C::GetDownloadSizeState(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const Context::Handle context_handle = rp.Pop<u32>();

    LOG_INFO(Service_HTTP, "called");

    const auto* session_data = EnsureSessionInitialized(ctx, rp);
    if (!session_data) {
        return;
    }

    Context& http_context = GetContext(context_handle);

    // On the real console, the current downloaded progress and the total size of the content gets
    // returned. Since we do not support chunked downloads on the host, always return the content
    // length if the download is complete and 0 otherwise.
    u32 content_length = 0;
    const bool is_complete = http_context.request_future.wait_for(std::chrono::milliseconds(0)) ==
                             std::future_status::ready;
    if (is_complete) {
        const auto& headers = http_context.response.headers;
        const auto& it = headers.find("Content-Length");
        if (it != headers.end()) {
            content_length = std::stoi(it->second);
        }
    }

    LOG_DEBUG(Service_HTTP, "current={}, total={}", http_context.current_copied_data,
              content_length);

    IPC::RequestBuilder rb = rp.MakeBuilder(3, 0);
    rb.Push(ResultSuccess);
    rb.Push(static_cast<u32>(http_context.current_copied_data));
    rb.Push(content_length);
}

SessionData* HTTP_C::EnsureSessionInitialized(Kernel::HLERequestContext& ctx,
                                              IPC::RequestParser rp) {
    auto* session_data = GetSessionData(ctx.Session());
    ASSERT(session_data);

    if (!session_data->initialized) {
        LOG_ERROR(Service_HTTP, "Tried to make a request on an uninitialized session");
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ErrorStateError);
        return nullptr;
    }

    return session_data;
}

bool HTTP_C::PerformStateChecks(Kernel::HLERequestContext& ctx, IPC::RequestParser rp,
                                Context::Handle context_handle) {
    const auto* session_data = EnsureSessionInitialized(ctx, rp);
    if (!session_data) {
        return false;
    }

    // This command can only be called with a bound context
    if (!session_data->current_http_context) {
        LOG_ERROR(Service_HTTP, "Tried to make a request without a bound context");

        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ErrorNotImplemented);
        return false;
    }

    if (session_data->current_http_context != context_handle) {
        LOG_ERROR(
            Service_HTTP,
            "Tried to make a request on a mismatched session input context={} session context={}",
            context_handle, *session_data->current_http_context);
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ErrorStateError);
        return false;
    }

    return true;
}

void HTTP_C::DecryptClCertA() {
    if (!HW::AES::IsNormalKeyAvailable(HW::AES::KeySlotID::SSLKey)) {
        LOG_ERROR(Service_HTTP, "NormalKey in KeySlot 0x0D missing");
        return;
    }

    HW::AES::AESKey key = HW::AES::GetNormalKey(HW::AES::KeySlotID::SSLKey);
    static constexpr u32 iv_length = 16;
    std::vector<u8> cert_file_data;
    std::vector<u8> key_file_data;

    FileSys::NCCHArchive archive(0x0004001b00010002, Service::FS::MediaType::NAND);

    std::array<char, 8> exefs_filepath;
    FileSys::Path file_path = FileSys::MakeNCCHFilePath(
        FileSys::NCCHFileOpenType::NCCHData, 0, FileSys::NCCHFilePathType::RomFS, exefs_filepath);
    FileSys::Mode open_mode = {};
    open_mode.read_flag.Assign(1);
    auto file_result = archive.OpenFile(file_path, open_mode, 0);
    if (file_result.Failed()) {
        LOG_ERROR(Service_HTTP, "ClCertA file missing, using default");

        cert_file_data.resize(ctr_common_1_cert_bin_size);
        memcpy(cert_file_data.data(), ctr_common_1_cert_bin, cert_file_data.size());

        key_file_data.resize(ctr_common_1_key_bin_size);
        memcpy(key_file_data.data(), ctr_common_1_key_bin, key_file_data.size());
    } else {
        auto romfs = std::move(file_result).Unwrap();
        std::vector<u8> romfs_buffer(romfs->GetSize());
        romfs->Read(0, romfs_buffer.size(), romfs_buffer.data());
        romfs->Close();

        const RomFS::RomFSFile cert_file =
            RomFS::GetFile(romfs_buffer.data(), {u"ctr-common-1-cert.bin"});
        if (cert_file.Length() == 0) {
            LOG_ERROR(Service_HTTP, "ctr-common-1-cert.bin missing");
            return;
        }
        if (cert_file.Length() <= iv_length) {
            LOG_ERROR(Service_HTTP, "ctr-common-1-cert.bin size is too small. Size: {}",
                      cert_file.Length());
            return;
        }

        cert_file_data.resize(cert_file.Length());
        memcpy(cert_file_data.data(), cert_file.Data(), cert_file.Length());

        const RomFS::RomFSFile key_file =
            RomFS::GetFile(romfs_buffer.data(), {u"ctr-common-1-key.bin"});
        if (key_file.Length() == 0) {
            LOG_ERROR(Service_HTTP, "ctr-common-1-key.bin missing");
            return;
        }
        if (key_file.Length() <= iv_length) {
            LOG_ERROR(Service_HTTP, "ctr-common-1-key.bin size is too small. Size: {}",
                      key_file.Length());
            return;
        }

        key_file_data.resize(key_file.Length());
        memcpy(key_file_data.data(), key_file.Data(), key_file.Length());
    }

    std::vector<u8> cert_data(cert_file_data.size() - iv_length);

    std::array<u8, iv_length> cert_iv;
    std::memcpy(cert_iv.data(), cert_file_data.data(), iv_length);
    if (!Common::Crypto::AESCBCDecrypt(
            std::span<const u8>(cert_file_data.data() + iv_length, cert_file_data.size() - iv_length),
            cert_data, key, cert_iv)) {
        LOG_ERROR(Service_HTTP, "Failed to decrypt ctr-common-1-cert.bin");
        return;
    }

    std::vector<u8> key_data(key_file_data.size() - iv_length);

    std::array<u8, iv_length> key_iv;
    std::memcpy(key_iv.data(), key_file_data.data(), iv_length);
    if (!Common::Crypto::AESCBCDecrypt(
            std::span<const u8>(key_file_data.data() + iv_length, key_file_data.size() - iv_length),
            key_data, key, key_iv)) {
        LOG_ERROR(Service_HTTP, "Failed to decrypt ctr-common-1-key.bin");
        return;
    }

    ClCertA.certificate = std::move(cert_data);
    ClCertA.private_key = std::move(key_data);
    ClCertA.init = true;
}

HTTP_C::HTTP_C() : ServiceFramework("http:C", 32) {
    static const FunctionInfo functions[] = {
        // clang-format off
        {0x0001, &HTTP_C::Initialize, "Initialize"},
        {0x0002, &HTTP_C::CreateContext, "CreateContext"},
        {0x0003, &HTTP_C::CloseContext, "CloseContext"},
        {0x0004, &HTTP_C::CancelConnection, "CancelConnection"},
        {0x0005, &HTTP_C::GetRequestState, "GetRequestState"},
        {0x0006, &HTTP_C::GetDownloadSizeState, "GetDownloadSizeState"},
        {0x0007, nullptr, "GetRequestError"},
        {0x0008, &HTTP_C::InitializeConnectionSession, "InitializeConnectionSession"},
        {0x0009, &HTTP_C::BeginRequest, "BeginRequest"},
        {0x000A, &HTTP_C::BeginRequestAsync, "BeginRequestAsync"},
        {0x000B, &HTTP_C::ReceiveData, "ReceiveData"},
        {0x000C, &HTTP_C::ReceiveDataTimeout, "ReceiveDataTimeout"},
        {0x000D, nullptr, "SetProxy"},
        {0x000E, &HTTP_C::SetProxyDefault, "SetProxyDefault"},
        {0x000F, nullptr, "SetBasicAuthorization"},
        {0x0010, nullptr, "SetSocketBufferSize"},
        {0x0011, &HTTP_C::AddRequestHeader, "AddRequestHeader"},
        {0x0012, &HTTP_C::AddPostDataAscii, "AddPostDataAscii"},
        {0x0013, &HTTP_C::AddPostDataBinary, "AddPostDataBinary"},
        {0x0014, &HTTP_C::AddPostDataRaw, "AddPostDataRaw"},
        {0x0015, &HTTP_C::SetPostDataType, "SetPostDataType"},
        {0x0016, &HTTP_C::SendPostDataAscii, "SendPostDataAscii"},
        {0x0017, &HTTP_C::SendPostDataAsciiTimeout, "SendPostDataAsciiTimeout"},
        {0x0018, &HTTP_C::SendPostDataBinary, "SendPostDataBinary"},
        {0x0019, &HTTP_C::SendPostDataBinaryTimeout, "SendPostDataBinaryTimeout"},
        {0x001A, &HTTP_C::SendPostDataRaw, "SendPostDataRaw"},
        {0x001B, &HTTP_C::SendPostDataRawTimeout, "SendPostDataRawTimeout"},
        {0x001C, &HTTP_C::SetPostDataEncoding, "SetPostDataEncoding"},
        {0x001D, &HTTP_C::NotifyFinishSendPostData, "NotifyFinishSendPostData"},
        {0x001E, &HTTP_C::GetResponseHeader, "GetResponseHeader"},
        {0x001F, &HTTP_C::GetResponseHeaderTimeout, "GetResponseHeaderTimeout"},
        {0x0020, &HTTP_C::GetResponseData, "GetResponseData"},
        {0x0021, &HTTP_C::GetResponseDataTimeout, "GetResponseDataTimeout"},
        {0x0022, &HTTP_C::GetResponseStatusCode, "GetResponseStatusCode"},
        {0x0023, &HTTP_C::GetResponseStatusCodeTimeout, "GetResponseStatusCodeTimeout"},
        {0x0024, &HTTP_C::AddTrustedRootCA, "AddTrustedRootCA"},
        {0x0025, &HTTP_C::AddDefaultCert, "AddDefaultCert"},
        {0x0026, nullptr, "SelectRootCertChain"},
        {0x0027, nullptr, "SetClientCert"},
        {0x0028, &HTTP_C::SetDefaultClientCert, "SetDefaultClientCert"},
        {0x0029, &HTTP_C::SetClientCertContext, "SetClientCertContext"},
        {0x002A, &HTTP_C::GetSSLError, "GetSSLError"},
        {0x002B, &HTTP_C::SetSSLOpt, "SetSSLOpt"},
        {0x002C, nullptr, "SetSSLClearOpt"},
        {0x002D, nullptr, "CreateRootCertChain"},
        {0x002E, nullptr, "DestroyRootCertChain"},
        {0x002F, nullptr, "RootCertChainAddCert"},
        {0x0030, nullptr, "RootCertChainAddDefaultCert"},
        {0x0031, nullptr, "RootCertChainRemoveCert"},
        {0x0032, &HTTP_C::OpenClientCertContext, "OpenClientCertContext"},
        {0x0033, &HTTP_C::OpenDefaultClientCertContext, "OpenDefaultClientCertContext"},
        {0x0034, &HTTP_C::CloseClientCertContext, "CloseClientCertContext"},
        {0x0035, nullptr, "SetDefaultProxy"},
        {0x0036, nullptr, "ClearDNSCache"},
        {0x0037, &HTTP_C::SetKeepAlive, "SetKeepAlive"},
        {0x0038, &HTTP_C::SetPostDataTypeSize, "SetPostDataTypeSize"},
        {0x0039, &HTTP_C::Finalize, "Finalize"},
        // clang-format on
    };
    RegisterHandlers(functions);

    DecryptClCertA();
}

std::shared_ptr<HTTP_C> GetService(Core::System& system) {
    return system.ServiceManager().GetService<HTTP_C>("http:C");
}

void InstallInterfaces(Core::System& system) {
    auto& service_manager = system.ServiceManager();
    std::make_shared<HTTP_C>()->InstallAsService(service_manager);
}
} // namespace Service::HTTP
