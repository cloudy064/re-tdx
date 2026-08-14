#include "formula_context_relations_detail.hpp"
#include "formula_context_relations_catalog.hpp"

#include "formula_context_support_internal.hpp"
#include "formula_nested_internal.hpp"

#include "tdx/common.hpp"
#include "tdx/daily.hpp"
#include "tdx/market.hpp"
#include "tdx/minute.hpp"
#include "tdx/session_audit.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace tdx::formula_context_detail {

struct BetaBenchmark {
    std::string market;
    std::string code;
    std::string kind;
};

BetaBenchmark beta_benchmark_for(std::string market, const std::string& code) {
    market = lower_ascii(std::move(market));
    if (market == "0") market = "sz";
    else if (market == "1") market = "sh";
    else if (market == "2") market = "bj";
    if (market == "sz") {
        const bool growth = code == "399006" ||
            (code.size() >= 2 && code[0] == '3' && code[1] != '9');
        return {"sz", growth ? "399006" : "399001", "index"};
    }
    if (market == "sh") {
        const bool star = code == "000688" || starts_with(code, "688") ||
                          starts_with(code, "689");
        return {"sh", star ? "000688" : "999999", "index"};
    }
    if (market == "bj") return {"bj", "899050", "index"};
    if (market == "27" || market == "31" || market == "48" ||
        market == "49" || market == "71")
        return {"27", "HSI", "index"};
    if (market == "47" || market == "28" || market == "29" ||
        market == "30" || market == "66") {
        std::string prefix;
        for (const unsigned char ch : code) {
            if (std::isdigit(ch)) break;
            prefix.push_back(static_cast<char>(ch));
        }
        return {market, prefix + "L9", "auto"};
    }
    return {"sh", "999999", "index"};
}

Json beta_benchmark_kline(const Json& target, const BetaBenchmark& benchmark,
                          int timeout_ms) {
    const auto period = text_or(target, "period", "day");
    const int count = std::max(1, integer_or(target, "count", 800));
    const int page_size = std::min(800, std::max(
        1, integer_or(target, "page_size", std::min(800, count))));
    const int pages = std::min(20, std::max(
        1, (std::max(count, integer_or(target, "downloaded", count)) +
            page_size - 1) / page_size));
    const int start = std::max(0, integer_or(target, "start", 0));
    const auto cache_key = benchmark.market + benchmark.code + ":" + period + ":" +
                           std::to_string(pages) + ":" + std::to_string(page_size) + ":" +
                           std::to_string(start);
    static std::mutex cache_mutex;
    static std::map<std::string, Json> cache;
    std::lock_guard<std::mutex> lock(cache_mutex);
    const auto found = cache.find(cache_key);
    if (found != cache.end()) return found->second;
    auto result = fetch_kline_document(benchmark.market, benchmark.code, benchmark.kind,
                                       period, pages, page_size, start, "all", timeout_ms);
    cache.emplace(cache_key, result);
    return result;
}

void bind_beta_series(Json& context, const Json& target, const std::string& market,
                      const std::string& code,
                      const std::set<std::string>& dependencies, int timeout_ms) {
    if (!dependencies.count("BETA")) return;
    const auto binding = beta_benchmark_for(market, code);
    const auto document = beta_benchmark_kline(target, binding, timeout_ms);
    const auto* rows = optional(document, "bars");
    if (!rows || !rows->is_array()) throw Error("BETA benchmark K-line context returned no bars");
    Json points = Json::object();
    for (const auto& row : rows->as_array()) {
        const auto date = text_or(row, "date"), time = text_or(row, "time");
        const auto* close = optional(row, "close");
        if (!date.empty() && close && close->is_number())
            points[date + "|" + time] = *close;
    }
    if (!context.as_object().count("series")) context["series"] = Json::object();
    context["series"]["__BETA_BENCHMARK_CLOSE"] = std::move(points);
    Json metadata = Json::object();
    metadata["security"] = binding.market + ":" + binding.code;
    metadata["market"] = binding.market;
    metadata["code"] = binding.code;
    metadata["mode"] =
        "TCalc-sub_10023F40-native-security-market-benchmark-selection";
    context["beta_benchmark"] = std::move(metadata);
}

