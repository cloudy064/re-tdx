#include "tdx/server.hpp"

#include "server_catalog_internal.hpp"
#include "server_core_internal.hpp"

#include "tdx/blocks_quote.hpp"
#include "server_formula_internal.hpp"
#include "server_state_internal.hpp"

#include "tdx/common.hpp"
#include "tdx/level2.hpp"
#include "tdx/market_stream.hpp"

#include <algorithm>
#include <array>
#include <filesystem>
#include <iostream>
#include <limits>
#include <memory>
#include <sstream>
#include <string_view>
#include <thread>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace tdx {
namespace server_detail {
#ifdef _WIN32
std::string receive_request(SOCKET client) {
    std::string result;
    std::array<char, 4096> buffer{};
    std::size_t header_end = std::string::npos;
    while ((header_end = result.find("\r\n\r\n")) == std::string::npos) {
        const int count = recv(client, buffer.data(), static_cast<int>(buffer.size()), 0);
        if (count == SOCKET_ERROR) throw Error("HTTP receive failed: WSA " + std::to_string(WSAGetLastError()));
        if (count == 0) break;
        result.append(buffer.data(), static_cast<std::size_t>(count));
        if (result.find("\r\n\r\n") == std::string::npos && result.size() > 65536)
            throw Error("HTTP request header exceeds 64 KiB");
    }
    header_end = result.find("\r\n\r\n");
    if (header_end == std::string::npos) return result;
    const auto lowered_header = lower_ascii(result.substr(0, header_end));
    std::size_t content_length = 0;
    const auto marker = lowered_header.find("\r\ncontent-length:");
    if (marker != std::string::npos) {
        const auto value_start = marker + std::string("\r\ncontent-length:").size();
        const auto value_end = lowered_header.find("\r\n", value_start);
        const auto raw = trim(lowered_header.substr(
            value_start, value_end == std::string::npos
                ? lowered_header.size() - value_start : value_end - value_start));
        try {
            std::size_t used = 0;
            content_length = static_cast<std::size_t>(std::stoull(raw, &used));
            if (used != raw.size()) throw std::invalid_argument("trailing");
        } catch (...) {
            throw Error("invalid HTTP Content-Length");
        }
        if (content_length > maximum_post_request_bytes)
            throw Error("HTTP request body exceeds 1 MiB");
        constexpr std::string_view project_prefix =
            "post /api/v1/level2/project";
        const bool project_request =
            lowered_header.rfind(project_prefix, 0) == 0 &&
            lowered_header.size() > project_prefix.size() &&
            (lowered_header[project_prefix.size()] == ' ' ||
             lowered_header[project_prefix.size()] == '?');
        if (project_request && content_length > level2_offline_payload_limit)
            throw Error("Level2 project request body exceeds the 384 KiB API limit");
    }
    const auto total = header_end + 4 + content_length;
    while (result.size() < total) {
        const auto wanted = std::min<std::size_t>(buffer.size(), total - result.size());
        const int count = recv(client, buffer.data(), static_cast<int>(wanted), 0);
        if (count == SOCKET_ERROR)
            throw Error("HTTP receive failed: WSA " + std::to_string(WSAGetLastError()));
        if (count == 0) throw Error("HTTP client closed before request body completed");
        result.append(buffer.data(), static_cast<std::size_t>(count));
    }
    if (result.size() > total) result.resize(total);
    return result;
}

std::string_view http_request_body(std::string_view request) {
    const auto separator = request.find("\r\n\r\n");
    return separator == std::string_view::npos
        ? std::string_view{} : request.substr(separator + 4);
}

void send_all(SOCKET client, std::string_view data) {
    std::size_t sent = 0;
    while (sent < data.size()) {
        const auto remaining = std::min<std::size_t>(data.size() - sent,
                                                     static_cast<std::size_t>(std::numeric_limits<int>::max()));
        const int count = send(client, data.data() + sent, static_cast<int>(remaining), 0);
        if (count == SOCKET_ERROR) throw Error("HTTP send failed: WSA " + std::to_string(WSAGetLastError()));
        if (count == 0) throw Error("HTTP client closed while sending");
        sent += static_cast<std::size_t>(count);
    }
}

void send_response(SOCKET client, const HttpResponse& response) {
    std::ostringstream header;
    header << "HTTP/1.1 " << response.status << ' ' << response.reason << "\r\n"
           << "Content-Type: " << response.content_type << "\r\n"
           << "Content-Length: " << response.body.size() << "\r\n"
           << "Access-Control-Allow-Origin: *\r\n"
           << "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
           << "Access-Control-Allow-Headers: Content-Type, X-TDX-Action\r\n"
           << "Cache-Control: no-store\r\n"
           << "Connection: close\r\n\r\n";
    send_all(client, header.str());
    send_all(client, response.body);
}

void send_market_stream(SOCKET client, const std::shared_ptr<MarketStreamHub>& hub,
                        const std::string& market, const std::string& code,
                        int max_events, int wait_timeout_ms) {
    const auto subscriber = hub->subscribe(market, code);
    try {
        const std::string header =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/event-stream; charset=utf-8\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "Cache-Control: no-cache, no-transform\r\n"
            "X-Accel-Buffering: no\r\n"
            "Connection: keep-alive\r\n\r\n"
            "retry: 2000\n\n";
        send_all(client, header);
        int emitted = 0;
        while (max_events == 0 || emitted < max_events) {
            Json event;
            if (hub->next(subscriber, event, wait_timeout_ms)) {
                send_all(client, format_market_stream_sse_event(event));
                ++emitted;
            } else {
                send_all(client, ": keepalive\n\n");
            }
        }
    } catch (...) {
        hub->unsubscribe(subscriber);
        throw;
    }
    hub->unsubscribe(subscriber);
}

#endif

fs::path locate_web_root(const std::string& explicit_path) {
    std::vector<fs::path> candidates;
    if (!explicit_path.empty()) candidates.push_back(from_utf8(explicit_path));
    candidates.push_back(fs::path("web") / "dist");
#ifdef _WIN32
    const auto executable = running_executable_path();
    if (!executable.empty())
        candidates.push_back(executable.parent_path().parent_path() /
                             "share" / "tdx-tool" / "web");
#endif
    for (const auto& candidate : candidates) {
        if (fs::is_directory(candidate) && fs::is_regular_file(candidate / "index.html"))
            return fs::weakly_canonical(candidate);
    }
    throw Error("cannot locate the Svelte build (web/dist/index.html); run npm run build "
                "in web or pass --web-root PATH");
}

fs::path locate_jsn_root(const std::string& explicit_path) {
    std::vector<fs::path> candidates;
    if (!explicit_path.empty()) candidates.push_back(from_utf8(explicit_path));
    candidates.push_back(fs::path("output") / "tdx-jsn");
    for (const auto& candidate : candidates)
        if (fs::is_directory(candidate)) return fs::weakly_canonical(candidate);
    if (!explicit_path.empty()) throw Error("JSN data directory does not exist: " + explicit_path);
    return {};
}

void print_help() {
    std::cout <<
        "Usage: tdx-tool serve [options]\n\n"
        "Pure C++ local dashboard and read-only JSON API.\n\n"
        "Options:\n"
        "  --root PATH             TDX installation root\n"
        "  --bind ADDRESS          Default 127.0.0.1\n"
        "  --port N                Default 8765\n"
        "  --web-root PATH         Override Svelte dist directory\n"
        "  --jsn-root PATH         Downloaded JSN directory (auto-detect output/tdx-jsn)\n"
        "  --cache-root PATH       Runtime cache directory (default output/tdx-runtime-cache)\n"
        "  --include-user-formulas Load private T0002/PriGS.dat formulas read-only\n"
        "                          System formulas/icons always use the bundled snapshot\n"
        "  --allow-remote          Required for non-loopback bind\n"
        "  --self-test             Build indexes and exercise routes without listening\n"
        "  --max-requests N        Stop after N requests; 0 runs until interrupted\n";
}

}  // namespace server_detail

