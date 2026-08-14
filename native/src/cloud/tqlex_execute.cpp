#include "tqlex_internal.hpp"

#include <filesystem>

namespace fs = std::filesystem;

namespace tdx {

Json execute_tqlex_config(
    const fs::path& root, const std::string& request_id,
    const std::map<std::string, std::string>& replacements,
    const std::map<std::string, std::string>& overrides,
    const std::string& entry, const std::string& source_file,
    const std::vector<std::string>& body_contains, bool all_pages,
    int page, int page_size, int max_pages,
    const std::string& base_url, int timeout_ms) {
    const int macro_page = page < 0 ? 0 : page;
    const int macro_page_size = page_size < 1 ? 20 : page_size;
    auto spec = find_tqlex_config_spec(root, request_id, entry, source_file,
                                       body_contains, replacements, macro_page, macro_page_size);
    for (const auto& [name, value] : overrides)
        set_tqlex_request_value(spec.request, name, value);
    set_tqlex_request_value(spec.request, "ReqId", request_id);
    if (page >= 0) set_tqlex_request_value(spec.request, "Page", std::to_string(page));
    if (page_size > 0)
        set_tqlex_request_value(spec.request, "PageSize", std::to_string(page_size));
    const int effective_page = page >= 0 ? page : detail::tqlex_request_integer(spec.request, "Page", 0);
    const int effective_page_size = page_size > 0 ? page_size
        : detail::tqlex_request_integer(spec.request, "PageSize", macro_page_size);
    auto response = all_pages
        ? query_tqlex_all_pages(spec.entry, spec.request, effective_page,
                                effective_page_size, max_pages,
                                base_url, timeout_ms)
        : query_tqlex(spec.entry, spec.request, base_url, timeout_ms);
    Json document = Json::object();
    document["schema"] = "tdx-tqlex-native-v1";
    document["request_id"] = request_id;
    document["entry"] = spec.entry;
    document["source_file"] = spec.source_file;
    document["all_pages"] = all_pages;
    document["request"] = spec.request;
    document["response"] = std::move(response);
    return document;
}

}  // namespace tdx
