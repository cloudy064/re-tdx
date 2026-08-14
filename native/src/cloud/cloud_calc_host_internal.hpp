#pragma once

#include "cloud_calc_config_internal.hpp"

#include "tdx/blocks.hpp"
#include "tdx/json.hpp"
#include "tdx/seal_order.hpp"

#include <initializer_list>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace tdx::cloud_calc_detail {

using SecurityKey = std::pair<int, std::string>;
using QuoteIndex = std::map<SecurityKey, const Json*>;
using FinanceIndex = std::map<SecurityKey, const Json*>;

struct HostValue {
    Json value;
    std::string source;
};

struct GroupMember {
    std::optional<SecurityKey> public_security;
    std::string token;
};

struct GroupMembers {
    bool source_present{};
    std::vector<GroupMember> values;
    std::vector<std::string> invalid_tokens;
    std::uint64_t unsupported_market_count{};
};

struct GroupAggregate {
    std::size_t member_count{};
    std::size_t mapped_quote_count{};
    std::size_t valid_change_count{};
    std::size_t weighted_member_count{};
    double average_change{};
    double weighted_change{};
    double maximum_change{};
    double up_rate{};
    std::string leader_name;
    bool has_weighted_change{};
    bool has_leader{};
};

const Json* host_optional(const Json& object, std::string_view key);
bool meaningful(const Json* value);
std::string scalar_text(const Json& value);
std::optional<int> host_market(const Json& value);
std::optional<SecurityKey> row_security(const Json& row, std::string_view suffix);
std::string security_text(const SecurityKey& value);
const Json::Array& snapshot_records(const Json& document);
Json::Array& mutable_snapshot_records(Json& document);
QuoteIndex index_snapshots(const Json& document);
std::optional<double> quote_number(const Json& quote, std::string_view name);
std::optional<HostValue> quote_host_value(std::string_view syscol, const Json& quote);
bool seal_syscol(std::string_view value);
bool depth_syscol(std::string_view value);
bool speed_syscol(std::string_view value);
bool quote_syscol(std::string_view value);
Json enrich_seal_snapshots(Json& snapshots, const BlockData& blocks,
                           const LimitRuleConfig& rules,
                           const Json& special_limits, int as_of_yyyymmdd);
bool finance_syscol(std::string_view value);
bool group_syscol(std::string_view value);
GroupMembers parse_group_members(const Json& row);
std::optional<double> group_change_pct(const Json& quote);
GroupAggregate aggregate_group(const GroupMembers& members,
                               const QuoteIndex& quotes,
                               const FinanceIndex& finance);
bool known_tbigdata_syscol(std::string_view value);
bool recognized_host_syscol(std::string_view value);
const Json* nested_value(const Json& value,
                         std::initializer_list<std::string_view> path);
FinanceIndex index_finance(const Json& document);
std::optional<double> nested_number(
    const Json& value, std::initializer_list<std::string_view> path);
std::optional<HostValue> finance_host_value(std::string_view syscol,
                                            const Json& finance,
                                            const Json* quote);
const Block* host_industry_block(const BlockData& data,
                                 const SecurityKey& security);
std::optional<double> current_coupon_rate(const Json& row);
std::optional<double> bond_accrued_interest(const Json& row, int as_of);
bool config_references(const Config& config, std::string_view code);
Json resolve_host_fields(const Config& config, const Json& source_row,
                         const Json& snapshots, const Json& finance_document,
                         const BlockData* block_data, int as_of_yyyymmdd);

}  // namespace tdx::cloud_calc_detail
