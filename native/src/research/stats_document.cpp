#include "stats_internal.hpp"

#include <cmath>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace tdx::stats_detail {

template <typename Value>
Json optional_json(const std::optional<Value>& value) {
    return value ? Json(*value) : Json(nullptr);
}

Json stat_json(const TdxStatRow& row) {
    Json value = Json::object();
    value["market_id"] = row.market_id;
    value["code"] = row.code;
    value["stats_date"] = optional_json(row.stats_date);
    value["beta_60d"] = optional_json(row.beta_60d);
    value["pe_ttm"] = optional_json(row.pe_ttm);
    value["pe_static"] = optional_json(row.pe_static);
    value["free_float_shares_10k"] = optional_json(row.free_float_shares_10k);
    value["free_float_shares"] = row.free_float_shares_10k
        ? Json(*row.free_float_shares_10k * 10000.0) : Json(nullptr);
    value["shape_packed"] = optional_json(row.shape_packed);
    value["shape_short"] = optional_json(row.shape_short);
    value["shape_mid"] = optional_json(row.shape_mid);
    value["shape_long"] = optional_json(row.shape_long);
    value["year_limit_up_days"] = optional_json(row.year_limit_up_days);
    value["limit_stat_days"] = optional_json(row.limit_stat_days);
    value["limit_up_count_in_stat_days"] = optional_json(row.limit_up_count_in_stat_days);
    value["limit_up_streak_days"] = optional_json(row.limit_up_streak_days);
    return value;
}

Json stat2_json(const TdxStat2Row& row) {
    Json value = Json::object();
    value["market_id"] = row.market_id;
    value["code"] = row.code;
    value["stats_date"] = optional_json(row.stats_date);
    value["amount_10k"] = optional_json(row.amount_10k);
    value["amount_yuan"] = row.amount_10k ? Json(*row.amount_10k * 10000.0) : Json(nullptr);
    value["seal_amount_10k"] = optional_json(row.seal_amount_10k);
    value["seal_amount_yuan"] = row.seal_amount_10k
        ? Json(*row.seal_amount_10k * 10000.0) : Json(nullptr);
    value["prev_amount_10k"] = optional_json(row.prev_amount_10k);
    value["prev_amount_yuan"] = row.prev_amount_10k
        ? Json(*row.prev_amount_10k * 10000.0) : Json(nullptr);
    value["prev_seal_amount_10k"] = optional_json(row.prev_seal_amount_10k);
    value["prev_seal_amount_yuan"] = row.prev_seal_amount_10k
        ? Json(*row.prev_seal_amount_10k * 10000.0) : Json(nullptr);
    value["prev2_amount_10k"] = optional_json(row.prev2_amount_10k);
    value["prev2_amount_yuan"] = row.prev2_amount_10k
        ? Json(*row.prev2_amount_10k * 10000.0) : Json(nullptr);
    value["prev2_seal_amount_10k"] = optional_json(row.prev2_seal_amount_10k);
    value["prev2_seal_amount_yuan"] = row.prev2_seal_amount_10k
        ? Json(*row.prev2_seal_amount_10k * 10000.0) : Json(nullptr);
    value["open_volume_hand"] = optional_json(row.open_volume_hand);
    value["prev_open_volume_hand"] = optional_json(row.prev_open_volume_hand);
    value["open_amount_10k"] = optional_json(row.open_amount_10k);
    value["open_amount_yuan"] = row.open_amount_10k
        ? Json(*row.open_amount_10k * 10000.0) : Json(nullptr);
    value["prev_open_amount_10k"] = optional_json(row.prev_open_amount_10k);
    value["prev_open_amount_yuan"] = row.prev_open_amount_10k
        ? Json(*row.prev_open_amount_10k * 10000.0) : Json(nullptr);
    return value;
}

Json tip_info_event_json(const TdxTipInfoEventRow& row) {
    Json value = Json::object();
    value["market_id"] = row.market_id;
    value["code"] = row.code;
    value["northbound_date"] = optional_json(row.northbound_date);
    value["northbound_direction"] = optional_json(row.northbound_direction);
    value["repurchase_plan_date"] = optional_json(row.repurchase_plan_date);
    value["repurchase_ratio"] = optional_json(row.repurchase_ratio);
    value["incentive_plan_date"] = optional_json(row.incentive_plan_date);
    return value;
}

const Json* object_member(const Json& value, std::string_view key) {
    if (!value.is_object()) return nullptr;
    const auto found = value.as_object().find(std::string(key));
    return found == value.as_object().end() ? nullptr : &found->second;
}

const Json* record_for(const Json* document, int market_id,
                       const std::string& code) {
    if (!document) return nullptr;
    const auto* records = object_member(*document, "records");
    if (!records || !records->is_array()) return nullptr;
    for (const auto& row : records->as_array()) {
        const auto* market = object_member(row, "market_id");
        const auto* row_code = object_member(row, "code");
        if (market && market->is_number() && row_code && row_code->is_string() &&
            static_cast<int>(market->as_number()) == market_id &&
            row_code->as_string() == code) return &row;
    }
    return nullptr;
}

