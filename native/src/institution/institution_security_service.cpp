#include "institution_internal.hpp"

#include "tdx/common.hpp"
#include "tdx/jsn.hpp"

#include <algorithm>
#include <ctime>
#include <exception>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace tdx {

using namespace institution_detail;

InstitutionService::InstitutionService(
    std::map<std::pair<int, std::string>, Security> securities,
    TqlexTransport holder_transport,
    fs::path cache_directory)
    : securities_(std::move(securities)),
      holder_transport_(std::move(holder_transport)),
      cache_directory_(std::move(cache_directory)) {}

Json InstitutionService::query_security(const InstitutionQuery& options) {
    const int id = market_id(options.market);
    if (!six_digits(options.code)) throw Error("code must contain six digits");
    if (options.cache_ttl_seconds < 0 || options.cache_ttl_seconds > 86400)
        throw Error("cache_ttl_seconds must be in 0..86400");
    if (options.timeout_ms < 100 || options.timeout_ms > 60000)
        throw Error("timeout_ms must be in 100..60000");
    const auto key = std::to_string(id) + options.code;
    const auto now = std::time(nullptr);
    for (auto item = security_cache_.begin(); item != security_cache_.end();) {
        if (now - item->second.fetched_at >= options.cache_ttl_seconds)
            item = security_cache_.erase(item);
        else ++item;
    }
    const auto cached = security_cache_.find(key);
    const int age = cached == security_cache_.end() ? 0
        : static_cast<int>(std::max<std::time_t>(0, now - cached->second.fetched_at));
    if (!options.refresh && cached != security_cache_.end() &&
        age < options.cache_ttl_seconds) {
        auto result = cached->second.document;
        result["cache"]["refreshed"] = false;
        result["cache"]["age_seconds"] = age;
        return result;
    }

    const std::vector<std::string> resources{
        "cgfxmx1/" + key + ".jsn", "cgfxmx2/" + key + ".jsn"};
    Json documents = Json::array();
    Json errors = Json::array();
    try {
        documents = fetch_jsn_resources_rows(resources, "bi", options.timeout_ms);
    } catch (const std::exception& batch_error) {
        for (const auto& resource : resources) {
            try { documents.push_back(fetch_jsn_resource_rows(resource, "bi", options.timeout_ms)); }
            catch (const std::exception& error) {
                Json failure = Json::object();
                failure["resource"] = resource;
                failure["message"] = error.what();
                errors.push_back(std::move(failure));
            }
        }
        if (documents.size() == 0) throw Error(std::string("institution resources unavailable: ") +
                                               batch_error.what());
    }

    Json institution_rows = Json::array(), holder_rows = Json::array(), sources = Json::array();
    for (const auto& document : documents.as_array()) {
        const auto resource = text_value(document, "resource");
        if (resource.find("cgfxmx1/") == 0) institution_rows = document.at("rows");
        else if (resource.find("cgfxmx2/") == 0) holder_rows = document.at("rows");
        sources.push_back(source_summary(document));
    }
    Json counts = Json::object();
    counts["institution_history"] = static_cast<std::uint64_t>(institution_rows.size());
    counts["top_float_holders"] = static_cast<std::uint64_t>(holder_rows.size());
    std::uint64_t queryable = 0;
    auto holders = normalize_holders(holder_rows, options.code);
    for (const auto& holder : holders.as_array())
        if (holder.at("queryable").as_bool()) ++queryable;
    counts["queryable_holders"] = queryable;
    Json cache = Json::object();
    cache["ttl_seconds"] = options.cache_ttl_seconds;
    cache["refreshed"] = true;
    cache["age_seconds"] = 0;
    Json result = Json::object();
    result["schema"] = "tdx-security-profile-native-v2";
    result["generated_at"] = now_text();
    result["market"] = market_name(id);
    result["code"] = options.code;
    result["security"] = security_document(std::to_string(id), options.code, securities_);
    result["counts"] = std::move(counts);
    result["institution_history"] = institution_rows;
    result["top_float_holders"] = holder_rows;
    result["institution_records"] = normalize_institution_history(institution_rows);
    result["holders"] = std::move(holders);
    result["sources"] = std::move(sources);
    result["errors"] = std::move(errors);
    result["cache"] = std::move(cache);
    security_cache_[key] = {result, std::time(nullptr)};
    return result;
}

}  // namespace tdx
