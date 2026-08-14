#include "convertible_bonds_internal.hpp"

#include "tdx/common.hpp"
#include "tdx/jsn.hpp"
#include "tdx/jsn_data.hpp"
#include "tdx/market.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <optional>
#include <set>
#include <sstream>

namespace fs = std::filesystem;

namespace tdx {

using namespace convertible_bond_detail;

Json normalize_convertible_bond_pricing_rows(
    const Json& source_rows, const Json& quote_rows,
    const std::map<std::pair<int, std::string>, Security>& securities,
    const std::string& as_of_date) {
    if (!source_rows.is_array())
        throw Error("convertible-bond pricing rows must be an array");
    const auto as_of = as_of_date.empty() ? today_compact() : [&] {
        auto value = trim(as_of_date);
        value.erase(std::remove(value.begin(), value.end(), '-'), value.end());
        if (!civil_days(value)) throw Error("as_of_date must be YYYYMMDD or YYYY-MM-DD");
        return value;
    }();
    const Json* quotes = &quote_rows;
    if (quote_rows.is_object()) {
        const auto found = quote_rows.as_object().find("records");
        if (found != quote_rows.as_object().end()) quotes = &found->second;
    }
    std::map<std::pair<int, std::string>, const Json*> quote_index;
    if (quotes->is_array()) {
        for (const auto& quote : quotes->as_array()) {
            const auto market = quote_number(quote, "market_id");
            const auto* code = field(quote, "code");
            if (market && code && code->is_string())
                quote_index[{static_cast<int>(*market), code->as_string()}] = &quote;
        }
    }
    const auto quote_for = [&](int market, const std::string& code) -> const Json* {
        const auto found = quote_index.find({market, code});
        return found == quote_index.end() ? nullptr : found->second;
    };
    Json result = Json::array();
    for (const auto& row : source_rows.as_array()) {
        const auto bond_code = value_text(row, "$ZQDM");
        const auto stock_code = value_text(row, "$ZQDM1");
        if (!valid_code(bond_code)) continue;
        int bond_market = -1, stock_market = -1;
        try { bond_market = market_id(value_text(row, "$SC")); }
        catch (...) { continue; }
        try { stock_market = market_id(value_text(row, "$SC1")); }
        catch (...) {}
        const bool has_underlying = stock_market >= 0 && valid_code(stock_code);
        const auto* bond_quote = quote_for(bond_market, bond_code);
        const auto* stock_quote = has_underlying ? quote_for(stock_market, stock_code) : nullptr;
        const auto bond_price = usable_quote_price(bond_quote);
        const auto stock_price = usable_quote_price(stock_quote);
        const auto face = value_number(row, "MZ");
        const auto conversion_price = value_number(row, "ZGJ");
        const auto interest = accrued_interest(row, as_of);
        const auto full_price = bond_price && interest
            ? std::optional<double>(*bond_price + *interest) : std::nullopt;
        const auto conversion_value = stock_price && face && conversion_price &&
            *conversion_price > 0
            ? std::optional<double>(*stock_price * *face / *conversion_price)
            : std::nullopt;
        const auto premium = full_price && conversion_value && *conversion_value > 0
            ? std::optional<double>((*full_price / *conversion_value - 1.0) * 100.0)
            : std::nullopt;
        const auto flows = remaining_cash_flows(row, as_of);
        const auto ytm = full_price ? solve_ytm(flows, *full_price) : std::nullopt;
        const auto curve = value_number(row, "SYNXSYL");
        const auto pure_value = curve ? discounted_value(flows, *curve) : std::nullopt;
        const auto pure_premium = full_price && pure_value && *pure_value > 0
            ? std::optional<double>((*full_price / *pure_value - 1.0) * 100.0)
            : std::nullopt;

        Json item = Json::object();
        item["bond"] = security_document(bond_market, bond_code,
            value_text(row, "ZQJC"), securities);
        item["underlying"] = has_underlying
            ? security_document(stock_market, stock_code, value_text(row, "ZGSM"), securities)
            : Json(nullptr);
        item["instrument_type"] = value_text(row, "ZQLX").find("交换") != std::string::npos
            ? "exchangeable-bond" : "convertible-bond";
        item["active"] = !value_text(row, "DQRQ").empty() &&
            value_text(row, "DQRQ") >= as_of;

        Json terms = Json::object();
        terms["face_value"] = optional_number(face);
        terms["return_since_listing_pct"] = number(row, "ZF1");
        terms["return_5d_pct"] = number(row, "ZF2");
        terms["return_10d_pct"] = number(row, "ZF3");
        terms["conversion_price"] = optional_number(conversion_price);
        terms["conversion_start_date"] = value_text(row, "ZGQSR");
        terms["conversion_end_date"] = value_text(row, "ZGJZR");
        terms["listing_date"] = value_text(row, "SSRQ");
        terms["interest_start_date"] = value_text(row, "QXRQ");
        terms["maturity_date"] = value_text(row, "DQRQ");
        terms["remaining_years"] = number(row, "SYNX");
        terms["bond_rating"] = value_text(row, "ZQPJ");
        terms["issuer_rating"] = value_text(row, "ZTPJ");
        terms["bond_type"] = value_text(row, "ZQLX");
        terms["rate_type"] = value_text(row, "LLLX");
        terms["rate_type_code"] = value_text(row, "LLLXBZ");
        terms["payment_dates"] = text_array(value_text(row, "FXRQXL"));
        terms["payment_rates"] = numeric_array(value_text(row, "FXLLXL"));
        terms["remaining_payment_count"] = number(row, "SYFXCS");
        terms["remaining_payment_dates"] = text_array(value_text(row, "SYFXRQXL"));
        terms["remaining_payment_rates"] = numeric_array(value_text(row, "SYFXLLXL"));
        terms["previous_payment_date"] = value_text(row, "SGFXRQ");
        terms["next_payment_date"] = value_text(row, "XGFXRQ");
        terms["payment_frequency_months"] = number(row, "FXPL1");
        terms["remaining_balance_pct"] = number(row, "YEZB");
        terms["revision_trigger_ratio_pct"] = number(row, "XXCFBL");
        terms["sellback_trigger_ratio_pct"] = number(row, "HSCFBL");
        terms["redemption_trigger_ratio_pct"] = number(row, "QSCFBL");
        terms["revision_trigger_price"] = conversion_price
            ? optional_number(value_number(row, "XXCFBL").has_value()
                ? std::optional<double>(*conversion_price *
                    *value_number(row, "XXCFBL") / 100.0) : std::nullopt)
            : Json(nullptr);
        terms["sellback_trigger_price"] = conversion_price
            ? optional_number(value_number(row, "HSCFBL").has_value()
                ? std::optional<double>(*conversion_price *
                    *value_number(row, "HSCFBL") / 100.0) : std::nullopt)
            : Json(nullptr);
        terms["redemption_trigger_price"] = conversion_price
            ? optional_number(value_number(row, "QSCFBL").has_value()
                ? std::optional<double>(*conversion_price *
                    *value_number(row, "QSCFBL") / 100.0) : std::nullopt)
            : Json(nullptr);
        terms["curve_short_years"] = number(row, "ZSQX1");
        terms["curve_short_yield_pct"] = value_number(row, "ZSSYL1")
            ? Json(*value_number(row, "ZSSYL1")) : Json(nullptr);
        terms["curve_long_years"] = number(row, "ZSQX2");
        terms["curve_long_yield_pct"] = value_number(row, "ZSSYL2")
            ? Json(*value_number(row, "ZSSYL2")) : Json(nullptr);
        terms["interpolated_curve_yield_pct"] = curve
            ? Json(*curve * 100.0) : Json(nullptr);
        item["terms"] = std::move(terms);

        Json quote = Json::object();
        quote["bond_last_price"] = optional_number(bond_price);
        quote["bond_price_source"] = quote_price_source(bond_quote);
        quote["bond_change_pct"] = bond_quote
            ? optional_number(quote_number(*bond_quote, "change_pct")) : Json(nullptr);
        quote["bond_amount_yuan"] = bond_quote
            ? optional_number(quote_number(*bond_quote, "amount")) : Json(nullptr);
        quote["underlying_last_price"] = optional_number(stock_price);
        quote["underlying_price_source"] = quote_price_source(stock_quote);
        quote["underlying_change_pct"] = stock_quote
            ? optional_number(quote_number(*stock_quote, "change_pct")) : Json(nullptr);
        quote["underlying_amount_yuan"] = stock_quote
            ? optional_number(quote_number(*stock_quote, "amount")) : Json(nullptr);
        quote["bond_available"] = bond_price.has_value();
        quote["underlying_available"] = stock_price.has_value();
        item["quote"] = std::move(quote);

        Json valuation = Json::object();
        valuation["as_of_date"] = as_of;
        valuation["accrued_interest"] = optional_number(interest);
        valuation["accrued_interest_source"] = interest
            ? "coupon-schedule-derived-actual-365" : "unavailable";
        valuation["full_price"] = optional_number(full_price);
        valuation["conversion_value"] = optional_number(conversion_value);
        valuation["conversion_premium_pct"] = optional_number(premium);
        valuation["maturity_yield_pct"] = ytm
            ? Json(*ytm * 100.0) : Json(nullptr);
        valuation["pure_bond_value"] = optional_number(pure_value);
        valuation["pure_bond_premium_pct"] = optional_number(pure_premium);
        valuation["double_low_score"] = bond_price && premium
            ? Json(*bond_price + *premium) : Json(nullptr);
        valuation["cash_flow_count"] = static_cast<std::uint64_t>(flows.size());
        valuation["availability"] = full_price && conversion_value
            ? "complete" : bond_price ? "bond-only" : "terms-only";
        valuation["calculation_note"] =
            "full price uses public L1 clean price plus schedule-derived accrued interest; YTM and pure-bond value are reconstructed locally from disclosed cash flows";
        item["valuation"] = std::move(valuation);
        item["raw"] = row;
        result.push_back(std::move(item));
    }
    return result;
}

void sort_convertible_bond_pricing_rows(Json& rows,
                                        const std::string& sort_value,
                                        const std::string& order_value) {
    if (!rows.is_array()) throw Error("convertible-bond pricing rows must be an array");
    const auto sort = lower_ascii(trim(sort_value.empty() ? "double-low" : sort_value));
    const auto order = lower_ascii(trim(order_value.empty() ? "asc" : order_value));
    const std::map<std::string, std::pair<std::string, std::string>> fields{
        {"double-low", {"valuation", "double_low_score"}},
        {"premium", {"valuation", "conversion_premium_pct"}},
        {"ytm", {"valuation", "maturity_yield_pct"}},
        {"pure-bond", {"valuation", "pure_bond_value"}},
        {"price", {"quote", "bond_last_price"}},
        {"remaining-years", {"terms", "remaining_years"}},
        {"return", {"terms", "return_since_listing_pct"}}};
    const auto selected = fields.find(sort);
    if (selected == fields.end())
        throw Error("pricing sort must be double-low, premium, ytm, pure-bond, price, remaining-years, or return");
    if (order != "asc" && order != "desc") throw Error("order must be asc or desc");
    const bool descending = order == "desc";
    std::stable_sort(rows.as_array().begin(), rows.as_array().end(),
        [&](const Json& left, const Json& right) {
            const auto& l = left.at(selected->second.first).at(selected->second.second);
            const auto& r = right.at(selected->second.first).at(selected->second.second);
            const bool lm = l.is_null(), rm = r.is_null();
            if (lm != rm) return !lm;
            if (lm) return left.at("bond").at("code").as_string() <
                right.at("bond").at("code").as_string();
            return descending ? l.as_number() > r.as_number()
                              : l.as_number() < r.as_number();
        });
}

}  // namespace tdx