void bind_index_series(Json& context, const Json& target, const std::string& market,
                       const std::string& code,
                       const std::set<std::string>& dependencies, int timeout_ms) {
    if (!has_any_dependency(dependencies, index_series_symbols)) return;
    const auto binding = beta_benchmark_for(market, code);
    const auto benchmark = beta_benchmark_kline(target, binding, timeout_ms);
    const auto* rows = optional(benchmark, "bars");
    if (!rows || !rows->is_array()) throw Error("benchmark K-line context returned no bars");
    Json series = Json::object();
    for (const auto name : index_series_symbols)
        if (dependencies.count(std::string(name)))
            series[std::string(name)] = Json::object();
    for (const auto& row : rows->as_array()) {
        const auto date = text_or(row, "date"), time = text_or(row, "time");
        if (date.empty()) continue;
        const auto key = date + "|" + time;
        const auto bind = [&](std::string_view symbol, std::string_view field) {
            if (!dependencies.count(std::string(symbol))) return;
            if (const auto* value = optional(row, field); value && value->is_number())
                series[std::string(symbol)][key] = *value;
        };
        bind("INDEXO", "open"); bind("INDEXH", "high"); bind("INDEXL", "low");
        bind("INDEXC", "close"); bind("INDEXA", "amount"); bind("INDEXV", "volume");
        bind("ADVANCE", "extra_1"); bind("INDEXADV", "extra_1");
        bind("DECLINE", "extra_2"); bind("INDEXDEC", "extra_2");
    }
    if (!context.as_object().count("series")) context["series"] = Json::object();
    for (auto& [name, points] : series.as_object())
        context["series"][name] = std::move(points);
    context["benchmark"] = binding.market + ":" + binding.code;
    context["benchmark_mode"] =
        "TCalc-opcodes1201-1202-native-market-code-selection-offsets31-33";
}

std::optional<ExternalSecurityBinding> external_security_binding(
    const std::string& dependency) {
    constexpr std::string_view prefix = "EXTERNAL#";
    if (dependency.rfind(prefix, 0) != 0) return std::nullopt;
    const auto dollar = dependency.rfind('$');
    if (dollar == std::string::npos || dollar <= prefix.size() ||
        dollar + 1 >= dependency.size()) return std::nullopt;
    auto security = dependency.substr(prefix.size(), dollar - prefix.size());
    auto field = dependency.substr(dollar + 1);
    std::transform(field.begin(), field.end(), field.begin(), [](unsigned char ch) {
        return static_cast<char>(std::toupper(ch));
    });
    static const std::map<std::string, std::string> fields{
        {"O", "open"}, {"OPEN", "open"}, {"H", "high"}, {"HIGH", "high"},
        {"L", "low"}, {"LOW", "low"}, {"C", "close"}, {"CLOSE", "close"},
        {"V", "volume"}, {"VOL", "volume"}, {"VOLUME", "volume"},
        {"A", "amount"}, {"AMO", "amount"}, {"AMOUNT", "amount"}};
    const auto mapped = fields.find(field);
    if (mapped == fields.end()) return std::nullopt;

    std::string market;
    if (security.size() == 8) {
        const auto explicit_market = lower_ascii(security.substr(0, 2));
        if (explicit_market != "sh" && explicit_market != "sz" && explicit_market != "bj")
            return std::nullopt;
        market = explicit_market; security.erase(0, 2);
    }
    if (security.size() != 6 ||
        !std::all_of(security.begin(), security.end(), [](unsigned char ch) {
            return std::isdigit(ch) != 0;
        })) return std::nullopt;
    if (market.empty()) {
        if (security == "999999" || security.front() == '6') market = "sh";
        else if (starts_with(security, "899")) market = "bj";
        else market = "sz";
    }
    return ExternalSecurityBinding{dependency, market, security, mapped->second};
}

Json external_security_kline(const Json& target, const ExternalSecurityBinding& binding,
                             int timeout_ms) {
    const auto period = text_or(target, "period", "day");
    const int count = std::max(1, integer_or(target, "count", 800));
    const int page_size = std::min(800, std::max(
        1, integer_or(target, "page_size", std::min(800, count))));
    const int pages = std::min(20, std::max(
        1, integer_or(target, "downloaded", count) + page_size - 1) / page_size);
    const int start = std::max(0, integer_or(target, "start", 0));
    const bool index = (binding.market == "sh" &&
                        (binding.code == "999999" || binding.code.front() == '0')) ||
                       (binding.market == "sz" && starts_with(binding.code, "399")) ||
                       (binding.market == "bj" && starts_with(binding.code, "899"));
    const auto cache_key = binding.market + binding.code + ":" + period + ":" +
                           std::to_string(pages) + ":" + std::to_string(page_size) + ":" +
                           std::to_string(start);
    static std::mutex cache_mutex;
    static std::map<std::string, Json> cache;
    std::lock_guard<std::mutex> lock(cache_mutex);
    const auto found = cache.find(cache_key);
    if (found != cache.end()) return found->second;
    auto result = fetch_kline_document(binding.market, binding.code,
        index ? "index" : "auto", period, pages, page_size, start, "all", timeout_ms);
    cache.emplace(cache_key, result);
    return result;
}

