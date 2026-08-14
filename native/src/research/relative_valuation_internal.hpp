#pragma once

#include "tdx/common.hpp"
#include "tdx/relative_valuation.hpp"

#include <filesystem>
#include <optional>
#include <string_view>

namespace tdx::detail::relative_valuation {

enum class IndexTypeKind { broad, industry, composite, theme, scale };
enum class MethodKind { pe_ttm, pb_mrq, ps_ttm };

struct IndexTypeSpec {
    const char* id;
    const char* code;
    const char* name;
    IndexTypeKind kind;
};
struct MethodSpec {
    const char* id;
    const char* code;
    const char* name;
    MethodKind kind;
};
struct BenchmarkSpec {
    const char* code;
    const char* name;
};
struct QueryPlan {
    RelativeValuationQuery options;
    const IndexTypeSpec* index_type{};
    const BenchmarkSpec* benchmark{};
    const MethodSpec* method{};
    std::map<std::string, std::string> replacements;
    std::string master_cache_key;
};

const IndexTypeSpec& index_type_spec(std::string_view raw);
const MethodSpec& method_spec(std::string_view raw);
const BenchmarkSpec& benchmark_spec(std::string_view raw);
QueryPlan make_query_plan(const RelativeValuationQuery& input);

std::filesystem::path native_path(const std::string& value);
std::string now_text();
std::string today_text();
bool digits(const std::string& value);
bool leap_year(int year);
std::string compact_date(std::string value, std::string_view name);
std::string years_before(const std::string& date, int years);
std::string display_date(const std::string& value);
const Json* field(const Json& row, std::string_view key);
std::string text(const Json& row, std::string_view key);
std::optional<double> number(const Json& row, std::string_view key);
Json number_json(const std::optional<double>& value);
int bounded(const std::string& raw, std::string_view name,
            int minimum, int maximum);

int market_id(const std::string& raw);
std::string market_name(int value);
std::string fallback_index_name(int market, const std::string& code);
Json security_document(
    int market, const std::string& code,
    const std::map<std::pair<int, std::string>, Security>& securities);
Json relative_position(const std::optional<double>& quantile);
bool transient_error(const std::string& message);
Json source_document(const Json& upstream, const Json& raw_rows,
                     const std::string& transport, const std::string& request_id);
Json range_summary(const Json& history);
Json parameters_document(const QueryPlan& plan);
Json methodology_document();

}  // namespace tdx::detail::relative_valuation
