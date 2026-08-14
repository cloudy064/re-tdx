#pragma once

#include "tdx/block_trades.hpp"

#include <array>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace tdx::block_trade_detail {

enum class CoreDataset {
    month,
    trades,
    intentions,
};

struct CoreResourceSpec {
    CoreDataset dataset;
    std::string_view resource;
};

inline constexpr std::array<CoreResourceSpec, 3> core_resources{{
    {CoreDataset::month, "list/func_dzjy101_1.jsn"},
    {CoreDataset::trades, "list/func_dzjy104_1.jsn"},
    {CoreDataset::intentions, "list/func_dzjy1012_1.jsn"},
}};

struct BrokerPeriodSpec {
    std::string_view id;
    std::string_view label;
    std::string_view resource;
    std::string_view detail_namespace;
};

inline constexpr std::array<BrokerPeriodSpec, 4> broker_periods{{
    {"1m", "近一月", "list/func_dzjy107_1.jsn", "yybph22401"},
    {"3m", "近三月", "list/func_dzjy108_1.jsn", "yybph22402"},
    {"6m", "近半年", "list/func_dzjy109_1.jsn", "yybph22403"},
    {"1y", "近一年", "list/func_dzjy1010_1.jsn", "yybph22404"},
}};

inline constexpr std::array<std::string_view, 4> supported_views{{
    "trades", "intentions", "brokers", "industries"}};

const CoreResourceSpec& core_resource(CoreDataset dataset);
const BrokerPeriodSpec& broker_period(std::string_view id);
std::vector<std::string> core_resource_names();
bool valid_view(std::string_view view);

std::filesystem::path native_path(const std::string& value);
std::string now_text();
const Json* value_ptr(const Json& object, std::string_view name);
std::string text_value(const Json& object, std::string_view name);
std::optional<double> number_value(const Json& object, std::string_view name);
Json number_or_null(const std::optional<double>& value);
Json scaled_number(const Json& row, std::string_view name, double scale);
bool six_digits(const std::string& value);
bool valid_month(const std::string& value);
int canonical_market_id(const std::string& value);
Json security_document(
    int market_id, const std::string& code,
    const std::map<std::pair<int, std::string>, Security>& securities);
const Json& document_for_resource(const Json& documents,
                                  std::string_view resource);
Json source_summary(const Json& document);
bool json_contains(const Json& value, const std::string& needle);
Json limited_filtered(const Json& values, const std::string& query, int limit);
int bounded_integer(const std::string& text, const std::string& name,
                    int minimum, int maximum);
Json performance_point(int days, const Json& row,
                       const char* success, const char* average);

}  // namespace tdx::block_trade_detail
