// Shared front half of the factor query pipeline: argument validation, the
// small response fragments every view family repeats, and the TQLEX retry
// policy. The four view-family units depend on this unit, never on each other.
#include "factors_internal.hpp"

#include "tdx/common.hpp"
#include "tdx/tqlex.hpp"

#include <algorithm>
#include <chrono>
#include <thread>

namespace fs = std::filesystem;

namespace tdx::factor_detail {

const ViewDefinition& validate_factor_query(FactorQuery& options) {
    options.view = lower_ascii(trim(options.view));
    options.factor_id = trim(options.factor_id);
    options.query = trim(options.query);
    options.market = lower_ascii(trim(options.market));
    options.code = trim(options.code);
    const auto* definition = view_definition(options.view);
    if (!definition)
        throw Error("view must be catalog, members, dashboard, patterns, pattern-members, intraday-radar, security, pattern-matrix, standard-matrix, or overview");
    const bool member_view = options.view == "members" || options.view == "pattern-members";
    const bool security_view = options.view == "security";
    const bool relation_matrix_view =
        options.view == "pattern-matrix" || options.view == "standard-matrix";
    const bool overview_view = options.view == "overview";
    if (member_view && options.factor_id.empty())
        throw Error("factor_id is required for members and pattern-members");
    if (!member_view && !options.factor_id.empty())
        throw Error("factor_id is only valid for members and pattern-members");
    if (security_view) {
        if (options.market.empty() || options.code.empty())
            throw Error("market and code are required for the security view");
        options.market = market_name(market_id(options.market));
        if (options.code.size() != 6 ||
            !std::all_of(options.code.begin(), options.code.end(), [](unsigned char ch) {
                return ch >= '0' && ch <= '9';
            })) throw Error("code must contain six digits");
        if (!options.query.empty()) throw Error("query is not valid for the security view");
    } else if (!options.market.empty() || !options.code.empty()) {
        throw Error("market and code are only valid for the security view");
    }
    if (options.include_patterns && !security_view)
        throw Error("include_patterns is only valid for the security view");
    if (options.enrich_quotes && (relation_matrix_view || overview_view))
        throw Error("quotes are not valid for relation matrices or overview");
    if (options.limit < 1 || options.limit > 30000 || options.max_pages < 1 ||
        options.max_pages > 1000 || options.cache_ttl_seconds < 0 ||
        options.cache_ttl_seconds > 86400 || options.timeout_ms < 100 ||
        options.timeout_ms > 60000)
        throw Error("factor query limits are invalid");
    return *definition;
}

Json fresh_cache_document(int ttl_seconds) {
    Json cache = Json::object();
    cache["hit"] = false;
    cache["stale"] = false;
    cache["age_seconds"] = 0;
    cache["ttl_seconds"] = ttl_seconds;
    return cache;
}

void append_sources(Json& sources, const Json& document) {
    for (const auto& source : document.at("sources").as_array())
        sources.push_back(source);
}

bool apply_limit(Json& records, int limit) {
    const bool truncated = records.as_array().size() > static_cast<std::size_t>(limit);
    if (truncated) records.as_array().resize(static_cast<std::size_t>(limit));
    return truncated;
}

Json filter_by_factor_field(const Json& records, const std::string& query) {
    Json filtered = Json::array();
    for (const auto& record : records.as_array()) {
        const auto* factor = field(record, "factor");
        if (query.empty() || (factor && searchable(*factor, query)))
            filtered.push_back(record);
    }
    return filtered;
}

namespace {

// The single place the TQLEX call shape is spelled out. Both public retry
// wrappers below share it so their request parameters cannot drift apart.
Json execute_once(
    const fs::path& root, const std::string& request_id,
    const std::map<std::string, std::string>& replacements,
    bool all_pages, int page, int page_size, int max_pages, int timeout_ms) {
    return execute_tqlex_config(
        root, request_id, replacements, {}, {}, kTqlexSourceFile, {},
        all_pages, page, page_size, max_pages, kTqlexEndpoint, timeout_ms);
}

void backoff(int attempt) {
    std::this_thread::sleep_for(std::chrono::milliseconds(250 * (1 << attempt)));
}

}  // namespace

Json fetch_factor_rows(
    const fs::path& root, const std::string& request_id,
    const std::map<std::string, std::string>& replacements,
    bool all_pages, int page, int page_size, int max_pages, int timeout_ms) {
    Json upstream;
    for (int attempt = 0; attempt < kFetchAttempts; ++attempt) {
        try {
            upstream = execute_once(root, request_id, replacements, all_pages,
                                    page, page_size, max_pages, timeout_ms);
            break;
        } catch (const Error& error) {
            if (!transient_error(error.what()) || attempt == kFetchAttempts - 1) throw;
            backoff(attempt);
        }
    }
    return upstream;
}

bool try_fetch_factor_rows(
    const fs::path& root, const std::string& request_id,
    const std::map<std::string, std::string>& replacements,
    bool all_pages, int page, int page_size, int max_pages, int timeout_ms,
    Json& upstream, std::string& last_error) {
    for (int attempt = 0; attempt < kFetchAttempts; ++attempt) {
        try {
            upstream = execute_once(root, request_id, replacements, all_pages,
                                    page, page_size, max_pages, timeout_ms);
            return true;
        } catch (const Error& error) {
            last_error = error.what();
            // A non-transient failure is final regardless of the attempt count.
            if (!transient_error(last_error)) throw;
            if (attempt == kFetchAttempts - 1) return false;
            backoff(attempt);
        }
    }
    return false;
}

}  // namespace tdx::factor_detail
