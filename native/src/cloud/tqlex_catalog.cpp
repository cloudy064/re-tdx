#include "tqlex_internal.hpp"

#include <algorithm>
#include <filesystem>
#include <set>
#include <string_view>

namespace fs = std::filesystem;

namespace tdx {

std::vector<TqlexConfig> inventory_cloud_configs(const fs::path& root,
                                                 const std::string& request_format) {
    if (request_format.empty()) throw Error("cloud request format is required");
    const auto directory = root / "T0002" / "cloud_cfg";
    if (!fs::is_directory(directory))
        throw Error("TDX cloud_cfg directory is unavailable: " + path_utf8(directory));
    std::vector<fs::path> files;
    for (const auto& entry : fs::directory_iterator(directory))
        if (entry.is_regular_file() && lower_ascii(entry.path().extension().string()) == ".xml")
            files.push_back(entry.path());
    std::sort(files.begin(), files.end(), [](const fs::path& left, const fs::path& right) {
        return lower_ascii(path_utf8(left.filename())) < lower_ascii(path_utf8(right.filename()));
    });
    std::vector<TqlexConfig> result;
    for (const auto& path : files) {
        const auto text = detail::remove_xml_comments(detail::read_tqlex_config_text(path));
        const auto lowered = lower_ascii(text);
        std::size_t offset = 0;
        while ((offset = lowered.find("<datasource", offset)) != std::string::npos) {
            std::size_t end = offset + 11;
            char quote = 0;
            for (; end < text.size(); ++end) {
                const char ch = text[end];
                if (quote) { if (ch == quote) quote = 0; }
                else if (ch == '\'' || ch == '"') quote = ch;
                else if (ch == '>') break;
            }
            if (end >= text.size()) throw Error("unterminated datasource tag: " + path_utf8(path));
            const auto attrs = detail::parse_xml_attributes(std::string_view(text).substr(
                offset + 11, end - offset - 11));
            const auto format = attrs.find("reqformat");
            const auto entry = attrs.find("name");
            const auto body = attrs.find("body");
            if (format != attrs.end() && format->second == request_format &&
                entry != attrs.end() && body != attrs.end()) {
                result.push_back(TqlexConfig{
                    path_utf8(path.filename()), trim(entry->second), format->second,
                    detail::tqlex_request_id(body->second), detail::tqlex_placeholders(body->second),
                    trim(body->second)});
            }
            offset = end + 1;
        }
    }
    return result;
}

std::vector<TqlexConfig> inventory_tqlex_configs(const fs::path& root) {
    return inventory_cloud_configs(root, "2");
}

Json tqlex_configs_document(const fs::path& root) {
    const auto configs = inventory_tqlex_configs(root);
    std::set<std::string> entries, request_ids;
    Json records = Json::array();
    for (const auto& config : configs) {
        entries.insert(config.entry);
        if (!config.request_id.empty()) request_ids.insert(config.request_id);
        Json value = Json::object();
        value["source_file"] = config.source_file;
        value["entry"] = config.entry;
        value["request_format"] = config.request_format;
        value["request_id"] = config.request_id;
        Json placeholders = Json::array();
        for (const auto& placeholder : config.placeholders) placeholders.push_back(placeholder);
        value["placeholders"] = std::move(placeholders);
        value["body"] = config.body;
        records.push_back(std::move(value));
    }
    Json document = Json::object();
    document["schema"] = "tdx-tqlex-configs-native-v1";
    document["config_count"] = static_cast<std::uint64_t>(configs.size());
    document["entry_count"] = static_cast<std::uint64_t>(entries.size());
    document["request_id_count"] = static_cast<std::uint64_t>(request_ids.size());
    document["records"] = std::move(records);
    return document;
}

}  // namespace tdx