std::optional<double> number_path(const Json* value,
                                  std::initializer_list<std::string_view> path) {
    for (const auto key : path) {
        if (!value) return std::nullopt;
        value = object_member(*value, key);
    }
    if (!value || !value->is_number() || !std::isfinite(value->as_number()))
        return std::nullopt;
    return value->as_number();
}

std::optional<std::string> text_path(
    const Json* value, std::initializer_list<std::string_view> path) {
    for (const auto key : path) {
        if (!value) return std::nullopt;
        value = object_member(*value, key);
    }
    if (!value || !value->is_string() || value->as_string().empty())
        return std::nullopt;
    return value->as_string();
}

Json valuation_metric(std::string_view code, std::string_view label,
                      const std::optional<double>& value,
                      std::string_view source, const Json& as_of) {
    Json metric = Json::object();
    metric["code"] = std::string(code);
    metric["label"] = std::string(label);
    metric["value"] = optional_json(value);
    metric["source"] = std::string(source);
    metric["as_of"] = as_of;
    return metric;
}

}  // namespace tdx::stats_detail

namespace tdx {

using namespace stats_detail;

Json stats_resource_document(const TdxStatsResource& resource,
                             const std::string& endpoint,
                             const std::string& server_name,
                             std::size_t archive_size,
                             const std::vector<std::string>& securities,
                             const BlockData* block_data,
                             const Json* transport) {
    std::set<std::pair<int, std::string>> keys;
    if (securities.empty()) {
        for (const auto& [key, row] : resource.stat) { (void)row; keys.insert(key); }
        for (const auto& [key, row] : resource.stat2) { (void)row; keys.insert(key); }
        for (const auto& [key, row] : resource.tip_info_events) {
            (void)row;
            keys.insert(key);
        }
    } else {
        for (const auto& security : securities) keys.insert(parse_security(security).key());
    }
    Json records = Json::array();
    for (const auto& key : keys) {
        const auto stat = resource.stat.find(key);
        const auto stat2 = resource.stat2.find(key);
        const auto tip = resource.tip_info_events.find(key);
        if (stat == resource.stat.end() && stat2 == resource.stat2.end() &&
            tip == resource.tip_info_events.end()) continue;
        Json record = Json::object();
        record["market_id"] = key.first;
        record["code"] = key.second;
        record["security_id"] = security_id(key.first, key.second);
        if (block_data) {
            const auto name = block_data->securities.find(key);
            record["name"] = name == block_data->securities.end() ? "" : name->second.name;
            record["name_resolved"] = name != block_data->securities.end();
        } else {
            record["name"] = "";
            record["name_resolved"] = false;
        }
        record["stat"] = stat == resource.stat.end() ? Json(nullptr) : stat_json(stat->second);
        record["stat2"] = stat2 == resource.stat2.end() ? Json(nullptr) : stat2_json(stat2->second);
        record["tip_info_event"] = tip == resource.tip_info_events.end()
            ? Json(nullptr) : tip_info_event_json(tip->second);
        records.push_back(std::move(record));
    }
    Json document = Json::object();
    document["schema"] = "tdx-stats-native-v1";
    document["generated_at"] = now_text();
    document["command"] = endpoint.empty() ? Json(nullptr) : Json("0x06B9");
    document["source_path"] = resource.source_path;
    document["endpoint"] = endpoint;
    document["server_name"] = server_name;
    document["archive_size"] = static_cast<std::uint64_t>(archive_size);
    if (transport) document["transport"] = *transport;
    document["stats_date"] = optional_json(resource.stats_date);
    document["stats_date_coverage"] = resource.stats_date_coverage;
    document["stat_count"] = static_cast<std::uint64_t>(resource.stat.size());
    document["stat2_count"] = static_cast<std::uint64_t>(resource.stat2.size());
    document["tip_info_count"] =
        static_cast<std::uint64_t>(resource.tip_info_events.size());
    std::uint64_t northbound_event_count = 0;
    std::uint64_t repurchase_event_count = 0;
    std::uint64_t incentive_event_count = 0;
    for (const auto& [key, row] : resource.tip_info_events) {
        (void)key;
        if (row.northbound_date && row.northbound_direction &&
            std::abs(*row.northbound_direction) > 0.0001) ++northbound_event_count;
        if (row.repurchase_plan_date) ++repurchase_event_count;
        if (row.incentive_plan_date) ++incentive_event_count;
    }
    document["tip_info_northbound_event_count"] = northbound_event_count;
    document["tip_info_repurchase_event_count"] = repurchase_event_count;
    document["tip_info_incentive_event_count"] = incentive_event_count;
    document["requested_securities"] = static_cast<std::uint64_t>(securities.size());
    document["returned"] = static_cast<std::uint64_t>(records.size());
    document["records"] = std::move(records);
    return document;
}

Json derive_security_valuation_document(
    int market_id, const std::string& code, const TdxStatRow* stat,
    const Json* snapshot_document, const Json* finance_document,
    const std::vector<std::string>& errors) {
    validate_security(market_id, code);
    const auto* quote = record_for(snapshot_document, market_id, code);
    const auto* finance = record_for(finance_document, market_id, code);

    constexpr float threshold = 0.00009999999747378752f;
    std::optional<double> price;
    std::string price_field;
    if (const auto last = number_path(quote, {"last_price"}); last && *last > threshold) {
        price = *last;
        price_field = "last_price";
    } else if (const auto previous = number_path(quote, {"pre_close_price"});
               previous && *previous > threshold) {
        price = *previous;
        price_field = "pre_close_price";
    }

    const auto total_shares = number_path(finance, {"shares", "total"});
    const auto net_profit = number_path(
        finance, {"income_statement", "net_profit_yuan"});
    const auto report_months = number_path(finance, {"reserved_2"});
    std::optional<double> annualized_eps;
    std::optional<double> pe_dynamic;
    if (price && total_shares && net_profit && report_months &&
        *total_shares > 1.0 && *report_months > 0.0) {
        const float value = static_cast<float>(
            *net_profit * 12.0 / *report_months / *total_shares);
        if (value > threshold) {
            annualized_eps = static_cast<double>(value);
            pe_dynamic = static_cast<double>(
                static_cast<float>(*price / static_cast<double>(value)));
        }
    }

    auto net_assets_per_share = number_path(finance, {"per_share", "net_assets"});
    std::string nav_source = "0x0010/per_share.net_assets";
    if ((!net_assets_per_share || std::abs(*net_assets_per_share) < threshold) &&
        total_shares && *total_shares > 1.0) {
        if (const auto net_assets = number_path(
                finance, {"balance_sheet", "net_assets_yuan"})) {
            net_assets_per_share = *net_assets / *total_shares;
            nav_source = "0x0010/net_assets_yuan/total_shares";
        }
    }
    std::optional<double> pb_mrq;
    if (price && net_assets_per_share && *net_assets_per_share > threshold)
        pb_mrq = static_cast<double>(static_cast<float>(
            *price / *net_assets_per_share));

    const auto host_float = [](const std::optional<double>& value) {
        return value ? std::optional<double>(static_cast<double>(
                           static_cast<float>(*value)))
                     : std::nullopt;
    };
    const auto pe_static = stat ? host_float(stat->pe_static) : std::nullopt;
    const auto pe_ttm = stat ? host_float(stat->pe_ttm) : std::nullopt;
    const Json stats_as_of = stat && stat->stats_date
        ? Json(*stat->stats_date) : Json(nullptr);
    const auto quote_as_of = text_path(snapshot_document, {"generated_at"});
    const Json live_as_of = quote_as_of ? Json(*quote_as_of) : Json(nullptr);

    Json metrics = Json::object();
    metrics["pe_dynamic"] = valuation_metric(
        "$PE", "动态市盈率", pe_dynamic,
        "TdxW type120/sub_5956D0: L1 price / annualized report-period EPS",
        live_as_of);
    metrics["pe_static"] = valuation_metric(
        "$PES", "静态市盈率", pe_static,
        "TdxW type163+95 <- cache+117 <- zhb.zip/tdxstat.cfg field 10",
        stats_as_of);
    metrics["pe_ttm"] = valuation_metric(
        "$PETTM", "市盈率(TTM)", pe_ttm,
        "TdxW type163+99 <- cache+113 <- zhb.zip/tdxstat.cfg field 4",
        stats_as_of);
    metrics["pb_mrq"] = valuation_metric(
        "$PBMRQ", "市净率(MRQ)", pb_mrq,
        "TdxW type163+380: L1 price / latest net assets per share",
        live_as_of);

    std::uint64_t available = 0;
    for (const auto* key : {"pe_dynamic", "pe_static", "pe_ttm", "pb_mrq"})
        if (!metrics.at(key).at("value").is_null()) ++available;
    Json inputs = Json::object();
    inputs["price"] = optional_json(price);
    inputs["price_field"] = price_field.empty() ? Json(nullptr) : Json(price_field);
    inputs["annualized_eps"] = optional_json(annualized_eps);
    inputs["net_profit_yuan"] = optional_json(net_profit);
    inputs["report_months"] = optional_json(report_months);
    inputs["total_shares"] = optional_json(total_shares);
    inputs["net_assets_per_share"] = optional_json(net_assets_per_share);
    inputs["net_assets_per_share_source"] = nav_source;
    inputs["finance_updated_date"] = text_path(finance, {"updated_date"})
        ? Json(*text_path(finance, {"updated_date"})) : Json(nullptr);
    inputs["stats_date"] = stats_as_of;

    Json error_values = Json::array();
    for (const auto& error : errors) error_values.push_back(error);
    Json document = Json::object();
    document["schema"] = "tdx-security-valuation-native-v1";
    document["generated_at"] = now_text();
    document["market_id"] = market_id;
    document["code"] = code;
    document["security_id"] = security_id(market_id, code);
    document["availability"] = available == 4 ? "complete"
        : available ? "partial" : "unavailable";
    document["available_metric_count"] = available;
    document["metrics"] = std::move(metrics);
    document["inputs"] = std::move(inputs);
    document["errors"] = std::move(error_values);
    return document;
}


}  // namespace tdx
