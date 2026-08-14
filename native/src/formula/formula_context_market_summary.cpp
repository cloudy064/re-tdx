#include "formula_context_market_summary_internal.hpp"

#include "tdx/common.hpp"
#include "tdx/daily.hpp"
#include "tdx/market.hpp"
#include "tdx/minute.hpp"
#include "tdx/stats.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <ctime>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string_view>

namespace tdx::formula_context_detail {
namespace {

const Json* optional(const Json& object, std::string_view key) {
    if (!object.is_object()) return nullptr;
    const auto found = object.as_object().find(key);
    return found == object.as_object().end() ? nullptr : &found->second;
}

std::string text_or(const Json& object, std::string_view key,
                    std::string fallback = {}) {
    const auto* value = optional(object, key);
    return value && value->is_string() ? value->as_string()
                                       : std::move(fallback);
}

int integer_or(const Json& object, std::string_view key, int fallback) {
    const auto* value = optional(object, key);
    return value && value->is_number()
        ? static_cast<int>(value->as_number()) : fallback;
}

void bind_formula_scalar(Json& context, const std::string& name,
                         const std::optional<double>& value) {
    if (!context.as_object().count("formula_scalar_bindings"))
        context["formula_scalar_bindings"] = Json::object();
    context["formula_scalar_bindings"][name] =
        value ? Json(*value) : Json(nullptr);
}

}  // namespace

struct FormulaStatsCacheEntry {
    std::shared_ptr<const TdxStatsResource> resource;
    std::filesystem::file_time_type stat_time{};
    std::filesystem::file_time_type stat2_time{};
    bool local{};
    std::time_t loaded_at{};
};

std::shared_ptr<const TdxStatsResource> cached_formula_stats_resource(
    const std::filesystem::path& root, int timeout_ms) {
    static std::mutex mutex;
    static std::map<std::string, FormulaStatsCacheEntry> cache;
    const auto directory = root / "T0002" / "hq_cache";
    const auto stat_path = directory / "tdxstat.cfg";
    const auto stat2_path = directory / "tdxstat2.cfg";
    const bool local = std::filesystem::is_regular_file(stat_path) &&
                       std::filesystem::is_regular_file(stat2_path);
    const auto stat_time = local ? std::filesystem::last_write_time(stat_path)
                                 : std::filesystem::file_time_type{};
    const auto stat2_time = local ? std::filesystem::last_write_time(stat2_path)
                                  : std::filesystem::file_time_type{};
    const auto key = path_utf8(root);
    const auto now = std::time(nullptr);
    std::lock_guard<std::mutex> lock(mutex);
    const auto found = cache.find(key);
    if (found != cache.end() && found->second.resource &&
        ((local && found->second.local && found->second.stat_time == stat_time &&
          found->second.stat2_time == stat2_time) ||
         (!local && !found->second.local &&
          std::difftime(now, found->second.loaded_at) < 300.0)))
        return found->second.resource;

    TdxStatsResource resource = local
        ? load_local_stats(directory)
        : download_stats_resource({}, "zhb.zip", 30000, timeout_ms, root).resource;
    FormulaStatsCacheEntry entry;
    entry.resource = std::make_shared<const TdxStatsResource>(std::move(resource));
    entry.stat_time = stat_time;
    entry.stat2_time = stat2_time;
    entry.local = local;
    entry.loaded_at = now;
    cache[key] = entry;
    return entry.resource;
}

void bind_security_stat_functions(Json& context, Json& symbols,
                                  const std::filesystem::path& root,
                                  int market_id, const std::string& code,
                                  const std::set<std::string>& dependencies,
                                  int timeout_ms) {
    static const std::set<std::string> names{
        "BETAVALUE", "SHAPE_LONG", "SHAPE_MID", "SHAPE_SHORT"};
    bool needed = false;
    for (const auto& name : names)
        if (dependencies.count(name)) { needed = true; break; }
    if (!needed) return;
    if (market_id < 0 || market_id > 2) {
        for (const auto& name : names)
            if (dependencies.count(name)) symbols[name] = 0.0;
        Json metadata = Json::object();
        metadata["source"] = Json(nullptr);
        metadata["security_found"] = false;
        metadata["stats_date"] = Json(nullptr);
        metadata["shape_packed"] = Json(nullptr);
        metadata["mode"] =
            "TCalc-opcodes1333-1345-1347-non-equity-zero-record";
        context["security_stat_functions"] = std::move(metadata);
        return;
    }
    const auto resource = cached_formula_stats_resource(root, timeout_ms);
    const auto found = resource->stat.find({market_id, code});
    const TdxStatRow* row = found == resource->stat.end() ? nullptr : &found->second;
    const auto number = [&](const std::optional<double>& value) {
        return value.value_or(0.0);
    };
    const auto integer = [&](const std::optional<int>& value) {
        return static_cast<double>(value.value_or(0));
    };
    if (dependencies.count("BETAVALUE"))
        symbols["BETAVALUE"] = row ? number(row->beta_60d) : 0.0;
    if (dependencies.count("SHAPE_SHORT"))
        symbols["SHAPE_SHORT"] = row ? integer(row->shape_short) : 0.0;
    if (dependencies.count("SHAPE_MID"))
        symbols["SHAPE_MID"] = row ? integer(row->shape_mid) : 0.0;
    if (dependencies.count("SHAPE_LONG"))
        symbols["SHAPE_LONG"] = row ? integer(row->shape_long) : 0.0;
    Json metadata = Json::object();
    metadata["source"] = resource->source_path;
    metadata["security_found"] = row != nullptr;
    metadata["stats_date"] = row && row->stats_date
        ? Json(*row->stats_date) : Json(nullptr);
    metadata["shape_packed"] = row && row->shape_packed
        ? Json(*row->shape_packed) : Json(nullptr);
    metadata["mode"] =
        "TCalc-opcodes1333-1345-1347-TdxW-type163-tdxstat-columns3-23";
    metadata["shape_value_meanings"] =
        "1=倒V型反转;2=V型反转;3=W底;4=M顶;5=盘整;6=盘整后上行;"
        "7=盘整后下跌;8=上升通道;9=下降通道;10=拐头下跌;"
        "11=拐头上升;12=上行盘整;13=下跌盘整;14=其它形态";
    context["security_stat_functions"] = std::move(metadata);
}

double quote_number_or_zero(const Json& row, std::string_view field) {
    const auto* value = optional(row, field);
    return value && value->is_number() ? value->as_number() : 0.0;
}

double quote_level_volume(const Json& row, std::string_view side,
                          std::size_t level) {
    const auto* values = optional(row, side);
    if (!values || !values->is_array() || level >= values->as_array().size())
        return 0.0;
    return quote_number_or_zero(values->as_array()[level], "volume_hand");
}

double index_three_day_change(const std::filesystem::path& root,
                              const std::string& market,
                              const std::string& code,
                              double current, double previous,
                              int timeout_ms) {
    const auto document = fetch_kline_document(
        market, code, "index", "day", 1, 8, 0, "all", timeout_ms, root);
    const auto* bars = optional(document, "bars");
    if (!bars || !bars->is_array()) return 0.0;
    std::vector<double> closes;
    for (const auto& bar : bars->as_array()) {
        const auto* close = optional(bar, "close");
        if (close && close->is_number() && close->as_number() > 0.00001)
            closes.push_back(close->as_number());
    }
    if (closes.size() < 4) return 0.0;
    const double latest = closes.back();
    const bool quote_missing = current <= 0.00001;
    if (quote_missing) current = latest;
    const bool current_included = quote_missing ||
        std::abs(latest - current) <=
            std::max(0.01, std::abs(current) * 0.00002) ||
        std::abs(latest - current) < std::abs(latest - previous);
    const std::size_t bars_back = current_included ? 3 : 2;
    if (closes.size() <= bars_back) return 0.0;
    const double base = closes[closes.size() - 1 - bars_back];
    return base > 0.00001 ? (current / base - 1.0) * 100.0 : 0.0;
}

std::pair<std::string, std::string> main_index_security(
    int selector, int market_id, const std::string& code, bool zero_special) {
    if (selector == 1) return {"sh", "999999"};
    if (selector == 2) return {"sz", "399001"};
    if (selector == 3) return {"sz", "399006"};
    if (selector == 4) return {"sh", "000688"};
    if (selector == 5) return {"bj", "899050"};
    if (zero_special) {
        if (market_id == 0 &&
            (code == "399006" ||
             (code.size() >= 2 && code[0] == '3' && code[1] != '9')))
            return {"sz", "399006"};
        if (market_id == 1 &&
            (code == "000688" || code.rfind("688", 0) == 0 ||
             code.rfind("689", 0) == 0))
            return {"sh", "000688"};
    }
    return market_id == 0 ? std::make_pair(std::string("sz"), std::string("399001"))
         : market_id == 2 ? std::make_pair(std::string("bj"), std::string("899050"))
                          : std::make_pair(std::string("sh"), std::string("999999"));
}

void bind_public_market_summary_functions(
    Json& context, const std::filesystem::path& root, int market_id,
    const std::string& code, const std::set<std::string>& dependencies,
    const std::vector<std::string>& main_bindings,
    const BlockData* block_data, int timeout_ms) {
    const bool main = dependencies.count("MAINZSHQ") != 0;
    const bool total = dependencies.count("TOTALHQINFO") != 0;
    const bool total_amount = dependencies.count("TOTALMMPAMO") != 0;
    if (!main && !total && !total_amount) return;
    std::vector<std::string> securities;
    if (main) securities = {
        "sh:999999", "sz:399001", "sz:399006", "sh:000688", "bj:899050"};
    if (total) securities.push_back("sh:880005");
    if (total_amount) securities.push_back("sh:999997");
    const auto depth = fetch_market_depth_document(
        root, securities, timeout_ms, block_data);
    std::map<std::pair<int, std::string>, const Json*> quotes;
    if (const auto* records = optional(depth, "records"); records && records->is_array())
        for (const auto& row : records->as_array())
            quotes[{integer_or(row, "market_id", -1), text_or(row, "code")}] = &row;

    std::set<int> three_day_selectors;
    bool dynamic = false;
    for (const auto& binding : main_bindings) {
        if (binding == "MAINZSHQ#DYNAMIC") { dynamic = true; break; }
        const auto parts = split(binding, '#');
        if (parts.size() != 3) continue;
        try {
            if (std::stoi(parts[2]) == 9) three_day_selectors.insert(std::stoi(parts[1]));
        } catch (...) { /* Analyzer supplies validated integer bindings. */ }
    }
    if (dynamic)
        for (int selector = 0; selector <= 5; ++selector)
            three_day_selectors.insert(selector);
    std::map<std::pair<std::string, std::string>, double> three_day_cache;
    const auto bind_main = [&](const std::string& selector_name, int selector,
                               bool zero_special, bool needs_three_day) {
        const auto security = main_index_security(
            selector, market_id, code, zero_special);
        const int quote_market = security.first == "sz" ? 0 : security.first == "bj" ? 2 : 1;
        const auto found = quotes.find({quote_market, security.second});
        const Json* row = found == quotes.end() ? nullptr : found->second;
        std::array<double, 10> values{};
        if (row) {
            const double last = quote_number_or_zero(*row, "last_price");
            const double previous = quote_number_or_zero(*row, "pre_close_price");
            values[0] = last >= 0.00001 ? last : previous;
            values[1] = previous;
            values[2] = quote_number_or_zero(*row, "open_price");
            values[3] = quote_number_or_zero(*row, "high_price");
            values[4] = quote_number_or_zero(*row, "low_price");
            values[5] = quote_number_or_zero(*row, "amount");
            values[6] = quote_level_volume(*row, "buy_levels", 0);
            values[7] = quote_level_volume(*row, "sell_levels", 0);
            values[8] = last > 0.00001 && previous > 0.00001
                ? (last - previous) / previous * 100.0 : 0.0;
            if (needs_three_day) {
                const auto key = std::make_pair(security.first, security.second);
                auto cached = three_day_cache.find(key);
                if (cached == three_day_cache.end())
                    cached = three_day_cache.emplace(
                        key, index_three_day_change(root, security.first,
                            security.second, last, previous, timeout_ms)).first;
                values[9] = cached->second;
            }
        }
        for (std::size_t field = 0; field < values.size(); ++field)
            bind_formula_scalar(context, "MAINZSHQ#" + selector_name + "#" +
                std::to_string(field), values[field]);
    };
    if (main) {
        for (int selector = 0; selector <= 5; ++selector)
            bind_main(std::to_string(selector), selector, selector == 0,
                      dynamic || three_day_selectors.count(selector));
        const bool default_three_day = dynamic || std::any_of(
            three_day_selectors.begin(), three_day_selectors.end(),
            [](int selector) { return selector < 0 || selector > 5; });
        bind_main("DEFAULT", -1, false, default_three_day);
    }
    if (total) {
        const auto found = quotes.find({1, "880005"});
        const Json* row = found == quotes.end() ? nullptr : found->second;
        std::array<double, 7> values{};
        if (row) {
            values[1] = quote_number_or_zero(*row, "last_price");
            values[2] = quote_level_volume(*row, "sell_levels", 0);
            values[3] = quote_level_volume(*row, "buy_levels", 4);
            values[4] = quote_level_volume(*row, "sell_levels", 4);
            values[5] = quote_number_or_zero(*row, "pre_close_price");
            values[6] = quote_number_or_zero(*row, "amount") / 100000000.0;
        }
        for (int field = 1; field <= 6; ++field)
            bind_formula_scalar(context, "TOTALHQINFO#" + std::to_string(field),
                                values[static_cast<std::size_t>(field)]);
    }
    if (total_amount) {
        const auto found = quotes.find({1, "999997"});
        const Json* row = found == quotes.end() ? nullptr : found->second;
        const Json* summary = row ? optional(*row, "market_amount_summary") : nullptr;
        const Json* values = summary ? optional(*summary, "values_by_selector") : nullptr;
        for (int field = 1; field <= 4; ++field) {
            double value = 0.0;
            if (values && values->is_array() &&
                values->as_array().size() > static_cast<std::size_t>(field) &&
                values->as_array()[static_cast<std::size_t>(field)].is_number())
                value = values->as_array()[static_cast<std::size_t>(field)].as_number();
            bind_formula_scalar(context, "TOTALMMPAMO#" + std::to_string(field), value);
        }
    }
    Json metadata = Json::object();
    metadata["command"] = "0x0547";
    metadata["endpoint"] = optional(depth, "endpoint")
        ? *optional(depth, "endpoint") : Json(nullptr);
    metadata["received"] = optional(depth, "received")
        ? *optional(depth, "received") : Json(0);
    metadata["mainzshq_mode"] =
        "TCalc-opcode1368-TdxW-type102-index-L1-broadcast";
    metadata["totalhqinfo_mode"] =
        "TCalc-opcode1384-SH880005-public-L1-broadcast";
    metadata["totalmmpamo_mode"] = total_amount
        ? Json("TCalc-opcode1376-TdxW-type168-SH999997-public-L1-selectors1-4")
        : Json(nullptr);
    metadata["totalmmpamo_unit"] = total_amount
        ? Json("100m-yuan") : Json(nullptr);
    metadata["totalmmpamo_level2_unavailable_selectors"] = Json::array();
    if (total_amount) {
        metadata["totalmmpamo_level2_unavailable_selectors"].push_back(5);
        metadata["totalmmpamo_level2_unavailable_selectors"].push_back(6);
    }
    metadata["three_day_mode"] =
        "TdxW-field313-three-session-compounded-return-from-public-daily-bars";
    context["public_market_summary_functions"] = std::move(metadata);
}

}  // namespace tdx::formula_context_detail
