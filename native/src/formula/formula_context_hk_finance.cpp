#include "formula_context_hk_finance_internal.hpp"

#include "tdx/common.hpp"
#include "tdx/hk_finance.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <ctime>
#include <sstream>

namespace tdx::formula_context_detail {
namespace {

constexpr std::array<int, 19> kSupportedSelectors{
    1, 2, 3, 6, 7, 9, 10, 16, 19, 20, 30, 31,
    32, 33, 34, 37, 38, 42, 53};

const Json* optional(const Json& object, std::string_view key) {
    if (!object.is_object()) return nullptr;
    const auto found = object.as_object().find(key);
    return found == object.as_object().end() ? nullptr : &found->second;
}

const Json* nested(const Json& object,
                   std::initializer_list<std::string_view> path) {
    const Json* current = &object;
    for (const auto part : path) {
        current = optional(*current, part);
        if (!current) return nullptr;
    }
    return current;
}

double required_number(const Json& record,
                       std::initializer_list<std::string_view> path,
                       std::string_view label) {
    const auto* value = nested(record, path);
    if (!value || !value->is_number())
        throw Error("HK FINANCE source lacks " + std::string(label));
    return value->as_number();
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

int listing_days(const Json& record) {
    const auto* raw = optional(record, "listing_date");
    if (!raw || !raw->is_string() || raw->as_string().size() != 8) return 0;
    int date = 0;
    try {
        std::size_t used = 0;
        date = std::stoi(raw->as_string(), &used);
        if (used != raw->as_string().size()) return 0;
    } catch (...) {
        return 0;
    }
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

Json selectors_json(int market_id) {
    Json result = Json::array();
    for (const int selector : kSupportedSelectors)
        if (is_tcalc_hk_finance_selector(market_id, selector))
            result.push_back(selector);
    return result;
}

}  // namespace

bool is_tcalc_hk_finance_market(int market_id) {
    return market_id == 71 || market_id == 31 || market_id == 48;
}

bool is_tcalc_hk_finance_selector(int market_id, int selector) {
    if (!is_tcalc_hk_finance_market(market_id) || !std::binary_search(
            kSupportedSelectors.begin(), kSupportedSelectors.end(), selector))
        return false;
    // Current 7727 directories prove category 2 for every market 31/48
    // security. Market 71 remains in TCalc's static HK branch but has no
    // current directory sample from which to prove its type-103 fallback.
    return (selector != 1 && selector != 7) ||
        market_id == 31 || market_id == 48;
}

std::optional<int> tcalc_finance_binding_selector(std::string_view binding) {
    constexpr std::string_view prefix = "FINANCE#";
    if (binding.rfind(prefix, 0) != 0 || binding.size() == prefix.size())
        return std::nullopt;
    try {
        std::size_t used = 0;
        const int selector = std::stoi(
            std::string(binding.substr(prefix.size())), &used);
        return used == binding.size() - prefix.size()
            ? std::optional<int>(selector) : std::nullopt;
    } catch (...) {
        return std::nullopt;
    }
}

std::set<int> tcalc_finance_selectors(const Json& analysis) {
    std::set<int> result;
    const auto* bindings = optional(analysis, "context_bindings_required");
    if (!bindings || !bindings->is_array()) return result;
    for (const auto& binding : bindings->as_array()) {
        if (!binding.is_string()) continue;
        if (const auto selector =
                tcalc_finance_binding_selector(binding.as_string()))
            result.insert(*selector);
    }
    return result;
}

bool supports_tcalc_hk_finance_analysis(const Json& analysis, int market_id) {
    const auto selectors = tcalc_finance_selectors(analysis);
    return !selectors.empty() && std::all_of(
        selectors.begin(), selectors.end(), [market_id](int selector) {
            return is_tcalc_hk_finance_selector(market_id, selector);
        });
}

Json project_tcalc_hk_finance_values(
    const Json& record,
    int market_id,
    int security_type,
    const std::set<int>& selectors) {
    if (!is_tcalc_hk_finance_market(market_id))
        throw Error("HK FINANCE projection requires TCalc market 71/31/48");
    if (selectors.empty())
        throw Error("HK FINANCE requires constant selector bindings");
    for (const int selector : selectors)
        if (!is_tcalc_hk_finance_selector(market_id, selector))
            throw Error(
                "TCalc HK FINANCE selector is not locally recoverable: " +
                std::to_string(selector));

    const double h_shares = required_number(
        record, {"finance", "shares", "h_10k"}, "H-share capital");
    const double total_shares = required_number(
        record, {"finance", "shares", "total_10k"}, "total capital");
    const double total_assets = required_number(
        record, {"finance", "balance_sheet", "total_assets_10k"},
        "total assets");
    const double net_assets = required_number(
        record, {"finance", "balance_sheet", "net_assets_10k"},
        "net assets");
    const double minority_interest = required_number(
        record, {"finance", "balance_sheet", "minority_interest_10k"},
        "minority interest");
    const double revenue = required_number(
        record, {"finance", "income_statement", "revenue_10k"},
        "revenue");
    const double net_profit = required_number(
        record, {"finance", "income_statement", "net_profit_10k"},
        "net profit");
    const double dividend = required_number(
        record, {"finance", "per_share", "dividend"}, "dividend per share");
    const double eps = required_number(
        record, {"finance", "per_share", "earnings"}, "earnings per share");
    const double nav = required_number(
        record, {"finance", "per_share", "net_assets"},
        "net assets per share");

    const double total_shares_native = total_shares * 10000.0;
    const double total_assets_native = total_assets * 10000.0;
    const double net_assets_native = net_assets * 10000.0;
    const double minority_native = minority_interest * 10000.0;
    const double net_profit_native = net_profit * 10000.0;
    const double liability_ratio = total_assets > 0.0
        ? (total_assets - net_assets - minority_interest) * 100.0 /
              total_assets
        : 0.0;
    const double report_period_ratio =
        total_shares_native > 1.0 && std::abs(eps) > 0.00001
        ? net_profit_native / total_shares_native / eps * 4.0
        : 0.0;

    Json all = Json::object();
    // TCalc FINANCE(1/7) select the first/second float returned by host
    // request type 103. TdxW broadcasts security+152 into both slots for
    // expansion category 2; current market 31/48 HK directories contain
    // category-2 securities only. hkcw stores that field as H-share capital.
    all["1"] = h_shares * 10000.0;
    all["2"] = market_id;
    all["3"] = security_type;
    all["6"] = h_shares * 10000.0;
    all["7"] = h_shares * 10000.0;
    all["9"] = liability_ratio;
    all["10"] = total_assets_native;
    all["16"] = minority_native;
    all["19"] = net_assets_native;
    all["20"] = revenue * 10000.0;
    all["30"] = net_profit_native;
    all["31"] = 0.0;
    all["32"] = 0.0;
    all["33"] = eps;
    all["34"] = nav;
    all["37"] = report_period_ratio;
    all["38"] = eps;
    all["42"] = listing_days(record);
    all["53"] = dividend;

    Json result = Json::object();
    for (const int selector : selectors)
        result[std::to_string(selector)] = all.at(std::to_string(selector));
    return result;
}

CurrentFinanceContext build_tcalc_hk_finance_context(
    const std::filesystem::path& root,
    const std::string& code,
    const CurrentFinanceRequirements& requirements,
    int market_id,
    int security_type) {
    if (!requirements.current_selectors)
        throw Error("HK current-finance context requires FINANCE selectors");
    if (requirements.dynamic_pe || requirements.capital ||
        requirements.capital_history || requirements.region ||
        requirements.turnover || requirements.total_capital)
        throw Error(
            "HK automatic finance context currently supports only proven "
            "TCalc FINANCE selectors");

    HkFinanceQuery query;
    query.code = code;
    query.limit = 1;
    CurrentFinanceContext result;
    result.document = load_local_hk_finance(root, query);
    const auto* rows = optional(result.document, "rows");
    if (!rows || !rows->is_array() || rows->as_array().empty())
        throw Error("selected HK security is absent from local hkcwdata.dat");
    result.values = project_tcalc_hk_finance_values(
        rows->as_array().front(), market_id, security_type,
        requirements.selectors);
    result.metadata["finance_market_mode"] = "tcalc-type105-hk-local";
    result.metadata["finance_market_id"] = market_id;
    result.metadata["finance_supported_selectors"] = selectors_json(market_id);
    result.metadata["finance_source"] =
        result.document.at("source").at("path");
    result.metadata["finance_selector_evidence"] =
        "TCalc!sub_10026B20 -> host type 103 for FINANCE(1/7) and type 105 "
        "for the financial-field selectors; TdxW type-103 category 2 uses "
        "security+152 for both capital slots; TdxW!sub_60E580 case 0x69 "
        "handles market 71/31/48";
    return result;
}

}  // namespace tdx::formula_context_detail
