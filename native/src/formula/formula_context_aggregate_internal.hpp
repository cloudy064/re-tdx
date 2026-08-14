#pragma once

#include "tdx/blocks.hpp"
#include "tdx/daily.hpp"
#include "tdx/json.hpp"

#include <cstdint>
#include <filesystem>
#include <initializer_list>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace tdx::formula_context_detail {

struct HorcalcBinding {
    std::string name;
    std::string block_name;
    int item{};
    int calculation{};
    int weight{};
};

struct IndicatorAggregateBinding {
    std::string name;
    std::string function;
    std::string block_name;
    std::string formula_name;
    int output{};
    int mode{};
};

using AggregateSecurityKey = std::pair<int, std::string>;
using AggregateDailyMap =
    std::map<AggregateSecurityKey, std::vector<DailyBar>>;

struct AggregateUniverseCatalog {
    int industry_mode{};
    std::string active_industry;
    CustomBlockCountCatalog custom;
};

struct AggregateUniverseResolution {
    std::string lookup_name;
    std::string family;
    std::string block_code;
    std::string source;
    std::vector<AggregateSecurityKey> members;
    bool found{};
};

enum class AggregateMemberPolicy {
    horcalc,
    indicator,
};

const Json* aggregate_optional(const Json& object, std::string_view key);
std::string aggregate_text_or(const Json& object, std::string_view key,
                              std::string fallback = {});
int aggregate_integer_or(const Json& object, std::string_view key,
                         int fallback);
std::string aggregate_compact_date(const std::string& value);
bool aggregate_starts_with(std::string_view value, std::string_view prefix);
const Json* aggregate_nested(
    const Json& value, std::initializer_list<std::string_view> path);
std::string aggregate_market_name(int market_id);

AggregateUniverseCatalog load_aggregate_universe_catalog(
    const std::filesystem::path& root);
AggregateUniverseResolution resolve_aggregate_universe(
    const std::filesystem::path& root, const BlockData& data,
    const AggregateUniverseCatalog& catalog, const std::string& requested_name,
    AggregateMemberPolicy member_policy);
AggregateDailyMap load_aggregate_daily(
    const std::filesystem::path& root,
    const std::vector<AggregateUniverseResolution>& resolutions,
    std::uint64_t& member_file_count);

void bind_horcalc(
    Json& context, const std::filesystem::path& root, const BlockData& data,
    const Json& target, const std::vector<HorcalcBinding>& bindings,
    int timeout_ms);

void bind_indicator_aggregates(
    Json& context, const std::filesystem::path& root, const BlockData& data,
    const Json& target, int target_market_id, const std::string& target_code,
    const std::vector<IndicatorAggregateBinding>& bindings,
    const Json& formula_library);

}  // namespace tdx::formula_context_detail