void bind_external_security_series(Json& context, const Json& target,
                                   const std::set<std::string>& dependencies,
                                   int timeout_ms) {
    std::vector<ExternalSecurityBinding> bindings;
    for (const auto& dependency : dependencies)
        if (const auto binding = external_security_binding(dependency))
            bindings.push_back(*binding);
    if (bindings.empty()) return;
    if (!context.as_object().count("series")) context["series"] = Json::object();
    Json metadata = Json::array();
    for (const auto& binding : bindings) {
        const auto document = external_security_kline(target, binding, timeout_ms);
        const auto* rows = optional(document, "bars");
        if (!rows || !rows->is_array())
            throw Error("external security K-line context returned no bars");
        Json points = Json::object();
        for (const auto& row : rows->as_array()) {
            const auto date = text_or(row, "date"), time = text_or(row, "time");
            const auto* value = optional(row, binding.field);
            if (!date.empty() && value && value->is_number())
                points[date + "|" + time] = *value;
        }
        context["series"][binding.name] = std::move(points);
        Json item = Json::object();
        item["binding"] = binding.name;
        item["security"] = binding.market + ":" + binding.code;
        item["field"] = binding.field;
        metadata.push_back(std::move(item));
    }
    context["external_security_series"] = std::move(metadata);
    context["external_security_series_mode"] = "tdx-0x052d-date-time-aligned";
}

std::optional<OptionInstrument> resolve_formula_option_instrument(
    const Json& target, const std::string& market, const std::string& code,
    int timeout_ms) {
    int market_id = -1;
    try {
        std::size_t used = 0;
        market_id = std::stoi(market, &used);
        if (used != market.size()) market_id = -1;
    } catch (...) {
        market_id = -1;
    }
    if (market_id != 4 && market_id != 5 && market_id != 6 &&
        market_id != 7 && market_id != 67)
        return std::nullopt;

    std::optional<OptionInstrument> option;
    const auto supplied_name = text_or(target, "option_name");
    if (!supplied_name.empty())
        option = parse_option_instrument(market_id, code, supplied_name);
    if (option) return option;
    const auto catalog = fetch_option_catalog_document(
        market_id, {}, {}, "all", code, 100, false, 300, timeout_ms);
    for (const auto& row : catalog.at("options").as_array()) {
        if (integer_or(row, "market_id", -1) != market_id ||
            text_or(row, "code") != code)
            continue;
        option = parse_option_instrument(market_id, code, text_or(row, "name"));
        if (option) break;
    }
    return option;
}

void bind_ivolat_context(Json& context, const std::filesystem::path& root,
                         const Json& target,
                         const std::string& market, const std::string& code,
                         const std::set<std::string>& dependencies,
                         int timeout_ms) {
    if (!dependencies.count("IVOLAT")) return;
    int market_id = -1;
    try {
        std::size_t used = 0;
        market_id = std::stoi(market, &used);
        if (used != market.size()) market_id = -1;
    } catch (...) {
        market_id = -1;
    }
    if (market_id != 4 && market_id != 5 && market_id != 6 &&
        market_id != 7 && market_id != 67) {
        context["ivolat_status"] = "not_option_market";
        return;
    }

    auto option = resolve_formula_option_instrument(
        target, market, code, timeout_ms);
    if (!option)
        throw Error("IVOLAT could not resolve the selected option in the active TDX catalog; "
                    "pass option_name when evaluating archived contracts");

    const auto underlying = fetch_kline_document(
        std::to_string(option->underlying_market_id), option->underlying_code,
        "stock", "day", 1, 800, 0, "all", timeout_ms);
    const auto* bars = optional(underlying, "bars");
    if (!bars || !bars->is_array() || bars->as_array().size() < 2)
        throw Error("IVOLAT underlying daily K-line history is insufficient");

    Json history = Json::array();
    for (const auto& bar : bars->as_array()) {
        const auto date = text_or(bar, "date");
        const auto* close = optional(bar, "close");
        if (date.empty() || !close || !close->is_number()) continue;
        Json point = Json::object();
        point["date"] = date;
        point["close"] = *close;
        history.push_back(std::move(point));
    }
    if (history.size() < 2) throw Error("IVOLAT underlying history has fewer than two valid closes");

    Json binding = Json::object();
    binding["option"] = option_instrument_document(*option);
    binding["strike"] = option->strike;
    binding["call"] = option->call;
    binding["american"] = option->american;
    binding["futures_model"] = option->futures_model;
    binding["divisor"] = 1.0;
    binding["risk_free"] = optional(target, "option_risk_free") &&
            optional(target, "option_risk_free")->is_number()
        ? optional(target, "option_risk_free")->as_number() : 0.0187;
    auto expiry = text_or(target, "option_expiry");
    if (expiry.empty()) {
        try {
            auto resolution = resolve_option_expiry_document(root, *option);
            if (const auto* value = optional(resolution, "expiry"); value && value->is_string())
                expiry = value->as_string();
            binding["expiry_resolution"] = std::move(resolution);
        } catch (const std::exception& error) {
            binding["expiry_resolution_error"] = error.what();
        }
    }
    binding["expiry"] = expiry.empty() ? Json(nullptr) : Json(expiry);
    binding["history"] = std::move(history);
    binding["history_period"] = "day";
    binding["underlying_security"] =
        std::to_string(option->underlying_market_id) + ":" + option->underlying_code;
    binding["source"] = "TdxW-selector35-cross-security-context";
    context["ivolat"] = std::move(binding);
    context["ivolat_history_count"] = static_cast<std::uint64_t>(
        context.at("ivolat").at("history").size());
}


} // namespace tdx::formula_context_detail
