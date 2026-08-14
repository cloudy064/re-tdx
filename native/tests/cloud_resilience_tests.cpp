#include "tdx/cloud_resilience.hpp"
#include "tdx/common.hpp"

#include <iostream>
#include <string>

namespace {

void require(bool value, const char* message) {
    if (!value) throw tdx::Error(message);
}

}  // namespace

int main() {
    try {
        require(tdx::detail::is_transient_tqlex_error(
                    "TQLEX HTTP status 503") &&
                tdx::detail::is_transient_tqlex_error(
                    "TQLEX server returned ErrorCode 4: busy") &&
                !tdx::detail::is_transient_tqlex_error(
                    "TQLEX server returned ErrorCode 5: stable"),
                "TQLEX transient classifier boundary");
        require(tdx::detail::is_transient_pbrpc_error(
                    "PBRPC server rejected request with RpcID -1") &&
                tdx::detail::is_transient_pbrpc_error(
                    "PBRPC business ErrorCode 4") &&
                !tdx::detail::is_transient_pbrpc_error(
                    "PBRPC business ErrorCode 5"),
                "PBRPC transient classifier boundary");
        require(tdx::detail::is_transient_cloud_error(
                    "WinHttpReceiveResponse failed: 12030"),
                "shared cloud classifier must recognize WinHTTP receive failures");

        int calls = 0;
        int attempts = 0;
        const auto recovered = tdx::detail::retry_cloud_json([&] {
            ++calls;
            if (calls < 3) throw tdx::Error("TQLEX HTTP status 503");
            auto result = tdx::Json::object();
            result["ok"] = true;
            return result;
        }, tdx::detail::is_transient_tqlex_error, attempts, 3, 0);
        require(calls == 3 && attempts == 3 && recovered.at("ok").as_bool(),
                "cloud retry did not recover on the third whole-query attempt");

        calls = 0;
        bool stable_failed = false;
        try {
            (void)tdx::detail::retry_cloud_json([&]() -> tdx::Json {
                ++calls;
                throw tdx::Error("TQLEX server returned ErrorCode 5: stable");
            }, tdx::detail::is_transient_tqlex_error, attempts, 3, 0);
        } catch (const tdx::Error& error) {
            stable_failed = std::string(error.what()).find("ErrorCode 5") !=
                std::string::npos;
        }
        require(stable_failed && calls == 1 && attempts == 1,
                "stable cloud errors must not be retried");

        calls = 0;
        bool exhausted = false;
        try {
            (void)tdx::detail::retry_cloud_json([&]() -> tdx::Json {
                ++calls;
                throw tdx::Error(
                    "PBRPC server rejected request with RpcID -1");
            }, tdx::detail::is_transient_pbrpc_error, attempts, 3, 0);
        } catch (const tdx::Error& error) {
            exhausted = std::string(error.what()).find("RpcID -1") !=
                std::string::npos;
        }
        require(exhausted && calls == 3 && attempts == 3,
                "PBRPC retry exhaustion must preserve the upstream error");

        std::cout << "cloud resilience tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
