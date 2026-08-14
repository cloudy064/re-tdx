#include "cloud_calc_host_internal.hpp"

#include "cloud_calc_builtins_internal.hpp"
#include "tdx/common.hpp"

#include <algorithm>
#include <cstdint>
#include <set>
#include <string>
#include <utility>

namespace tdx::cloud_calc_detail {

bool config_references(const Config& config, std::string_view code) {
    for (const auto& unit : config.units)
        for (const auto& column : unit.columns)
            if (std::find(column.refs.begin(), column.refs.end(), code) != column.refs.end()) return true;
    return false;
}

Json resolve_host_fields(const Config& config, const Json& source_row,
                         const Json& snapshots, const Json& finance_document,
                         const BlockData* block_data, int as_of) {
    if (!source_row.is_object()) throw Error("cloud-calc host input row must be a JSON object");
    if (as_of != 0) validate_date(as_of);
    Json row = source_row;
    const auto quotes = index_snapshots(snapshots);
    const auto finance = index_finance(finance_document);
    const auto group_members = parse_group_members(row);
    const auto group_aggregate = aggregate_group(group_members, quotes, finance);
    Json bindings = Json::array(), unresolved = Json::array(), requested = Json::array();
    Json requested_depth = Json::array(), requested_speed = Json::array(),
         requested_seal = Json::array();
    Json requested_finance = Json::array(), requested_industry = Json::array();
    std::set<std::string, std::less<>> handled;
    std::set<std::pair<int, std::string>> requested_keys, requested_depth_keys,
        requested_speed_keys, requested_seal_keys;
    std::set<std::pair<int, std::string>> requested_finance_keys, requested_industry_keys;
    bool group_requested = false;

    auto add_binding = [&](const std::string& code, const Json& value,
                           const std::string& source, const std::string& syscol,
                           const std::optional<std::pair<int, std::string>>& security) {
        row[code] = value;
        Json item = Json::object();
        item["code"] = code;
        item["value"] = value;
        item["source"] = source;
        if (!syscol.empty()) item["system_column"] = syscol;
        if (security) item["security"] = security_text(*security);
        bindings.push_back(std::move(item));
    };
    auto add_unresolved = [&](const std::string& code, const std::string& syscol,
                              const std::string& reason,
                              const std::optional<std::pair<int, std::string>>& security) {
        Json item = Json::object();
        item["code"] = code;
        if (!syscol.empty()) item["system_column"] = syscol;
        item["reason"] = reason;
        if (security) item["security"] = security_text(*security);
        unresolved.push_back(std::move(item));
    };

    for (const auto& unit : config.units) {
        for (const auto& column : unit.columns) {
            if (!handled.insert(column.code).second || meaningful(host_optional(row, column.code))) continue;
            std::string syscol = column.syscol;
            if (syscol.empty() && recognized_host_syscol(column.code))
                syscol = column.code;
            if (syscol.empty()) continue;
            if (syscol == "$BONDAI") {
                const auto value = bond_accrued_interest(row, as_of);
                if (value) add_binding(column.code, *value, "row-bond-actual-365", syscol, std::nullopt);
                else add_unresolved(column.code, syscol, "bond schedule fields are incomplete or as-of is outside the coupon period", std::nullopt);
                continue;
            }
            if (group_syscol(syscol)) {
                group_requested = true;
                if (!group_members.source_present) {
                    add_unresolved(column.code, syscol,
                                   "row has no $S_ZQDM member list", std::nullopt);
                    continue;
                }
                if (group_members.values.empty()) {
                    add_unresolved(column.code, syscol,
                                   "$S_ZQDM contains no valid member tokens", std::nullopt);
                    continue;
                }
                if (syscol != "$S_NUM") {
                    for (const auto& member : group_members.values)
                        if (member.public_security) requested_keys.insert(*member.public_security);
                }
                if (syscol == "$S_JQZF") {
                    for (const auto& member : group_members.values)
                        if (member.public_security) requested_finance_keys.insert(*member.public_security);
                }
                if (syscol == "$S_NUM") {
                    add_binding(column.code,
                                static_cast<std::uint64_t>(group_aggregate.member_count),
                                "tbigdata-id48-s-zqdm-member-count", syscol, std::nullopt);
                } else if (quotes.empty()) {
                    add_unresolved(column.code, syscol,
                                   "group members need a supplied public L1 snapshot", std::nullopt);
                } else if (syscol == "$S_AVGZF") {
                    add_binding(column.code, group_aggregate.average_change,
                                "tbigdata-id44-member-change-sum-over-total-count",
                                syscol, std::nullopt);
                } else if (syscol == "$S_JQZF") {
                    if (group_aggregate.has_weighted_change)
                        add_binding(column.code, group_aggregate.weighted_change,
                                    "tbigdata-id45-total-share-weighted-change",
                                    syscol, std::nullopt);
                    else
                        add_unresolved(column.code, syscol,
                                       "no member has both a valid change and supplied total shares",
                                       std::nullopt);
                } else if (syscol == "$S_LTG") {
                    if (group_aggregate.has_leader && !group_aggregate.leader_name.empty())
                        add_binding(column.code, group_aggregate.leader_name,
                                    "tbigdata-id46-maximum-change-security-name",
                                    syscol, std::nullopt);
                    else
                        add_unresolved(column.code, syscol,
                                       "no valid maximum-change member name is available",
                                       std::nullopt);
                } else if (syscol == "$S_MAXZF") {
                    if (group_aggregate.has_leader)
                        add_binding(column.code, group_aggregate.maximum_change,
                                    "tbigdata-id47-maximum-member-change",
                                    syscol, std::nullopt);
                    else
                        add_unresolved(column.code, syscol,
                                       "no member has a native-valid current/pre-close pair",
                                       std::nullopt);
                } else if (syscol == "$S_UPRATE") {
                    add_binding(column.code, group_aggregate.up_rate,
                                "tbigdata-id49-up-count-over-total-count",
                                syscol, std::nullopt);
                }
                continue;
            }
            const auto security = row_security(
                row, column.refzqdm.empty() ? unit.refunit : column.refzqdm);
            if (syscol == "$TDXHY" || syscol == "$TDXHYCODE") {
                if (!security) {
                    add_unresolved(column.code, syscol, "referenced $SC/$ZQDM identity is missing or invalid", std::nullopt);
                    continue;
                }
                requested_industry_keys.insert(*security);
                const auto* block = block_data ? host_industry_block(*block_data, *security) : nullptr;
                if (!block) {
                    add_unresolved(column.code, syscol, "security has no supplied local direct industry assignment", security);
                    continue;
                }
                add_binding(column.code,
                            syscol == "$TDXHYCODE" ? Json(block->block_code) : Json(block->name),
                            "local-tdx-industry-hierarchy", syscol, security);
                continue;
            }
            if (finance_syscol(syscol)) {
                if (!security) {
                    add_unresolved(column.code, syscol, "referenced $SC/$ZQDM identity is missing or invalid", std::nullopt);
                    continue;
                }
                requested_finance_keys.insert(*security);
                if (syscol == "$J_LTSZ" || syscol == "$J_ZSZ" || syscol == "$HSL" ||
                    syscol == "$PE")
                    requested_keys.insert(*security);
                const auto finance_found = finance.find(*security);
                const auto quote_found = quotes.find(*security);
                if (finance_found == finance.end()) {
                    add_unresolved(column.code, syscol, "security is absent from the supplied public finance document", security);
                    continue;
                }
                const auto value = finance_host_value(
                    syscol, *finance_found->second,
                    quote_found == quotes.end() ? nullptr : quote_found->second);
                if (value) add_binding(column.code, value->value, value->source, syscol, security);
                else add_unresolved(column.code, syscol,
                    syscol == "$J_ZSZ"
                        ? "market cap needs a usable A quote and no unresolved B-share component"
                        : "required finance or L1 field is unavailable",
                    security);
                continue;
            }
            if (!quote_syscol(syscol)) {
                add_unresolved(column.code, syscol,
                    "not derivable from the public L1 snapshot (0x054C/0x0547)", security);
                continue;
            }
            if (!security) {
                add_unresolved(column.code, syscol, "referenced $SC/$ZQDM identity is missing or invalid", std::nullopt);
                continue;
            }
            requested_keys.insert(*security);
            if (depth_syscol(syscol)) requested_depth_keys.insert(*security);
            if (speed_syscol(syscol)) requested_speed_keys.insert(*security);
            if (seal_syscol(syscol)) requested_seal_keys.insert(*security);
            const auto found = quotes.find(*security);
            if (found == quotes.end()) {
                add_unresolved(column.code, syscol, "security is absent from the supplied public L1 snapshot", security);
                continue;
            }
            const auto value = quote_host_value(syscol, *found->second);
            if (value) add_binding(column.code, value->value, value->source, syscol, security);
            else add_unresolved(column.code, syscol, "requested L1 field is unavailable", security);
        }
    }

    if (config_references(config, "DQLL2") && !meaningful(host_optional(row, "DQLL2"))) {
        const auto rate = current_coupon_rate(row);
        if (rate) add_binding("DQLL2", *rate, "row-current-coupon-rate", "", std::nullopt);
        else add_unresolved("DQLL2", "", "SYFXLLXL has no current coupon rate", std::nullopt);
    }
    for (const auto& security : requested_keys) requested.push_back(security_text(security));
    for (const auto& security : requested_depth_keys)
        requested_depth.push_back(security_text(security));
    for (const auto& security : requested_speed_keys)
        requested_speed.push_back(security_text(security));
    for (const auto& security : requested_seal_keys)
        requested_seal.push_back(security_text(security));
    for (const auto& security : requested_finance_keys) requested_finance.push_back(security_text(security));
    for (const auto& security : requested_industry_keys) requested_industry.push_back(security_text(security));
    Json result = Json::object();
    result["schema"] = "tdx-tbigdata-host-fields-v1";
    result["source_contract"] = "TdxW BigData_RegisterCallBack plus CFG syscol/refzqdm";
    result["as_of"] = as_of == 0 ? local_yyyymmdd() : as_of;
    result["snapshot_record_count"] = static_cast<std::uint64_t>(snapshot_records(snapshots).size());
    result["finance_record_count"] = static_cast<std::uint64_t>(snapshot_records(finance_document).size());
    result["requested_securities"] = std::move(requested);
    result["requested_depth_securities"] = std::move(requested_depth);
    result["requested_speed_securities"] = std::move(requested_speed);
    result["requested_seal_securities"] = std::move(requested_seal);
    result["requested_finance_securities"] = std::move(requested_finance);
    result["requested_industry_securities"] = std::move(requested_industry);
    if (group_requested) {
        Json group = Json::object();
        group["source_field"] = "$S_ZQDM";
        group["member_count"] = static_cast<std::uint64_t>(group_aggregate.member_count);
        group["public_supported_member_count"] =
            static_cast<std::uint64_t>(group_aggregate.member_count -
                                       group_members.unsupported_market_count);
        group["unsupported_market_member_count"] = group_members.unsupported_market_count;
        group["invalid_token_count"] =
            static_cast<std::uint64_t>(group_members.invalid_tokens.size());
        group["mapped_quote_count"] =
            static_cast<std::uint64_t>(group_aggregate.mapped_quote_count);
        group["valid_change_count"] =
            static_cast<std::uint64_t>(group_aggregate.valid_change_count);
        group["weighted_member_count"] =
            static_cast<std::uint64_t>(group_aggregate.weighted_member_count);
        Json invalid = Json::array();
        for (const auto& token : group_members.invalid_tokens) invalid.push_back(token);
        group["invalid_tokens"] = std::move(invalid);
        result["group_aggregation"] = std::move(group);
    }
    result["bindings"] = std::move(bindings);
    result["unresolved"] = std::move(unresolved);
    result["row"] = std::move(row);
    return result;
}

}  // namespace tdx::cloud_calc_detail
