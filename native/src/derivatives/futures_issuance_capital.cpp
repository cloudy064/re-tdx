#include "futures_issuance_internal.hpp"

#include "tdx/common.hpp"
#include "tdx/jsn.hpp"

#include <cmath>
#include <map>
#include <set>

namespace tdx::futures_issuance_detail {

Json private_placement_summary(const Json& rows) {
    std::map<std::string, std::uint64_t, std::less<>> lifecycle_counts;
    std::map<std::string, std::uint64_t, std::less<>> stage_counts;
    std::set<std::string> securities;
    double actual_gross = 0.0;
    double expected = 0.0;
    for (const auto& row : rows.as_array()) {
        ++lifecycle_counts[text_value(row, "lifecycle")];
        ++stage_counts[text_value(row, "stage")];
        securities.insert(row.at("security").at("security_id").as_string());
        if (const auto* value = value_ptr(row, "actual_gross_10k_yuan");
            value && value->is_number()) actual_gross += value->as_number();
        if (const auto* value = value_ptr(row, "expected_raise_10k_yuan");
            value && value->is_number()) expected += value->as_number();
    }
    Json result = Json::object();
    result["records"] = static_cast<std::uint64_t>(rows.size());
    result["unique_securities"] = static_cast<std::uint64_t>(securities.size());
    result["actual_gross_10k_yuan"] = actual_gross;
    result["expected_raise_10k_yuan"] = expected;
    Json lifecycles = Json::array();
    for (const auto& [label, count] : lifecycle_counts) {
        Json item = Json::object();
        item["label"] = label;
        item["count"] = count;
        lifecycles.push_back(std::move(item));
    }
    Json stages = Json::array();
    for (const auto& [label, count] : stage_counts) {
        Json item = Json::object();
        item["label"] = label;
        item["count"] = count;
        stages.push_back(std::move(item));
    }
    result["lifecycle_counts"] = std::move(lifecycles);
    result["stage_counts"] = std::move(stages);
    return result;
}

Json rights_offering_summary(const Json& rows) {
    std::map<std::string, std::uint64_t, std::less<>> phase_counts;
    std::map<std::string, std::uint64_t, std::less<>> status_counts;
    std::set<std::string> securities;
    double implemented_raised = 0.0, planned_raised = 0.0;
    double implemented_shares = 0.0, planned_shares = 0.0;
    for (const auto& row : rows.as_array()) {
        ++phase_counts[text_value(row, "phase")];
        ++status_counts[text_value(row, "stage")];
        securities.insert(row.at("security").at("security_id").as_string());
        const auto raised = number_value(row, "raised_yuan");
        const auto shares = number_value(row, "offered_shares");
        const bool implemented = text_value(row, "phase") == "implemented";
        if (raised.is_number())
            (implemented ? implemented_raised : planned_raised) += raised.as_number();
        if (shares.is_number())
            (implemented ? implemented_shares : planned_shares) += shares.as_number();
    }
    Json result = Json::object();
    result["records"] = static_cast<std::uint64_t>(rows.size());
    result["unique_securities"] = static_cast<std::uint64_t>(securities.size());
    result["implemented_raised_yuan"] = implemented_raised;
    result["planned_raised_yuan"] = planned_raised;
    result["implemented_shares"] = implemented_shares;
    result["planned_shares"] = planned_shares;
    result["phase_counts"] = Json::object();
    for (const auto& [label, count] : phase_counts)
        result["phase_counts"][label] = count;
    result["status_counts"] = Json::object();
    for (const auto& [label, count] : status_counts)
        result["status_counts"][label] = count;
    return result;
}

Json preferred_share_summary(const Json& rows) {
    std::set<std::string> underlying_securities;
    std::set<std::string> preferred_codes;
    double issue_shares = 0.0, issue_size = 0.0;
    std::uint64_t cumulative = 0, adjustable = 0;
    for (const auto& row : rows.as_array()) {
        underlying_securities.insert(
            row.at("underlying_security").at("security_id").as_string());
        preferred_codes.insert(text_value(row, "preferred_code"));
        const auto shares = number_value(row, "issue_shares");
        const auto size = number_value(row, "issue_size_yuan");
        if (shares.is_number()) issue_shares += shares.as_number();
        if (size.is_number()) issue_size += size.as_number();
        if (text_value(row, "cumulative_dividend") == "是") ++cumulative;
        if (text_value(row, "adjustable_dividend") == "是") ++adjustable;
    }
    Json result = Json::object();
    result["records"] = static_cast<std::uint64_t>(rows.size());
    result["unique_underlying_securities"] =
        static_cast<std::uint64_t>(underlying_securities.size());
    result["unique_preferred_codes"] =
        static_cast<std::uint64_t>(preferred_codes.size());
    result["total_issue_shares"] = issue_shares;
    result["total_issue_size_yuan"] = issue_size;
    result["cumulative_dividend_count"] = cumulative;
    result["adjustable_dividend_count"] = adjustable;
    return result;
}

bool placement_status_matches(const Json& row, const std::string& status) {
    const auto lifecycle = text_value(row, "lifecycle");
    if (status == "all") return true;
    if (status == "locked") return lifecycle == "implemented-locked";
    if (status == "unlocked") return lifecycle == "implemented-unlocked";
    if (status == "active") return lifecycle == "plan-active";
    if (status == "stopped") return lifecycle == "plan-stopped";
    if (status == "implemented")
        return lifecycle == "implemented" || lifecycle == "implemented-locked" ||
               lifecycle == "implemented-unlocked";
    if (status == "registered") return lifecycle == "registered";
    return false;
}


}  // namespace tdx::futures_issuance_detail

