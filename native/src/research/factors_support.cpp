#include "factors_internal.hpp"
#include "tdx/time.hpp"

#include "tdx/cloud_resilience.hpp"
#include "tdx/common.hpp"
#include "tdx/market.hpp"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>

namespace fs = std::filesystem;

namespace tdx::factor_detail {

const ViewDefinition* view_definition(const std::string& view) {
    for (const auto& value : view_definitions)
        if (view == value.view) return &value;
    return nullptr;
}
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

std::string text(const Json& row, std::string_view key) {
    const auto* value = field(row, key);
    if (!value || value->is_null()) return {};
    if (value->is_string()) return trim(value->as_string());
    if (value->is_number()) {
        std::ostringstream output;
        output << std::setprecision(15) << value->as_number();
        return output.str();
    }
    return {};
}

std::optional<double> number(const Json& row, std::string_view key) {
    const auto* value = field(row, key);
    if (!value || value->is_null()) return std::nullopt;
    if (value->is_number()) return value->as_number();
    if (!value->is_string()) return std::nullopt;
    auto raw = trim(value->as_string());
    raw.erase(std::remove(raw.begin(), raw.end(), ','), raw.end());
    if (!raw.empty() && raw.back() == '%') raw.pop_back();
    if (raw.empty() || raw == "-" || raw == "--") return std::nullopt;
    try {
        std::size_t used = 0;
        const auto result = std::stod(raw, &used);
        if (used == raw.size() && std::isfinite(result)) return result;
    } catch (...) {}
    return std::nullopt;
}

Json number_json(const std::optional<double>& value) {
    return value ? Json(*value) : Json(nullptr);
}

int market_id(std::string raw) {
    raw = lower_ascii(trim(std::move(raw)));
    if (raw == "sz" || raw == "0") return 0;
    if (raw == "sh" || raw == "1") return 1;
    if (raw == "bj" || raw == "2" || raw == "44") return 2;
    throw Error("factor row has invalid TDX market: " + raw);
}

std::string market_name(int market) {
    return market == 0 ? "sz" : market == 1 ? "sh" : "bj";
}

std::string market_prefix(int market) {
    return market == 0 ? "SZ" : market == 1 ? "SH" : "BJ";
}

Json security_document(const Json& row, const BlockData& blocks) {
    const auto code = text(row, "code");
    if (code.empty()) throw Error("factor security row has no code");
    const int market = market_id(text(row, "market"));
    auto name = text(row, "name");
    const auto known = blocks.securities.find({market, code});
    if (name.empty() && known != blocks.securities.end()) name = known->second.name;
    Json result = Json::object();
    result["entity_type"] = "security";
    result["market"] = market_name(market);
    result["market_id"] = market;
    result["code"] = code;
    result["security_id"] = market_prefix(market) + code;
    result["name"] = name;
    result["name_resolved"] = !name.empty();
    return result;
}

Json split_factors(const std::string& raw) {
    Json result = Json::array();
    std::string item;
    auto flush = [&]() {
        item = trim(std::move(item));
        if (!item.empty()) result.push_back(item);
        item.clear();
    };
    for (std::size_t i = 0; i < raw.size();) {
        const unsigned char ch = static_cast<unsigned char>(raw[i]);
        if (raw[i] == ',' || raw[i] == ';' || raw[i] == '|' ||
            (i + 2 < raw.size() && ch == 0xE3 &&
             static_cast<unsigned char>(raw[i + 1]) == 0x80 &&
             static_cast<unsigned char>(raw[i + 2]) == 0x81)) {
            flush();
            i += raw[i] == ',' || raw[i] == ';' || raw[i] == '|' ? 1 : 3;
        } else {
            item.push_back(raw[i++]);
        }
    }
    flush();
    return result;
}

Json catalog_record(const Json& row, bool pattern) {
    Json result = Json::object();
    result["factor_id"] = text(row, "ID");
    result["name"] = text(row, "name");
    result["factor_type"] = text(row, "type");
    result["description"] = text(row, "description");
    result["factor_kind"] = pattern ? "pattern" : "standard";
    if (!pattern) {
        const auto direction = text(row, "BullBear");
        result["direction_code"] = direction;
        result["direction"] = direction == "0" ? "bullish" :
            direction == "1" ? "bearish" : "unknown";
        result["family"] = text(row, "formulaName");
        result["formula_output_position"] = number_json(number(row, "pos"));
    }
    Json live = Json::object();
    live["today_return_pct"] = number_json(number(row, "near2dayZDF"));
    live["five_day_return_pct"] = number_json(number(row, "near5dayZDF"));
    result["live_performance"] = std::move(live);
    Json returns = Json::object();
    returns["1"] = number_json(number(row, "A1dZdf"));
    returns["3"] = number_json(number(row, "A3dZdf"));
    returns["5"] = number_json(number(row, "A5dZdf"));
    returns["10"] = number_json(number(row, "A10dZdf"));
    returns["20"] = number_json(number(row, "A20dZdf"));
    returns["60"] = number_json(number(row, "A60dZdf"));
    Json backtest = Json::object();
    backtest["window_days"] = 120;
    backtest["forward_returns_pct"] = std::move(returns);
    backtest["maximum_drawdown_pct"] = number_json(number(row, "maxRetracement"));
    backtest["sharpe_ratio"] = number_json(number(row, "sharpRatio"));
    result["backtest"] = std::move(backtest);
    return result;
}

bool searchable(const Json& record, const std::string& raw_query) {
    if (raw_query.empty()) return true;
    const auto query = lower_ascii(raw_query);
    return lower_ascii(record.dump(-1)).find(query) != std::string::npos;
}

bool transient_error(const std::string& message) {
    return detail::is_transient_cloud_error(message);
}

Json source_document(const Json& upstream, const ViewDefinition& definition,
                     std::size_t row_count, bool all_pages) {
    Json result = Json::object();
    result["transport"] = "TQLEX reqformat=2";
    result["entry"] = upstream.at("entry");
    result["request_id"] = definition.request_id;
    result["source_file"] = upstream.at("source_file");
    result["page_size"] = definition.page_size;
    result["all_pages"] = all_pages;
    result["row_count"] = static_cast<std::uint64_t>(row_count);
    result["raw_response_retained"] = false;
    return result;
}

std::string cache_key(const FactorQuery& query) {
    std::ostringstream output;
    output << query.view << '|' << query.factor_id << '|' << query.query << '|'
           << query.market << '|' << query.code << '|'
           << query.include_patterns << '|' << query.all_pages << '|'
           << query.enrich_quotes << '|'
           << query.limit << '|' << query.max_pages;
    return output.str();
}

int bounded(const std::string& raw, std::string_view name,
            int minimum, int maximum) {
    try {
        std::size_t used = 0;
        const int value = std::stoi(raw, &used);
        if (used != raw.size() || value < minimum || value > maximum)
            throw std::invalid_argument("range");
        return value;
    } catch (...) {
        throw Error(std::string(name) + " must be in " + std::to_string(minimum) +
                    ".." + std::to_string(maximum));
    }
}

Json views_document() {
    Json result = Json::array();
    for (const auto* name : {"catalog", "members", "dashboard", "patterns",
                             "pattern-members", "intraday-radar", "security",
                             "pattern-matrix", "standard-matrix", "overview"}) {
        const auto* definition = view_definition(name);
        Json item = Json::object();
        item["view"] = definition->view;
        item["title"] = definition->title;
        item["scope"] = definition->scope;
        item["request_id"] = definition->request_id;
        item["client_page_size"] = definition->page_size;
        result.push_back(std::move(item));
    }
    return result;
}

Json enrich_quotes(const fs::path& root, Json& records,
                   const BlockData& blocks, int timeout_ms) {
    std::vector<std::string> requested;
    std::set<std::string> seen;
    for (const auto& record : records.as_array()) {
        const auto* security = field(record, "security");
        if (!security || !security->is_object()) continue;
        const auto market = text(*security, "market");
        const auto code = text(*security, "code");
        if ((market != "sz" && market != "sh" && market != "bj") || code.empty())
            continue;
        const auto key = market + ":" + code;
        if (seen.insert(key).second) requested.push_back(key);
    }
    Json result = Json::object();
    result["requested"] = true;
    result["requested_securities"] = static_cast<std::uint64_t>(requested.size());
    if (requested.empty()) {
        result["status"] = "empty";
        result["received_securities"] = 0;
        result["matched_records"] = 0;
        return result;
    }
    const auto quotes = fetch_market_snapshot_document(root, requested, timeout_ms, &blocks);
    std::map<std::string, Json> indexed;
    for (const auto& quote : quotes.at("records").as_array())
        indexed.emplace(text(quote, "security_id"), quote);
    std::size_t matched = 0;
    for (auto& record : records.as_array()) {
        const auto* security = field(record, "security");
        if (!security || !security->is_object()) continue;
        const auto found = indexed.find(text(*security, "security_id"));
        if (found == indexed.end()) continue;
        record["current_quote"] = found->second;
        ++matched;
    }
    result["status"] = "live";
    result["received_securities"] = quotes.at("received");
    result["matched_records"] = static_cast<std::uint64_t>(matched);
    result["generated_at"] = quotes.at("generated_at");
    result["command"] = quotes.at("command");
    result["endpoint"] = quotes.at("endpoint");
    result["server_name"] = quotes.at("server_name");
    return result;
}

}  // namespace tdx::factor_detail
