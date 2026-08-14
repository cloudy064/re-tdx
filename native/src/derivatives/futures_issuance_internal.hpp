#pragma once

#include "tdx/futures_issuance.hpp"

#include <array>
#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace tdx::futures_issuance_detail {

struct ResourceDefinition {
    std::string_view resource;
    std::string_view category;
};

inline constexpr std::array<ResourceDefinition, 3> futures_resources{{
    {"list/func_qhtj101_1.jsn", "commodity"},
    {"list/func_qhtj103_1.jsn", "monthly"},
    {"list/func_qhtj104_1.jsn", "index"},
}};

inline constexpr std::array<ResourceDefinition, 3> ipo_resources{{
    {"list/func_ipotj101_1.jsn", "annual"},
    {"list/zq_ssfxr201.jsn", "listed-issuers"},
    {"list/zq_wssfxr201.jsn", "unlisted-issuers"},
}};

inline constexpr std::array<ResourceDefinition, 6> placement_resources{{
    {"list/func_qxfa201_1.jsn", "implemented-locked"},
    {"list/func_qxfa202_1.jsn", "implemented-unlocked"},
    {"list/func_qxfa301_1.jsn", "plan-active"},
    {"list/func_qxfa302_1.jsn", "plan-stopped"},
    {"list/func_qxfa402_1.jsn", "implemented"},
    {"list/func_qxfa601_1.jsn", "registered"},
}};

inline constexpr std::array<ResourceDefinition, 3> rights_resources{{
    {"list/func_qxfa107_1.jsn", "implemented"},
    {"list/func_qxfa108_1.jsn", "deliberating"},
    {"list/func_qxfa109_1.jsn", "abnormal"},
}};

inline constexpr std::array<ResourceDefinition, 1> preferred_share_resources{{
    {"list/func_yxg101_3.jsn", "preferred-shares"},
}};

inline constexpr std::array<std::string_view, 6> sections{{
    "all", "futures", "ipo", "placements", "rights", "preferred-shares",
}};

inline constexpr std::array<std::string_view, 7> placement_statuses{{
    "all", "locked", "unlocked", "active", "stopped", "implemented", "registered",
}};

template <typename Value, std::size_t Size>
bool contains_value(const std::array<Value, Size>& catalog, std::string_view value) {
    for (const auto& candidate : catalog)
        if (candidate == value) return true;
    return false;
}

template <std::size_t Size>
void append_resource_paths(std::vector<std::string>& result,
                           const std::array<ResourceDefinition, Size>& catalog) {
    for (const auto& definition : catalog)
        result.emplace_back(definition.resource);
}

std::filesystem::path native_path(const std::string& value);
const Json* value_ptr(const Json& object, std::string_view name);
std::string text_value(const Json& object, std::string_view name);
Json number_value(const Json& object, std::string_view name);
bool digits(const std::string& value, std::size_t count);
bool safe_key(const std::string& value, std::size_t minimum, std::size_t maximum);
int market_id(const std::string& value);
std::string market_name(int value);
std::string market_prefix(int value);
std::string normalized_market_id(const std::string& value);
bool contains_folded(const std::string& value, const std::string& needle);

Json private_placement_summary(const Json& rows);
Json rights_offering_summary(const Json& rows);
Json preferred_share_summary(const Json& rows);
bool placement_status_matches(const Json& row, const std::string& status);

Json security_document(
    const std::string& market, const std::string& code,
    const std::map<std::pair<int, std::string>, Security>& securities);
const Json& document_for(const Json& documents, std::string_view resource);
Json source_summary(const Json& document);
Json limited(Json rows, int limit);

Json normalize_monthly_futures(const Json& rows);
Json normalize_index_futures(const Json& rows);
Json normalize_related_stocks(
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities);
Json normalize_position_history(const Json& rows);

Json normalize_ipo_annual(const Json& rows);
Json normalize_ipo_industries(const Json& rows);
Json normalize_ipo_monthly(const Json& rows);
Json normalize_listed_issuers(
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities);
Json normalize_unlisted_issuers(const Json& rows);

std::string now_text();
int bounded_integer(const std::string& text, const std::string& name,
                    int minimum, int maximum);

}  // namespace tdx::futures_issuance_detail

