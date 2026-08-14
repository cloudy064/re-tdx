#include "formula_context_relations_detail.hpp"
#include "formula_context_relations_catalog.hpp"

#include "formula_context_support_internal.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace tdx::formula_context_detail {

int market_id_for(std::string market) {
    market = normalized_market(std::move(market));
    return market == "sh" ? 1 : market == "bj" ? 2 : 0;
}

std::string tdx_region_name(int province_id) {
    // Exact ten-byte GBK table used by TdxW type 120 at return offset +155.
    // Index zero is unused; the host only accepts IDs 1..32.
    if (province_id < 1 ||
        static_cast<std::size_t>(province_id) >= tdx_region_names.size())
        return {};
    return std::string(tdx_region_names[static_cast<std::size_t>(province_id)]);
}

std::string concept_block_text(const BlockData& data, const std::string& market,
                               const std::string& code) {
    const int market_id = market_id_for(market);
    std::set<std::string> names;
    for (const auto& member : data.members) {
        if (member.market_id == market_id && member.code == code &&
            member.family == "concept" && member.membership == "direct" &&
            !member.block_name.empty())
            names.insert(member.block_name);
    }
    std::string result;
    for (const auto& name : names) {
        if (!result.empty()) result.push_back(' ');
        result += name;
    }
    return result;
}

struct FormulaMainIndex {
    std::string market;
    std::string code;
    std::string name;
};

FormulaMainIndex formula_main_index(std::string market,
                                    const std::string& code) {
    market = lower_ascii(trim(std::move(market)));
    if (market == "sz") market = "0";
    else if (market == "sh") market = "1";
    else if (market == "bj") market = "2";
    if (market == "0") {
        const bool growth = code == "399006" ||
            (code.size() >= 2 && code[0] == '3' && code[1] != '9');
        return {"sz", growth ? "399006" : "399001",
                growth ? "创业板指" : "深圳成指"};
    }
    if (market == "1") {
        const bool star = code == "000688" || starts_with(code, "688") ||
                          starts_with(code, "689");
        return {"sh", star ? "000688" : "999999",
                star ? "科创50" : "上证指数"};
    }
    if (market == "2")
        return {"bj", "899050", "北证50"};
    if (market == "27" || market == "31" || market == "48" ||
        market == "49" || market == "71")
        return {"27", "HSI", "恒生指数"};
    return {"sh", "999999", "上证指数"};
}

void bind_formula_main_index_text(Json& context, Json& symbols,
                                  const std::string& market,
                                  const std::string& code,
                                  const std::set<std::string>& dependencies) {
    if (!dependencies.count("DPZSCODE") && !dependencies.count("DPZSNAME"))
        return;
    const auto index = formula_main_index(market, code);
    if (dependencies.count("DPZSCODE"))
        bind_formula_text_symbol(
            context, symbols, "DPZSCODE", index.code,
            "TCalc-opcode1323-sub_10044AA0-native-main-index-code");
    if (dependencies.count("DPZSNAME"))
        bind_formula_text_symbol(
            context, symbols, "DPZSNAME", index.name,
            "TCalc-opcode1357-sub_10044C00-native-GBK-index-name");
    Json metadata = Json::object();
    metadata["security"] = index.market + ":" + index.code;
    metadata["market"] = index.market;
    metadata["code"] = index.code;
    metadata["name"] = index.name;
    metadata["mode"] =
        "TCalc-sub_10044AA0-sub_10044C00-exact-market-code-selection";
    context["main_index_identity"] = std::move(metadata);
}

struct FormulaUnderlying {
    int market_id{};
    std::string code;
    std::string source;
};

bool formula_six_digit_code(const std::string& value) {
    return value.size() == 6 &&
        std::all_of(value.begin(), value.end(), [](unsigned char ch) {
            return std::isdigit(ch) != 0;
        });
}

std::optional<FormulaUnderlying> formula_convertible_underlying(
    const std::filesystem::path& root, int market_id, const std::string& code) {
    const auto path = root / "T0002" / "hq_cache" / "speckzzdata.txt";
    if (!std::filesystem::is_regular_file(path)) return std::nullopt;
    for (auto line : split(read_text_utf8(path), '\n')) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const auto fields = split(line, ',');
        if (fields.size() < 3 || trim(fields[1]) != code) continue;
        int row_market = -1;
        try {
            std::size_t used = 0;
            const auto value = trim(fields[0]);
            row_market = std::stoi(value, &used);
            if (used != value.size()) row_market = -1;
        } catch (...) { row_market = -1; }
        const auto underlying = trim(fields[2]);
        if (row_market != market_id || row_market < 0 || row_market > 2 ||
            !formula_six_digit_code(underlying))
            continue;
        return FormulaUnderlying{
            row_market, underlying,
            "T0002/hq_cache/speckzzdata.txt-client-convertible-master"};
    }
    return std::nullopt;
}

