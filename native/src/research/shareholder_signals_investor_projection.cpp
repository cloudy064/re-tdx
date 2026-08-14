#include "shareholder_signals_internal.hpp"

#include "tdx/jsn.hpp"
#include "tdx/jsn_data.hpp"

#include <cmath>

namespace tdx::detail::shareholder_signals {

InvestorProjection project_investor(
    const QueryPlan& plan, const Json& master, const std::filesystem::path& jsn_root,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    InvestorProjection result;
    const bool include_directory = plan.view->directory ||
        !plan.options.investor_id.empty();
    const auto investor_needle = lower_ascii(trim(
        plan.options.investor_query.empty()
            ? (plan.view->directory ? plan.options.query : std::string{})
            : plan.options.investor_query));
    if (include_directory) {
        for (const auto& row : master.at("investor_directory").as_array()) {
            if (!investor_needle.empty() &&
                lower_ascii(row.dump(-1)).find(investor_needle) == std::string::npos)
                continue;
            result.directory.push_back(row);
        }
        sort_signal_rows(result.directory, *plan.sort, plan.options.order);
    }
    result.directory_matched = result.directory.size();
    if (static_cast<int>(result.directory.size()) > plan.options.limit)
        result.directory.as_array().resize(
            static_cast<std::size_t>(plan.options.limit));

    if (plan.options.investor_id.empty()) return result;
    for (const auto& row : master.at("investor_directory").as_array())
        if (text_value(row, "investor_id") == plan.options.investor_id) {
            result.selected = row;
            break;
        }
    if (result.selected.is_null())
        throw Error("investor_id is absent from the active notable-investor directory");

    const auto resource = "nscg/" + plan.options.investor_id + ".jsn";
    Json document;
    bool loaded = false;
    if (!plan.options.refresh && !jsn_root.empty()) {
        try {
            document = load_local_resource_rows(jsn_root, resource);
            loaded = true;
        } catch (...) {}
    }
    if (!loaded) {
        const auto documents = fetch_jsn_resources_rows(
            {resource}, "bi", plan.options.timeout_ms);
        document = documents.as_array().front();
    }
    result.holdings = normalize_notable_investor_holding_rows(
        plan.options.investor_id, text_value(result.selected, "investor_name"),
        document.at("rows"), securities);
    sort_signal_rows(result.holdings, *plan.sort, plan.options.order);
    result.detail_source = source_summary(document, result.holdings.size());

    double shares = 0.0;
    double value = 0.0;
    for (const auto& row : result.holdings.as_array()) {
        shares += normalized_number(row, "holding_shares").value_or(0.0);
        value += normalized_number(row, "holding_value_yuan").value_or(0.0);
    }
    const auto expected_count = normalized_number(
        result.selected, "holding_company_count").value_or(0.0);
    const auto expected_shares = normalized_number(
        result.selected, "holding_shares").value_or(0.0);
    const auto expected_value = normalized_number(
        result.selected, "holding_value_yuan").value_or(0.0);
    result.reconciliation["expected_company_count"] = expected_count;
    result.reconciliation["actual_company_count"] =
        static_cast<std::uint64_t>(result.holdings.size());
    result.reconciliation["expected_holding_shares"] = expected_shares;
    result.reconciliation["actual_holding_shares"] = shares;
    result.reconciliation["expected_holding_value_yuan"] = expected_value;
    result.reconciliation["actual_holding_value_yuan"] = value;
    result.reconciliation["company_count_matches"] =
        std::abs(expected_count - static_cast<double>(result.holdings.size())) < 0.5;
    result.reconciliation["holding_shares_match"] =
        std::abs(expected_shares - shares) < 0.01;
    result.reconciliation["holding_value_matches"] =
        std::abs(expected_value - value) < 0.01;
    return result;
}

}  // namespace tdx::detail::shareholder_signals