using namespace server_detail;

int command_serve(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) { print_help(); return 0; }
    const auto root_text = args.take_option("--root");
    auto bind_address = lower_ascii(trim(args.take_option("--bind", "127.0.0.1")));
    const int port = parse_bounded(args.take_option("--port", "8765"), "--port", 1, 65535);
    const int max_requests = parse_bounded(args.take_option("--max-requests", "0"),
                                           "--max-requests", 0, 100000000);
    const auto web_root_text = args.take_option("--web-root");
    const auto jsn_root_text = args.take_option("--jsn-root");
    const auto cache_root_text = args.take_option(
        "--cache-root", "output/tdx-runtime-cache");
    const bool include_user_formulas =
        args.take_flag("--include-user-formulas");
    const bool allow_remote = args.take_flag("--allow-remote");
    const bool self_test = args.take_flag("--self-test");
    args.require_empty();
    if (bind_address == "localhost") bind_address = "127.0.0.1";
    const bool loopback = bind_address == "127.0.0.1" || bind_address == "::1";
    if (!loopback && !allow_remote)
        throw Error("non-loopback bind requires --allow-remote");

#ifdef _WIN32
    const auto root = find_tdx_root(root_text.empty() ? fs::path{} : from_utf8(root_text));
    const auto web_root = locate_web_root(web_root_text);
    const auto jsn_root = locate_jsn_root(jsn_root_text);
    const auto cache_root = fs::absolute(from_utf8(cache_root_text));
    std::cout << "loading TDX indexes..." << std::flush;
    auto state = build_api_state(
        root, web_root, jsn_root, cache_root, include_user_formulas);
    state.serving_executable = running_executable_path();
    if (!state.serving_executable.empty() &&
        fs::is_regular_file(state.serving_executable))
        state.serving_executable_sha256 = lower_ascii(
            sha256_file(state.serving_executable));
    const auto web_index = state.web_root / "index.html";
    if (fs::is_regular_file(web_index))
        state.web_index_sha256 = lower_ascii(sha256_file(web_index));
    std::cout << " ready\n";
    if (self_test) {
        const auto health = health_document(state);
        const auto features = feature_document();
        const auto blocks = query_blocks(state, parse_target(
            "/api/v1/blocks?q=%E9%93%B6%E8%A1%8C&family=industry&limit=100"));
        const auto security = query_security_blocks(state, parse_target(
            "/api/v1/securities/blocks?market=sz&code=000001"));
        const auto formulas = query_formulas(formula_http_state(state), parse_target(
            "/api/v1/formulas?kind=technical&q=MACD&limit=100"));
        const auto [block_market, block_code] = query_kline_security(parse_target(
            "/api/v1/kline?code=881385&period=day"));
        const auto block_quote = resolve_block_quote_target(
            state.block_data, "880471");
        Json result = Json::object();
        result["ok"] = health.at("ok");
        result["feature_count"] = features.at("count");
        result["bank_blocks"] = blocks.at("block_count");
        result["bank_memberships"] = blocks.at("membership_count");
        result["security_blocks"] = security.at("block_count");
        result["formula_matches"] = formulas.at("match_count");
        result["user_formulas_enabled"] =
            formulas.at("user_library_enabled");
        result["user_formula_count"] = formulas.at("user_formula_count");
        result["serving_executable"] = health.at("serving_executable");
        result["serving_executable_sha256"] =
            health.at("serving_executable_sha256");
        result["web_index_sha256"] = health.at("web_index_sha256");
        result["block_kline_market"] = block_market;
        result["block_kline_code"] = block_code;
        result["block_quote_name"] = block_quote.block.name;
        result["index_bytes"] = static_cast<std::uint64_t>(fs::file_size(state.web_root / "index.html"));
        result["asset_files"] = static_cast<std::uint64_t>(std::count_if(
            fs::recursive_directory_iterator(state.web_root), fs::recursive_directory_iterator{},
            [](const fs::directory_entry& entry) { return entry.is_regular_file(); }));
        result["jsn_available"] = !state.jsn_root.empty();
        std::cout << result.dump(2) << '\n';
        return 0;
    }

    WSADATA winsock{};
    const int startup = WSAStartup(MAKEWORD(2, 2), &winsock);
    if (startup != 0) throw Error("WSAStartup failed: " + std::to_string(startup));
    SOCKET listener = INVALID_SOCKET;
    try {
        listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (listener == INVALID_SOCKET) throw Error("HTTP socket failed: WSA " + std::to_string(WSAGetLastError()));
        BOOL reuse = TRUE;
        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR,
                   reinterpret_cast<const char*>(&reuse), sizeof(reuse));
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = htons(static_cast<u_short>(port));
        if (inet_pton(AF_INET, bind_address.c_str(), &address.sin_addr) != 1)
            throw Error("--bind currently requires an IPv4 literal");
        if (bind(listener, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR)
            throw Error("HTTP bind failed: WSA " + std::to_string(WSAGetLastError()));
        if (listen(listener, SOMAXCONN) == SOCKET_ERROR)
            throw Error("HTTP listen failed: WSA " + std::to_string(WSAGetLastError()));
        std::cout << "tdx-tool dashboard: http://" << bind_address << ':' << port << "/\n"
                  << "read-only API: http://" << bind_address << ':' << port << "/api/v1/openapi.json\n"
                  << "Press Ctrl+C to stop.\n" << std::flush;
        int handled = 0;
        while (max_requests == 0 || handled < max_requests) {
            SOCKET client = accept(listener, nullptr, nullptr);
            if (client == INVALID_SOCKET) throw Error("HTTP accept failed: WSA " + std::to_string(WSAGetLastError()));
            // Chromium-family browsers keep speculative localhost connections open
            // without sending a request.  This server deliberately handles ordinary
            // requests serially, so an unbounded recv here would starve every API
            // behind that idle socket.  Real local GET/POST headers arrive at once;
            // two seconds still leaves ample room for the bounded 1 MiB JSON body.
            const DWORD receive_timeout_ms = 2000;
            setsockopt(client, SOL_SOCKET, SO_RCVTIMEO,
                       reinterpret_cast<const char*>(&receive_timeout_ms),
                       sizeof(receive_timeout_ms));
            bool stream_handed_off = false;
            try {
                const auto request = receive_request(client);
                const auto line_end = request.find("\r\n");
                const auto first_line = request.substr(0, line_end);
                const auto first_space = first_line.find(' ');
                const auto second_space = first_space == std::string::npos
                    ? std::string::npos : first_line.find(' ', first_space + 1);
                if (first_space == std::string::npos || second_space == std::string::npos)
                    throw Error("malformed HTTP request line");
                const auto method = first_line.substr(0, first_space);
                HttpResponse response;
                if (method == "OPTIONS") response = HttpResponse{204, "No Content", "text/plain", ""};
                else if (method != "GET" && method != "POST") {
                    Json error = Json::object();
                    error["error"] = "method_not_allowed";
                    response = json_response(std::move(error), 405, "Method Not Allowed");
                } else {
                    try {
                        const auto target = parse_target(first_line.substr(
                            first_space + 1, second_space - first_space - 1));
                        if (method == "GET" && target.path == "/api/v1/market/stream") {
                            if (!state.market_stream_hub)
                                throw Error("market stream hub is unavailable");
                            const auto [market, code] = query_security(target);
                            const int max_events = parse_bounded(
                                query_value(target, "max_events", "0"),
                                "max_events", 0, 100000);
                            const int wait_timeout_ms = parse_bounded(
                                query_value(target, "wait_timeout_ms", "30000"),
                                "wait_timeout_ms", 1000, 600000);
                            const auto hub = state.market_stream_hub;
                            std::thread([client, hub, market, code, max_events,
                                         wait_timeout_ms] {
                                try {
                                    send_market_stream(client, hub, market, code,
                                                       max_events, wait_timeout_ms);
                                } catch (...) {}
                                closesocket(client);
                            }).detach();
                            stream_handed_off = true;
                        } else {
                            const auto lowered_request = lower_ascii(request);
                            const bool download_header = lowered_request.find(
                                "\r\nx-tdx-action: jsn-download\r\n") != std::string::npos;
                            const bool discovery_header = lowered_request.find(
                                "\r\nx-tdx-action: jsn-discovery-baseline\r\n") !=
                                std::string::npos;
                            const bool formula_evaluate_header = lowered_request.find(
                                "\r\nx-tdx-action: formula-evaluate\r\n") !=
                                std::string::npos;
                            const bool formula_context_import_header = lowered_request.find(
                                "\r\nx-tdx-action: formula-context-import\r\n") !=
                                std::string::npos;
                            const bool formula_scan_header = lowered_request.find(
                                "\r\nx-tdx-action: formula-scan\r\n") !=
                                std::string::npos;
                            const bool formula_backtest_header = lowered_request.find(
                                "\r\nx-tdx-action: formula-backtest\r\n") !=
                                std::string::npos;
                            const bool formula_strategy_scan_header = lowered_request.find(
                                "\r\nx-tdx-action: formula-strategy-scan\r\n") !=
                                std::string::npos;
                            const bool formula_strategy_backtest_header = lowered_request.find(
                                "\r\nx-tdx-action: formula-strategy-backtest\r\n") !=
                                std::string::npos;
                            const bool formula_cloud_calc_header = lowered_request.find(
                                "\r\nx-tdx-action: formula-cloud-calc\r\n") !=
                                std::string::npos;
                            const bool formula_cloud_calc_batch_header = lowered_request.find(
                                "\r\nx-tdx-action: formula-cloud-calc-batch\r\n") !=
                                std::string::npos;
                            const bool pool_evaluate_header = lowered_request.find(
                                "\r\nx-tdx-action: pool-evaluate\r\n") !=
                                std::string::npos;
                            const bool level2_build_header = lowered_request.find(
                                "\r\nx-tdx-action: level2-build\r\n") !=
                                std::string::npos;
                            const bool level2_decode_header = lowered_request.find(
                                "\r\nx-tdx-action: level2-decode\r\n") !=
                                std::string::npos;
                            const bool level2_project_header = lowered_request.find(
                                "\r\nx-tdx-action: level2-project\r\n") !=
                                std::string::npos;
                            const bool disclosure_archive_header = lowered_request.find(
                                "\r\nx-tdx-action: disclosure-archive\r\n") !=
                                std::string::npos;
                            const bool confirmed_download =
                                target.path == "/api/v1/jsn/resource" &&
                                lower_ascii(query_value(target, "action")) == "download" &&
                                download_header;
                            const bool confirmed_discovery =
                                target.path == "/api/v1/jsn/discovery" &&
                                lower_ascii(query_value(target, "action")) == "capture" &&
                                discovery_header;
                            const bool confirmed_formula =
                                (target.path == "/api/v1/formulas/context-import" &&
                                 formula_context_import_header) ||
                                (target.path == "/api/v1/formulas/evaluate" &&
                                 formula_evaluate_header) ||
                                (target.path == "/api/v1/formulas/scan" &&
                                 formula_scan_header) ||
                                (target.path == "/api/v1/formulas/backtest" &&
                                 formula_backtest_header) ||
                                (target.path == "/api/v1/formulas/strategy/scan" &&
                                 formula_strategy_scan_header) ||
                                (target.path == "/api/v1/formulas/strategy/backtest" &&
                                 formula_strategy_backtest_header) ||
                                (target.path == "/api/v1/formulas/cloud-calc" &&
                                 formula_cloud_calc_header) ||
                                (target.path == "/api/v1/formulas/cloud-calc/batch" &&
                                 formula_cloud_calc_batch_header);
                            const bool confirmed_pool_evaluate =
                                target.path == "/api/v1/pools/evaluate" &&
                                pool_evaluate_header;
                            const bool confirmed_level2 =
                                (target.path == "/api/v1/level2/build" &&
                                 level2_build_header) ||
                                (target.path == "/api/v1/level2/decode" &&
                                 level2_decode_header) ||
                                (target.path == "/api/v1/level2/project" &&
                                 level2_project_header);
                            const bool confirmed_disclosure_archive =
                                method == "POST" && target.path ==
                                    "/api/v1/market/disclosures/archive" &&
                                disclosure_archive_header;
                            if (method == "POST" &&
                                !confirmed_download && !confirmed_discovery &&
                                !confirmed_formula && !confirmed_pool_evaluate &&
                                !confirmed_level2 &&
                                !confirmed_disclosure_archive) {
                                Json body = Json::object();
                                body["error"] = "method_not_allowed";
                                body["message"] = "POST is restricted to confirmed formula, TPool, offline Level2, disclosure archive, JSN download, or discovery baseline operations";
                                response = json_response(std::move(body), 405, "Method Not Allowed");
                            } else {
                                Json formula_body;
                                const Json* formula_body_pointer = nullptr;
                                if (confirmed_formula || confirmed_pool_evaluate ||
                                    confirmed_level2 ||
                                    confirmed_disclosure_archive) {
                                    const auto raw_body = http_request_body(request);
                                    if (raw_body.empty())
                                        throw Error("confirmed POST requires a JSON request body");
                                    if (target.path == "/api/v1/level2/project" &&
                                        raw_body.size() > level2_offline_payload_limit)
                                        throw Error("Level2 project request body exceeds the 384 KiB API limit");
                                    formula_body = Json::parse(raw_body);
                                    formula_body_pointer = &formula_body;
                                }
                                response = route(
                                    state, target,
                                    method == "POST" &&
                                        (confirmed_download || confirmed_discovery),
                                    formula_body_pointer);
                            }
                        }
                    } catch (const std::exception& error) {
                        const std::string message = error.what();
                        if (upstream_unavailable_error(message)) {
                            response = upstream_unavailable_response(message);
                        } else {
                            Json body = Json::object();
                            body["error"] = "bad_request";
                            body["message"] = message;
                            response = json_response(std::move(body), 400, "Bad Request");
                        }
                    }
                }
                if (!stream_handed_off) send_response(client, response);
            } catch (const std::exception& error) {
                try { if (!stream_handed_off) {
                    Json body = Json::object();
                    body["error"] = "request_failed";
                    body["message"] = error.what();
                    send_response(client, json_response(std::move(body), 400, "Bad Request"));
                } } catch (...) {}
            }
            if (!stream_handed_off) closesocket(client);
            ++handled;
        }
        closesocket(listener);
        listener = INVALID_SOCKET;
        WSACleanup();
        return 0;
    } catch (...) {
        if (listener != INVALID_SOCKET) closesocket(listener);
        WSACleanup();
        throw;
    }
#else
    (void)root_text; (void)port; (void)max_requests; (void)web_root_text; (void)jsn_root_text;
    (void)allow_remote; (void)self_test;
    throw Error("the embedded HTTP server is currently implemented for Windows");
#endif
}

}  // namespace tdx
