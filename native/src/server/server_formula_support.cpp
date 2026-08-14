#include "server_formula_support_internal.hpp"

#include "tdx/common.hpp"
#include "tdx/corporate.hpp"
#include "tdx/hk_actions.hpp"
#include "tdx/minute.hpp"

#include <atomic>
#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace tdx::server_detail {

std::string requested_kline_adjustment_mode(const RequestTarget& target) {
    return normalize_kline_adjustment_mode(
        query_value(target, "adjust", "none"));
}

void attach_multi_security_adjustment_summary(Json& result,
                                              const std::vector<Json>& klines,
                                              const std::string& mode) {
    result["adjustment_mode"] = mode;
    if (mode == "none") return;
    result["adjustment_summary"] = summarize_kline_adjustments(klines, mode);
}

std::vector<FormulaKlineFetchOutcome> fetch_adjusted_formula_klines(
    const FormulaHttpState& state,
    const RequestTarget& target,
    const std::vector<std::pair<std::string, std::string>>& securities,
    const std::string& period,
    int pages,
    int page_size,
    int timeout,
    int workers) {
    std::vector<FormulaKlineFetchOutcome> outcomes(securities.size());
    std::atomic<std::size_t> next{0};
    auto worker = [&] {
        while (true) {
            const auto index = next.fetch_add(1);
            if (index >= securities.size()) break;
            const auto& [market, code] = securities[index];
            try {
                auto kline = fetch_kline_document(
                    market, code, "stock", period, pages, page_size, 0, "all",
                    timeout, state.root);
                outcomes[index].kline = apply_requested_kline_adjustment(
                    state.root, target, market, code, "stock", timeout,
                    std::move(kline));
            } catch (const std::exception& error) {
                outcomes[index].error = error.what();
            }
        }
    };
    std::vector<std::thread> threads;
    const auto count = std::min<std::size_t>(
        static_cast<std::size_t>(workers), securities.size());
    threads.reserve(count);
    for (std::size_t index = 0; index < count; ++index)
        threads.emplace_back(worker);
    for (auto& thread : threads) thread.join();
    return outcomes;
}

double parse_formula_parameter(const std::string& text, std::string_view name) {
    try {
        std::size_t used = 0;
        const double value = std::stod(text, &used);
        if (used != text.size() || !std::isfinite(value) || std::abs(value) > 1000000000.0)
            throw std::invalid_argument("range");
        return value;
    } catch (...) {
        throw Error(std::string(name) + " must be a finite formula parameter");
    }
}

bool formula_dependency_supported_in_expansion(std::string_view name,
                                               std::string_view market) {
    if (name == "DIVFACTOR" && is_hk_action_market(market)) return true;
    return name == "EXTDATA_USER" || name == "EXTERNSTR" || name == "EXTERNVALUE" ||
           name == "SIGNALS_SYS" || name == "SIGNALS_USER" ||
           name == "IVOLAT" || name == "IST0CODE" || name == "ISSTCODE" ||
           name == "ISQUITCODE" || name == "ISQHQQCODE" ||
           name == "ISJYDATE" || name == "LOCALDAYNUM" ||
           name == "MULTIPLIER";
}


} // namespace tdx::server_detail
