#pragma once

#include "tdx/leverage.hpp"

#include <array>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>

namespace tdx::leverage_detail {

struct ResourceDefinition {
    std::string_view category;
    std::string_view resource;
};

inline constexpr std::array<ResourceDefinition, 13> margin_resources{{
    {"balance", "list/func_rzrq102_1.jsn"},
    {"financing-ratio", "list/func_rzrq103_1.jsn"},
    {"short-ratio", "list/func_rzrq104_1.jsn"},
    {"financing-down-continuous", "list/func_rzrq107_1.jsn"},
    {"short-up-large", "list/func_rzrq108_1.jsn"},
    {"short-down-large", "list/func_rzrq109_1.jsn"},
    {"financing-up-continuous", "list/func_rzrq110_1.jsn"},
    {"financing-up-large", "list/func_rzrq111_1.jsn"},
    {"financing-down-large", "list/func_rzrq112_1.jsn"},
    {"short-up-continuous", "list/func_rzrq113_1.jsn"},
    {"short-down-continuous", "list/func_rzrq114_1.jsn"},
    {"short-balance", "list/func_rzrq120_1.jsn"},
    {"etf", "list/func_rzrq201_1.jsn"},
}};

inline constexpr std::array<ResourceDefinition, 3> margin_classification_resources{{
    {"industry", "rzrq6/"},
    {"concept", "rzrq7/"},
    {"style", "rzrq8/"},
}};

inline constexpr std::array<ResourceDefinition, 8> flow_resources{{
    {"sh-northbound", "list/func_hsgt101_1.jsn"},
    {"sh-southbound", "list/func_hsgt102_1.jsn"},
    {"sz-northbound", "list/func_hsgt103_1.jsn"},
    {"sz-southbound", "list/func_hsgt104_1.jsn"},
    {"northbound-total", "list/func_hsgt113_1.jsn"},
    {"southbound-total", "list/func_hsgt114_1.jsn"},
    {"northbound-weekly", "list/func_gghq_hsgt_lgt_1.jsn"},
    {"southbound-weekly", "list/func_gghq_hsgt_ggt_1.jsn"},
}};

inline constexpr std::array<ResourceDefinition, 5> holding_resources{{
    {"hong-kong-current", "list/func_hsgt201_1.jsn"},
    {"hong-kong-removed", "list/func_hsgt202_1.jsn"},
    {"mainland-current", "list/func_hsgt203_1.jsn"},
    {"mainland-removed", "list/func_hsgt204_1.jsn"},
    {"mainland-quarterly", "list/func_hsgt212_1.jsn"},
}};

inline constexpr std::array<ResourceDefinition, 7> activity_resources{{
    {"daily-increase", "list/func_hsgt205_1.jsn"},
    {"daily-decrease", "list/func_hsgt206_1.jsn"},
    {"continuous-increase", "list/func_hsgt207_1.jsn"},
    {"continuous-decrease", "list/func_hsgt208_1.jsn"},
    {"five-day-increase", "list/func_hsgt209_1.jsn"},
    {"five-day-decrease", "list/func_hsgt210_1.jsn"},
    {"frequent-increase", "list/func_hsgt211_1.jsn"},
}};

inline constexpr std::array<ResourceDefinition, 5> industry_resources{{
    {"northbound-all", "list/func_hsgt112_1.jsn"},
    {"northbound-industry", "list/func_hsgt107_1.jsn"},
    {"northbound-concept", "list/func_hsgt110_1.jsn"},
    {"northbound-style", "list/func_hsgt111_1.jsn"},
    {"southbound-industry", "list/func_hsgt301_1.jsn"},
}};

template <std::size_t Size>
const ResourceDefinition *find_resource(
    const std::array<ResourceDefinition, Size> &catalog,
    std::string_view category) {
    for (const auto &definition : catalog)
        if (definition.category == category)
            return &definition;
    return nullptr;
}

std::filesystem::path native_path(const std::string &value);
std::string now_text();
const Json *ptr(const Json &object, std::string_view key);
std::string value_text(const Json &object, std::string_view key);
std::optional<double> value_number(const Json &object, std::string_view key);
Json number_json(const std::optional<double> &value, double multiplier = 1.0);
bool digits(const std::string &value, std::size_t size);
bool safe_group_id(const std::string &value);
int market_id(std::string value, bool mainland_only = false);
std::string market_name(int id);
std::string market_prefix(int id);
Json security_json(
    int id, const std::string &code,
    const std::map<std::pair<int, std::string>, Security> &securities);
Json source_summary(const Json &source);
bool zero_length_resource(const std::exception &error);
Json missing_source(const std::string &resource, const std::string &message);
bool contains(const Json &value, const std::string &needle);
Json filter_rows(const Json &rows, const std::string &query, int limit);
int bounded(const std::string &text, std::string_view name, int minimum,
            int maximum);
void sort_date(Json &rows, bool descending);
int date_age_days(const std::string &compact);
std::string snapshot_freshness(const std::string &date);
std::string active_channel_suffix(std::string value);
Json query_cache(bool refreshed, int age);

} // namespace tdx::leverage_detail
