#include "tdx/http.hpp"

#include <algorithm>
#include <iomanip>
#include <limits>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
#endif

namespace tdx {
namespace {

#ifdef _WIN32
class InternetHandle {
public:
    explicit InternetHandle(HINTERNET value = nullptr) : value_(value) {}
    ~InternetHandle() { if (value_) WinHttpCloseHandle(value_); }
    InternetHandle(const InternetHandle&) = delete;
    InternetHandle& operator=(const InternetHandle&) = delete;
    HINTERNET get() const noexcept { return value_; }
private:
    HINTERNET value_{};
};

std::string winhttp_error(const char* operation) {
    return std::string(operation) + " failed: WinHTTP " +
           std::to_string(GetLastError());
}
#endif

}  // namespace

std::string url_encode(std::string_view value) {
    std::ostringstream output;
    output << std::uppercase << std::hex << std::setfill('0');
    for (const auto raw : value) {
        const auto ch = static_cast<unsigned char>(raw);
        if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
            (ch >= '0' && ch <= '9') || ch == '-' || ch == '_' || ch == '.' || ch == '~')
            output << static_cast<char>(ch);
        else output << '%' << std::setw(2) << static_cast<unsigned>(ch);
    }
    return output.str();
}

namespace {

HttpResult http_request(
    const std::string& method, const std::string& url, const Bytes& body,
    const std::vector<std::pair<std::string, std::string>>& headers,
    int timeout_ms, std::size_t maximum_response_bytes) {
#ifdef _WIN32
    if (timeout_ms < 1 || timeout_ms > 600000) throw Error("HTTP timeout is invalid");
    if (!maximum_response_bytes) throw Error("HTTP response limit must be positive");
    const auto wide_url = utf8_to_wide(url);
    URL_COMPONENTS components{};
    components.dwStructSize = sizeof(components);
    components.dwSchemeLength = static_cast<DWORD>(-1);
    components.dwHostNameLength = static_cast<DWORD>(-1);
    components.dwUrlPathLength = static_cast<DWORD>(-1);
    components.dwExtraInfoLength = static_cast<DWORD>(-1);
    if (!WinHttpCrackUrl(wide_url.c_str(), static_cast<DWORD>(wide_url.size()),
                         0, &components))
        throw Error(winhttp_error("WinHttpCrackUrl"));
    if (components.nScheme != INTERNET_SCHEME_HTTP &&
        components.nScheme != INTERNET_SCHEME_HTTPS)
        throw Error("HTTP client only supports http and https URLs");
    const std::wstring host(components.lpszHostName, components.dwHostNameLength);
    std::wstring path(components.lpszUrlPath, components.dwUrlPathLength);
    if (components.dwExtraInfoLength)
        path.append(components.lpszExtraInfo, components.dwExtraInfoLength);
    if (path.empty()) path = L"/";

    InternetHandle session(WinHttpOpen(
        L"tdx-tool/0.2.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
    if (!session.get()) throw Error(winhttp_error("WinHttpOpen"));
    if (!WinHttpSetTimeouts(session.get(), timeout_ms, timeout_ms, timeout_ms, timeout_ms))
        throw Error(winhttp_error("WinHttpSetTimeouts"));
    InternetHandle connection(WinHttpConnect(session.get(), host.c_str(),
                                              components.nPort, 0));
    if (!connection.get()) throw Error(winhttp_error("WinHttpConnect"));
    const DWORD flags = components.nScheme == INTERNET_SCHEME_HTTPS
        ? WINHTTP_FLAG_SECURE : 0;
    const auto wide_method = utf8_to_wide(method);
    InternetHandle request(WinHttpOpenRequest(
        connection.get(), wide_method.c_str(), path.c_str(), nullptr, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES, flags));
    if (!request.get()) throw Error(winhttp_error("WinHttpOpenRequest"));

    std::wstring wide_headers;
    for (const auto& [name, value] : headers) {
        wide_headers += utf8_to_wide(name + ": " + value + "\r\n");
    }
    if (body.size() > std::numeric_limits<DWORD>::max())
        throw Error("HTTP request body exceeds WinHTTP DWORD limit");
    const auto body_size = static_cast<DWORD>(body.size());
    void* body_pointer = body.empty() ? WINHTTP_NO_REQUEST_DATA
        : const_cast<std::uint8_t*>(body.data());
    if (!WinHttpSendRequest(request.get(),
                            wide_headers.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS
                                                 : wide_headers.c_str(),
                            wide_headers.empty() ? 0 : static_cast<DWORD>(-1),
                            body_pointer, body_size, body_size, 0))
        throw Error(winhttp_error("WinHttpSendRequest"));
    if (!WinHttpReceiveResponse(request.get(), nullptr))
        throw Error(winhttp_error("WinHttpReceiveResponse"));

    DWORD status = 0, status_size = sizeof(status);
    if (!WinHttpQueryHeaders(request.get(),
                             WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                             WINHTTP_HEADER_NAME_BY_INDEX, &status, &status_size,
                             WINHTTP_NO_HEADER_INDEX))
        throw Error(winhttp_error("WinHttpQueryHeaders(status)"));
    std::string content_type;
    DWORD type_size = 0;
    WinHttpQueryHeaders(request.get(), WINHTTP_QUERY_CONTENT_TYPE,
                        WINHTTP_HEADER_NAME_BY_INDEX, WINHTTP_NO_OUTPUT_BUFFER,
                        &type_size, WINHTTP_NO_HEADER_INDEX);
    if (GetLastError() == ERROR_INSUFFICIENT_BUFFER && type_size) {
        std::wstring wide_type(type_size / sizeof(wchar_t), L'\0');
        if (WinHttpQueryHeaders(request.get(), WINHTTP_QUERY_CONTENT_TYPE,
                                WINHTTP_HEADER_NAME_BY_INDEX, wide_type.data(),
                                &type_size, WINHTTP_NO_HEADER_INDEX)) {
            while (!wide_type.empty() && wide_type.back() == L'\0') wide_type.pop_back();
            content_type = wide_to_utf8(wide_type);
        }
    }

    Bytes response;
    while (true) {
        DWORD available = 0;
        if (!WinHttpQueryDataAvailable(request.get(), &available))
            throw Error(winhttp_error("WinHttpQueryDataAvailable"));
        if (!available) break;
        if (available > maximum_response_bytes - std::min(response.size(), maximum_response_bytes))
            throw Error("HTTP response exceeds configured safety limit");
        const auto old_size = response.size();
        response.resize(old_size + available);
        DWORD received = 0;
        if (!WinHttpReadData(request.get(), response.data() + old_size, available, &received))
            throw Error(winhttp_error("WinHttpReadData"));
        response.resize(old_size + received);
        if (!received) break;
    }
    return HttpResult{static_cast<int>(status), std::move(content_type), std::move(response)};
#else
    (void)method; (void)url; (void)body; (void)headers;
    (void)timeout_ms; (void)maximum_response_bytes;
    throw Error("native HTTP transport is currently implemented for Windows only");
#endif
}

}  // namespace

HttpResult http_get(
    const std::string& url,
    const std::vector<std::pair<std::string, std::string>>& headers,
    int timeout_ms, std::size_t maximum_response_bytes) {
    return http_request("GET", url, {}, headers, timeout_ms, maximum_response_bytes);
}

HttpResult http_post(
    const std::string& url, const Bytes& body,
    const std::vector<std::pair<std::string, std::string>>& headers,
    int timeout_ms, std::size_t maximum_response_bytes) {
    return http_request("POST", url, body, headers, timeout_ms, maximum_response_bytes);
}

}  // namespace tdx
