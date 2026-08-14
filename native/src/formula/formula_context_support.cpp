#include "formula_context_support_internal.hpp"

#include "tdx/common.hpp"

#include <map>
#include <mutex>
#include <string>
#include <utility>

namespace tdx::formula_context_detail {

std::string normalized_market(std::string market) {
    market = lower_ascii(std::move(market));
    if (market == "0") return "sz";
    if (market == "1") return "sh";
    if (market == "2") return "bj";
    return market;
}

int formula_market_id(std::string market) {
    market = lower_ascii(trim(std::move(market)));
    if (market == "sz" || market == "0") return 0;
    if (market == "sh" || market == "1") return 1;
    if (market == "bj" || market == "2" || market == "44") return 2;
    if (market == "qz") return 28;
    if (market == "qd") return 29;
    if (market == "qs") return 30;
    if (market == "cz") return 47;
    if (market == "qg") return 66;
    try {
        std::size_t used = 0;
        const int value = std::stoi(market, &used);
        return used == market.size() && value >= 0 && value <= 255 ? value : -1;
    } catch (...) {
        return -1;
    }
}

ExpansionInstrument cached_expansion_security_record(
    int market_id, const std::string& code, int timeout_ms) {
    static std::mutex mutex;
    static std::map<std::pair<int, std::string>, ExpansionInstrument> cache;
    const auto key = std::make_pair(market_id, lower_ascii(code));
    {
        std::lock_guard<std::mutex> lock(mutex);
        const auto found = cache.find(key);
        if (found != cache.end()) return found->second;
    }
    const auto record = fetch_expansion_instrument_record(
        market_id, code, timeout_ms);
    if (!record)
        throw Error("selected expansion security is absent from the TDX 7727 instrument directory");
    {
        std::lock_guard<std::mutex> lock(mutex);
        cache.emplace(key, *record);
    }
    return *record;
}

void bind_formula_text_symbol(Json& context, Json& symbols,
                              std::string_view name, std::string value,
                              std::string_view source) {
    symbols[std::string(name)] = 0.0;
    if (!context.as_object().count("formula_text_symbols"))
        context["formula_text_symbols"] = Json::object();
    if (!context.as_object().count("formula_text_symbol_sources"))
        context["formula_text_symbol_sources"] = Json::object();
    context["formula_text_symbols"][std::string(name)] = std::move(value);
    context["formula_text_symbol_sources"][std::string(name)] =
        std::string(source);
    context["formula_text_symbols_mode"] =
        "tcalc-string-pool-handles-materialized-as-utf8-metadata";
}

}  // namespace tdx::formula_context_detail
