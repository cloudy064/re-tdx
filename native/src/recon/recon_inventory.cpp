#include "tdx/recon.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <chrono>
#include <ctime>

namespace fs = std::filesystem;

namespace tdx {
namespace {

bool interesting_binary(const fs::path& path) {
    const auto extension = lower_ascii(path.extension().string());
    return extension == ".exe" || extension == ".dll";
}

std::string file_time_text(const fs::file_time_type& value) {
    const auto system_time = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        value - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
    const std::time_t stamp = std::chrono::system_clock::to_time_t(system_time);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &stamp);
#else
    localtime_r(&stamp, &local);
#endif
    char buffer[32]{};
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S", &local);
    return buffer;
}

void collect_binaries(const fs::path& directory, bool recursive,
                      std::vector<fs::path>& files) {
    if (!fs::is_directory(directory)) return;
    if (recursive) {
        for (const auto& item : fs::recursive_directory_iterator(
                 directory, fs::directory_options::skip_permission_denied))
            if (item.is_regular_file() && interesting_binary(item.path()))
                files.push_back(item.path());
        return;
    }
    for (const auto& item : fs::directory_iterator(directory))
        if (item.is_regular_file() && interesting_binary(item.path()))
            files.push_back(item.path());
}

}  // namespace

Json inventory_document(const fs::path& root, bool recursive, bool hash) {
    std::vector<fs::path> files;
    collect_binaries(root, recursive, files);
    if (!recursive) {
        static constexpr const char* plugin_directories[] = {
            "TCPlugins", "SEPlugins", "GNPlugins", "QHPlugins", "ZDPlugins"
        };
        for (const auto* directory : plugin_directories)
            collect_binaries(root / directory, true, files);
    }
    std::sort(files.begin(), files.end());
    files.erase(std::unique(files.begin(), files.end()), files.end());

    Json report = Json::object();
    report["schema"] = "tdx-native-install-inventory-v1";
    report["root"] = path_utf8(root);
    Json rows = Json::array();
    for (const auto& file : files) {
        Json row = Json::object();
        row["path"] = path_utf8(fs::relative(file, root));
        row["size"] = static_cast<std::uint64_t>(fs::file_size(file));
        row["modified"] = file_time_text(fs::last_write_time(file));
        if (hash) row["sha256"] = sha256_file(file);
        rows.push_back(std::move(row));
    }
    report["count"] = static_cast<std::uint64_t>(files.size());
    report["artifacts"] = std::move(rows);
    return report;
}

}  // namespace tdx
