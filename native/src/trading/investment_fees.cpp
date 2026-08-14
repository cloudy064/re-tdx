#include "investment_internal.hpp"

#include <algorithm>
#include <cmath>

namespace tdx::investment_detail {

const FeeRule* select_fee_rule(const std::vector<FeeRule>& rules,
                               int market_kind,
                               const std::string& code) {
    const FeeRule* selected = nullptr;
    std::size_t longest = 0;
    for (const auto& rule : rules) {
        if (rule.market_kind != market_kind ||
            code.compare(0, rule.prefix.size(), rule.prefix) != 0)
            continue;
        if (!selected || rule.prefix.size() > longest) {
            selected = &rule;
            longest = rule.prefix.size();
        }
    }
    return selected;
}

FeeEstimate estimate_fee(const FeeRule& rule, std::string_view raw_side,
                         double price, std::uint64_t quantity) {
    const auto side = lower_ascii(trim(std::string(raw_side)));
    if (side != "buy" && side != "sell")
        throw Error("fee estimate side must be buy or sell");
    if (!std::isfinite(price) || price <= 0.0 || quantity == 0)
        throw Error("fee estimate requires a positive price and quantity");
    FeeEstimate result;
    result.notional = price * static_cast<double>(quantity);
    if (!std::isfinite(result.notional))
        throw Error("fee estimate notional is not finite");
    result.commission = std::max(
        result.notional * rule.commission_rate, rule.minimum_commission);
    result.stamp_tax = side == "sell"
        ? result.notional * rule.stamp_tax_rate : 0.0;
    result.transfer_fee = std::max(
        result.notional * rule.transfer_fee_rate,
        rule.minimum_transfer_fee);
    result.fixed_fee = rule.fixed_fee;
    result.total_fee = result.commission + result.stamp_tax +
        result.transfer_fee + result.fixed_fee;
    return result;
}

Json fee_estimate_document(const FeeRule& rule, std::string_view side,
                           double price, std::uint64_t quantity) {
    const auto estimate = estimate_fee(rule, side, price, quantity);
    Json result = Json::object();
    result["side"] = lower_ascii(trim(std::string(side)));
    result["price"] = price;
    result["quantity"] = quantity;
    result["notional"] = estimate.notional;
    result["commission"] = estimate.commission;
    result["stamp_tax"] = estimate.stamp_tax;
    result["transfer_fee"] = estimate.transfer_fee;
    result["fixed_fee"] = estimate.fixed_fee;
    result["total_fee"] = estimate.total_fee;
    result["formula_source"] = "invest.dll:sub_1000D390";
    return result;
}

}  // namespace tdx::investment_detail
