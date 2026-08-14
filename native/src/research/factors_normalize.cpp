#include "factors_internal.hpp"

#include "tdx/common.hpp"

#include <map>

namespace tdx {

using namespace factor_detail;
Json normalize_factor_rows(const Json& rows, const std::string& raw_view,
                           const BlockData& blocks) {
    if (!rows.is_array()) throw Error("factor rows must be an array");
    const auto view = lower_ascii(trim(raw_view));
    if (!view_definition(view)) throw Error("unknown factor view: " + view);
    Json result = Json::array();
    for (const auto& row : rows.as_array()) {
        if (!row.is_object()) continue;
        if (view == "catalog" || view == "patterns") {
            const auto record = catalog_record(row, view == "patterns");
            if (!text(record, "factor_id").empty() && !text(record, "name").empty())
                result.push_back(record);
            continue;
        }
        Json record = Json::object();
        record["security"] = security_document(row, blocks);
        record["change_pct"] = number_json(number(row, "zdf%"));
        record["price"] = number_json(number(row, "price"));
        if (view == "members") {
            record["factor_kind"] = "standard";
        } else if (view == "pattern-members") {
            record["factor_kind"] = "pattern";
            auto ten_day = number(row, "10rzdf%");
            if (!ten_day) ten_day = number(row, "10rzf");
            record["ten_day_change_pct"] = number_json(ten_day);
        } else if (view == "dashboard") {
            const auto factor_text = text(row, "rxyz");
            record["signal_count"] = number_json(number(row, "Score"));
            record["selected_factors_text"] = factor_text;
            record["selected_factors"] = split_factors(factor_text);
            record["ten_day_change_pct"] = number_json(number(row, "near10zdf%"));
            record["safety_score"] = number_json(number(row, "safety"));
        } else if (view == "intraday-radar") {
            record["selected_factor"] = text(row, "rxyz");
            record["selection_time"] = text(row, "rxsj");
            record["since_selection_pct"] = number_json(number(row, "rxhzf"));
            record["safety_score"] = number_json(number(row, "safety"));
        }
        result.push_back(std::move(record));
    }
    return result;
}

Json factor_hits_for_security(const Json& catalog_records,
                              const Json& dashboard_records,
                              const std::string& raw_market,
                              const std::string& code) {
    if (!catalog_records.is_array() || !dashboard_records.is_array())
        throw Error("factor reverse lookup requires catalog and dashboard arrays");
    const auto market = market_name(market_id(raw_market));
    const Json* selected = nullptr;
    for (const auto& record : dashboard_records.as_array()) {
        const auto* security = field(record, "security");
        if (security && text(*security, "market") == market &&
            text(*security, "code") == code) {
            selected = &record;
            break;
        }
    }
    std::map<std::string, Json> by_name;
    for (const auto& factor : catalog_records.as_array())
        by_name.emplace(text(factor, "name"), factor);
    Json factors = Json::array();
    Json unresolved = Json::array();
    if (selected) {
        const auto* names = field(*selected, "selected_factors");
        if (names && names->is_array()) {
            for (const auto& name_value : names->as_array()) {
                if (!name_value.is_string()) continue;
                const auto found = by_name.find(name_value.as_string());
                if (found == by_name.end()) unresolved.push_back(name_value);
                else factors.push_back(found->second);
            }
        }
    }
    Json result = Json::object();
    result["found"] = selected != nullptr;
    result["dashboard_record"] = selected ? *selected : Json(nullptr);
    result["factors"] = std::move(factors);
    result["unresolved_factor_names"] = std::move(unresolved);
    return result;
}

Json pattern_hits_for_security(const Json& pattern_matrix_records,
                               const std::string& raw_market,
                               const std::string& code) {
    if (!pattern_matrix_records.is_array())
        throw Error("pattern reverse lookup requires a pattern matrix array");
    const auto market = market_name(market_id(raw_market));
    const auto security_id = market_prefix(market_id(market)) + code;
    Json factors = Json::array();
    Json evidence = Json::array();
    for (const auto& matrix : pattern_matrix_records.as_array()) {
        const auto* factor = field(matrix, "factor");
        const auto* members = field(matrix, "members");
        if (!factor || !factor->is_object() || !members || !members->is_array()) continue;
        for (const auto& member : members->as_array()) {
            const auto* security = field(member, "security");
            if (!security || text(*security, "security_id") != security_id) continue;
            factors.push_back(*factor);
            Json item = Json::object();
            item["factor_id"] = text(*factor, "factor_id");
            item["factor_name"] = text(*factor, "name");
            item["member_record"] = member;
            evidence.push_back(std::move(item));
            break;
        }
    }
    Json result = Json::object();
    result["security_id"] = security_id;
    result["factors"] = std::move(factors);
    result["evidence"] = std::move(evidence);
    return result;
}

}  // namespace tdx