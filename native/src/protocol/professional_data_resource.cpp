#include "professional_data_internal.hpp"

#include "tdx/http.hpp"

#include <algorithm>
#include <ctime>
#include <filesystem>
#include <utility>

namespace fs = std::filesystem;

namespace tdx::professional_data_detail {

std::vector<ManifestEntry> parse_manifest(const Bytes& payload) {
    const std::string text(reinterpret_cast<const char*>(payload.data()), payload.size());
    std::vector<ManifestEntry> result;
    for (auto line : split(text, '\n')) {
        line = trim(std::move(line));
        auto parts = split(line, ',');
        for (auto& part : parts) part = trim(std::move(part));
        if (!parts.empty()) parts[0].erase(std::remove_if(parts[0].begin(), parts[0].end(),
            [](unsigned char ch) {
                return !((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
                         (ch >= '0' && ch <= '9') || ch == '.' || ch == '_' || ch == '-');
            }), parts[0].end());
        if (parts.size() != 3 || parts[0].empty() || parts[1].size() != 32) continue;
        try {
            std::size_t used = 0;
            const auto size = static_cast<std::size_t>(std::stoull(parts[2], &used));
            if (used == parts[2].size()) result.push_back({parts[0], lower_ascii(parts[1]), size});
        } catch (...) { /* Ignore malformed manifest rows. */ }
    }
    if (result.empty()) throw Error("professional-data manifest contains no valid entries");
    return result;
}

std::vector<ManifestEntry> fetch_manifest(std::string_view path, int timeout_ms,
                                          std::size_t limit) {
    const auto url = std::string(data_root) + std::string(path) + "?v=" +
                     std::to_string(static_cast<long long>(std::time(nullptr)));
    const auto response = http_get(url,
        {{"User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) tdx-tool/0.2.0"},
         {"Cache-Control", "no-cache"}, {"Pragma", "no-cache"}},
        timeout_ms, limit);
    if (response.status != 200)
        throw Error("professional-data manifest returned HTTP " + std::to_string(response.status));
    return parse_manifest(response.body);
}

const ManifestEntry& find_manifest_entry(const std::vector<ManifestEntry>& entries,
                                         const std::string& name) {
    const auto found = std::find_if(entries.begin(), entries.end(), [&](const auto& item) {
        return item.name == name;
    });
    if (found == entries.end()) throw Error("professional-data manifest has no " + name);
    return *found;
}

Bytes load_verified_resource(const ManifestEntry& entry, std::string_view remote_directory,
                             fs::path cache_directory, int timeout_ms, bool refresh,
                             std::size_t maximum_bytes) {
    if (entry.size > maximum_bytes) throw Error("professional-data resource exceeds safety limit");
    if (cache_directory.empty()) cache_directory = default_cache_directory();
    const auto target = cache_directory / fs::u8path(entry.name);
    bool valid_cache = false;
    if (!refresh && fs::is_regular_file(target)) {
        std::error_code ec;
        valid_cache = fs::file_size(target, ec) == entry.size && !ec &&
                      lower_ascii(md5_file(target)) == entry.md5;
    }
    if (valid_cache) return read_bytes(target);
    std::string mismatch;
    for (int attempt = 0; attempt < 3; ++attempt) {
        auto url = std::string(data_root) + std::string(remote_directory) + entry.name +
                   "?md5=" + entry.md5 + "&attempt=" + std::to_string(attempt + 1);
        const auto response = http_get(url,
            {{"Cache-Control", "no-cache"}, {"Pragma", "no-cache"}},
            timeout_ms, maximum_bytes);
        if (response.status != 200)
            throw Error(entry.name + " returned HTTP " + std::to_string(response.status));
        if (response.body.size() != entry.size) {
            mismatch = "size does not match the official manifest";
            continue;
        }
        if (lower_ascii(md5_bytes(response.body)) != entry.md5) {
            mismatch = "MD5 does not match the official manifest";
            continue;
        }
        atomic_write_bytes(target, response.body);
        return response.body;
    }
    throw Error(entry.name + " " + mismatch + " after 3 bounded attempts");
}

}  // namespace tdx::professional_data_detail
