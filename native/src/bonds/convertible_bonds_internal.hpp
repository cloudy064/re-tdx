#pragma once

#include "tdx/convertible_bonds.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace tdx::convertible_bond_detail {

extern const std::vector<std::string> master_resources;
extern const std::string exchangeable_resource;
extern const std::string exchangeable_projection_resource;
extern const std::string pending_resource;
extern const std::vector<std::string> pending_projection_resources;
extern const std::string subscription_resource;
extern const std::string new_bond_projection_resource;
extern const std::string pricing_resource;

struct CashFlow {
    double years{};
    double amount{};
};

const Json* field(const Json& row, std::string_view name);
std::string value_text(const Json& row, std::string_view name);
std::optional<double> value_number(const Json& row, std::string_view name);
Json number(const Json& row, std::string_view name);
Json optional_number(const std::optional<double>& value);
std::string compact_date_prefix(const std::string& source);
Json empty_rows_document();
Json security_set_reconciliation(
    const Json& primary_rows,
    const Json& projection_rows,
    std::string_view member_name,
    const std::string& primary_resource,
    const std::string& projection_resource);
Json raw_document_reconciliation(
    const Json& primary_document,
    const Json& projection_document,
    const std::string& primary_resource,
    const std::string& projection_resource);
std::optional<long long> civil_days(const std::string& compact);
std::string today_compact();
std::optional<double> quote_number(const Json& row, const char* name);
std::optional<double> usable_quote_price(const Json* row);
std::string quote_price_source(const Json* row);
std::optional<double> accrued_interest(
    const Json& row, const std::string& as_of);
std::vector<CashFlow> remaining_cash_flows(
    const Json& row, const std::string& as_of);
std::optional<double> discounted_value(
    const std::vector<CashFlow>& flows, double annual_rate);
std::optional<double> solve_ytm(
    const std::vector<CashFlow>& flows, double full_price);
int market_id(const std::string& value);
bool valid_code(const std::string& code);
std::string key_for(const Json& row);
Json security_document(
    int id,
    const std::string& code,
    const std::string& fallback,
    const std::map<std::pair<int, std::string>, Security>& securities);
Json text_array(const std::string& source);
Json numeric_array(const std::string& source);
Json trigger_document(
    const Json& row,
    const char* start,
    const char* price,
    const char* conversion,
    const char* current_days,
    const char* status,
    const char* history_count,
    const char* history_dates,
    const char* available_days);
Json source_summary(const Json& document);
Json normalize_detail(const Json& source, const std::string& kind);
std::string now_text();
std::filesystem::path native_path(const std::string& value);
int bounded(
    const std::string& text, std::string_view name, int low, int high);

}  // namespace tdx::convertible_bond_detail
