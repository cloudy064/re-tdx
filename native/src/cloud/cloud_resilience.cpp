#include "tdx/cloud_resilience.hpp"

#include "tdx/common.hpp"

#include <chrono>
#include <string>
#include <thread>

namespace tdx::detail {
namespace {

bool contains(std::string_view message, std::string_view token) noexcept {
    return message.find(token) != std::string_view::npos;
}

bool transient_http_or_winhttp(std::string_view message,
                               std::string_view prefix) noexcept {
    for (const auto* status : {"429", "502", "503", "504"}) {
        const auto token = std::string(prefix) + " HTTP status " + status;
        if (contains(message, token)) return true;
    }
    for (const auto* token : {"WinHttpSendRequest failed",
                              "WinHttpReceiveResponse failed"})
        if (contains(message, token)) return true;
    return false;
}

}  // namespace

bool is_transient_tqlex_error(std::string_view message) noexcept {
    return transient_http_or_winhttp(message, "TQLEX") ||
           contains(message, "TQLEX server returned ErrorCode 4");
}

bool is_transient_pbrpc_error(std::string_view message) noexcept {
    return transient_http_or_winhttp(message, "PBRPC") ||
           contains(message, "PBRPC business ErrorCode 4") ||
           contains(message, "PBRPC server code 4") ||
           contains(message, "PBRPC server rejected request with RpcID -1") ||
           contains(message, "RpcID -1") || contains(message, "RPC ID -1");
}

bool is_transient_cloud_error(std::string_view message) noexcept {
    return is_transient_tqlex_error(message) ||
           is_transient_pbrpc_error(message);
}

Json retry_cloud_json(const CloudJsonAttempt& operation,
                      const CloudErrorClassifier& classifier,
                      int& attempts,
                      int max_attempts,
                      int retry_delay_ms) {
    if (!operation) throw Error("cloud retry operation is empty");
    if (!classifier) throw Error("cloud retry classifier is empty");
    if (max_attempts < 1 || max_attempts > 10 || retry_delay_ms < 0 ||
        retry_delay_ms > 60000)
        throw Error("cloud retry options are outside the safe range");
    attempts = 0;
    for (;;) {
        ++attempts;
        try {
            return operation();
        } catch (const std::exception& error) {
            if (attempts >= max_attempts || !classifier(error.what())) throw;
        }
        if (retry_delay_ms)
            std::this_thread::sleep_for(std::chrono::milliseconds(
                retry_delay_ms * attempts));
    }
}

}  // namespace tdx::detail
