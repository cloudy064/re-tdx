#include "formula_context_current_finance_internal.hpp"
#include "formula_context_hk_finance_internal.hpp"

#include "tdx/common.hpp"
#include "tdx/corporate.hpp"
#include "tdx/session_audit.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <ctime>
#include <limits>
#include <string_view>

namespace tdx::formula_context_detail {
namespace {

struct FinanceFieldBinding {
    int selector;
    std::array<std::string_view, 2> path;
    std::size_t depth;
};

constexpr std::array<FinanceFieldBinding, 27> kCurrentFinanceFields{{
    {1, {"shares", "total"}, 2},
    {5, {"shares", "b_share"}, 2},
    {6, {"shares", "h_share"}, 2},
    {7, {"shares", "circulating"}, 2},
    {9, {"balance_sheet", "total_assets_yuan"}, 2},
    {10, {"balance_sheet", "current_assets_yuan"}, 2},
    {11, {"balance_sheet", "fixed_assets_yuan"}, 2},
    {12, {"balance_sheet", "intangible_assets_yuan"}, 2},
    {13, {"shareholder_count", ""}, 1},
    {14, {"balance_sheet", "current_liabilities_yuan"}, 2},
    {15, {"balance_sheet", "long_term_liabilities_yuan"}, 2},
    {16, {"balance_sheet", "capital_reserve_yuan"}, 2},
    {19, {"balance_sheet", "net_assets_yuan"}, 2},
    {20, {"income_statement", "revenue_yuan"}, 2},
    {23, {"income_statement", "operating_profit_yuan"}, 2},
    {24, {"cash_flow", "operating_yuan"}, 2},
    {25, {"cash_flow", "total_yuan"}, 2},
    {26, {"balance_sheet", "inventory_yuan"}, 2},
    {27, {"income_statement", "total_profit_yuan"}, 2},
    {28, {"income_statement", "net_profit_yuan"}, 2},
    {29, {"income_statement", "after_tax_profit_yuan"}, 2},
    {30, {"income_statement", "net_profit_yuan"}, 2},
    {31, {"income_statement", "undistributed_profit_yuan"}, 2},
    {33, {"per_share", "eps"}, 2},
    {34, {"per_share", "net_assets"}, 2},
    {41, {"income_statement", "revenue_yuan"}, 2},
    {56, {"shareholder_count", ""}, 1},
}};

// TCalc's CAPITAL/TOTALCAPITAL handlers read the type-105 share count from a
// float slot, perform the unit conversion, and write a float result.  Preserve
// that two-stage landing without changing the existing public divisor policy.
double tcalc_capital_hands(double shares) {
    const double native_shares = static_cast<double>(static_cast<float>(shares));
    return static_cast<double>(static_cast<float>(native_shares / 100.0));
}

const Json* optional(const Json& object, std::string_view key) {
    if (!object.is_object()) return nullptr;
    const auto found = object.as_object().find(key);
    return found == object.as_object().end() ? nullptr : &found->second;
}

const Json* nested(const Json& value,
                   const std::array<std::string_view, 2>& path,
                   std::size_t depth = 2) {
    const Json* current = &value;
    for (std::size_t i = 0; i < depth; ++i) {
        current = optional(*current, path[i]);
        if (!current) return nullptr;
    }
    return current;
}

long long civil_day_number(int year, unsigned month, unsigned day) {
    year -= month <= 2;
    const int era = (year >= 0 ? year : year - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(year - era * 400);
    const unsigned adjusted_month = month > 2 ? month - 3 : month + 9;
    const unsigned doy = (153 * adjusted_month + 2) / 5 + day - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return static_cast<long long>(era) * 146097 + doe;
}

int listing_days(const Json& finance) {
    const auto* raw = optional(finance, "listing_date_raw");
    if (!raw || !raw->is_number()) return 0;
    const int date = static_cast<int>(raw->as_number());
    const std::time_t now = std::time(nullptr);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    const auto start = civil_day_number(
        date / 10000, static_cast<unsigned>((date / 100) % 100),
        static_cast<unsigned>(date % 100));
    const auto today = civil_day_number(
        local.tm_year + 1900, static_cast<unsigned>(local.tm_mon + 1),
        static_cast<unsigned>(local.tm_mday));
    return static_cast<int>(std::max<long long>(0, today - start));
}

class CurrentFinanceContextBuilder {
public:
    CurrentFinanceContextBuilder(
        Json& symbols,
        const std::filesystem::path& root,
        const std::string& market,
        const std::string& code,
        const CurrentFinanceRequirements& requirements,
        int market_id,
        int security_type,
        int timeout_ms)
        : symbols_(symbols), root_(root), market_(market), code_(code),
          requirements_(requirements), security_type_(security_type),
          market_id_(market_id), timeout_ms_(timeout_ms) {
        result_.circulating_shares =
            std::numeric_limits<double>::quiet_NaN();
    }

    CurrentFinanceContext build() {
        if (!requirements_.any()) return std::move(result_);
        result_.document = fetch_finance_document(
            {market_ + ":" + code_},
            load_public_quote_endpoints(root_).endpoints,
            timeout_ms_, 80, false);
        const auto* record = result_.record();
        if (!record)
            throw Error("finance context returned no selected security");
        bind_configured_fields(*record);
        bind_derived_fields(*record);
        bind_capital_symbols(*record);
        if (const auto* province = optional(*record, "province_id");
            province && province->is_number())
            result_.province_id = static_cast<int>(province->as_number());
        return std::move(result_);
    }

private:
    void bind_configured_fields(const Json& record) {
        for (const auto& binding : kCurrentFinanceFields) {
            const auto* value = nested(record, binding.path, binding.depth);
            if (value && value->is_number())
                result_.values[std::to_string(binding.selector)] = *value;
        }
    }

    void bind_derived_fields(const Json& record) {
        result_.values["2"] = market_id_;
        result_.values["3"] = security_type_;
        result_.values["42"] = listing_days(record);
        result_.values["53"] = 0.0;
        const std::array<std::string_view, 2> revenue_path{
            "income_statement", "revenue_yuan"};
        const std::array<std::string_view, 2> gross_path{
            "income_statement", "main_profit_yuan"};
        const auto* revenue = nested(record, revenue_path);
        const auto* gross = nested(record, gross_path);
        if (revenue && gross && revenue->is_number() && gross->is_number())
            result_.values["21"] =
                revenue->as_number() - gross->as_number();

        const std::array<std::string_view, 2> total_shares_path{
            "shares", "total"};
        const std::array<std::string_view, 2> h_shares_path{
            "shares", "h_share"};
        const std::array<std::string_view, 2> undistributed_path{
            "income_statement", "undistributed_profit_yuan"};
        const std::array<std::string_view, 2> net_profit_path{
            "income_statement", "net_profit_yuan"};
        const std::array<std::string_view, 2> eps_path{
            "per_share", "eps"};
        const auto* total_shares = nested(record, total_shares_path);
        const auto* h_shares = nested(record, h_shares_path);
        const auto* undistributed = nested(record, undistributed_path);
        const auto* net_profit = nested(record, net_profit_path);
        const auto* eps = nested(record, eps_path);
        if (total_shares && total_shares->is_number() &&
            total_shares->as_number() > 1.0) {
            if (undistributed && undistributed->is_number())
                result_.values["32"] =
                    undistributed->as_number() / total_shares->as_number();
            if (net_profit && net_profit->is_number()) {
                result_.values["38"] =
                    net_profit->as_number() / total_shares->as_number();
                result_.values["37"] =
                    eps && eps->is_number() &&
                            std::abs(eps->as_number()) > 0.00001
                        ? net_profit->as_number() /
                              total_shares->as_number() /
                              eps->as_number() * 4.0
                        : 0.0;
            }
        } else if (h_shares && h_shares->is_number() &&
                   h_shares->as_number() > 1.0 && net_profit &&
                   net_profit->is_number()) {
            result_.values["38"] =
                net_profit->as_number() / h_shares->as_number();
        }
    }

    void bind_capital_symbols(const Json& record) {
        const std::array<std::string_view, 2> circulating_path{
            "shares", "circulating"};
        const std::array<std::string_view, 2> total_path{"shares", "total"};
        if (const auto* circulating = nested(record, circulating_path);
            circulating && circulating->is_number()) {
            result_.circulating_shares = circulating->as_number();
            symbols_["CAPITAL"] = tcalc_capital_hands(circulating->as_number());
        }
        if (const auto* total = nested(record, total_path);
            total && total->is_number())
            symbols_["TOTALCAPITAL"] = tcalc_capital_hands(total->as_number());
    }

    Json& symbols_;
    const std::filesystem::path& root_;
    const std::string& market_;
    const std::string& code_;
    const CurrentFinanceRequirements& requirements_;
    int security_type_;
    int market_id_;
    int timeout_ms_;
    CurrentFinanceContext result_;
};

}  // namespace

const Json* CurrentFinanceContext::record() const {
    const auto* records = optional(document, "records");
    if (records && records->is_array() && !records->as_array().empty())
        return &records->as_array().front();
    const auto* rows = optional(document, "rows");
    return rows && rows->is_array() && !rows->as_array().empty()
        ? &rows->as_array().front() : nullptr;
}

CurrentFinanceRequirements current_finance_requirements(
    const std::set<std::string>& dependencies,
    const std::set<int>& finance_bindings,
    bool point_in_time_finance,
    bool dynamic_pe,
    bool capital_history) {
    CurrentFinanceRequirements result;
    result.current_selectors =
        dependencies.count("FINANCE") && !point_in_time_finance;
    if (result.current_selectors) result.selectors = finance_bindings;
    result.dynamic_pe = dynamic_pe;
    result.capital = dependencies.count("CAPITAL") != 0;
    result.capital_history = capital_history;
    result.region = dependencies.count("DYBLOCK") != 0;
    result.turnover = dependencies.count("HSL") != 0;
    result.total_capital = dependencies.count("TOTALCAPITAL") != 0;
    return result;
}

CurrentFinanceContext build_current_finance_context(
    Json& symbols,
    const std::filesystem::path& root,
    const std::string& market,
    const std::string& code,
    const CurrentFinanceRequirements& requirements,
    int market_id,
    int security_type,
    int timeout_ms) {
    if (is_tcalc_hk_finance_market(market_id) && requirements.any())
        return build_tcalc_hk_finance_context(
            root, code, requirements, market_id, security_type);
    return CurrentFinanceContextBuilder(
        symbols, root, market, code, requirements,
        market_id, security_type, timeout_ms).build();
}

}  // namespace tdx::formula_context_detail
