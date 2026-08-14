#include "tdx/common.hpp"
#include "tdx/transport.hpp"

#include <iostream>
#include <string>

namespace {

void require(bool value, const char* message) {
    if (!value) throw tdx::Error(message);
}

}  // namespace

int main() {
    try {
        require(tdx::detail::is_transient_quote_transport_error(
                    "server closed connection after 0 of 16 bytes") &&
                    tdx::detail::is_transient_quote_transport_error(
                    "receive failed: WSA 10060") &&
                    tdx::detail::is_transient_quote_transport_error(
                    "cannot connect to test:7709: WSA 10061"),
                "known TCP failures must be retryable");
        require(!tdx::detail::is_transient_quote_transport_error(
                    "snapshot response count exceeds request count") &&
                    !tdx::detail::is_transient_quote_transport_error(
                    "7709 response command mismatch"),
                "decoder and protocol failures must not be retried");

        int calls = 0;
        int attempts = 0;
        const auto recovered = tdx::detail::retry_quote_transport([&] {
            ++calls;
            if (calls < 3)
                throw tdx::Error(
                    "server closed connection after 0 of 16 bytes");
            return std::string("live");
        }, attempts, 3, 0);
        require(recovered == "live" && calls == 3 && attempts == 3,
                "transient TCP closure must recover on a fresh bounded attempt");

        calls = 0;
        attempts = 0;
        bool decoder_error_preserved = false;
        try {
            (void)tdx::detail::retry_quote_transport([&]() -> int {
                ++calls;
                throw tdx::Error("snapshot response count exceeds request count");
            }, attempts, 3, 0);
        } catch (const tdx::Error& error) {
            decoder_error_preserved = std::string(error.what()).find(
                "snapshot response count") != std::string::npos;
        }
        require(decoder_error_preserved && calls == 1 && attempts == 1,
                "stable decoder errors must fail without retry");

        calls = 0;
        attempts = 0;
        bool exhausted = false;
        try {
            (void)tdx::detail::retry_quote_transport([&]() -> int {
                ++calls;
                throw tdx::Error("receive failed: WSA 10054");
            }, attempts, 3, 0);
        } catch (const tdx::Error& error) {
            exhausted = std::string(error.what()).find("WSA 10054") !=
                        std::string::npos;
        }
        require(exhausted && calls == 3 && attempts == 3,
                "persistent TCP failure must stop at the configured bound");

        std::cout << "quote transport retry tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
