#include "special_situations_internal.hpp"

namespace tdx {
using namespace special_situations_detail;

void apply_special_situation_quotes(Json& rows, const Json& quote_rows) {
    if (!rows.is_array() || !quote_rows.is_array())
        throw Error("special-situation records and quotes must be arrays");
    std::map<std::pair<int, std::string>, Json> quotes;
    for (const auto& quote : quote_rows.as_array()) {
        const auto id = number_value(quote, "market_id");
        const auto code = text_value(quote, "code");
        if (id && digits(code)) quotes[{static_cast<int>(*id), code}] = quote;
    }
    for (auto& row : rows.as_array()) {
        auto primary_quote = quote_for(row.at("primary_security"), quotes);
        auto related_quote = quote_for(row.at("related_security"), quotes);
        row["primary_quote"] = primary_quote;
        row["related_quote"] = related_quote;
        const auto primary_price = primary_quote.is_null()
            ? std::optional<double>{} : quote_price(primary_quote);
        const auto related_price = related_quote.is_null()
            ? std::optional<double>{} : quote_price(related_quote);
        const auto kind = text_value(row, "kind");
        if (kind == "merger") {
            row["absorber_exchange_premium_pct"] = premium(
                primary_price, number_value(row, "absorber_exchange_price"));
            row["cash_option_premium_pct"] = premium(
                related_price, number_value(row, "absorbed_cash_option_price"));
            row["absorbed_exchange_premium_pct"] = premium(
                related_price, number_value(row, "absorbed_exchange_price"));
        } else if (kind == "b-to-h") {
            row["cash_option_premium_pct"] = premium(
                primary_price, number_value(row, "cash_option_price"));
        } else {
            row["live_high_to_current_change_pct"] = premium(
                primary_price, number_value(row, "one_year_high_price"));
        }
    }
}

}  // namespace tdx
