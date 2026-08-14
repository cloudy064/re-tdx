#include "cloud_calc_host_internal.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

namespace tdx::cloud_calc_detail {

bool finance_syscol(std::string_view value) {
    return value == "$J_LTGB" || value == "$J_LTSZ" || value == "$J_ZSZ" ||
           value == "$J_ZGB" || value == "$HSL" || value == "$PE";
}

bool group_syscol(std::string_view value) {
    return value == "$S_AVGZF" || value == "$S_JQZF" ||
           value == "$S_LTG" || value == "$S_MAXZF" ||
           value == "$S_NUM" || value == "$S_UPRATE";
}

GroupMembers parse_group_members(const Json& row) {
    GroupMembers result;
    const auto* source = host_optional(row, "$S_ZQDM");
    if (!source || !meaningful(source)) return result;
    result.source_present = true;
    for (auto token : split(scalar_text(*source), ',')) {
        token = trim(std::move(token));
        if (token.empty()) continue;
        const auto bar = token.find('|');
        if (bar == std::string::npos) {
            result.invalid_tokens.push_back(std::move(token));
            continue;
        }
        auto market_text = trim(token.substr(0, bar));
        auto code = trim(token.substr(bar + 1));
        double market_number = 0.0;
        if (!parse_double(market_text, market_number) ||
            std::floor(market_number) != market_number || market_number < 0.0 ||
            market_number > 255.0 || code.size() != 6 ||
            !std::all_of(code.begin(), code.end(), [](char ch) {
                return std::isdigit(static_cast<unsigned char>(ch));
            })) {
            result.invalid_tokens.push_back(std::move(token));
            continue;
        }
        const int source_market = static_cast<int>(market_number);
        GroupMember member;
        member.token = token;
        int public_market = source_market;
        if (public_market == 44) public_market = 2;
        if (public_market >= 0 && public_market <= 2)
            member.public_security = std::make_pair(public_market, std::move(code));
        else
            ++result.unsupported_market_count;
        result.values.push_back(std::move(member));
    }
    return result;
}

std::optional<double> group_change_pct(const Json& quote) {
    const auto current = quote_number(quote, "last_price");
    const auto previous = quote_number(quote, "pre_close_price");
    if (!current || !previous || *current < 0.0001 || *previous < 0.0001)
        return std::nullopt;
    return (*current - *previous) * 100.0 / *previous;
}

GroupAggregate aggregate_group(const GroupMembers& members, const QuoteIndex& quotes,
                               const FinanceIndex& finance) {
    GroupAggregate result;
    result.member_count = members.values.size();
    result.maximum_change = -11.0;  // TBigData!sub_100BCAC0 native sentinel.
    double change_sum = 0.0;
    double weighted_sum = 0.0;
    double total_share_sum = 0.0;
    std::size_t up_count = 0;
    for (const auto& member : members.values) {
        if (!member.public_security) continue;
        const auto quote_found = quotes.find(*member.public_security);
        if (quote_found == quotes.end()) continue;
        ++result.mapped_quote_count;
        const auto change = group_change_pct(*quote_found->second);
        if (!change) continue;
        ++result.valid_change_count;
        change_sum += *change;
        const auto current = quote_number(*quote_found->second, "last_price");
        const auto previous = quote_number(*quote_found->second, "pre_close_price");
        if (current && previous && *current > *previous) ++up_count;
        if (*change > result.maximum_change) {
            result.maximum_change = *change;
            result.has_leader = true;
            const auto* name = host_optional(*quote_found->second, "name");
            result.leader_name = meaningful(name) ? scalar_text(*name) : "";
        }
        const auto finance_found = finance.find(*member.public_security);
        if (finance_found != finance.end()) {
            const auto total_shares = nested_number(*finance_found->second, {"shares", "total"});
            if (total_shares && *total_shares > 0.0) {
                weighted_sum += *total_shares * *change;
                total_share_sum += *total_shares;
                ++result.weighted_member_count;
            }
        }
    }
    if (result.member_count) {
        // Native code divides by the original member count. Missing/unmapped
        // quotes therefore contribute zero rather than reducing the divisor.
        result.average_change = change_sum / static_cast<double>(result.member_count);
        result.up_rate = static_cast<double>(up_count) * 100.0 /
                         static_cast<double>(result.member_count);
    }
    if (total_share_sum > 0.0) {
        result.weighted_change = weighted_sum / total_share_sum;
        result.has_weighted_change = true;
    }
    return result;
}

bool known_tbigdata_syscol(std::string_view value) {
    static constexpr std::string_view fields[] = {
        "$ZQJC", "$CLOSE", "$OPEN", "$MAX", "$MIN", "$NOW", "$NOW2", "$NOW3",
        "$CJL", "$NOWV", "$QRSD", "$ZAF", "$ZEF", "$ZS", "$J_LTGB", "$J_LTSZ",
        "$J_ZSZ", "$J_ZGB", "$ZCJJE", "$INP", "$OUTP", "$HSL", "$CJJJ", "$BP1",
        "$SP1", "$BPV1", "$SPV1", "$UPN", "$DOWNN", "$TDXHY", "$TDXHYCODE",
        "$SWHY", "$SWHYCODE", "$FCAMO", "$FCB", "$JJJZ", "$BONDAI", "$ZTGPNUM",
        "$S_AVGZF", "$S_JQZF", "$S_LTG", "$S_MAXZF", "$S_NUM", "$S_UPRATE",
    };
    for (const auto field : fields) if (value == field) return true;
    return false;
}

bool recognized_host_syscol(std::string_view value) {
    return known_tbigdata_syscol(value) || value == "$PE";
}

}  // namespace tdx::cloud_calc_detail
