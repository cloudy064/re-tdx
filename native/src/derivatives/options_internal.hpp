#pragma once

#include "tdx/options.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace tdx::option_detail {

inline constexpr int kBinomialNodes = 20;

std::string upper_ascii(std::string value);
double parse_number(
    const std::string& text,
    std::string_view name,
    double minimum,
    double maximum);
int parse_integer(
    const std::string& value,
    std::string_view name,
    int minimum,
    int maximum);
int option_market_id(std::string value);
std::string option_market_name(int market_id);
const Json* object_value(const Json& object, std::string_view key);
Json cached_full_option_catalog(
    bool refresh, int cache_ttl_seconds, int timeout_ms);

std::string normalize_date(std::string value);
long long civil_days(std::string value);

const Json& select_bar(const Json& bars, const std::string& requested_date);
std::optional<OptionInstrument> find_option_in_catalog(
    const Json& catalog,
    int market_id,
    const std::string& wire_code);
double json_number_or(
    const Json& object,
    std::string_view key,
    double fallback = 0.0);
const Json* find_expansion_quote(
    const Json& batch,
    int market_id,
    const std::string& code);
double top_of_book(const Json& quote, std::string_view side);
std::string local_calendar_date();

}  // namespace tdx::option_detail
