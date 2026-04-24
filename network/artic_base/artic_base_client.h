#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace Network::ArticBase {

class Client {
public:
    class Request {
    public:
        void AddParameterS32(std::int32_t) {}
        void AddParameterS64(std::int64_t) {}
        void AddParameterU32(std::uint32_t) {}
        void AddParameterU64(std::uint64_t) {}
        void AddParameterU8(std::uint8_t) {}
        void AddParameterS8(std::int8_t) {}
        void AddParameterBuffer(const void*, std::size_t) {}
    };

    class Response {
    public:
        bool Succeeded() const { return false; }
        int GetMethodResult() const { return -1; }
        std::optional<std::int32_t> GetResponseS32(int) const { return std::nullopt; }
        std::optional<std::int64_t> GetResponseS64(int) const { return std::nullopt; }
        std::optional<std::uint64_t> GetResponseU64(int) const { return std::nullopt; }
        std::optional<std::vector<std::uint8_t>> GetResponseBuffer(int) const { return std::nullopt; }
    };

    Request NewRequest(const std::string&) { return {}; }
    std::optional<Response> Send(const Request&) { return std::nullopt; }
    std::size_t GetServerRequestMaxSize() const { return 4096; }
};

} // namespace Network::ArticBase
