#include "formula_scan_internal.hpp"

namespace tdx::formula_scan_detail {
namespace fs = std::filesystem;

SecurityKey parse_security(std::string value) {
    value = lower_ascii(trim(value));
    SecurityKey key;
    const auto colon = value.find(':');
    if (colon != std::string::npos) { key.market = value.substr(0, colon); key.code = value.substr(colon + 1); }
    else if (value.size() == 8 && (value.rfind("sz", 0) == 0 || value.rfind("sh", 0) == 0 || value.rfind("bj", 0) == 0)) {
        key.market = value.substr(0, 2); key.code = value.substr(2);
    } else {
        key.code = value;
        if (value.size() == 6) key.market = value.rfind("92", 0) == 0 || value[0] == '8' ? "bj" :
                                                  value[0] == '6' || value[0] == '9' ? "sh" : "sz";
    }
    if ((key.market != "sz" && key.market != "sh" && key.market != "bj") || key.code.size() != 6 ||
        !std::all_of(key.code.begin(), key.code.end(), [](char ch) { return ch >= '0' && ch <= '9'; }))
        throw Error("invalid scan security: " + value);
    return key;
}

std::vector<SecurityKey> securities_from_document(const Json& document) {
    const Json* rows = nullptr;
    if (document.is_array()) rows = &document;
    else if (const auto* value = optional(document, "securities"); value && value->is_array()) rows = value;
    if (!rows) return {};
    std::vector<SecurityKey> result;
    for (const auto& row : rows->as_array()) {
        if (!row.is_object() || optional(row, "bars")) continue;
        const auto* market = optional(row, "market"); const auto* code = optional(row, "code");
        if (!market || !market->is_string() || !code || !code->is_string()) continue;
        auto key = parse_security(market->as_string() + ":" + code->as_string());
        if (const auto* name = optional(row, "name"); name && name->is_string()) key.name = name->as_string();
        result.push_back(std::move(key));
    }
    return result;
}

std::vector<Json> klines_from_document(const Json& document) {
    if (document.is_object() && optional(document, "bars")) return {document};
    const Json* rows = document.is_array() ? &document : optional(document, "securities");
    std::vector<Json> result;
    if (!rows || !rows->is_array()) return result;
    for (const auto& row : rows->as_array()) {
        if (row.is_object() && optional(row, "bars")) result.push_back(row);
        else if (row.is_object()) {
            const auto* kline = optional(row, "kline");
            if (kline && kline->is_object() && optional(*kline, "bars")) result.push_back(*kline);
        }
    }
    return result;
}


std::vector<FetchOutcome> fetch_klines(const std::vector<SecurityKey>& securities,
                                      const std::string& period, int pages, int page_size,
                                      int timeout, int workers, const fs::path& cache_dir,
                                      bool refresh, const fs::path& root,
                                      const Json& analysis,
                                      const Json* formula_library,
                                      const std::map<std::string, double>& formula_parameters,
                                      bool point_in_time_finance,
                                      const std::string& adjustment_mode,
                                      const std::string& anchor_date,
                                      int adjustment_cache_ttl,
                                      bool refresh_adjustment) {
    std::vector<FetchOutcome> outcomes(securities.size());
    std::atomic<std::size_t> next{0};
    auto worker = [&] {
        while (true) {
            const auto index = next.fetch_add(1);
            if (index >= securities.size()) break;
            const auto& key = securities[index]; auto& outcome = outcomes[index]; outcome.index = index;
            try {
                const auto cache = cache_dir.empty() ? fs::path{} : cache_dir /
                    (key.market + key.code + "-" + period + "-p" +
                     std::to_string(pages) + "-s" + std::to_string(page_size) +
                     ".json");
                if (!refresh && !cache.empty() && fs::is_regular_file(cache))
                    outcome.kline = Json::parse(read_text_utf8(cache));
                else {
                    outcome.kline = fetch_kline_document(key.market, key.code, "stock", period,
                                                         pages, page_size, 0, "all", timeout,
                                                         root);
                    if (!key.name.empty()) outcome.kline["name"] = key.name;
                    if (!cache.empty()) atomic_write_text(cache, outcome.kline.dump(-1) + "\n");
                }
                outcome.kline = adjust_security_kline_document(
                    std::move(outcome.kline), key.market, key.code, "stock",
                    adjustment_mode, anchor_date, root, {}, timeout,
                    adjustment_cache_ttl, refresh_adjustment);
                if (analysis.at("has_external_dependency").as_bool())
                    outcome.kline["formula_context"] = build_formula_market_context_document(
                        root, key.market, key.code, analysis, timeout, nullptr, &outcome.kline,
                        point_in_time_finance, formula_library, {}, nullptr,
                        &formula_parameters);
            } catch (const std::exception& error) { outcome.error = error.what(); }
        }
    };
    std::vector<std::thread> threads;
    const auto count = std::min<std::size_t>(static_cast<std::size_t>(workers), securities.size());
    for (std::size_t i = 0; i < count; ++i) threads.emplace_back(worker);
    for (auto& thread : threads) thread.join();
    return outcomes;
}

}  // namespace tdx::formula_scan_detail
