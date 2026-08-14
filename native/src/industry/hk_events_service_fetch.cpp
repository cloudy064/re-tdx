#include "hk_events_internal.hpp"

#include "tdx/jsn.hpp"

#include <algorithm>
#include <ctime>
#include <filesystem>
#include <map>

namespace fs = std::filesystem;

namespace tdx {

HkEventService::HkEventService(fs::path jsn_root)
    : jsn_root_(std::move(jsn_root)) {}

Json HkEventService::fetch_master(const HkEventQuery& options,
                                  bool& refreshed, int& age_seconds) {
    const auto now = std::time(nullptr);
    age_seconds = cache_time_
        ? static_cast<int>(std::max<std::time_t>(0, now - cache_time_)) : 0;
    if (!options.refresh && cache_time_ && age_seconds < options.cache_ttl_seconds) {
        refreshed = false;
        return cache_;
    }
    Json documents = Json::array();
    const auto& resources = detail::hk_event_resource_paths();
    if (!options.refresh && !jsn_root_.empty()) {
        try {
            for (const auto& resource : resources)
                documents.push_back(detail::load_local_resource_rows(jsn_root_, resource));
        } catch (...) {
            documents = Json::array();
        }
    }
    if (documents.as_array().empty())
        documents = fetch_jsn_resources_rows(resources, "bi", options.timeout_ms);

    Json rows = Json::array();
    Json sources = Json::array();
    std::map<std::pair<int, std::string>, std::string> names;
    for (const auto& resource : resources) {
        const auto& document = detail::hk_document_for(documents, resource);
        sources.push_back(detail::hk_source_summary(document));
        for (const auto& raw : document.at("rows").as_array()) {
            const auto id = detail::hk_market_id(raw);
            const auto code = detail::json_text(raw, "$ZQDM");
            const auto name = detail::json_text(raw, "ZQJC");
            if (id >= 0 && detail::valid_hk_code(code) && !name.empty())
                names[{id, code}] = name;
        }
    }
    for (const auto& resource : resources) {
        auto normalized = normalize_hk_event_rows(
            resource, detail::hk_document_for(documents, resource).at("rows"));
        for (auto& row : normalized.as_array()) {
            auto& security = row["security"];
            if (!security.is_null() && security.at("name").as_string().empty()) {
                const auto id = static_cast<int>(security.at("market_id").as_number());
                const auto found = names.find({id, security.at("code").as_string()});
                if (found != names.end()) {
                    security["name"] = found->second;
                    security["name_resolved"] = true;
                }
            }
            rows.push_back(std::move(row));
        }
    }
    Json master = Json::object();
    master["rows"] = std::move(rows);
    master["sources"] = std::move(sources);
    cache_ = master;
    cache_time_ = std::time(nullptr);
    refreshed = true;
    age_seconds = 0;
    return master;
}

}  // namespace tdx
