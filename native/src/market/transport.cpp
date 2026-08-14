#include "tdx/transport.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <sstream>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#include <zlib.h>

namespace tdx {
namespace {

constexpr std::uint8_t request_prefix = 0x0C;
constexpr std::uint8_t expansion_request_prefix = 0x01;
constexpr std::array<std::uint8_t, 4> response_prefix{0xB1, 0xCB, 0x74, 0x00};
constexpr std::uint8_t control_default = 1;
constexpr std::uint16_t handshake_type = 0x000D;
constexpr std::uint16_t expansion_handshake_type = 0x2454;
constexpr std::size_t response_header_size = 16;
constexpr std::uintptr_t invalid_socket_value = ~std::uintptr_t{0};

#ifdef _WIN32
class WinsockRuntime {
public:
    WinsockRuntime() {
        WSADATA data{};
        const int result = WSAStartup(MAKEWORD(2, 2), &data);
        if (result != 0) throw Error("WSAStartup failed: " + std::to_string(result));
    }
    ~WinsockRuntime() { WSACleanup(); }
};

WinsockRuntime& winsock_runtime() {
    static WinsockRuntime runtime;
    return runtime;
}

std::string socket_error(const char* operation) {
    return std::string(operation) + " failed: WSA " + std::to_string(WSAGetLastError());
}
#endif

void append_u16(Bytes& output, std::uint16_t value) {
    output.push_back(static_cast<std::uint8_t>(value));
    output.push_back(static_cast<std::uint8_t>(value >> 8));
}

void append_u32(Bytes& output, std::uint32_t value) {
    for (int shift = 0; shift < 32; shift += 8)
        output.push_back(static_cast<std::uint8_t>(value >> shift));
}

}  // namespace

std::string Endpoint::address() const {
    return host + ":" + std::to_string(port);
}

Endpoint parse_endpoint(std::string_view value) {
    const std::string text = trim(std::string(value));
    if (text.empty()) throw Error("server endpoint cannot be empty");

    std::string host;
    std::string port_text;
    if (text.front() == '[') {
        const auto close = text.find(']');
        if (close == std::string::npos) throw Error("invalid endpoint: " + text);
        host = text.substr(1, close - 1);
        if (close + 1 < text.size()) {
            if (text[close + 1] != ':') throw Error("invalid endpoint: " + text);
            port_text = text.substr(close + 2);
        }
    } else {
        const auto colon = text.rfind(':');
        if (colon != std::string::npos && text.find(':') == colon) {
            host = text.substr(0, colon);
            port_text = text.substr(colon + 1);
        } else {
            host = text;
        }
    }
    if (host.empty()) throw Error("invalid endpoint: " + text);
    unsigned long port = 7709;
    if (!port_text.empty()) {
        if (!std::all_of(port_text.begin(), port_text.end(), [](unsigned char ch) {
                return ch >= '0' && ch <= '9';
            })) throw Error("invalid endpoint port: " + text);
        try {
            port = std::stoul(port_text);
        } catch (...) {
            throw Error("invalid endpoint port: " + text);
        }
    }
    if (port < 1 || port > 65535) throw Error("endpoint port out of range: " + text);
    return Endpoint{host, static_cast<std::uint16_t>(port), "command line"};
}

bool detail::is_transient_quote_transport_error(
    std::string_view message) noexcept {
    static constexpr std::string_view markers[] = {
        "cannot resolve ",
        "cannot connect to ",
        "server closed connection",
        "send failed: WSA ",
        "receive failed: WSA ",
    };
    return std::any_of(std::begin(markers), std::end(markers),
                       [&](std::string_view marker) {
                           return message.find(marker) != std::string_view::npos;
                       });
}

Bytes build_request_frame(std::uint32_t message_id, std::uint16_t message_type,
                          const Bytes& data, std::uint8_t prefix) {
    if (data.size() + 2 > std::numeric_limits<std::uint16_t>::max())
        throw Error("7709 request payload is too large");
    const auto length = static_cast<std::uint16_t>(data.size() + 2);
    Bytes result;
    result.reserve(12 + data.size());
    if (prefix != request_prefix && prefix != expansion_request_prefix)
        throw Error("market-data request prefix must be 0x0C or 0x01");
    result.push_back(prefix);
    append_u32(result, message_id);
    result.push_back(control_default);
    append_u16(result, length);
    append_u16(result, length);
    append_u16(result, message_type);
    result.insert(result.end(), data.begin(), data.end());
    return result;
}

QuoteConnection::QuoteConnection(Endpoint endpoint, int timeout_ms, QuoteProtocol protocol)
    : endpoint_(std::move(endpoint)), protocol_(protocol) {
#ifdef _WIN32
    if (timeout_ms < 1) throw Error("network timeout must be positive");
    (void)winsock_runtime();
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    addrinfo* addresses = nullptr;
    const auto port = std::to_string(endpoint_.port);
    const int lookup = getaddrinfo(endpoint_.host.c_str(), port.c_str(), &hints, &addresses);
    if (lookup != 0) throw Error("cannot resolve " + endpoint_.address() + ": " +
                                 gai_strerrorA(lookup));

    int last_error = 0;
    for (auto* address = addresses; address; address = address->ai_next) {
        const SOCKET candidate = ::socket(address->ai_family, address->ai_socktype,
                                           address->ai_protocol);
        if (candidate == INVALID_SOCKET) {
            last_error = WSAGetLastError();
            continue;
        }
        const DWORD timeout = static_cast<DWORD>(timeout_ms);
        setsockopt(candidate, SOL_SOCKET, SO_RCVTIMEO,
                   reinterpret_cast<const char*>(&timeout), sizeof(timeout));
        setsockopt(candidate, SOL_SOCKET, SO_SNDTIMEO,
                   reinterpret_cast<const char*>(&timeout), sizeof(timeout));
        if (::connect(candidate, address->ai_addr, static_cast<int>(address->ai_addrlen)) == 0) {
            socket_ = static_cast<std::uintptr_t>(candidate);
            break;
        }
        last_error = WSAGetLastError();
        closesocket(candidate);
    }
    freeaddrinfo(addresses);
    if (socket_ == invalid_socket_value)
        throw Error("cannot connect to " + endpoint_.address() + ": WSA " +
                    std::to_string(last_error));

    try {
        if (protocol_ == QuoteProtocol::expansion) {
            next_message_id_ = 0;
            static const Bytes setup{
                0x1f, 0x32, 0xc6, 0xe5, 0xd5, 0x3d, 0xfb, 0x41,
                0x1f, 0x32, 0xc6, 0xe5, 0xd5, 0x3d, 0xfb, 0x41,
                0x1f, 0x32, 0xc6, 0xe5, 0xd5, 0x3d, 0xfb, 0x41,
                0x1f, 0x32, 0xc6, 0xe5, 0xd5, 0x3d, 0xfb, 0x41,
                0x1f, 0x32, 0xc6, 0xe5, 0xd5, 0x3d, 0xfb, 0x41,
                0x1f, 0x32, 0xc6, 0xe5, 0xd5, 0x3d, 0xfb, 0x41,
                0x1f, 0x32, 0xc6, 0xe5, 0xd5, 0x3d, 0xfb, 0x41,
                0x1f, 0x32, 0xc6, 0xe5, 0xd5, 0x3d, 0xfb, 0x41,
                0xcc, 0xe1, 0x6d, 0xff, 0xd5, 0xba, 0x3f, 0xb8,
                0xcb, 0xc5, 0x7a, 0x05, 0x4f, 0x77, 0x48, 0xea
            };
            (void)call(expansion_handshake_type, setup);
            server_name_ = "TDX expansion market";
        } else {
            const auto response = call(handshake_type, Bytes{1});
            if (response.data.size() < 189)
                throw Error("handshake response is too short: " +
                            std::to_string(response.data.size()));
            Bytes encoded_name;
            encoded_name.reserve(84);
            for (std::size_t index = 68; index < 152; ++index)
                if (response.data[index] != 0) encoded_name.push_back(response.data[index]);
            server_name_ = trim(decode_gbk(encoded_name));
        }
    } catch (...) {
        close();
        throw;
    }
#else
    (void)timeout_ms;
    throw Error("7709 transport is currently implemented for Windows only");
#endif
}

QuoteConnection::~QuoteConnection() { close(); }

void QuoteConnection::close() noexcept {
#ifdef _WIN32
    if (socket_ != invalid_socket_value) {
        closesocket(static_cast<SOCKET>(socket_));
        socket_ = invalid_socket_value;
    }
#endif
}

void QuoteConnection::send_all(const Bytes& data) {
#ifdef _WIN32
    std::size_t sent = 0;
    while (sent < data.size()) {
        const auto remaining = std::min<std::size_t>(data.size() - sent,
                                                     std::numeric_limits<int>::max());
        const int count = ::send(static_cast<SOCKET>(socket_),
                                 reinterpret_cast<const char*>(data.data() + sent),
                                 static_cast<int>(remaining), 0);
        if (count == SOCKET_ERROR) throw Error(socket_error("send"));
        if (count == 0) throw Error("server closed connection while sending");
        sent += static_cast<std::size_t>(count);
    }
#else
    (void)data;
#endif
}

Bytes QuoteConnection::receive_exact(std::size_t size) {
    Bytes result(size);
#ifdef _WIN32
    std::size_t received = 0;
    while (received < size) {
        const auto remaining = std::min<std::size_t>(size - received,
                                                     std::numeric_limits<int>::max());
        const int count = ::recv(static_cast<SOCKET>(socket_),
                                 reinterpret_cast<char*>(result.data() + received),
                                 static_cast<int>(remaining), 0);
        if (count == SOCKET_ERROR) throw Error(socket_error("receive"));
        if (count == 0)
            throw Error("server closed connection after " + std::to_string(received) +
                        " of " + std::to_string(size) + " bytes");
        received += static_cast<std::size_t>(count);
    }
#endif
    return result;
}

ResponseFrame QuoteConnection::read_response() {
    const auto header = receive_exact(response_header_size);
    if (!std::equal(response_prefix.begin(), response_prefix.end(), header.begin())) {
        std::ostringstream detail;
        detail << "invalid 7709 response prefix";
        throw Error(detail.str());
    }
    ResponseFrame frame;
    frame.control = header[4];
    frame.message_id = read_u32_le(header.data() + 5);
    frame.message_type = read_u16_le(header.data() + 10);
    const auto compressed_size = read_u16_le(header.data() + 12);
    const auto decoded_size = read_u16_le(header.data() + 14);
    auto wire = receive_exact(compressed_size);
    if (compressed_size == decoded_size) {
        frame.data = std::move(wire);
    } else {
        frame.data.resize(decoded_size);
        uLongf output_size = decoded_size;
        const int result = uncompress(frame.data.data(), &output_size, wire.data(),
                                      static_cast<uLong>(wire.size()));
        if (result != Z_OK)
            throw Error("7709 zlib decompression failed: " + std::to_string(result));
        frame.data.resize(static_cast<std::size_t>(output_size));
    }
    if (frame.data.size() != decoded_size)
        throw Error("7709 decoded response length mismatch");
    return frame;
}

ResponseFrame QuoteConnection::call(std::uint16_t message_type, const Bytes& data) {
    if (socket_ == invalid_socket_value) throw Error("market-data connection is not open");
    const auto message_id = next_message_id_++;
    send_all(build_request_frame(message_id, message_type, data,
        protocol_ == QuoteProtocol::expansion ? expansion_request_prefix : request_prefix));
    auto response = read_response();
    if (response.message_id != message_id)
        throw Error("7709 response message ID mismatch");
    if (response.message_type != message_type)
        throw Error("7709 response command mismatch");
    return response;
}

const Endpoint& QuoteConnection::endpoint() const noexcept { return endpoint_; }
const std::string& QuoteConnection::server_name() const noexcept { return server_name_; }

}  // namespace tdx
