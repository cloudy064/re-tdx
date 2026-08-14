#include "technical_signals_internal.hpp"
#include "tdx/time.hpp"

#include "tdx/common.hpp"
#include "tdx/market.hpp"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <optional>
#include <set>
#include <sstream>

namespace fs = std::filesystem;

namespace tdx::technical_signals_detail {
fs::path native_path(const std::string& value) {
#ifdef _WIN32
    return fs::path(utf8_to_wide(value));
#else
    return fs::path(value);
#endif
}

std::string now_text() {
    const auto now = std::time(nullptr);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    std::ostringstream output;
    output << local_timestamp_text(local);
    return output.str();
}

const Json* field(const Json& row, std::string_view key) {
    if (!row.is_object()) return nullptr;
    const auto exact = row.as_object().find(key);
    if (exact != row.as_object().end()) return &exact->second;
    const auto wanted = lower_ascii(std::string(key));
    for (const auto& [name, value] : row.as_object())
        if (lower_ascii(name) == wanted) return &value;
    return nullptr;
}

const Json* prefix_field(const Json& row, std::string_view prefix) {
    if (!row.is_object()) return nullptr;
    const auto wanted = lower_ascii(std::string(prefix));
    for (const auto& [name, value] : row.as_object())
        if (lower_ascii(name).rfind(wanted, 0) == 0) return &value;
    return nullptr;
}

std::string text(const Json& row, std::string_view key) {
    const auto* value = field(row, key);
    if (!value || value->is_null()) return {};
    if (value->is_string()) return trim(value->as_string());
    if (value->is_number()) {
        std::ostringstream output; output << std::setprecision(15) << value->as_number();
        return output.str();
    }
    if (value->is_bool()) return value->as_bool() ? "true" : "false";
    return {};
}

std::optional<double> number_value(const Json* value) {
    if (!value || value->is_null()) return std::nullopt;
    if (value->is_number()) return value->as_number();
    if (!value->is_string()) return std::nullopt;
    auto raw = trim(value->as_string());
    raw.erase(std::remove(raw.begin(), raw.end(), ','), raw.end());
    if (!raw.empty() && raw.back() == '%') raw.pop_back();
    if (raw.empty() || raw == "--" || raw == "-") return std::nullopt;
    try {
        std::size_t used = 0;
        const auto value_number = std::stod(raw, &used);
        if (used == raw.size() && std::isfinite(value_number)) return value_number;
    } catch (...) {}
    return std::nullopt;
}

std::optional<double> number(const Json& row, std::string_view key) {
    return number_value(field(row, key));
}

Json number_json(const std::optional<double>& value) {
    return value ? Json(*value) : Json(nullptr);
}

std::string date_text(std::string raw) {
    raw = trim(std::move(raw));
    if (raw.size() == 8 && std::all_of(raw.begin(), raw.end(), ::isdigit))
        return raw.substr(0, 4) + "-" + raw.substr(4, 2) + "-" + raw.substr(6, 2);
    return raw;
}

std::string time_text(std::string raw) {
    raw = trim(std::move(raw));
    if (raw.empty()) return raw;
    if (raw.size() < 6 && std::all_of(raw.begin(), raw.end(), ::isdigit))
        raw.insert(raw.begin(), 6 - raw.size(), '0');
    if (raw.size() == 6 && std::all_of(raw.begin(), raw.end(), ::isdigit))
        return raw.substr(0, 2) + ":" + raw.substr(2, 2) + ":" + raw.substr(4, 2);
    return raw;
}

int market_id(std::string value) {
    value = lower_ascii(trim(std::move(value)));
    if (value == "sz") return 0;
    if (value == "sh") return 1;
    if (value == "bj") return 2;
    try {
        std::size_t used = 0;
        int result = std::stoi(value, &used);
        if (used != value.size() || (result != 0 && result != 1 && result != 2 && result != 44))
            throw std::invalid_argument("market");
        return result == 44 ? 2 : result;
    } catch (...) { throw Error("technical signal row has an invalid market: " + value); }
}

std::string market_name(int id) { return id == 0 ? "sz" : id == 1 ? "sh" : "bj"; }
std::string market_prefix(int id) { return id == 0 ? "SZ" : id == 1 ? "SH" : "BJ"; }

Json security_document(const Json& row, const BlockData& blocks) {
    const auto code = text(row, "code");
    const int id = market_id(text(row, "market"));
    auto name = text(row, "name");
    const auto found = blocks.securities.find({id, code});
    if (name.empty() && found != blocks.securities.end()) name = found->second.name;
    Json result = Json::object();
    result["entity_type"] = "security";
    result["market"] = market_name(id);
    result["market_id"] = id;
    result["code"] = code;
    result["security_id"] = market_prefix(id) + code;
    result["name"] = name;
    result["name_resolved"] = !name.empty();
    return result;
}

Json block_document(const Json& row, const BlockData& blocks) {
    const auto code = text(row, "code");
    auto name = text(row, "name");
    const Block* selected = nullptr;
    for (const auto& block : blocks.blocks) {
        if (block.block_code != code) continue;
        if (!selected || block.family == "research-industry") selected = &block;
    }
    if (name.empty() && selected) name = selected->name;
    Json result = Json::object();
    result["entity_type"] = "block";
    result["code"] = code;
    result["name"] = name;
    result["name_resolved"] = !name.empty();
    result["upstream_market"] = text(row, "market");
    result["block_id"] = selected ? Json(selected->block_id) : Json(nullptr);
    result["family"] = selected ? Json(selected->family) : Json(nullptr);
    return result;
}

void add_rps_period(Json& periods, int days, const Json& row,
                    std::string_view rps, std::string_view growth) {
    Json period = Json::object();
    period["days"] = days;
    period["rps"] = number_json(number(row, rps));
    period["growth_pct"] = number_json(number(row, growth));
    periods.push_back(std::move(period));
}

const ModelStrategyDefinition* model_strategy(const std::string& view) {
    const auto found = std::find_if(model_strategies.begin(), model_strategies.end(),
        [&](const ModelStrategyDefinition& item) { return view == item.view; });
    return found == model_strategies.end() ? nullptr : &*found;
}

const AuctionStrategyDefinition* auction_strategy(const std::string& view) {
    const auto found = std::find_if(auction_strategies.begin(), auction_strategies.end(),
        [&](const AuctionStrategyDefinition& item) { return view == item.view; });
    return found == auction_strategies.end() ? nullptr : &*found;
}

const FactorSignalDefinition* factor_signal(const std::string& view) {
    const auto found = std::find_if(factor_signals.begin(), factor_signals.end(),
        [&](const FactorSignalDefinition& item) { return view == item.view; });
    return found == factor_signals.end() ? nullptr : &*found;
}

const OpportunityDefinition* opportunity_signal(const std::string& view) {
    const auto found = std::find_if(opportunity_signals.begin(), opportunity_signals.end(),
        [&](const OpportunityDefinition& item) { return view == item.view; });
    return found == opportunity_signals.end() ? nullptr : &*found;
}

Json factor_signal_semantics(const FactorSignalDefinition& definition,
                             const std::string& signal_code) {
    Json result = Json::object();
    result["code"] = signal_code;
    if (signal_code == definition.first_code) {
        result["label"] = definition.first_label;
        result["polarity"] = definition.first_polarity;
    } else if (signal_code == definition.second_code) {
        result["label"] = definition.second_label;
        result["polarity"] = definition.second_polarity;
    } else {
        result["label"] = "unknown";
        result["polarity"] = "unknown";
    }
    return result;
}

std::string factor_time_text(std::string raw) {
    raw = trim(std::move(raw));
    if (raw.size() == 4 && std::all_of(raw.begin(), raw.end(), ::isdigit))
        return raw.substr(0, 2) + ":" + raw.substr(2, 2);
    return time_text(std::move(raw));
}

bool known_view(const std::string& view) {
    return std::find(technical_signal_views.begin(), technical_signal_views.end(), view) !=
           technical_signal_views.end();
}

bool passes_client_filter(const Json& record, const TechnicalSignalsQuery& query) {
    const auto view = lower_ascii(query.view);
    if (!query.apply_client_filters) return true;
    if (view == "nine-turn") {
        const auto direction = lower_ascii(query.direction);
        return direction == "all" || record.at("direction").as_string() == direction;
    }
    if (view == "new-high" || view == "new-low" || view == "breakout" ||
        view == "strong-start" || factor_signal(view) ||
        (opportunity_signal(view) && opportunity_signal(view)->safety_filter)) {
        const auto score = number(record, "safety_score");
        return score && *score >= 60.0;
    }
    if (view == "trend-up") {
        const auto duration = number(record, "duration_days");
        const auto gain = number(record, "since_start_pct");
        const auto support = number(record, "support_price");
        const auto score = number(record, "overall_safety_score");
        return duration && *duration != 0.0 && gain && *gain > 0.0 &&
               support && *support != 0.0 && score && *score >= 60.0;
    }
    if (view == "trend-down") {
        const auto duration = number(record, "duration_days");
        return duration && *duration >= 50.0;
    }
    if (view == "limit-up-gap") {
        const auto style = number(record, "auction_style_code");
        return style && *style == 1.0;
    }
    return true;
}

Json client_filters(const TechnicalSignalsQuery& query) {
    Json result = Json::array();
    if (!query.apply_client_filters) return result;
    const auto view = lower_ascii(query.view);
    if (view == "nine-turn" && lower_ascii(query.direction) != "all")
        result.push_back("direction=" + lower_ascii(query.direction));
    if (view == "new-high" || view == "new-low" || view == "breakout" ||
        view == "strong-start" || factor_signal(view) ||
        (opportunity_signal(view) && opportunity_signal(view)->safety_filter))
        result.push_back("safety_score>=60");
    if (view == "trend-up") {
        result.push_back("duration_days!=0"); result.push_back("since_start_pct>0");
        result.push_back("support_price!=0"); result.push_back("overall_safety_score>=60");
    }
    if (view == "trend-down") result.push_back("duration_days>=50");
    if (view == "limit-up-gap") result.push_back("auction_style_code=1 (涨停高开)");
    return result;
}

int bounded(const std::string& raw, std::string_view name, int minimum, int maximum) {
    try {
        std::size_t used = 0; const int value = std::stoi(raw, &used);
        if (used != raw.size() || value < minimum || value > maximum)
            throw std::invalid_argument("range");
        return value;
    } catch (...) {
        throw Error(std::string(name) + " must be in " + std::to_string(minimum) + ".." +
                    std::to_string(maximum));
    }
}

int board_id(const std::string& raw) {
    const auto value = lower_ascii(trim(raw));
    if (value == "main" || value == "1") return 1;
    if (value == "gem" || value == "chinext" || value == "2") return 2;
    if (value == "star" || value == "kcb" || value == "3") return 3;
    if (value == "bj" || value == "bse" || value == "4") return 4;
    throw Error("board must be main, gem, star, or bj");
}

std::string board_name(int id) {
    return id == 1 ? "main" : id == 2 ? "gem" : id == 3 ? "star" : "bj";
}

std::string cache_key(const TechnicalSignalsQuery& query) {
    std::ostringstream key;
    key << lower_ascii(query.view) << '|' << lower_ascii(query.direction) << '|'
        << lower_ascii(query.board) << '|' << query.duration1 << '|' << query.rps1 << '|'
        << query.duration2 << '|' << query.rps2 << '|' << query.duration3 << '|'
        << query.rps3 << '|' << query.index_period << '|' << query.history_period << '|'
           << query.retracement << '|' << query.sideways_period << '|' << query.amplitude << '|'
        << query.breakout_period << '|' << query.apply_client_filters << '|'
        << query.enrich_quotes << '|' << query.limit;
    return key.str();
}

Json parameters_document(const TechnicalSignalsQuery& query) {
    Json value = Json::object();
    const auto view = lower_ascii(query.view);
    if (view == "nine-turn") value["direction"] = lower_ascii(query.direction);
    if (view == "rps-stock" || view == "rps-block") {
        Json periods = Json::array();
        for (const auto [days, threshold] : std::vector<std::pair<int, int>>{
                 {query.duration1, query.rps1}, {query.duration2, query.rps2},
                 {query.duration3, query.rps3}}) {
            Json period = Json::object(); period["days"] = days;
            period["minimum_rps"] = threshold; periods.push_back(std::move(period));
        }
        value["periods"] = std::move(periods);
    }
    if (view == "new-high" || view == "new-low") {
        value["index_period"] = query.index_period;
        value["history_period"] = query.history_period;
        value["retracement_pct"] = query.retracement;
    }
    if (view == "breakout") {
        value["sideways_period"] = query.sideways_period;
        value["amplitude_pct"] = query.amplitude;
        value["breakout_period"] = query.breakout_period;
    }
    if (view == "trend-up" || view == "trend-down")
        value["board"] = board_name(board_id(query.board));
    if (const auto* strategy = model_strategy(view)) {
        value["strategy_title"] = strategy->title;
        value["xg_name"] = strategy->xg_name;
        value["client_rule_disclosed"] = strategy->rule[0] != '\0';
        value["client_rule"] = strategy->rule[0] == '\0' ? Json(nullptr) : Json(strategy->rule);
    }
    if (const auto* strategy = auction_strategy(view)) {
        value["strategy_title"] = strategy->title;
        value["request_id"] = strategy->request_id;
        value["client_rule_disclosed"] = false;
        value["client_rule"] = Json(nullptr);
        value["client_filter_disclosed"] = view == "limit-up-gap";
    }
    if (const auto* signal = factor_signal(view)) {
        value["strategy_title"] = signal->title;
        value["request_id"] = "200662";
        value["flag"] = signal->flag;
        value["client_rule_disclosed"] = true;
        value["client_rule"] = signal->rule;
        value["refresh_interval_seconds"] = 30;
        value["quote_enrichment_requested"] = query.enrich_quotes;
    }
    if (const auto* opportunity = opportunity_signal(view)) {
        value["strategy_title"] = opportunity->title;
        value["request_id"] = "200661";
        value["flag"] = opportunity->flag;
        value["client_rule_disclosed"] = true;
        value["client_rule"] = opportunity->rule;
        value["refresh_interval_seconds"] = 10;
        value["quote_enrichment_requested"] = query.enrich_quotes;
    }
    value["client_filters_applied"] = query.apply_client_filters;
    return value;
}


}  // namespace tdx::technical_signals_detail
