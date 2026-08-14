#include "shareholder_signals_internal.hpp"

#include <map>
#include <set>

namespace tdx {

Json ShareholderSignalsService::query(const ShareholderSignalsQuery& input) {
    using namespace detail::shareholder_signals;
    const auto plan = make_query_plan(input);
    const auto& options = plan.options;
    bool refreshed = false;
    int age_seconds = 0;
    const auto master = fetch_master(options, refreshed, age_seconds);
    const auto needle = lower_ascii(options.query);

    Json records = Json::array();
    if (!plan.view->directory) {
        for (const auto& row : master.at("records").as_array()) {
            if (plan.view->kind != ViewKind::all &&
                text_value(row, "kind") != plan.view->signal_kind)
                continue;
            const auto& security = row.at("security");
            if (!plan.selected_market.empty() &&
                (security.at("market").as_string() != plan.selected_market ||
                 security.at("code").as_string() != options.code))
                continue;
            if (!needle.empty() &&
                lower_ascii(row.dump(-1)).find(needle) == std::string::npos)
                continue;
            records.push_back(row);
        }
    }
    sort_signal_rows(records, *plan.sort, options.order);
    std::map<std::string, std::uint64_t> counts;
    std::set<std::string> security_ids;
    for (const auto& row : records.as_array()) {
        ++counts[text_value(row, "kind")];
        security_ids.insert(row.at("security").at("security_id").as_string());
    }
    const auto signal_matched = records.size();
    if (static_cast<int>(records.size()) > options.limit)
        records.as_array().resize(static_cast<std::size_t>(options.limit));

    auto investor = project_investor(plan, master, jsn_root_, securities_);
    if (!options.include_raw) {
        for (auto& row : records.as_array()) row.as_object().erase("raw");
        for (auto& row : investor.directory.as_array()) row.as_object().erase("raw");
        for (auto& row : investor.holdings.as_array()) row.as_object().erase("raw");
        if (!investor.selected.is_null()) investor.selected.as_object().erase("raw");
    }

    Json summary = Json::object();
    summary["notable-investors"] = counts["notable-investors"];
    summary["institution-accumulation"] = counts["institution-accumulation"];
    summary["research-growth"] = counts["research-growth"];
    summary["small-cap-institution"] = counts["small-cap-institution"];
    summary["unique_securities"] = static_cast<std::uint64_t>(security_ids.size());
    summary["investor_directory"] = static_cast<std::uint64_t>(
        master.at("investor_directory").size());
    summary["investor_directory_matched"] = static_cast<std::uint64_t>(
        investor.directory_matched);
    summary["investor_holdings"] = static_cast<std::uint64_t>(
        investor.holdings.size());
    const auto matched = plan.view->directory
        ? investor.directory_matched : signal_matched;

    Json result = Json::object();
    result["schema"] = "tdx-market-shareholder-signals-native-v1";
    result["generated_at"] = now_text();
    result["view"] = plan.view->id;
    result["sort"] = plan.sort->id;
    result["order"] = options.order;
    result["mode"] = !options.investor_id.empty() ? "investor" :
        plan.view->directory ? "investor-directory" :
        plan.selected_market.empty() ? "catalog" : "security";
    result["availability"] = matched || investor.holdings.size()
        ? "live" : "empty";
    result["match_count"] = static_cast<std::uint64_t>(matched);
    result["returned"] = plan.view->directory
        ? static_cast<std::uint64_t>(investor.directory.size())
        : static_cast<std::uint64_t>(records.size());
    result["records"] = std::move(records);
    result["investor_directory"] = std::move(investor.directory);
    result["selected_investor"] = std::move(investor.selected);
    result["investor_holdings"] = std::move(investor.holdings);
    result["investor_reconciliation"] = std::move(investor.reconciliation);
    result["investor_detail_source"] = std::move(investor.detail_source);
    result["summary"] = std::move(summary);
    result["sources"] = master.at("sources");
    result["semantics"] =
        "CWNSCG notable-investor holdings, JGXC institution accumulation with shrinking shareholder counts, JGZD profit/research activity, and XSZGP small-cap professional-institution accumulation. Holding share/value and change fields reproduce CFG formulas. JGXC growth/change source ratios are also exposed as percentage points; top-ten ratios and JGZD returns are already percent. NSCG provides an investor directory and on-demand investor-to-stock holdings; current shares, values and company counts are reconciled against the directory totals. XSZGP cgbd/zxcg, zcsz/ccsz and jgbhl/jgsl formulas reproduce the client CFG; the last two institution-count fields retain neutral source names because live jgsl values are not consistently integral.";
    Json cache = Json::object();
    cache["refreshed"] = refreshed;
    cache["age_seconds"] = age_seconds;
    result["cache"] = std::move(cache);
    return result;
}

}  // namespace tdx
