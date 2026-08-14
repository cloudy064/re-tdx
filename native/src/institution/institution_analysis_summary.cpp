#include "institution_analysis_internal.hpp"

#include "tdx/jsn.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <set>
#include <utility>

namespace tdx::institution_analysis_detail {

bool row_matches(const Json& row, const InstitutionAnalysisQuery& options,
                 int selected_market, const std::string& needle) {
    if (selected_market >= 0 && static_cast<int>(row.at("market_id").as_number()) != selected_market)
        return false;
    if (!options.code.empty() && row.at("code").as_string() != options.code) return false;
    if (needle.empty()) return true;
    return lower_ascii(row.at("code").as_string()).find(needle) != std::string::npos ||
           lower_ascii(row.at("name").as_string()).find(needle) != std::string::npos ||
           lower_ascii(row.at("data").dump(-1)).find(needle) != std::string::npos;
}

double data_number(const Json& row, std::string_view key) {
    const auto* value = value_ptr(row.at("data"), key);
    return value && value->is_number() ? value->as_number()
                                       : -std::numeric_limits<double>::infinity();
}

void sort_special_records(const ViewSpec& view, Json& rows) {
    if (view.layout == Layout::development_bank ||
        view.layout == Layout::named_holding ||
        view.layout == Layout::named_holding_summary ||
        view.layout == Layout::stake_building) {
        std::stable_sort(rows.as_array().begin(), rows.as_array().end(),
            [](const Json& left, const Json& right) {
                const auto left_date = text_value(left.at("data"), "report_date");
                const auto right_date = text_value(right.at("data"), "report_date");
                if (left_date != right_date) return left_date > right_date;
                return left.at("security_id").as_string() <
                       right.at("security_id").as_string();
            });
        return;
    }
    if (view.layout != Layout::exclusive_funds &&
        view.layout != Layout::notable_private_funds &&
        view.layout != Layout::national_team &&
        view.layout != Layout::social_security_summary) return;
    const auto metric = view.layout == Layout::exclusive_funds
        ? "float_share_pct" : view.layout == Layout::notable_private_funds
        ? "holding_market_value" : view.layout == Layout::national_team
        ? "combined_ratio_pct" : "holding_shares";
    std::stable_sort(rows.as_array().begin(), rows.as_array().end(),
        [&](const Json& left, const Json& right) {
            const auto a = data_number(left, metric);
            const auto b = data_number(right, metric);
            if (a != b) return a > b;
            if (view.layout == Layout::exclusive_funds) {
                const auto ah = data_number(left, "holding_shares");
                const auto bh = data_number(right, "holding_shares");
                if (ah != bh) return ah > bh;
            }
            const auto a_id = left.at("security_id").as_string();
            const auto b_id = right.at("security_id").as_string();
            if (a_id != b_id) return a_id < b_id;
            const auto identity = view.layout == Layout::notable_private_funds
                ? "private_fund_manager" : "fund_management_company";
            return text_value(left.at("data"), identity) <
                   text_value(right.at("data"), identity);
        });
}

Json section_summary(const ViewSpec& view, const Json& rows) {
    std::set<std::string> securities, report_dates, fund_companies,
                          private_fund_managers, industries;
    std::uint64_t names_resolved = 0, csf = 0, huijin = 0, both = 0,
                  formula_checked = 0, formula_mismatches = 0,
                  further_increase_yes = 0, insurance_capital_yes = 0;
    double holding_shares = 0.0, holding_market_value = 0.0;
    for (const auto& row : rows.as_array()) {
        securities.insert(row.at("security_id").as_string());
        if (row.at("name_resolved").as_bool()) ++names_resolved;
        const auto& data = row.at("data");
        const auto report_date = text_value(data, "report_date");
        if (!report_date.empty()) report_dates.insert(report_date);
        if (view.layout == Layout::exclusive_funds) {
            const auto company = text_value(data, "fund_management_company");
            if (!company.empty()) fund_companies.insert(company);
            const auto* shares = value_ptr(data, "holding_shares");
            if (shares && shares->is_number()) holding_shares += shares->as_number();
        } else if (view.layout == Layout::notable_private_funds) {
            const auto manager = text_value(data, "private_fund_manager");
            const auto industry = text_value(data, "industry");
            if (!manager.empty()) private_fund_managers.insert(manager);
            if (!industry.empty()) industries.insert(industry);
            const auto* value = value_ptr(data, "holding_market_value");
            if (value && value->is_number()) holding_market_value += value->as_number();
        } else if (view.layout == Layout::national_team) {
            const auto* csf_value = value_ptr(data, "china_securities_finance_ratio_pct");
            const auto* huijin_value = value_ptr(data, "central_huijin_ratio_pct");
            const bool has_csf = csf_value && csf_value->is_number();
            const bool has_huijin = huijin_value && huijin_value->is_number();
            if (has_csf) ++csf;
            if (has_huijin) ++huijin;
            if (has_csf && has_huijin) ++both;
            const auto* formula = value_ptr(data, "combined_ratio_formula_matches");
            if (formula && formula->is_bool()) {
                ++formula_checked;
                if (!formula->as_bool()) ++formula_mismatches;
            }
        } else if (view.layout == Layout::stake_building) {
            if (text_value(data, "further_increase") == "是")
                ++further_increase_yes;
            if (text_value(data, "insurance_capital") == "是")
                ++insurance_capital_yes;
        }
    }
    Json result = Json::object();
    result["rows"] = static_cast<std::uint64_t>(rows.size());
    result["unique_securities"] = static_cast<std::uint64_t>(securities.size());
    result["names_resolved"] = names_resolved;
    Json periods = Json::array();
    for (const auto& period : report_dates) periods.push_back(period);
    result["report_dates"] = std::move(periods);
    if (view.layout == Layout::exclusive_funds) {
        result["fund_management_companies"] =
            static_cast<std::uint64_t>(fund_companies.size());
        result["holding_shares"] = holding_shares;
        result["duplicate_company_security_rows"] =
            static_cast<std::uint64_t>(rows.size() - securities.size());
    } else if (view.layout == Layout::notable_private_funds) {
        result["private_fund_managers"] =
            static_cast<std::uint64_t>(private_fund_managers.size());
        result["industries"] = static_cast<std::uint64_t>(industries.size());
        result["holding_market_value"] = holding_market_value;
        result["duplicate_manager_security_rows"] =
            static_cast<std::uint64_t>(rows.size() - securities.size());
    } else if (view.layout == Layout::national_team) {
        result["china_securities_finance_securities"] = csf;
        result["central_huijin_securities"] = huijin;
        result["both_securities"] = both;
        result["combined_formula_checked"] = formula_checked;
        result["combined_formula_mismatches"] = formula_mismatches;
    } else if (view.layout == Layout::stake_building) {
        result["further_increase_yes"] = further_increase_yes;
        result["insurance_capital_yes"] = insurance_capital_yes;
    }
    return result;
}

Json section_document(const ViewSpec& view, const Json& source,
                      const InstitutionAnalysisQuery& options,
                      const std::map<std::pair<int, std::string>, Security>& securities) {
    const int selected_market = options.market.empty() ? -1 : market_id(options.market);
    const auto needle = lower_ascii(trim(options.query));
    const auto& rows = source.at("rows");
    if (!rows.is_array()) throw Error("CGFX source rows must be an array");
    Json matched_rows = Json::array();
    for (const auto& raw : rows.as_array()) {
        Json row;
        try { row = normalize_row(view, raw, securities); }
        catch (const std::exception&) { continue; }
        if (!row_matches(row, options, selected_market, needle)) continue;
        matched_rows.push_back(std::move(row));
    }
    sort_special_records(view, matched_rows);
    Json records = Json::array();
    for (std::size_t index = static_cast<std::size_t>(options.offset);
         index < matched_rows.size() && records.size() < static_cast<std::size_t>(options.limit);
         ++index)
        records.push_back(matched_rows.as_array()[index]);
    const auto matched = static_cast<std::uint64_t>(matched_rows.size());
    Json result = Json::object();
    result["view"] = view.id;
    result["label"] = view.label;
    result["layout"] = layout_name(view.layout);
    result["resource"] = view.resource;
    result["source_rows"] = static_cast<std::uint64_t>(rows.size());
    result["matched"] = matched;
    result["offset"] = options.offset;
    result["limit"] = options.limit;
    result["returned"] = static_cast<std::uint64_t>(records.size());
    result["truncated"] = matched > static_cast<std::uint64_t>(options.offset) + records.size();
    result["summary"] = section_summary(view, matched_rows);
    result["records"] = std::move(records);
    result["source"] = jsn_source_metadata(source);
    return result;
}

}  // namespace tdx::institution_analysis_detail
