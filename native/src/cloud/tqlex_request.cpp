#include "tqlex_internal.hpp"

#include <algorithm>
#include <regex>
#include <tuple>

namespace fs = std::filesystem;

namespace tdx {

Json parse_tqlex_config_body(std::string body,
                             const std::map<std::string, std::string>& replacements,
                             int page, int page_size) {
    if (page < 0 || page_size < 1) throw Error("TQLEX paging values are invalid");
    body = std::regex_replace(body,
        std::regex(R"(\$\$\$STARTPOS\$\s*\$\$)", std::regex_constants::icase),
        std::to_string(page));
    body = std::regex_replace(body,
        std::regex(R"(\$\$\$PAGEROWS\$\s*\$\$)", std::regex_constants::icase),
        std::to_string(page_size));
    for (const auto& [name, value] : replacements)
        detail::replace_all(body, "$$" + name + "$$", value);
    const auto unresolved = detail::tqlex_placeholders(body);
    if (!unresolved.empty()) {
        std::string error_detail = "unresolved TQLEX placeholders:";
        for (const auto& value : unresolved) error_detail += " " + value;
        throw Error(error_detail);
    }
    Json result;
    try {
        result = Json::parse(body);
    } catch (const std::exception&) {
        result = Json::parse(detail::single_quotes_to_json(body));
    }
    if (!result.is_array() && !result.is_object())
        throw Error("TQLEX request body must be an array or object");
    if (result.is_array() && result.as_array().empty())
        throw Error("TQLEX request array must not be empty");
    return result;
}

TqlexRequestSpec find_tqlex_config_spec(
    const fs::path& root, const std::string& request_id,
    const std::string& entry, const std::string& source_file,
    const std::vector<std::string>& body_contains,
    const std::map<std::string, std::string>& replacements,
    int page, int page_size) {
    const auto lowered_entry = lower_ascii(entry);
    const auto lowered_source = lower_ascii(source_file);
    std::vector<TqlexConfig> candidates;
    for (const auto& config : inventory_tqlex_configs(root)) {
        if (config.request_id != request_id) continue;
        if (!entry.empty() && lower_ascii(config.entry) != lowered_entry) continue;
        if (!source_file.empty() && lower_ascii(config.source_file) != lowered_source) continue;
        const auto lowered_body = lower_ascii(config.body);
        bool matches = true;
        for (const auto& needle : body_contains)
            if (lowered_body.find(lower_ascii(needle)) == std::string::npos) {
                matches = false;
                break;
            }
        if (matches) candidates.push_back(config);
    }
    if (candidates.empty()) throw Error("no reqformat=2 config found for ReqId " + request_id);
    std::stable_sort(candidates.begin(), candidates.end(), [](const auto& left, const auto& right) {
        return std::make_tuple(left.placeholders.size(), left.source_file) <
               std::make_tuple(right.placeholders.size(), right.source_file);
    });
    const auto& selected = candidates.front();
    return TqlexRequestSpec{selected.entry,
        parse_tqlex_config_body(selected.body, replacements, page, page_size),
        selected.source_file};
}

void set_tqlex_request_value(Json& request, const std::string& name, Json value) {
    auto& mapping = detail::tqlex_request_mapping(request);
    const auto folded = lower_ascii(name);
    auto found = std::find_if(mapping.begin(), mapping.end(), [&](const auto& item) {
        return lower_ascii(item.first) == folded;
    });
    if (found == mapping.end()) mapping[name] = std::move(value);
    else found->second = std::move(value);
}

}  // namespace tdx
