#pragma once

#include "tdx/ownership.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace tdx::ownership_detail {

inline constexpr const char* change_increase_resource = "list/func_zcjc101_1.jsn";
inline constexpr const char* change_decrease_resource = "list/func_zcjc107_1.jsn";
inline constexpr const char* plan_increase_resource = "list/func_zcjc105_1.jsn";
inline constexpr const char* plan_decrease_resource = "list/func_zcjc111_1.jsn";
inline constexpr const char* commitment_resource = "list/func_zcjc106_1.jsn";
inline constexpr const char* ranking_increase_ratio_resource = "list/func_zcjc102_1.jsn";
inline constexpr const char* ranking_increase_value_resource = "list/func_zcjc103_1.jsn";
inline constexpr const char* ranking_increase_count_resource = "list/func_zcjc104_1.jsn";
inline constexpr const char* ranking_decrease_ratio_resource = "list/func_zcjc108_1.jsn";
inline constexpr const char* ranking_decrease_value_resource = "list/func_zcjc109_1.jsn";
inline constexpr const char* ranking_decrease_count_resource = "list/func_zcjc110_1.jsn";
inline constexpr const char* shareholder_sh_main_resource = "list/func_gdrs101_1.jsn";
inline constexpr const char* shareholder_sz_main_resource = "list/func_gdrs102_1.jsn";
inline constexpr const char* shareholder_sz_sme_legacy_resource = "list/func_gdrs103_1.jsn";
inline constexpr const char* shareholder_chinext_resource = "list/func_gdrs104_1.jsn";
inline constexpr const char* shareholder_star_resource = "list/func_gdrs106_1.jsn";
inline constexpr const char* shareholder_bj_resource = "list/func_gdrs107_1.jsn";
inline constexpr const char* insider_resource = "list/func_cggg101_1.jsn";
inline constexpr const char* change_month_resource = "list/func_gdzjc102_1.jsn";
inline constexpr const char* change_year_resource = "list/func_gdzjc105_1.jsn";
inline constexpr const char* pledge_latest_resource = "list/func_gqzy101_1.jsn";
inline constexpr const char* pledge_warning_resource = "list/func_gqzy102_1.jsn";
inline constexpr const char* pledge_liquidation_resource = "list/func_gqzy103_1.jsn";
inline constexpr const char* pledge_month_resource = "list/func_gqzy105_1.jsn";
inline constexpr const char* pledge_trust_resource = "list/func_gqzy106_1.jsn";
inline constexpr const char* pledge_broker_resource = "list/func_gqzy108_1.jsn";
inline constexpr const char* pledge_release_resource = "list/func_gqzy109_1.jsn";

std::filesystem::path native_path(const std::string& value);
std::string now_text();
std::string today_text();
const Json* value_ptr(const Json& object, std::string_view name);
std::string text_value(const Json& object, std::string_view name);
std::optional<double> number_value(const Json& object, std::string_view name);
Json number_or_null(const std::optional<double>& value);
Json scaled_number(const Json& row, std::string_view name, double scale);
std::string normalized_multiline(std::string value);
std::string labeled_line(const std::string& value, const std::string& label);
bool digits(const std::string& value, std::size_t count);
bool safe_identifier(const std::string& value);
int market_id(const std::string& value);
std::optional<int> row_market_id(const Json& row);
std::string market_name(int id);
std::string market_prefix(int id);
Json security_document(
    int id,
    const std::string& code,
    const std::map<std::pair<int, std::string>, Security>& securities);
const Json& document_for_resource(const Json& documents, std::string_view resource);
Json source_summary(const Json& document);
Json missing_source_summary(const std::string& resource);
bool json_contains(const Json& value, const std::string& needle);
Json limited_filtered(const Json& values, const std::string& query, int limit);
Json select_security(
    const Json& rows,
    bool security_mode,
    int selected_market,
    const std::string& selected_code);
void append_rows(Json& destination, const Json& source);
int bounded_integer(
    const std::string& text,
    const std::string& name,
    int minimum,
    int maximum);
std::uint64_t unique_security_count(const std::vector<const Json*>& groups);

}  // namespace tdx::ownership_detail
