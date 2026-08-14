#pragma once

#include "tdx/equity_valuation.hpp"
#include "tdx/common.hpp"

#include <filesystem>
#include <map>
#include <optional>
#include <string_view>

namespace tdx::detail::equity_valuation {

enum class ViewKind {
    pe_industries,
    pe_security_history,
    pe_industry_members,
    pb_roe_industries,
    pb_roe_members,
};

enum class SortKind {
    history,
    industry,
    security,
};

using RowNormalizer = Json (*)(const Json&, const BlockData&, std::string&);

struct ViewSpec {
    const char* id;
    ViewKind kind;
    const char* request_id;
    const char* source_file;
    bool pe;
    bool history;
    bool industry_members;
    SortKind sort;
    RowNormalizer normalize_row;
};

struct RequestPlan {
    std::string request_id;
    std::string source_file;
    std::map<std::string, std::string> replacements;
};

const ViewSpec& view_spec(std::string_view raw);

std::filesystem::path native_path(const std::string& value);
std::string now_text();
std::string today_text();
bool ascii_digits(const std::string& value);
std::string compact_date(std::string value, std::string_view name);
std::string years_before(const std::string& date, int years);
std::string display_date(const std::string& value);
const Json* field(const Json& row, std::string_view key);
std::string text(const Json& row, std::string_view key);
std::optional<double> number(const Json& row, std::string_view key);
Json number_json(const std::optional<double>& value);
int integer_value(const std::string& raw, std::string_view name,
                  int minimum, int maximum);
double decimal_value(const std::string& raw, std::string_view name,
                     double minimum, double maximum);
int pe_type_value(std::string raw);
std::string pe_type_name(int value);
int security_market(std::string raw);
std::string market_name(int value);
std::string market_prefix(int value);
Json security_identity(const std::string& raw_market, const std::string& code,
                       std::string name, const BlockData& blocks);
Json industry_identity(const std::string& code, const std::string& raw_market,
                       std::string name, const BlockData& blocks);
Json judgment(const Json& row, std::string_view key,
              bool allow_reasonable = true);
Json forecast(const Json& row, const std::string& prefix);
std::string cache_key(const EquityValuationQuery& query);
bool transient_error(const std::string& message);
Json source_document(const Json& upstream, const std::string& request_id);
Json methodology(const ViewSpec& view);

Json normalize_pe_industry_row(
    const Json& row, const BlockData& blocks, std::string& identity);
Json normalize_pb_industry_row(
    const Json& row, const BlockData& blocks, std::string& identity);
Json normalize_pe_history_row(
    const Json& row, const BlockData& blocks, std::string& identity);
Json normalize_pe_member_row(
    const Json& row, const BlockData& blocks, std::string& identity);
Json normalize_pb_member_row(
    const Json& row, const BlockData& blocks, std::string& identity);

EquityValuationQuery normalize_query_options(
    const EquityValuationQuery& input, const ViewSpec& view, int& pe_type);
RequestPlan build_request_plan(
    const EquityValuationQuery& options, const ViewSpec& view, int pe_type);

}  // namespace tdx::detail::equity_valuation
