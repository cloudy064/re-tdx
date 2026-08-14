#pragma once

#include "tdx/unlocks.hpp"

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace tdx::unlocks_detail {

inline constexpr const char* master_resource = "list/func_dbljj04_1.jsn";
inline constexpr const char* recent_large_resource = "list/func_jqgz103_1.jsn";
inline constexpr const char* monthly_pressure_resource = "list/func_dxfjj101_1.jsn";

std::filesystem::path native_path(const std::string& value);
std::string now_text();
std::string today_text();
const Json* value_ptr(const Json& object, std::string_view name);
std::string text_value(const Json& object, std::string_view name);
std::optional<double> number_value(const Json& object, std::string_view name);
Json number_or_null(const std::optional<double>& value);
void append_unique_text(Json& values, const std::string& value);
bool event_label_matches(const Json& event, std::string_view scalar_name,
                         std::string_view array_name,
                         const std::string& expected);
bool six_digits(const std::string& value);
bool eight_digits(const std::string& value);
int canonical_market_id(const std::string& value);
std::string market_name(int value);
std::string market_prefix(int value);
Json security_document(
    int market_id, const std::string& code,
    const std::map<std::pair<int, std::string>, Security>& securities);
Json source_summary(const Json& document);
Json load_local_resource(const std::filesystem::path& root,
                         const std::string& resource);
int bounded_integer(const std::string& text, const std::string& name,
                    int minimum, int maximum);
bool json_contains(const Json& value, const std::string& needle);
Json summarize_events(const Json& events, std::uint64_t raw_rows);

}  // namespace tdx::unlocks_detail