std::optional<FormulaUnderlying> resolve_formula_underlying(
    const std::filesystem::path& root, const Json& target,
    const std::string& market, const std::string& code, int timeout_ms) {
    if (const auto option = resolve_formula_option_instrument(
            target, market, code, timeout_ms))
        return FormulaUnderlying{
            option->underlying_market_id, option->underlying_code,
            "TDX-option-catalog-name-decoder"};
    const int market_id = formula_market_id(market);
    if (market_id >= 0 && market_id <= 2)
        return formula_convertible_underlying(root, market_id, code);
    return std::nullopt;
}

Json formula_underlying_close_points(const Json& target,
                                     const Json& underlying) {
    const auto* target_bars = optional(target, "bars");
    const auto* source_bars = optional(underlying, "bars");
    if (!target_bars || !target_bars->is_array())
        throw Error("UNDERLYC context requires target K-line bars");
    if (!source_bars || !source_bars->is_array())
        throw Error("UNDERLYC context returned no underlying K-line bars");
    Json points = Json::object();
    std::map<std::string, double> exact_closes, daily_closes;
    for (const auto& source_bar : source_bars->as_array()) {
        const auto date = text_or(source_bar, "date");
        const auto time = text_or(source_bar, "time");
        const auto* close = optional(source_bar, "close");
        if (date.empty() || !close || !close->is_number()) continue;
        const double value = static_cast<double>(
            static_cast<float>(close->as_number()));
        exact_closes[date + "|" + time] = value;
        if (time.empty()) daily_closes[date] = value;
    }
    double previous = 0.0;
    bool has_previous = false;
    for (const auto& target_bar : target_bars->as_array()) {
        const auto date = text_or(target_bar, "date");
        const auto time = text_or(target_bar, "time");
        double value = 0.0;
        bool matched = false;
        if (const auto found = exact_closes.find(date + "|" + time);
            found != exact_closes.end()) {
            value = found->second;
            matched = true;
        } else if (time.empty()) {
            if (const auto found = daily_closes.find(date);
                found != daily_closes.end()) {
                value = found->second;
                matched = true;
            }
        }
        if (matched && value < 0.00001 && has_previous) value = previous;
        if (matched) { previous = value; has_previous = true; }
        if (!date.empty()) points[date + "|" + time] = value;
    }
    return points;
}

void bind_formula_underlying(Json& context, Json& symbols,
                             const std::filesystem::path& root,
                             const Json* target, const std::string& market,
                             const std::string& code,
                             const std::set<std::string>& dependencies,
                             int timeout_ms) {
    if (!dependencies.count("UNDERCODE") && !dependencies.count("UNDERLYC"))
        return;
    if (!target && dependencies.count("UNDERLYC"))
        throw Error("UNDERLYC formula context requires a K-line document");
    const Json empty_target = Json::object();
    const auto underlying = resolve_formula_underlying(
        root, target ? *target : empty_target, market, code, timeout_ms);
    const auto underlying_code = underlying ? underlying->code : std::string{};
    if (dependencies.count("UNDERCODE"))
        bind_formula_text_symbol(
            context, symbols, "UNDERCODE", underlying_code,
            underlying ? underlying->source
                       : "TCalc-type120-empty-for-unsupported-security-category");
    Json metadata = Json::object();
    metadata["available"] = underlying.has_value();
    metadata["market_id"] = underlying ? Json(underlying->market_id) : Json(nullptr);
    metadata["code"] = underlying ? Json(underlying->code) : Json("");
    metadata["source"] = underlying
        ? Json(underlying->source)
        : Json("TCalc-type120-empty-for-unsupported-security-category");
    metadata["host_layout"] = "type120-market-u16-at176-code-6bytes-at178";
    if (dependencies.count("UNDERLYC")) {
        if (!context.as_object().count("series")) context["series"] = Json::object();
        if (!underlying) {
            Json points = Json::object();
            for (const auto& bar : target->at("bars").as_array())
                points[text_or(bar, "date") + "|" + text_or(bar, "time")] = 0.0;
            context["series"]["UNDERLYC"] = std::move(points);
        } else {
            const ExternalSecurityBinding binding{
                "UNDERLYC", std::to_string(underlying->market_id),
                underlying->code, "close"};
            const auto document = external_security_kline(
                *target, binding, timeout_ms);
            context["series"]["UNDERLYC"] =
                formula_underlying_close_points(*target, document);
        }
        metadata["close_mode"] =
            "TCalc-opcode1320-date-time-aligned-35-byte-kline-close-offset19";
        metadata["unsupported_value"] = 0.0;
    }
    context["underlying_identity"] = std::move(metadata);
}


} // namespace tdx::formula_context_detail
