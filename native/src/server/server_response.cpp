#include "server_core_internal.hpp"

#include "tdx/common.hpp"

#include <filesystem>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace tdx::server_detail {
HttpResponse json_response(Json value, int status, std::string reason) {
    return HttpResponse{status, std::move(reason), "application/json; charset=utf-8",
                        value.dump(2) + "\n"};
}

bool upstream_unavailable_error(const std::string& message) {
    for (const auto* token : {
             "PBRPC HTTP status", "PBRPC business ErrorCode", "RpcID -1",
             "TQLEX HTTP status 429", "TQLEX HTTP status 502",
             "TQLEX HTTP status 503", "TQLEX HTTP status 504",
             "TQLEX server returned ErrorCode 4",
             "WinHttpSendRequest failed", "WinHttpReceiveResponse failed",
             "cannot resolve", "cannot connect to", "send failed: WSA",
             "receive failed: WSA", "server closed connection",
             "invalid 7709 response", "7709 response message ID mismatch",
             "7709 response command mismatch", "JSN metadata response is shorter",
             "JSN server returned an empty chunk", "downloaded JSN length mismatch"})
        if (message.find(token) != std::string::npos) return true;
    return false;
}

HttpResponse upstream_unavailable_response(const std::string& message) {
    Json body = Json::object();
    body["error"] = "upstream_unavailable";
    body["message"] = message;
    body["retryable"] = true;
    return json_response(std::move(body), 503, "Service Unavailable");
}

std::string content_type_for(const fs::path& path) {
    const auto extension = lower_ascii(path.extension().string());
    if (extension == ".html") return "text/html; charset=utf-8";
    if (extension == ".js" || extension == ".mjs") return "text/javascript; charset=utf-8";
    if (extension == ".css") return "text/css; charset=utf-8";
    if (extension == ".json" || extension == ".map") return "application/json; charset=utf-8";
    if (extension == ".svg") return "image/svg+xml";
    if (extension == ".png") return "image/png";
    if (extension == ".bmp") return "image/bmp";
    if (extension == ".jpg" || extension == ".jpeg") return "image/jpeg";
    if (extension == ".webp") return "image/webp";
    if (extension == ".ico") return "image/x-icon";
    if (extension == ".woff") return "font/woff";
    if (extension == ".woff2") return "font/woff2";
    return "application/octet-stream";
}

HttpResponse formula_signal_image_response(const ApiState& state,
                                           const RequestTarget& target) {
    const auto name = query_value(target, "name");
    if (name.empty() || name.size() > 250 || name.find('\0') != std::string::npos ||
        name.find('/') != std::string::npos || name.find('\\') != std::string::npos ||
        name.find(':') != std::string::npos)
        throw Error("signal image name must be one local T0002/signals basename");
    const auto leaf = from_utf8(name);
    if (leaf.filename() != leaf || leaf == fs::path(".") || leaf == fs::path(".."))
        throw Error("signal image name must not contain a path");

    const auto format = lower_ascii(trim(query_value(target, "format", "auto")));
    if (format != "auto" && format != "bmp" && format != "png")
        throw Error("signal image format must be auto, bmp, or png");
    const auto directory = state.root / "T0002" / "signals";
    std::vector<std::string> extensions;
    if (format == "auto" || format == "bmp") extensions.push_back(".bmp");
    if (format == "auto" || format == "png") extensions.push_back(".png");
    for (const auto& extension : extensions) {
        const auto candidate = directory / fs::path(leaf.native() + from_utf8(extension).native());
        if (!fs::is_regular_file(candidate)) continue;
        constexpr std::uintmax_t maximum_signal_image_bytes = 64ULL * 1024ULL * 1024ULL;
        if (fs::file_size(candidate) > maximum_signal_image_bytes)
            throw Error("signal image exceeds the 64 MiB browser resource limit");
        const auto bytes = read_bytes(candidate);
        return HttpResponse{200, "OK", content_type_for(candidate),
            std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size())};
    }
    return HttpResponse{404, "Not Found", "text/plain; charset=utf-8",
                        "signal image not found\n"};
}

bool path_starts_with(const fs::path& path, const fs::path& root) {
    auto path_part = path.begin();
    auto root_part = root.begin();
    for (; root_part != root.end(); ++root_part, ++path_part)
        if (path_part == path.end() || *path_part != *root_part) return false;
    return true;
}

HttpResponse static_response(const ApiState& state, const RequestTarget& target) {
    std::string relative = target.path;
    while (!relative.empty() && relative.front() == '/') relative.erase(relative.begin());
    if (relative.empty()) relative = "index.html";
    if (relative.find('\\') != std::string::npos || relative.find('\0') != std::string::npos)
        throw Error("invalid static resource path");

    const auto root = fs::weakly_canonical(state.web_root);
    auto candidate = fs::weakly_canonical(root / from_utf8(relative));
    if (!path_starts_with(candidate, root)) throw Error("static resource path escapes web root");
    if (!fs::is_regular_file(candidate)) {
        if (from_utf8(relative).has_extension())
            return HttpResponse{404, "Not Found", "text/plain; charset=utf-8",
                                "static resource not found\n"};
        // Let client-side routers own extensionless non-file routes.
        candidate = root / "index.html";
    }
    const auto bytes = read_bytes(candidate);
    return HttpResponse{200, "OK", content_type_for(candidate),
                        std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size())};
}

}  // namespace tdx::server_detail