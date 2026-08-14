#include "bond_reference_internal.hpp"

namespace tdx::bond_reference_detail {

int market_id(const std::string& value) {
    try {
        std::size_t used = 0;
        const int parsed = std::stoi(trim(value), &used);
        if (used == trim(value).size() && parsed >= 0 && parsed <= 255) return parsed;
    } catch (...) {}
    throw Error("bond reference market is invalid: " + value);
}

std::string market_name(int market) {
    if (market == 0) return "sz";
    if (market == 1) return "sh";
    if (market == 2 || market == 44) return "bj";
    return std::to_string(market);
}

std::string market_prefix(int market) {
    if (market == 0) return "SZ";
    if (market == 1) return "SH";
    if (market == 2 || market == 44) return "BJ";
    return "M" + std::to_string(market) + ":";
}

Json string_array(const std::string& source) {
    Json result = Json::array();
    for (auto value : split(source, ',')) {
        value = trim(std::move(value));
        if (!value.empty()) result.push_back(std::move(value));
    }
    return result;
}

Json coupon_schedule(const std::string& dates, const std::string& rates) {
    const auto date_values = split(dates, ',');
    const auto rate_values = split(rates, ',');
    Json result = Json::array();
    for (std::size_t index = 0; index < date_values.size(); ++index) {
        const auto date = trim(date_values[index]);
        if (date.empty()) continue;
        Json row = Json::object();
        row["date"] = date;
        if (index < rate_values.size()) {
            try {
                std::size_t used = 0;
                const auto raw = std::stod(trim(rate_values[index]), &used);
                row["rate_pct"] = used == trim(rate_values[index]).size() &&
                    std::isfinite(raw) ? Json(std::abs(raw) <= 1.0 ? raw * 100.0 : raw)
                                       : Json(nullptr);
            } catch (...) { row["rate_pct"] = nullptr; }
        } else row["rate_pct"] = nullptr;
        result.push_back(std::move(row));
    }
    return result;
}

Json normalize_row(const Json& raw, const BondReferenceSource& source,
                   bool retain_raw, std::size_t raw_index) {
    const auto profile = resource_profile(source);
    const bool policy_financial =
        profile.scale == ScaleSemantics::issue_100m_yuan;
    const bool reference_master = profile.reference_master;
    const auto code = text_value(raw, reference_master ? "$ZQDM1" : "$ZQDM");
    const auto market = market_id(text_value(raw, reference_master ? "$SC1" : "$SC"));
    Json security = Json::object();
    security["market"] = market_name(market);
    security["market_id"] = market;
    security["code"] = code;
    security["security_id"] = market_prefix(market) + code;
    security["name"] = text_value(raw, "ZQJC");
    security["name_resolved"] = !security.at("name").as_string().empty();
    Json row = Json::object();
    row["security"] = std::move(security);
    row["source_group"] = source.group;
    row["source_bucket"] = source.bucket;
    row["source_name"] = source.name;
    row["client_instrument_id"] = reference_master
        ? Json(text_value(raw, "$ZQDM")) : Json(nullptr);
    row["bond_type"] = text_value(raw, "ZQLX");
    row["bond_credit_rating"] = first_text_value(raw, {"ZQXY", "ZQPJ"});
    row["issuer_credit_rating"] = first_text_value(raw, {"ZTXY", "ZTPJ"});
    row["rate_type"] = text_value(raw, "LLLX");
    row["rate_type_flag"] = text_value(raw, "LLLXBZ");
    row["accrual_start_date"] = first_text_value(raw, {"QXSJ", "QXRQ"});
    row["maturity_date"] = first_text_value(raw, {"DQSJ", "DQRQ"});
    row["next_coupon_date"] = text_value(raw, "XGFXRQ");
    row["last_coupon_date"] = text_value(raw, "SGFXRQ");
    row["remaining_years"] = number_value(raw, "SYNX")
        ? Json(*number_value(raw, "SYNX")) : Json(nullptr);
    row["current_coupon_rate_pct"] = number_value(raw, "DQLL")
        ? Json(*number_value(raw, "DQLL")) : Json(nullptr);
    row["coupon_frequency_months"] = number_value(raw, "FXPL1")
        ? Json(*number_value(raw, "FXPL1")) : Json(nullptr);
    row["face_value_yuan"] = number_value(raw, "MZ")
        ? Json(*number_value(raw, "MZ")) : Json(nullptr);
    row["issue_price_yuan"] = number_value(raw, "FXJG")
        ? Json(*number_value(raw, "FXJG")) : Json(nullptr);
    const auto issue_size = number_value(raw, "GM");
    const bool category_market_projection =
        profile.scale == ScaleSemantics::outstanding_100m_yuan;
    const bool ambiguous_client_master =
        profile.scale == ScaleSemantics::client_master_hidden_unit;
    row["source_scale_raw"] = issue_size ? Json(*issue_size) : Json(nullptr);
    row["source_scale_semantics"] = policy_financial
        ? "issue-size-100m-yuan"
        : category_market_projection
            ? "outstanding-balance-100m-yuan"
            : ambiguous_client_master
                ? "client-master-hidden-unit"
                : "issue-size-yuan";
    row["issue_size_source_100m"] = policy_financial && issue_size
        ? Json(*issue_size) : Json(nullptr);
    row["issue_size_yuan"] = issue_size && !category_market_projection &&
            !ambiguous_client_master
        ? Json(*issue_size * (policy_financial ? 100000000.0 : 1.0))
        : Json(nullptr);
    row["outstanding_balance_source_100m"] =
        category_market_projection && issue_size ? Json(*issue_size) : Json(nullptr);
    row["outstanding_balance_yuan"] =
        category_market_projection && issue_size
        ? Json(*issue_size * 100000000.0) : Json(nullptr);
    row["guarantee_status"] = text_value(raw, "SFDB");
    const auto underlying_code = reference_master
        ? std::string{} : text_value(raw, "$ZQDM1");
    const auto underlying_market_text = reference_master
        ? std::string{} : text_value(raw, "$SC1");
    if (!underlying_code.empty() && !underlying_market_text.empty()) {
        const auto underlying_market = market_id(underlying_market_text);
        Json underlying = Json::object();
        underlying["market"] = market_name(underlying_market);
        underlying["market_id"] = underlying_market;
        underlying["code"] = underlying_code;
        underlying["security_id"] = market_prefix(underlying_market) + underlying_code;
        underlying["name"] = "";
        underlying["name_resolved"] = false;
        row["underlying"] = std::move(underlying);
    } else row["underlying"] = nullptr;
    row["listing_date"] = text_value(raw, "SSRQ");
    row["conversion_start_date"] = text_value(raw, "ZGQSR");
    row["conversion_end_date"] = text_value(raw, "ZGJZR");
    const auto conversion_price = first_number_value(raw, {"ZGJ"});
    row["conversion_price_yuan"] = conversion_price
        ? Json(*conversion_price) : Json(nullptr);
    const auto revision_trigger = first_number_value(raw, {"XXCFBL"});
    row["revision_trigger_pct"] = revision_trigger
        ? Json(*revision_trigger) : Json(nullptr);
    const auto put_trigger = first_number_value(raw, {"HSCFBL"});
    row["put_trigger_pct"] = put_trigger ? Json(*put_trigger) : Json(nullptr);
    const auto call_trigger = first_number_value(raw, {"QSCFBL"});
    row["call_trigger_pct"] = call_trigger ? Json(*call_trigger) : Json(nullptr);
    row["remaining_coupon_count"] = number_value(raw, "SYFXCS")
        ? Json(*number_value(raw, "SYFXCS")) : Json(nullptr);
    // The all-bond master contains long historical coupon sequences.  Filtering
    // and sorting only need scalar terms, so defer those arrays until a row is
    // actually returned to the caller.
    if (retain_raw) {
        row["coupon_dates"] = string_array(text_value(raw, "FXRQXL"));
        row["coupon_schedule"] = coupon_schedule(text_value(raw, "FXRQXL"),
                                                  text_value(raw, "FXLLXL"));
        row["remaining_coupon_schedule"] = coupon_schedule(
            text_value(raw, "SYFXRQXL"), text_value(raw, "SYFXLLXL"));
    } else {
        row["coupon_dates"] = Json::array();
        row["coupon_schedule"] = Json::array();
        row["remaining_coupon_schedule"] = Json::array();
    }
    row["source_resource"] = source.resource;
    if (retain_raw) row["raw"] = raw;
    else row["_raw_index"] = static_cast<std::uint64_t>(raw_index);
    return row;
}


}  // namespace tdx::bond_reference_detail

namespace tdx {

Json normalize_bond_reference_rows(const Json& rows,
                                   const BondReferenceSource& source) {
    if (!rows.is_array()) throw Error("bond-reference rows must be an array");
    Json result = Json::array();
    for (std::size_t index = 0; index < rows.size(); ++index)
        result.push_back(bond_reference_detail::normalize_row(
            rows.as_array()[index], source, true, index));
    return result;
}

}  // namespace tdx
