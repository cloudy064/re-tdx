#include "tdx/investment.hpp"

#include "investment_internal.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>

namespace fs = std::filesystem;

namespace tdx {
namespace {

Json path_document(const fs::path& path, const std::string& source,
                   bool directory) {
    std::error_code error;
    Json result = Json::object();
    result["path"] = path_utf8(path);
    result["source"] = source;
    result["available"] = directory
        ? fs::is_directory(path, error)
        : investment_detail::regular_file_available(path);
    return result;
}

Json fee_rule_document(const investment_detail::FeeRule& rule) {
    Json result = Json::object();
    result["record_index"] = static_cast<std::uint64_t>(rule.record_index);
    result["market_kind"] = rule.market_kind;
    result["market"] = investment_detail::market_name(rule.market_kind);
    result["prefix"] = rule.prefix;
    result["market_name"] = rule.market_name;
    result["commission_rate"] = rule.commission_rate;
    result["minimum_commission"] = rule.minimum_commission;
    result["stamp_tax_rate"] = rule.stamp_tax_rate;
    result["transfer_fee_rate"] = rule.transfer_fee_rate;
    result["minimum_transfer_fee"] = rule.minimum_transfer_fee;
    result["fixed_fee"] = rule.fixed_fee;
    return result;
}

int parse_market_kind(const std::string& market) {
    const auto value = lower_ascii(trim(market));
    if (value == "sz" || value == "0") return 0;
    if (value == "sh" || value == "1") return 1;
    if (value == "bj" || value == "2") return 2;
    throw Error("market must be sz/sh/bj or 0/1/2");
}

bool six_digits(const std::string& code) {
    return code.size() == 6 &&
        std::all_of(code.begin(), code.end(), [](char ch) {
            return ch >= '0' && ch <= '9';
        });
}

Json portfolio_document(const investment_detail::PortfolioRecord& portfolio,
                        const investment_detail::PortfolioFileInfo& file) {
    Json result = Json::object();
    result["name"] = portfolio.name;
    result["password_protected"] = !portfolio.password.empty();
    result["detail_lookup_allowed"] = file.lookup_allowed;
    result["detail_path"] = file.path.empty()
        ? Json(nullptr) : Json(path_utf8(file.path));
    result["detail_available"] = file.available;
    result["valid_record_size"] = file.valid_record_size;
    result["byte_size"] = static_cast<std::uint64_t>(file.byte_size);
    result["record_count"] = static_cast<std::uint64_t>(file.record_count);
    result["transaction_count"] =
        static_cast<std::uint64_t>(file.transaction_count);
    result["header_password_readable"] = file.header_password_readable;
    result["header_password_matches"] = file.header_password_matches;
    if (!file.diagnostic.empty()) result["diagnostic"] = file.diagnostic;
    return result;
}

std::string transaction_date(int value) {
    const auto digits = std::to_string(value);
    return digits.substr(0, 4) + "-" + digits.substr(4, 2) + "-" +
           digits.substr(6, 2);
}

Json transaction_document(
    const investment_detail::TransactionRecord& transaction,
    bool include_notes) {
    Json result = Json::object();
    result["file_record_index"] =
        static_cast<std::uint64_t>(transaction.file_record_index);
    result["date_code"] = transaction.date;
    result["date"] = transaction_date(transaction.date);
    result["type_code"] = transaction.type_code;
    result["type"] = investment_detail::transaction_type_name(
        transaction.type_code);
    result["type_label"] = investment_detail::transaction_type_label(
        transaction.type_code);
    result["cash_direction"] = transaction.cash_direction;
    result["cash_direction_name"] = transaction.cash_direction < 0
        ? "outflow" : transaction.cash_direction > 0 ? "inflow" : "none";
    result["quantity"] = transaction.quantity;
    result["unit_price"] = transaction.unit_price;
    result["fee"] = transaction.fee;
    result["recorded_total"] = transaction.recorded_total;
    if (investment_detail::transaction_has_security(transaction.type_code)) {
        Json security = Json::object();
        security["market_kind"] = transaction.market_kind;
        security["market"] = investment_detail::market_name(
            transaction.market_kind);
        security["code"] = transaction.code;
        result["security"] = std::move(security);
    } else {
        result["security"] = Json(nullptr);
    }
    result["note_present"] = transaction.note_present;
    if (include_notes) result["note"] = transaction.note;
    result["unparsed_bytes_present"] =
        transaction.unparsed_bytes_present;
    return result;
}

Json holding_document(const investment_detail::HoldingRecord& holding) {
    Json result = Json::object();
    Json security = Json::object();
    security["market_kind"] = holding.market_kind;
    security["market"] = investment_detail::market_name(holding.market_kind);
    security["code"] = holding.code;
    result["security"] = std::move(security);
    result["quantity"] = holding.quantity;
    result["closed"] = holding.quantity == 0;
    result["average_cost"] = holding.average_cost;
    result["position_cost"] =
        static_cast<double>(holding.quantity) * holding.average_cost;
    result["realized_profit"] = holding.realized_profit;
    result["cash_dividends"] = holding.cash_dividends;
    result["fees"] = holding.fees;
    result["transaction_count"] = holding.transaction_count;
    result["first_date"] = transaction_date(holding.first_date);
    result["last_date"] = transaction_date(holding.last_date);
    Json quantities = Json::object();
    quantities["buy"] = holding.buy_quantity;
    quantities["sell"] = holding.sell_quantity;
    quantities["bonus_or_derivative"] = holding.bonus_quantity;
    quantities["transfer_in"] = holding.transfer_in_quantity;
    quantities["transfer_out"] = holding.transfer_out_quantity;
    result["quantity_breakdown"] = std::move(quantities);
    return result;
}

void validate_quote(const InvestmentQuery& query, bool has_security) {
    const bool any_quote = query.price.has_value() || query.quantity.has_value() ||
                           !query.side.empty();
    const bool full_quote = query.price.has_value() && query.quantity.has_value() &&
                            !query.side.empty();
    if (any_quote && !full_quote)
        throw Error("--price, --quantity and --side must be supplied together");
    if (any_quote && !has_security)
        throw Error("a fee quote requires --market and --code");
    if (!any_quote) return;
    if (!std::isfinite(*query.price) || *query.price <= 0.0)
        throw Error("price must be a positive finite number");
    if (*query.quantity == 0)
        throw Error("quantity must be positive");
    const auto side = lower_ascii(trim(query.side));
    if (side != "buy" && side != "sell")
        throw Error("side must be buy or sell");
}

}  // namespace

Json load_local_investment(const fs::path& root,
                           const InvestmentQuery& input_query) {
    InvestmentQuery query = input_query;
    query.view = lower_ascii(trim(query.view));
    query.market = lower_ascii(trim(query.market));
    query.code = trim(query.code);
    query.side = lower_ascii(trim(query.side));
    query.portfolio_name = trim(query.portfolio_name);
    if (query.view != "summary" && query.view != "portfolios" &&
        query.view != "transactions" && query.view != "holdings" &&
        query.view != "valuation" && query.view != "fee-rules")
        throw Error("view must be summary, portfolios, transactions, holdings, valuation or fee-rules");
    const bool transaction_view = query.view == "transactions";
    const bool holdings_view = query.view == "holdings";
    const bool valuation_view = query.view == "valuation";
    const bool private_view = transaction_view || holdings_view || valuation_view;
    if (private_view && query.portfolio_name.empty())
        throw Error("transactions/holdings/valuation view requires --portfolio");
    if (!private_view && !query.portfolio_name.empty())
        throw Error("--portfolio requires transactions, holdings or valuation view");
    if (!transaction_view && (query.include_notes || query.offset != 0 ||
                              query.limit != 1000))
        throw Error("--include-notes/--offset/--limit require transactions view");
    if (!holdings_view && query.include_closed)
        throw Error("--include-closed requires holdings view");
    if (!valuation_view && !query.quote_snapshot_path.empty())
        throw Error("--quotes requires valuation view");
    if (valuation_view && (query.timeout_ms < 1 || query.timeout_ms > 600000))
        throw Error("valuation timeout must be in 1..600000");
    const bool has_market = !query.market.empty();
    const bool has_code = !query.code.empty();
    if (has_market != has_code)
        throw Error("--market and --code must be supplied together");
    if (has_code && !six_digits(query.code))
        throw Error("code must contain exactly six digits");
    const bool has_security = has_market && has_code;
    validate_quote(query, has_security);
    if (private_view && (has_security || query.price.has_value()))
        throw Error("private portfolio views cannot be combined with fee-rule selection");

    const auto paths = investment_detail::resolve_paths(
        root, query.private_directory, query.fee_rules_path);
    const bool need_portfolios = query.view != "fee-rules";
    const bool need_fee_rules = query.view == "summary" ||
        query.view == "fee-rules" || valuation_view || has_security;
    const auto portfolios = need_portfolios
        ? investment_detail::parse_portfolio_index(paths.portfolio_index)
        : std::vector<investment_detail::PortfolioRecord>{};
    const auto fee_rules = need_fee_rules
        ? investment_detail::parse_fee_rules(paths.fee_rules)
        : std::vector<investment_detail::FeeRule>{};

    Json portfolio_rows = Json::array();
    std::size_t available_details = 0;
    std::size_t valid_details = 0;
    std::size_t transactions = 0;
    const investment_detail::PortfolioRecord* selected_portfolio = nullptr;
    std::optional<investment_detail::PortfolioFileInfo> selected_file;
    if (need_portfolios) {
        for (const auto& portfolio : portfolios) {
            const auto file = investment_detail::inspect_portfolio_file(
                paths.private_directory, portfolio);
            if (file.available) ++available_details;
            if (file.valid_record_size && file.header_password_readable &&
                file.header_password_matches)
                ++valid_details;
            transactions += file.transaction_count;
            portfolio_rows.push_back(portfolio_document(portfolio, file));
            if (private_view && portfolio.name == query.portfolio_name) {
                if (selected_portfolio)
                    throw Error("portfolio index contains duplicate selected names");
                selected_portfolio = &portfolio;
                selected_file = file;
            }
        }
    }
    if (private_view && !selected_portfolio)
        throw Error("selected portfolio was not found");

    Json fee_rows = Json::array();
    for (const auto& rule : fee_rules)
        fee_rows.push_back(fee_rule_document(rule));

    std::optional<int> requested_market;
    const investment_detail::FeeRule* selected_rule = nullptr;
    if (has_security) {
        requested_market = parse_market_kind(query.market);
        selected_rule = investment_detail::select_fee_rule(
            fee_rules, *requested_market, query.code);
    }

    Json document = Json::object();
    document["schema"] = "tdx-investment-portfolio-v1";
    document["view"] = query.view;
    document["native_cpp"] = true;
    document["network_requests"] = 0;
    Json sources = Json::object();
    sources["private_directory"] = path_document(
        paths.private_directory, paths.private_directory_source, true);
    sources["portfolio_index"] = path_document(
        paths.portfolio_index, paths.private_directory_source, false);
    sources["fee_rules"] = path_document(
        paths.fee_rules, paths.fee_rules_source, false);
    sources["portfolio_record_size"] =
        static_cast<std::uint64_t>(investment_detail::kPortfolioRecordSize);
    sources["transaction_record_size"] =
        static_cast<std::uint64_t>(investment_detail::kTransactionRecordSize);
    sources["fee_rule_record_size"] =
        static_cast<std::uint64_t>(investment_detail::kFeeRuleRecordSize);
    sources["cipher"] = "Blowfish-ECB, little-endian words";
    sources["key_source"] = "invest.dll fixed literal";
    document["sources"] = std::move(sources);

    Json summary = Json::object();
    summary["portfolio_count"] = static_cast<std::uint64_t>(portfolios.size());
    summary["available_detail_files"] =
        static_cast<std::uint64_t>(available_details);
    summary["validated_detail_files"] =
        static_cast<std::uint64_t>(valid_details);
    summary["transaction_record_count"] =
        static_cast<std::uint64_t>(transactions);
    summary["fee_rule_count"] = static_cast<std::uint64_t>(fee_rules.size());
    summary["selected_rule_found"] = selected_rule != nullptr;
    document["summary"] = std::move(summary);

    if (need_portfolios && !private_view)
        document["portfolios"] = std::move(portfolio_rows);
    if (query.view == "summary" || query.view == "fee-rules" || has_security)
        document["fee_rules"] = std::move(fee_rows);

    if (has_security) {
        Json selection = Json::object();
        selection["market"] = investment_detail::market_name(*requested_market);
        selection["market_kind"] = *requested_market;
        selection["code"] = query.code;
        selection["rule"] = selected_rule
            ? fee_rule_document(*selected_rule) : Json(nullptr);
        document["selection"] = std::move(selection);
    }
    const bool has_quote = query.price.has_value();
    if (has_quote) {
        if (!selected_rule)
            throw Error("no trdpara.dat fee rule matches the selected security");
        document["fee_quote"] = investment_detail::fee_estimate_document(
            *selected_rule, query.side, *query.price, *query.quantity);
    }

    if (private_view) {
        if (!selected_file->available)
            throw Error("selected portfolio detail file is absent");
        if (!selected_file->valid_record_size)
            throw Error("selected portfolio detail file has invalid size");
        if (!selected_file->header_password_readable ||
            !selected_file->header_password_matches)
            throw Error("selected portfolio detail header does not match pinfo.dat");
        document["selected_portfolio"] = portfolio_document(
            *selected_portfolio, *selected_file);
    }
    if (transaction_view) {
        const auto records = investment_detail::parse_transaction_records(
            selected_file->path, *selected_portfolio, query.include_notes,
            query.offset, query.limit);
        Json rows = Json::array();
        for (const auto& record : records)
            rows.push_back(transaction_document(record, query.include_notes));
        document["transactions"] = std::move(rows);
        Json pagination = Json::object();
        pagination["offset"] = static_cast<std::uint64_t>(query.offset);
        pagination["limit"] = static_cast<std::uint64_t>(query.limit);
        pagination["returned"] = static_cast<std::uint64_t>(records.size());
        pagination["total"] = static_cast<std::uint64_t>(
            selected_file->transaction_count);
        pagination["has_more"] = query.offset + records.size() <
            selected_file->transaction_count;
        document["pagination"] = std::move(pagination);
        Json semantics = Json::object();
        semantics["recorded_total"] =
            "native per-type total; cash effect is determined by cash_direction";
        semantics["cash_direction"] = "-1 outflow, 0 no cash movement, 1 inflow";
        semantics["unparsed_bytes_present"] =
            "true when reserved bytes 127..146 or 175..199 are nonzero; bytes are not emitted";
        document["transaction_field_semantics"] = std::move(semantics);
    }
    if (holdings_view || valuation_view) {
        investment_detail::InvestmentLedger ledger;
        constexpr std::size_t page_size = 10000;
        for (std::size_t offset = 0;
             offset < selected_file->transaction_count; offset += page_size) {
            const auto page = investment_detail::parse_transaction_records(
                selected_file->path, *selected_portfolio, false,
                offset, page_size);
            investment_detail::apply_transaction_page(ledger, page);
        }
        std::size_t active = 0;
        std::size_t closed = 0;
        for (const auto& [key, holding] : ledger.holdings) {
            (void)key;
            if (holding.quantity == 0) {
                ++closed;
            } else {
                ++active;
            }
        }
        Json ledger_summary = Json::object();
        ledger_summary["transaction_count"] = ledger.transaction_count;
        ledger_summary["security_transaction_count"] =
            ledger.security_transaction_count;
        ledger_summary["unknown_type_count"] = ledger.unknown_type_count;
        ledger_summary["active_holding_count"] =
            static_cast<std::uint64_t>(active);
        ledger_summary["closed_holding_count"] =
            static_cast<std::uint64_t>(closed);
        if (holdings_view) {
            Json rows = Json::array();
            for (const auto& [key, holding] : ledger.holdings) {
                (void)key;
                if (holding.quantity == 0 && !query.include_closed) continue;
                rows.push_back(holding_document(holding));
            }
            document["holdings"] = std::move(rows);
            ledger_summary["returned_holding_count"] =
                static_cast<std::uint64_t>(
                    document.at("holdings").as_array().size());
        } else {
            ledger_summary["returned_holding_count"] =
                static_cast<std::uint64_t>(active);
        }
        document["ledger_summary"] = std::move(ledger_summary);
        Json cash = Json::object();
        cash["inflows"] = ledger.cash_inflows;
        cash["outflows"] = ledger.cash_outflows;
        cash["net_flow"] = ledger.net_cash_flow;
        cash["opening_balance_included"] =
            "only when represented by a cash-deposit transaction";
        document["cash_ledger"] = std::move(cash);
        if (holdings_view) {
            Json semantics = Json::object();
            semantics["algorithm"] =
                "invest.dll sub_10013B90/sub_10015230 chronological replay";
            semantics["average_cost"] =
                "buys/rights/transfers-in add price*quantity+fee; dividends reduce cost; bonus shares dilute cost";
            semantics["realized_profit"] =
                "native sell recorded_total minus quantity*running_average_cost; transfers-out do not realize profit";
            semantics["market_value"] =
                "use valuation view with a live or offline quote snapshot";
            document["holding_field_semantics"] = std::move(semantics);
        } else {
            auto valuation = investment_detail::value_investment_ledger(
                root, query, ledger, fee_rules);
            document["valuation"] = std::move(valuation.document);
            document["network_requests"] = valuation.network_requests;
        }
    }

    Json privacy = Json::object();
    privacy["password_values_emitted"] = false;
    privacy["opaque_pinfo_metadata_emitted"] = false;
    privacy["transaction_payload_decoded"] = private_view;
    privacy["transaction_notes_emitted"] =
        transaction_view && query.include_notes;
    privacy["unknown_transaction_bytes_emitted"] = false;
    privacy["scope"] = "local CLI output only; no HTTP route";
    document["privacy"] = std::move(privacy);
    return document;
}

}  // namespace tdx
