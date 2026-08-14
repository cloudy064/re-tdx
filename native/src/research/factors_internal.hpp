#pragma once

#include "tdx/cloud_endpoints.hpp"
#include "tdx/factors.hpp"

#include <array>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>

namespace tdx::factor_detail {

struct ViewDefinition {
    const char* view;
    const char* request_id;
    int page_size;
    const char* title;
    const char* scope;
};

inline constexpr std::array<ViewDefinition, 10> view_definitions{{
    {"catalog", "200626", 100, "因子目录", "33 个普通因子及 120 日回测指标"},
    {"members", "200636", 200, "因子成分", "指定普通因子下的全部股票"},
    {"dashboard", "200646", 200, "因子看板", "按入选因子总数排列的股票"},
    {"patterns", "200650", 100, "形态因子目录", "57 个 K 线形态因子及 120 日回测指标"},
    {"pattern-members", "200651", 200, "形态因子成分", "指定形态因子下的全部股票"},
    {"intraday-radar", "200660", 1000, "分时雷达", "安全分大于 60 的分时量价与趋势信号"},
    {"security", "200626+200646", 200, "个股因子反查", "从完整因子看板反查股票当前命中的普通因子"},
    {"pattern-matrix", "200650+200651", 200, "形态因子关系矩阵", "完整构建形态因子到股票的关系并支持反向索引"},
    {"standard-matrix", "200626+200636", 200, "普通因子关系矩阵", "完整构建普通因子到股票的关系并与因子看板对账"},
    {"overview", "200626+200646", 200, "因子市场广度", "统计普通因子覆盖率、安全分、收益和因子共现"},
}};

inline constexpr const char* kTqlexEndpoint = cloud_endpoints::tqlex;
inline constexpr const char* kTqlexSourceFile = "gp_gz_fsld.xml";
inline constexpr int kFetchAttempts = 5;

const ViewDefinition* view_definition(const std::string& view);
std::filesystem::path native_path(const std::string& value);
std::string now_text();
const Json* field(const Json& row, std::string_view key);
std::string text(const Json& row, std::string_view key);
std::optional<double> number(const Json& row, std::string_view key);
Json number_json(const std::optional<double>& value);
int market_id(std::string raw);
std::string market_name(int market);
std::string market_prefix(int market);
Json security_document(const Json& row, const BlockData& blocks);
Json split_factors(const std::string& raw);
Json catalog_record(const Json& row, bool pattern);
bool searchable(const Json& record, const std::string& raw_query);
bool transient_error(const std::string& message);
Json source_document(
    const Json& upstream,
    const ViewDefinition& definition,
    std::size_t row_count,
    bool all_pages);
std::string cache_key(const FactorQuery& query);
int bounded(
    const std::string& raw,
    std::string_view name,
    int minimum,
    int maximum);
Json views_document();
Json enrich_quotes(
    const std::filesystem::path& root,
    Json& records,
    const BlockData& blocks,
    int timeout_ms);

// ---- query pipeline (shared by the four view-family units) ----

// Trims and lower-cases the routable fields in place, rejects illegal
// view/factor_id/market/code/flag/limit combinations, and returns the matching
// view definition. Throws Error with the original messages.
const ViewDefinition& validate_factor_query(FactorQuery& options);

// The uniform four-key cache block every view family emits on a fresh document.
Json fresh_cache_document(int ttl_seconds);

// Appends document["sources"] onto sources, preserving order.
void append_sources(Json& sources, const Json& document);

// Truncates records to limit and reports whether truncation happened.
bool apply_limit(Json& records, int limit);

// Keeps records whose nested "factor" object matches the free-text query.
// An empty query keeps everything.
Json filter_by_factor_field(const Json& records, const std::string& query);

// One TQLEX request with up to kFetchAttempts tries and 250ms doubling backoff
// on transient errors. Non-transient errors and a transient error on the final
// attempt propagate unchanged.
Json fetch_factor_rows(
    const std::filesystem::path& root,
    const std::string& request_id,
    const std::map<std::string, std::string>& replacements,
    bool all_pages,
    int page,
    int page_size,
    int max_pages,
    int timeout_ms);

// Same retry policy, but a transient error on the final attempt yields false
// with the message in last_error instead of throwing, so the caller can fall
// back to a stale cache entry. Non-transient errors still propagate.
bool try_fetch_factor_rows(
    const std::filesystem::path& root,
    const std::string& request_id,
    const std::map<std::string, std::string>& replacements,
    bool all_pages,
    int page,
    int page_size,
    int max_pages,
    int timeout_ms,
    Json& upstream,
    std::string& last_error);

}  // namespace tdx::factor_detail
