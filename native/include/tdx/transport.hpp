#pragma once

#include "tdx/common.hpp"

#include <chrono>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>

namespace tdx {

struct Endpoint {
    std::string host;
    std::uint16_t port{7709};
    std::string name;
    std::string address() const;
};

struct ResponseFrame {
    std::uint8_t control{};
    std::uint32_t message_id{};
    std::uint16_t message_type{};
    Bytes data;
};

Endpoint parse_endpoint(std::string_view value);
Bytes build_request_frame(std::uint32_t message_id, std::uint16_t message_type,
                          const Bytes& data = {}, std::uint8_t prefix = 0x0C);

namespace detail {

// Keep automatic reconnects deliberately narrower than protocol/decoder errors:
// a fresh TCP connection can recover connection setup, send and receive failures,
// but it cannot make an unsupported command or malformed payload valid.
bool is_transient_quote_transport_error(std::string_view message) noexcept;

template <typename Operation>
auto retry_quote_transport(Operation&& operation, int& attempts,
                           int max_attempts = 3,
                           int retry_delay_ms = 50) -> decltype(operation()) {
    if (max_attempts < 1 || max_attempts > 10 || retry_delay_ms < 0 ||
        retry_delay_ms > 60000)
        throw Error("quote transport retry options are outside the safe range");
    attempts = 0;
    for (;;) {
        ++attempts;
        try {
            return operation();
        } catch (const std::exception& error) {
            if (attempts >= max_attempts ||
                !is_transient_quote_transport_error(error.what()))
                throw;
        }
        if (retry_delay_ms > 0)
            std::this_thread::sleep_for(std::chrono::milliseconds(
                retry_delay_ms * attempts));
    }
}

}  // namespace detail

enum class QuoteProtocol {
    standard,
    expansion
};

class QuoteConnection {
public:
    explicit QuoteConnection(Endpoint endpoint, int timeout_ms = 10000,
                             QuoteProtocol protocol = QuoteProtocol::standard);
    ~QuoteConnection();
    QuoteConnection(const QuoteConnection&) = delete;
    QuoteConnection& operator=(const QuoteConnection&) = delete;
    QuoteConnection(QuoteConnection&&) = delete;
    QuoteConnection& operator=(QuoteConnection&&) = delete;

    ResponseFrame call(std::uint16_t message_type, const Bytes& data = {});
    const Endpoint& endpoint() const noexcept;
    const std::string& server_name() const noexcept;

private:
    void close() noexcept;
    void send_all(const Bytes& data);
    Bytes receive_exact(std::size_t size);
    ResponseFrame read_response();

    Endpoint endpoint_;
    QuoteProtocol protocol_{QuoteProtocol::standard};
    std::uintptr_t socket_{~std::uintptr_t{0}};
    std::uint32_t next_message_id_{0x01640801};
    std::string server_name_;
};

}  // namespace tdx
