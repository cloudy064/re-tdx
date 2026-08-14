#include "equity_valuation_internal.hpp"

#include <cmath>
#include <iomanip>
#include <sstream>

namespace tdx::detail::equity_valuation {

EquityValuationQuery normalize_query_options(
    const EquityValuationQuery& input, const ViewSpec& view, int& pe_type) {
    auto options = input;
    options.view = view.id;
    pe_type = pe_type_value(options.pe_type);
    options.pe_type = pe_type_name(pe_type);
    options.end_date = compact_date(
        options.end_date.empty() ? today_text() : options.end_date, "end_date");
    options.start_date = compact_date(
        options.start_date.empty() ? years_before(options.end_date, 3) : options.start_date,
        "start_date");
    if (options.start_date > options.end_date)
        throw Error("start_date must not exceed end_date");

    if (view.history) {
        const int market = security_market(options.market);
        options.market = market_name(market);
        if (options.code.size() != 6 || !ascii_digits(options.code))
            throw Error("pe-security-history requires a six-digit code");
    } else if (!options.market.empty() || !options.code.empty()) {
        throw Error("market/code are only valid for pe-security-history");
    }

    if (view.industry_members) {
        if (options.industry_code.size() != 6 || !ascii_digits(options.industry_code))
            throw Error("industry member views require a six-digit industry_code");
        (void)integer_value(options.industry_market, "industry_market", 0, 255);
    } else if (!options.industry_code.empty()) {
        throw Error("industry_code is only valid for industry member views");
    }

    if (!std::isfinite(options.required_return_rate_pct) ||
        options.required_return_rate_pct < 0 || options.required_return_rate_pct > 100)
        throw Error("required_return_rate_pct must be in 0..100");
    if (options.limit < 1 || options.limit > 20000 ||
        options.cache_ttl_seconds < 0 || options.cache_ttl_seconds > 86400 ||
        options.timeout_ms < 100 || options.timeout_ms > 60000)
        throw Error("equity valuation query limits are invalid");
    return options;
}

RequestPlan build_request_plan(
    const EquityValuationQuery& options, const ViewSpec& view, int pe_type) {
    RequestPlan plan{view.request_id, view.source_file, {}};
    switch (view.kind) {
        case ViewKind::pe_industries:
            plan.replacements["PE_type"] = std::to_string(pe_type);
            break;
        case ViewKind::pe_security_history:
            plan.replacements = {{"PE_type", std::to_string(pe_type)},
                {"start_date", options.start_date}, {"end_date", options.end_date},
                {"stock_code", options.code},
                {"stock_market", std::to_string(security_market(options.market))}};
            break;
        case ViewKind::pe_industry_members: {
            std::ostringstream rate;
            rate << std::setprecision(15) << options.required_return_rate_pct;
            plan.replacements = {{"PE_type", std::to_string(pe_type)},
                {"start_date", options.start_date}, {"end_date", options.end_date},
                {"code_hy", options.industry_code}, {"market_hy", options.industry_market},
                {"required_return_rate", rate.str()}};
            break;
        }
        case ViewKind::pb_roe_industries:
            plan.replacements["end_date"] = options.end_date;
            break;
        case ViewKind::pb_roe_members:
            plan.replacements = {{"end_date", options.end_date},
                {"code_hy", options.industry_code},
                {"market_hy", options.industry_market}};
            break;
    }
    return plan;
}

}  // namespace tdx::detail::equity_valuation
