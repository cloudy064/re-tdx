#include "institution_internal.hpp"

#include "tdx/cloud_resilience.hpp"
#include "tdx/common.hpp"

#include <algorithm>
#include <ctime>
#include <set>
#include <string>
#include <utility>

namespace tdx {

using namespace institution_detail;

Json InstitutionService::query_holder(const HolderQuery& options) {
    validate_token(options.holder_id, "holder_id");
    validate_token(options.variant_id, "variant_id", true);
    if (!six_digits(options.reference_code))
        throw Error("reference_code must contain six digits");
    if (!options.stock_code.empty() && !six_digits(options.stock_code))
        throw Error("stock_code must contain six digits");
    if (options.offset < 0 || options.offset > 1000000)
        throw Error("offset must be in 0..1000000");
    if (options.limit < 1 || options.limit > 500)
        throw Error("limit must be in 1..500");
    if (options.cache_ttl_seconds < 0 || options.cache_ttl_seconds > 86400 ||
        options.detail_cache_ttl_seconds < 0 || options.detail_cache_ttl_seconds > 86400)
        throw Error("holder cache TTL is outside the supported range");
    if (options.timeout_ms < 100 || options.timeout_ms > 60000)
        throw Error("timeout_ms must be in 100..60000");

    const auto history_key = options.holder_id + "|" + options.variant_id + "|" +
                             options.reference_code;
    const auto detail_key = options.stock_code.empty() ? std::string{} :
        options.holder_id + "|" + options.variant_id + "|" + options.stock_code;
    const auto now = std::time(nullptr);
    for (auto item = holder_cache_.begin(); item != holder_cache_.end();) {
        if (item->first != history_key &&
            now - item->second.fetched_at >= options.cache_ttl_seconds)
            item = holder_cache_.erase(item);
        else ++item;
    }
    for (auto item = detail_cache_.begin(); item != detail_cache_.end();) {
        if (item->first != detail_key &&
            now - item->second.fetched_at >= options.detail_cache_ttl_seconds)
            item = detail_cache_.erase(item);
        else ++item;
    }
    bool history_persistent_restored = false;
    bool detail_persistent_restored = false;
    auto history_found = holder_cache_.find(history_key);
    if (history_found == holder_cache_.end()) {
        if (auto restored = read_persistent_cache(
                cache_directory_, "holder-history", history_key)) {
            holder_cache_[history_key] = std::move(*restored);
            history_found = holder_cache_.find(history_key);
            history_persistent_restored = true;
        }
    }
    int history_age = history_found == holder_cache_.end() ? 0
        : static_cast<int>(std::max<std::time_t>(0, now - history_found->second.fetched_at));
    bool history_refreshed = false;
    bool history_stale = false;
    int history_attempts = 0;
    std::string history_upstream_error;
    if (options.refresh || history_found == holder_cache_.end() ||
        history_age >= options.cache_ttl_seconds) {
        try {
            const auto response = query_holder_tqlex(
                tqlex_request("gdjc", options.reference_code,
                              options.holder_id, options.variant_id),
                options.timeout_ms, holder_transport_, history_attempts);
            Json document = Json::object();
            document["rows"] = normalize_holder_history_rows(
                tqlex_result_rows(response));
            holder_cache_[history_key] = {std::move(document), std::time(nullptr)};
            history_found = holder_cache_.find(history_key);
            write_persistent_cache(cache_directory_, "holder-history", history_key,
                                   history_found->second);
            history_age = 0;
            history_refreshed = true;
        } catch (const Error& error) {
            if (history_found == holder_cache_.end() ||
                !detail::is_transient_tqlex_error(error.what()))
                throw;
            history_stale = true;
            history_upstream_error = error.what();
        }
    }
    const auto& all_rows = history_found->second.document.at("rows").as_array();
    const auto begin = std::min<std::size_t>(static_cast<std::size_t>(options.offset), all_rows.size());
    const auto end = std::min<std::size_t>(begin + static_cast<std::size_t>(options.limit),
                                           all_rows.size());
    Json records = Json::array();
    std::set<std::pair<std::string, std::string>> securities;
    std::uint64_t current = 0;
    for (const auto& row : all_rows) {
        securities.emplace(scalar_text(row.at("market")), scalar_text(row.at("code")));
        if (row.at("currently_held").as_bool()) ++current;
    }
    for (std::size_t index = begin; index < end; ++index) records.push_back(all_rows[index]);

    Json detail = Json::array();
    bool detail_refreshed = false;
    bool detail_stale = false;
    int detail_attempts = 0;
    std::string detail_upstream_error;
    int detail_age = 0;
    if (!options.stock_code.empty()) {
        auto detail_found = detail_cache_.find(detail_key);
        if (detail_found == detail_cache_.end()) {
            if (auto restored = read_persistent_cache(
                    cache_directory_, "holder-detail", detail_key)) {
                detail_cache_[detail_key] = std::move(*restored);
                detail_found = detail_cache_.find(detail_key);
                detail_persistent_restored = true;
            }
        }
        detail_age = detail_found == detail_cache_.end() ? 0
            : static_cast<int>(std::max<std::time_t>(0, now - detail_found->second.fetched_at));
        if (options.refresh || detail_found == detail_cache_.end() ||
            detail_age >= options.detail_cache_ttl_seconds) {
            try {
                const auto response = query_holder_tqlex(
                    tqlex_request("gdjcxq", options.stock_code,
                                  options.holder_id, options.variant_id),
                    options.timeout_ms, holder_transport_, detail_attempts);
                Json document = Json::object();
                document["rows"] = normalize_holder_stock_rows(
                    tqlex_result_rows(response));
                detail_cache_[detail_key] = {
                    std::move(document), std::time(nullptr)};
                detail_found = detail_cache_.find(detail_key);
                write_persistent_cache(cache_directory_, "holder-detail", detail_key,
                                       detail_found->second);
                detail_age = 0;
                detail_refreshed = true;
            } catch (const Error& error) {
                if (detail_found == detail_cache_.end() ||
                    !detail::is_transient_tqlex_error(error.what()))
                    throw;
                detail_stale = true;
                detail_upstream_error = error.what();
            }
        }
        detail = detail_found->second.document.at("rows");
    }

    Json holder = Json::object();
    holder["holder_id"] = options.holder_id;
    holder["variant_id"] = options.variant_id;
    holder["name"] = options.holder_name;
    holder["reference_code"] = options.reference_code;
    Json pagination = Json::object();
    pagination["offset"] = options.offset;
    pagination["limit"] = options.limit;
    pagination["returned"] = static_cast<std::uint64_t>(records.size());
    pagination["total"] = static_cast<std::uint64_t>(all_rows.size());
    pagination["has_previous"] = begin > 0;
    pagination["has_next"] = end < all_rows.size();
    Json counts = Json::object();
    counts["records"] = static_cast<std::uint64_t>(all_rows.size());
    counts["unique_stocks"] = static_cast<std::uint64_t>(securities.size());
    counts["currently_held_records"] = current;
    counts["stock_periods"] = static_cast<std::uint64_t>(detail.size());
    Json stock_detail = Json::object();
    stock_detail["code"] = options.stock_code;
    stock_detail["records"] = std::move(detail);
    Json cache = Json::object();
    cache["history_ttl_seconds"] = options.cache_ttl_seconds;
    cache["history_refreshed"] = history_refreshed;
    cache["history_age_seconds"] = history_age;
    cache["history_stale"] = history_stale;
    cache["history_attempts"] = history_attempts;
    cache["history_upstream_error"] = history_upstream_error.empty()
        ? Json(nullptr) : Json(history_upstream_error);
    cache["detail_ttl_seconds"] = options.detail_cache_ttl_seconds;
    cache["detail_refreshed"] = detail_refreshed;
    cache["detail_age_seconds"] = detail_age;
    cache["detail_stale"] = detail_stale;
    cache["detail_attempts"] = detail_attempts;
    cache["detail_upstream_error"] = detail_upstream_error.empty()
        ? Json(nullptr) : Json(detail_upstream_error);
    cache["max_attempts"] = holder_max_attempts;
    cache["persistent_enabled"] = !cache_directory_.empty();
    cache["history_persistent_restored"] = history_persistent_restored;
    cache["detail_persistent_restored"] = detail_persistent_restored;
    Json result = Json::object();
    result["schema"] = "tdx-holder-history-native-v1";
    result["generated_at"] = now_text();
    result["availability"] = history_stale || detail_stale
        ? "stale-cache" : "live";
    result["holder"] = std::move(holder);
    result["counts"] = std::move(counts);
    result["pagination"] = std::move(pagination);
    result["records"] = std::move(records);
    result["stock_detail"] = std::move(stock_detail);
    result["cache"] = std::move(cache);
    return result;
}

}  // namespace tdx
