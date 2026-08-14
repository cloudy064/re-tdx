#include "tdx/investment.hpp"

#include "investment_internal.hpp"

#include "tdx/market.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <string_view>

namespace fs = std::filesystem;

namespace tdx::investment_detail {
namespace {

const Json* field(const Json& value, std::string_view name) {
    if (!value.is_object()) return nullptr;
    const auto found = value.as_object().find(std::string(name));
    return found == value.as_object().end() ? nullptr : &found->second;
}

std::string string_field(const Json& value, std::string_view name) {
    const auto* selected = field(value, name);
    return selected && selected->is_string() ? selected->as_string()
                                                : std::string{};
}

double finite_number(const Json& value, std::string_view name) {
    const auto* selected = field(value, name);
    if (!selected || !selected->is_number() ||
        !std::isfinite(selected->as_number()))
        throw Error("investment quote row has no finite " + std::string(name));
    return selected->as_number();
}

int integer_field(const Json& value, std::string_view name) {
    const auto number = finite_number(value, name);
    if (number != std::floor(number) || number < 0.0 || number > 255.0)
        throw Error("investment quote row has an invalid " + std::string(name));
    return static_cast<int>(number);
}

using QuoteIndex = std::map<std::pair<int, std::string>, const Json*>;

QuoteIndex index_quotes(const Json& document) {
    const auto* records = field(document, "records");
    if (!records || !records->is_array())
        throw Error("investment quote snapshot must contain a records array");
    QuoteIndex result;
    for (const auto& row : records->as_array()) {
        if (!row.is_object())
            throw Error("investment quote snapshot contains a non-object row");
        const auto market = integer_field(row, "market_id");
        const auto code = string_field(row, "code");
        if (code.size() != 6)
            throw Error("investment quote row code must contain six digits");
        if (!result.emplace(std::make_pair(market, code), &row).second)
            throw Error("investment quote snapshot contains a duplicate security");
    }
    return result;
}

double break_even_price(const FeeRule& rule, std::uint64_t quantity,
                        double position_cost) {
    if (!(position_cost > 0.0) || !std::isfinite(position_cost))
        return std::numeric_limits<double>::quiet_NaN();
    auto net_proceeds = [&](double price) {
        const auto fee = estimate_fee(rule, "sell", price, quantity);
        return fee.notional - fee.total_fee;
    };
    double low = 0.0;
    double high = std::max(1.0, position_cost / static_cast<double>(quantity));
    for (int attempt = 0; attempt < 64 && net_proceeds(high) < position_cost;
         ++attempt)
        high *= 2.0;
    if (!std::isfinite(high) || net_proceeds(high) < position_cost)
        return std::numeric_limits<double>::quiet_NaN();
    for (int iteration = 0; iteration < 80; ++iteration) {
        const auto middle = (low + high) / 2.0;
        if (net_proceeds(middle) < position_cost) low = middle;
        else high = middle;
    }
    return high;
}

Json position_document(const HoldingRecord& holding) {
    Json result = Json::object();
    result["quantity"] = holding.quantity;
    result["average_cost"] = holding.average_cost;
    result["position_cost"] =
        static_cast<double>(holding.quantity) * holding.average_cost;
    result["realized_profit"] = holding.realized_profit;
    result["cash_dividends"] = holding.cash_dividends;
    result["fees"] = holding.fees;
    result["transaction_count"] = holding.transaction_count;
    return result;
}

Json security_document(const HoldingRecord& holding, const Json* quote) {
    Json result = Json::object();
    result["market_kind"] = holding.market_kind;
    result["market"] = market_name(holding.market_kind);
    result["code"] = holding.code;
    result["name"] = quote ? string_field(*quote, "name") : std::string{};
    return result;
}

Json quote_document(const Json& quote) {
    Json result = Json::object();
    const auto* last = field(quote, "last_price");
    result["last_price"] = last && last->is_number() &&
            std::isfinite(last->as_number())
        ? Json(last->as_number()) : Json(nullptr);
    const auto* previous = field(quote, "pre_close_price");
    result["pre_close_price"] = previous && previous->is_number() &&
            std::isfinite(previous->as_number())
        ? Json(previous->as_number()) : Json(nullptr);
    const auto* change = field(quote, "change_pct");
    result["change_pct"] = change && change->is_number() &&
            std::isfinite(change->as_number())
        ? Json(change->as_number()) : Json(nullptr);
    result["time_raw"] = field(quote, "time_raw") &&
            field(quote, "time_raw")->is_number()
        ? *field(quote, "time_raw") : Json(nullptr);
    return result;
}

Json quote_source_document(const InvestmentQuery& query,
                           const Json& snapshot,
                           std::size_t requested,
                           std::uint64_t network_requests) {
    Json source = Json::object();
    source["mode"] = query.quote_snapshot_path.empty()
        ? "public-0x054c" : "offline-snapshot";
    source["path"] = query.quote_snapshot_path.empty()
        ? Json(nullptr) : Json(path_utf8(query.quote_snapshot_path));
    source["schema"] = string_field(snapshot, "schema");
    source["generated_at"] = string_field(snapshot, "generated_at");
    source["command"] = string_field(snapshot, "command");
    source["endpoint"] = query.quote_snapshot_path.empty()
        ? Json(string_field(snapshot, "endpoint")) : Json(nullptr);
    source["requested_holding_count"] =
        static_cast<std::uint64_t>(requested);
    source["network_request_batches"] = network_requests;
    if (const auto* transport = field(snapshot, "transport"))
        source["transport"] = *transport;
    return source;
}

}  // namespace

ValuationResult value_investment_ledger(
    const fs::path& root, const InvestmentQuery& query,
    const InvestmentLedger& ledger, const std::vector<FeeRule>& fee_rules) {
    std::vector<std::string> requested;
    for (const auto& [key, holding] : ledger.holdings) {
        (void)key;
        if (holding.quantity <= 0) continue;
        requested.push_back(
            market_name(holding.market_kind) + ":" + holding.code);
    }

    Json snapshot;
    std::uint64_t network_requests = 0;
    if (!requested.empty()) {
        if (query.quote_snapshot_path.empty()) {
            snapshot = fetch_market_snapshot_document(
                root, requested, query.timeout_ms);
            network_requests = static_cast<std::uint64_t>(
                (requested.size() + 79) / 80);
        } else {
            snapshot = Json::parse(read_text_utf8(query.quote_snapshot_path));
        }
    } else {
        snapshot = Json::object();
        snapshot["schema"] = "tdx-market-snapshot-native-v1";
        snapshot["records"] = Json::array();
    }
    const auto quotes = index_quotes(snapshot);

    Json rows = Json::array();
    Json errors = Json::array();
    std::size_t valued_count = 0;
    std::size_t missing_quote_count = 0;
    std::size_t missing_fee_rule_count = 0;
    double active_position_cost = 0.0;
    double covered_position_cost = 0.0;
    double market_value = 0.0;
    double unrealized_profit = 0.0;
    double exit_fees = 0.0;
    double net_liquidation_value = 0.0;
    double unrealized_after_exit_fees = 0.0;
    double realized_profit = 0.0;
    double cash_dividends = 0.0;
    for (const auto& [key, holding] : ledger.holdings) {
        realized_profit += holding.realized_profit;
        cash_dividends += holding.cash_dividends;
        if (holding.quantity <= 0) continue;
        const auto position_cost =
            static_cast<double>(holding.quantity) * holding.average_cost;
        active_position_cost += position_cost;
        const auto found = quotes.find(key);
        const Json* quote = found == quotes.end() ? nullptr : found->second;
        Json row = Json::object();
        row["security"] = security_document(holding, quote);
        row["position"] = position_document(holding);
        if (!quote) {
            ++missing_quote_count;
            row["quote"] = Json(nullptr);
            row["valuation"] = Json(nullptr);
            row["diagnostic"] = "selected security is absent from the quote snapshot";
            errors.push_back(row);
            rows.push_back(std::move(row));
            continue;
        }
        const auto* last_value = field(*quote, "last_price");
        if (!last_value || !last_value->is_number() ||
            !std::isfinite(last_value->as_number()) ||
            !(last_value->as_number() > 0.0)) {
            ++missing_quote_count;
            row["quote"] = quote_document(*quote);
            row["valuation"] = Json(nullptr);
            row["diagnostic"] = "quote has no positive current price";
            errors.push_back(row);
            rows.push_back(std::move(row));
            continue;
        }
        const auto last = last_value->as_number();
        ++valued_count;
        covered_position_cost += position_cost;
        const auto gross = last * static_cast<double>(holding.quantity);
        const auto profit = gross - position_cost;
        market_value += gross;
        unrealized_profit += profit;
        row["quote"] = quote_document(*quote);
        Json valuation = Json::object();
        valuation["market_value"] = gross;
        valuation["unrealized_profit"] = profit;
        valuation["unrealized_return_pct"] = position_cost != 0.0
            ? Json(profit / position_cost * 100.0) : Json(nullptr);
        const auto* previous = field(*quote, "pre_close_price");
        if (previous && previous->is_number() &&
            std::isfinite(previous->as_number()) && previous->as_number() > 0.0) {
            const auto today = (last - previous->as_number()) *
                static_cast<double>(holding.quantity);
            valuation["today_profit"] = today;
            valuation["today_return_pct"] =
                (last / previous->as_number() - 1.0) * 100.0;
        } else {
            valuation["today_profit"] = Json(nullptr);
            valuation["today_return_pct"] = Json(nullptr);
        }
        const auto* rule = select_fee_rule(
            fee_rules, holding.market_kind, holding.code);
        if (rule) {
            const auto quantity = static_cast<std::uint64_t>(holding.quantity);
            const auto fee = estimate_fee(*rule, "sell", last, quantity);
            const auto net = gross - fee.total_fee;
            const auto net_profit = net - position_cost;
            exit_fees += fee.total_fee;
            net_liquidation_value += net;
            unrealized_after_exit_fees += net_profit;
            valuation["estimated_exit_fee"] =
                fee_estimate_document(*rule, "sell", last, quantity);
            valuation["net_liquidation_value"] = net;
            valuation["unrealized_profit_after_exit_fee"] = net_profit;
            const auto break_even = break_even_price(
                *rule, quantity, position_cost);
            valuation["estimated_break_even_price"] =
                std::isfinite(break_even) ? Json(break_even) : Json(nullptr);
        } else {
            ++missing_fee_rule_count;
            valuation["estimated_exit_fee"] = Json(nullptr);
            valuation["net_liquidation_value"] = Json(nullptr);
            valuation["unrealized_profit_after_exit_fee"] = Json(nullptr);
            valuation["estimated_break_even_price"] = Json(nullptr);
        }
        row["valuation"] = std::move(valuation);
        rows.push_back(std::move(row));
    }

    const bool complete = missing_quote_count == 0;
    Json summary = Json::object();
    summary["active_holding_count"] =
        static_cast<std::uint64_t>(requested.size());
    summary["valued_holding_count"] =
        static_cast<std::uint64_t>(valued_count);
    summary["missing_quote_count"] =
        static_cast<std::uint64_t>(missing_quote_count);
    summary["missing_fee_rule_count"] =
        static_cast<std::uint64_t>(missing_fee_rule_count);
    summary["complete"] = complete;
    summary["fee_coverage_complete"] = missing_fee_rule_count == 0;
    summary["active_position_cost"] = active_position_cost;
    summary["covered_position_cost"] = covered_position_cost;
    summary["uncovered_position_cost"] =
        active_position_cost - covered_position_cost;
    summary["covered_market_value"] = market_value;
    summary["covered_unrealized_profit"] = unrealized_profit;
    summary["covered_estimated_exit_fee"] = exit_fees;
    summary["covered_net_liquidation_value"] = net_liquidation_value;
    summary["covered_unrealized_profit_after_exit_fee"] =
        unrealized_after_exit_fees;
    summary["realized_profit"] = realized_profit;
    summary["cash_dividends"] = cash_dividends;
    summary["known_total_profit"] = realized_profit + unrealized_profit;
    summary["total_profit"] = complete
        ? Json(realized_profit + unrealized_profit) : Json(nullptr);
    summary["reconstructed_cash_balance"] = ledger.net_cash_flow;
    summary["covered_total_assets"] = ledger.net_cash_flow + market_value;
    summary["total_assets"] = complete
        ? Json(ledger.net_cash_flow + market_value) : Json(nullptr);

    Json semantics = Json::object();
    semantics["quote"] = "public L1 0x054C snapshot or caller-provided snapshot";
    semantics["market_value"] = "quantity multiplied by current last price";
    semantics["unrealized_profit"] =
        "market value minus dividend-adjusted running position cost";
    semantics["estimated_exit_fee"] =
        "sell fee estimate using the longest-prefix trdpara.dat rule";
    semantics["estimated_break_even_price"] =
        "price whose net sell proceeds cover current running position cost";
    semantics["reconstructed_cash_balance"] =
        "transaction cash inflows minus outflows; complete only when opening cash is represented in the ledger";

    Json document = Json::object();
    document["quote_source"] = quote_source_document(
        query, snapshot, requested.size(), network_requests);
    document["summary"] = std::move(summary);
    document["positions"] = std::move(rows);
    document["errors"] = std::move(errors);
    document["field_semantics"] = std::move(semantics);
    return {std::move(document), network_requests};
}

}  // namespace tdx::investment_detail