namespace tdx {

using namespace futures_issuance_detail;

Json normalize_private_placement_rows(
    const Json& rows, const std::string& source_resource,
    const std::string& lifecycle,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    if (!rows.is_array()) throw Error("private placement rows must be an array");
    const std::map<std::string, std::string, std::less<>> lifecycle_labels{
        {"implemented-locked", "锁定中"},
        {"implemented-unlocked", "已解锁"},
        {"plan-active", "方案推进"},
        {"plan-stopped", "终止/中止/延期"},
        {"implemented", "已实施"},
        {"registered", "注册生效"},
    };
    const auto label = lifecycle_labels.find(lifecycle);
    if (label == lifecycle_labels.end())
        throw Error("unsupported private placement lifecycle: " + lifecycle);

    Json result = Json::array();
    for (const auto& row : rows.as_array()) {
        const auto market = text_value(row, "$SC");
        const auto code = text_value(row, "$ZQDM");
        Json item = Json::object();
        item["security"] = security_document(market, code, securities);
        item["source_resource"] = source_resource;
        item["lifecycle"] = lifecycle;
        item["lifecycle_label"] = label->second;
        const auto raw_stage = text_value(row, "fajd");
        item["stage"] = raw_stage.empty() ? label->second : raw_stage;
        item["industry"] = text_value(row, "hy");

        Json dates = Json::object();
        dates["board_approved"] = "";
        dates["shareholders_approved"] = "";
        dates["regulator_approved"] = "";
        dates["registered"] = "";
        dates["implemented"] = "";
        dates["listed"] = "";
        dates["unlock"] = "";
        std::string sort_date;
        if (lifecycle == "plan-active" || lifecycle == "plan-stopped" ||
            lifecycle == "registered") {
            dates["board_approved"] = text_value(row, "date1");
            dates["shareholders_approved"] = text_value(row, "date2");
            if (lifecycle == "registered")
                dates["registered"] = text_value(row, "date3");
            else
                dates["regulator_approved"] = text_value(row, "date3");
            sort_date = !text_value(row, "date3").empty() ? text_value(row, "date3")
                : !text_value(row, "date2").empty() ? text_value(row, "date2")
                : text_value(row, "date1");
        } else {
            dates["regulator_approved"] = text_value(row, "date3");
            dates["implemented"] = text_value(row, "date4");
            dates["listed"] = text_value(row, "date1");
            if (lifecycle == "implemented-locked" || lifecycle == "implemented-unlocked")
                dates["unlock"] = text_value(row, "date2");
            sort_date = !text_value(row, "date2").empty() ? text_value(row, "date2")
                : !text_value(row, "date4").empty() ? text_value(row, "date4")
                : text_value(row, "date1");
        }
        item["dates"] = std::move(dates);
        item["sort_date"] = sort_date;
        item["event_id"] = source_resource + ":" + market + ":" + code + ":" + sort_date;

        item["issue_price"] = number_value(row, "price0");
        item["approved_close"] = number_value(row, "price3");
        item["implemented_close"] = number_value(row, "price4");
        item["listing_close"] = number_value(row, "price1");
        item["unlock_close"] = number_value(row, "price2");
        item["actual_gross_10k_yuan"] = number_value(row, "mzze");
        item["actual_net_10k_yuan"] = (lifecycle == "plan-active" ||
            lifecycle == "plan-stopped" || lifecycle == "registered")
            ? Json(nullptr) : number_value(row, "mzje");
        item["expected_raise_10k_yuan"] = (lifecycle == "plan-active" ||
            lifecycle == "plan-stopped" || lifecycle == "registered")
            ? number_value(row, "mzje") : Json(nullptr);
        item["issue_shares_10k"] = lifecycle == "plan-active" || lifecycle == "plan-stopped" ||
            lifecycle == "registered" ? number_value(row, "fxgm") : number_value(row, "fxzl");
        item["post_issue_shares_10k"] = number_value(row, "fxhgb");

        const auto approved_close = number_value(row, "price3");
        const auto implemented_close = number_value(row, "price4");
        item["approval_to_implementation_return_pct"] =
            approved_close.is_number() && implemented_close.is_number() &&
            approved_close.as_number() != 0.0
            ? Json((implemented_close.as_number() / approved_close.as_number() - 1.0) * 100.0)
            : Json(nullptr);
        const auto listing_close = number_value(row, "price1");
        const auto unlock_close = number_value(row, "price2");
        item["lock_period_return_pct"] =
            listing_close.is_number() && unlock_close.is_number() &&
            listing_close.as_number() != 0.0
            ? Json((unlock_close.as_number() / listing_close.as_number() - 1.0) * 100.0)
            : Json(nullptr);
        const auto issue_shares = lifecycle == "plan-active" || lifecycle == "plan-stopped" ||
            lifecycle == "registered" ? number_value(row, "fxgm") : number_value(row, "fxzl");
        const auto post_issue = number_value(row, "fxhgb");
        item["issue_to_post_shares_pct"] =
            issue_shares.is_number() && post_issue.is_number() && post_issue.as_number() != 0.0
            ? Json(issue_shares.as_number() / post_issue.as_number() * 100.0)
            : Json(nullptr);
        item["issue_details"] = text_value(row, "fxxq");
        item["change_notes"] = text_value(row, "bgsm");
        item["registration_announcement"] = text_value(row, "hzgg");
        item["raw"] = row;
        result.push_back(std::move(item));
    }
    return result;
}

Json normalize_rights_offering_rows(
    const Json& rows, const std::string& source_resource,
    const std::string& phase,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    if (!rows.is_array()) throw Error("rights-offering rows must be an array");
    if (phase != "implemented" && phase != "deliberating" && phase != "abnormal")
        throw Error("unsupported rights-offering phase: " + phase);
    Json result = Json::array();
    for (const auto& row : rows.as_array()) {
        const auto market = text_value(row, "$SC");
        const auto code = text_value(row, "$ZQDM");
        if (!digits(code, 6) || market_id(market) < 0) continue;
        const auto announcement_date = text_value(row, "ggrq");
        const auto progress_date = text_value(row, "hzr");
        const auto sort_date = progress_date.empty() ? announcement_date : progress_date;
        Json item = Json::object();
        item["kind"] = "rights-offering";
        item["security"] = security_document(market, code, securities);
        item["phase"] = phase;
        item["phase_label"] = phase == "implemented" ? "配股实施" :
            phase == "deliberating" ? "配股审议中" : "配股进度异常";
        const auto raw_stage = text_value(row, "jd");
        item["stage"] = raw_stage.empty() ? "配股实施" : raw_stage;
        item["announcement_date"] = announcement_date;
        item["progress_date"] = progress_date;
        item["sort_date"] = sort_date;
        item["rights_code"] = text_value(row, "dm");
        item["rights_name"] = text_value(row, "jc");
        item["rights_per_10_shares"] = number_value(row, "bl");
        item["rights_price_yuan"] = number_value(row, "jg");
        item["offered_shares"] = number_value(row, "sl");
        item["raised_yuan"] = number_value(row, "zj");
        item["amount_semantics"] = phase == "implemented" ? "actual" : "planned";
        item["payment_start_date"] = text_value(row, "qsjkr");
        item["payment_end_date"] = text_value(row, "jzjkr");
        item["registration_date"] = text_value(row, "djr");
        item["ex_rights_date"] = text_value(row, "cqr");
        item["major_shareholder_subscription"] = text_value(row, "dgd");
        item["issue_details"] = text_value(row, "fxxq");
        item["source_resource"] = source_resource;
        item["event_id"] = source_resource + ":" + market + ":" + code + ":" +
            announcement_date + ":" + text_value(row, "dm");
        item["raw"] = row;
        result.push_back(std::move(item));
    }
    return result;
}

Json normalize_preferred_share_rows(
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    if (!rows.is_array()) throw Error("preferred-share rows must be an array");
    Json result = Json::array();
    for (const auto& row : rows.as_array()) {
        const auto market = text_value(row, "$SC");
        const auto code = text_value(row, "$ZQDM");
        const auto preferred_code = text_value(row, "dm");
        if (!digits(code, 6) || market_id(market) < 0 || preferred_code.empty()) continue;
        const auto issue_shares_10k = number_value(row, "fxsl");
        const auto issue_size_100m = number_value(row, "fxgm");
        Json item = Json::object();
        item["kind"] = "preferred-share";
        item["record_id"] = "preferred-share:" + market + ":" + code + ":" +
            preferred_code;
        item["underlying_security"] = security_document(market, code, securities);
        item["preferred_code"] = preferred_code;
        item["preferred_name"] = text_value(row, "jc");
        item["listing_date"] = text_value(row, "ssrq");
        item["par_value_yuan"] = number_value(row, "mgmz");
        item["issue_price_yuan"] = number_value(row, "fxjg");
        item["issue_shares_10k"] = issue_shares_10k;
        item["issue_shares"] = issue_shares_10k.is_number()
            ? Json(std::round(issue_shares_10k.as_number() * 10000.0)) : Json(nullptr);
        item["issue_size_100m_yuan"] = issue_size_100m;
        item["issue_size_yuan"] = issue_size_100m.is_number()
            ? Json(std::round(issue_size_100m.as_number() * 100000000.0)) : Json(nullptr);
        item["issue_method"] = text_value(row, "fxfs");
        item["initial_dividend_yield_pct"] = number_value(row, "cspmgxl");
        item["cumulative_dividend"] = text_value(row, "sflj");
        item["adjustable_dividend"] = text_value(row, "sftx");
        item["annual_payment_count"] = number_value(row, "nfxcs");
        item["payment_method"] = text_value(row, "zffs");
        item["source_resource"] = std::string(preferred_share_resources.front().resource);
        item["raw"] = row;
        result.push_back(std::move(item));
    }
    return result;
}


}  // namespace tdx

