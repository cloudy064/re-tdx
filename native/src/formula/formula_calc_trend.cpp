#include "formula_calc_internal.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <utility>
#include <vector>

namespace tdx::formula_calc_detail {

void calculate_ma(const std::vector<Bar>& bars, const std::map<std::string, int>& supplied,
                  Values& values, std::map<std::string, int>& actual) {
    reject_unknown(supplied, {"m1", "m2", "m3", "m4", "n1", "n2", "n3", "n4"});
    const auto input = closes(bars);
    const std::vector<int> defaults{5, 10, 20, 60};
    for (std::size_t index = 0; index < defaults.size(); ++index) {
        const auto slot = std::to_string(index + 1);
        const int n = parameter_alias(supplied, "m" + slot, "n" + slot, defaults[index]);
        actual["m" + slot] = n;
        values["MA" + slot] = rolling_mean(input, n);
    }
}

void calculate_macd(const std::vector<Bar>& bars, const std::map<std::string, int>& supplied,
                    Values& values, std::map<std::string, int>& actual) {
    reject_unknown(supplied, {"short", "long", "mid", "signal"});
    const int short_n = parameter(supplied, "short", 12);
    const int long_n = parameter(supplied, "long", 26);
    const int signal_n = parameter_alias(supplied, "mid", "signal", 9);
    if (short_n >= long_n) throw Error("MACD parameter short must be less than long");
    actual = {{"short", short_n}, {"long", long_n}, {"mid", signal_n}};
    const auto input = closes(bars);
    const auto fast = ema(input, short_n);
    const auto slow = ema(input, long_n);
    auto& dif = values["DIF"];
    dif.resize(input.size());
    for (std::size_t i = 0; i < input.size(); ++i) dif[i] = fast[i] - slow[i];
    values["DEA"] = ema(dif, signal_n);
    auto& histogram = values["MACD"];
    histogram.resize(input.size());
    for (std::size_t i = 0; i < input.size(); ++i)
        histogram[i] = 2.0 * (dif[i] - values["DEA"][i]);
}

void calculate_kdj(const std::vector<Bar>& bars, const std::map<std::string, int>& supplied,
                   Values& values, std::map<std::string, int>& actual) {
    reject_unknown(supplied, {"n", "m1", "m2"});
    const int n = parameter(supplied, "n", 9);
    const int m1 = parameter(supplied, "m1", 3);
    const int m2 = parameter(supplied, "m2", 3);
    actual = {{"n", n}, {"m1", m1}, {"m2", m2}};
    std::vector<double> rsv(bars.size(), 50.0);
    for (std::size_t i = 0; i < bars.size(); ++i) {
        const auto begin = i + 1 > static_cast<std::size_t>(n) ? i + 1 - n : 0;
        double highest = bars[begin].high;
        double lowest = bars[begin].low;
        for (std::size_t j = begin + 1; j <= i; ++j) {
            highest = std::max(highest, bars[j].high);
            lowest = std::min(lowest, bars[j].low);
        }
        if (highest > lowest) rsv[i] = (bars[i].close - lowest) * 100.0 / (highest - lowest);
    }
    values["K"] = tdx_sma(rsv, m1, 1, 50.0);
    values["D"] = tdx_sma(values["K"], m2, 1, 50.0);
    auto& j = values["J"];
    j.resize(bars.size());
    for (std::size_t i = 0; i < bars.size(); ++i)
        j[i] = 3.0 * values["K"][i] - 2.0 * values["D"][i];
}

void calculate_rsi(const std::vector<Bar>& bars, const std::map<std::string, int>& supplied,
                   Values& values, std::map<std::string, int>& actual) {
    reject_unknown(supplied, {"n1", "n2", "n3"});
    std::vector<double> gains(bars.size(), 0.0), moves(bars.size(), 0.0);
    for (std::size_t i = 1; i < bars.size(); ++i) {
        const double change = bars[i].close - bars[i - 1].close;
        gains[i] = std::max(change, 0.0);
        moves[i] = std::abs(change);
    }
    for (const auto& [name, fallback] : std::map<std::string, int>{{"n1", 6}, {"n2", 12},
             {"n3", 24}}) {
        const int n = parameter(supplied, name, fallback);
        actual[name] = n;
        const auto numerator = tdx_sma(gains, n, 1, 0.0);
        const auto denominator = tdx_sma(moves, n, 1, 0.0);
        const auto output_name = "RSI" + name.substr(1);
        auto& output = values[output_name];
        output.resize(bars.size(), missing);
        for (std::size_t i = 0; i < bars.size(); ++i)
            if (denominator[i] > 0.0) output[i] = numerator[i] * 100.0 / denominator[i];
    }
}

void calculate_boll(const std::vector<Bar>& bars, const std::map<std::string, int>& supplied,
                    Values& values, std::map<std::string, int>& actual) {
    reject_unknown(supplied, {"m", "n", "p"});
    const int n = parameter_alias(supplied, "m", "n", 20, 2);
    const int p = parameter(supplied, "p", 2, 1, 20);
    actual = {{"m", n}, {"p", p}};
    const auto input = closes(bars);
    values["BOLL"] = rolling_mean(input, n);
    auto& upper = values["UB"];
    auto& lower = values["LB"];
    upper.resize(input.size(), missing);
    lower.resize(input.size(), missing);
    for (std::size_t i = n - 1; i < input.size(); ++i) {
        double squares = 0.0;
        for (std::size_t j = i + 1 - n; j <= i; ++j) {
            const double delta = input[j] - values["BOLL"][i];
            squares += delta * delta;
        }
        const double deviation = std::sqrt(squares / n);
        upper[i] = values["BOLL"][i] + p * deviation;
        lower[i] = values["BOLL"][i] - p * deviation;
    }
}

void calculate_cci(const std::vector<Bar>& bars, const std::map<std::string, int>& supplied,
                   Values& values, std::map<std::string, int>& actual) {
    reject_unknown(supplied, {"n"});
    const int n = parameter(supplied, "n", 14, 2, 100);
    actual = {{"n", n}};
    std::vector<double> typical;
    typical.reserve(bars.size());
    for (const auto& bar : bars) typical.push_back((bar.high + bar.low + bar.close) / 3.0);
    const auto average = rolling_mean(typical, n);
    auto& output = values["CCI"];
    output.resize(bars.size(), missing);
    for (std::size_t i = static_cast<std::size_t>(n - 1); i < bars.size(); ++i) {
        double deviation = 0.0;
        for (std::size_t j = i + 1 - n; j <= i; ++j)
            deviation += std::abs(typical[j] - average[i]);
        deviation /= n;
        if (deviation > 0.0) output[i] = (typical[i] - average[i]) / (0.015 * deviation);
    }
}

std::vector<double> williams_r(const std::vector<Bar>& bars, int period) {
    std::vector<double> result(bars.size(), missing);
    for (std::size_t i = 0; i < bars.size(); ++i) {
        const auto begin = i + 1 > static_cast<std::size_t>(period) ? i + 1 - period : 0;
        double highest = bars[begin].high;
        double lowest = bars[begin].low;
        for (std::size_t j = begin + 1; j <= i; ++j) {
            highest = std::max(highest, bars[j].high);
            lowest = std::min(lowest, bars[j].low);
        }
        if (highest > lowest) result[i] = (highest - bars[i].close) * 100.0 / (highest - lowest);
    }
    return result;
}

void calculate_wr(const std::vector<Bar>& bars, const std::map<std::string, int>& supplied,
                  Values& values, std::map<std::string, int>& actual) {
    reject_unknown(supplied, {"n", "n1"});
    const int n = parameter(supplied, "n", 10, 2, 100);
    const int n1 = parameter(supplied, "n1", 6, 2, 100);
    actual = {{"n", n}, {"n1", n1}};
    values["WR1"] = williams_r(bars, n);
    values["WR2"] = williams_r(bars, n1);
}

void calculate_bias(const std::vector<Bar>& bars, const std::map<std::string, int>& supplied,
                    Values& values, std::map<std::string, int>& actual) {
    reject_unknown(supplied, {"n1", "n2", "n3"});
    const auto input = closes(bars);
    for (const auto& [name, fallback] : std::vector<std::pair<std::string, int>>{
             {"n1", 6}, {"n2", 12}, {"n3", 24}}) {
        const int n = parameter(supplied, name, fallback);
        actual[name] = n;
        const auto average = rolling_mean(input, n);
        auto& output = values["BIAS" + name.substr(1)];
        output.resize(bars.size(), missing);
        for (std::size_t i = 0; i < bars.size(); ++i)
            if (std::isfinite(average[i]) && std::abs(average[i]) > 1e-12)
                output[i] = (input[i] - average[i]) * 100.0 / average[i];
    }
}

void calculate_dma(const std::vector<Bar>& bars, const std::map<std::string, int>& supplied,
                   Values& values, std::map<std::string, int>& actual) {
    reject_unknown(supplied, {"n1", "n2", "m"});
    const int n1 = parameter(supplied, "n1", 10, 2, 1000);
    const int n2 = parameter(supplied, "n2", 50, 2, 1000);
    const int m = parameter(supplied, "m", 10, 2, 1000);
    if (n1 >= n2) throw Error("DMA parameter n1 must be less than n2");
    actual = {{"n1", n1}, {"n2", n2}, {"m", m}};
    const auto input = closes(bars);
    const auto short_average = rolling_mean(input, n1);
    const auto long_average = rolling_mean(input, n2);
    auto& dif = values["DIF"];
    dif.resize(bars.size(), missing);
    for (std::size_t i = 0; i < bars.size(); ++i)
        if (std::isfinite(short_average[i]) && std::isfinite(long_average[i]))
            dif[i] = short_average[i] - long_average[i];
    values["DIFMA"] = rolling_mean(dif, m);
}

void calculate_mtm(const std::vector<Bar>& bars, const std::map<std::string, int>& supplied,
                   Values& values, std::map<std::string, int>& actual) {
    reject_unknown(supplied, {"n", "m"});
    const int n = parameter(supplied, "n", 12);
    const int m = parameter(supplied, "m", 6);
    actual = {{"n", n}, {"m", m}};
    const auto input = closes(bars);
    auto& mtm = values["MTM"];
    mtm.resize(bars.size(), missing);
    for (std::size_t i = static_cast<std::size_t>(n); i < bars.size(); ++i)
        mtm[i] = input[i] - input[i - n];
    values["MTMMA"] = rolling_mean(mtm, m);
}

void calculate_roc(const std::vector<Bar>& bars, const std::map<std::string, int>& supplied,
                   Values& values, std::map<std::string, int>& actual) {
    reject_unknown(supplied, {"n", "m"});
    const int n = parameter(supplied, "n", 12);
    const int m = parameter(supplied, "m", 6);
    actual = {{"n", n}, {"m", m}};
    const auto input = closes(bars);
    auto& roc = values["ROC"];
    roc.resize(bars.size(), missing);
    for (std::size_t i = static_cast<std::size_t>(n); i < bars.size(); ++i)
        if (std::abs(input[i - n]) > 1e-12)
            roc[i] = (input[i] - input[i - n]) * 100.0 / input[i - n];
    values["MAROC"] = rolling_mean(roc, m);
}

void calculate_trix(const std::vector<Bar>& bars, const std::map<std::string, int>& supplied,
                    Values& values, std::map<std::string, int>& actual) {
    reject_unknown(supplied, {"n", "m"});
    const int n = parameter(supplied, "n", 12);
    const int m = parameter(supplied, "m", 9);
    actual = {{"n", n}, {"m", m}};
    const auto first = ema(closes(bars), n);
    const auto second = ema(first, n);
    const auto third = ema(second, n);
    auto& trix = values["TRIX"];
    trix.resize(bars.size(), missing);
    for (std::size_t i = 1; i < bars.size(); ++i)
        if (std::abs(third[i - 1]) > 1e-12)
            trix[i] = (third[i] - third[i - 1]) * 100.0 / third[i - 1];
    values["MATRIX"] = rolling_mean(trix, m);
}

void calculate_atr(const std::vector<Bar>& bars, const std::map<std::string, int>& supplied,
                   Values& values, std::map<std::string, int>& actual) {
    reject_unknown(supplied, {"n"});
    const int n = parameter(supplied, "n", 14);
    actual = {{"n", n}};
    auto& true_range = values["MTR"];
    true_range.resize(bars.size(), missing);
    for (std::size_t i = 0; i < bars.size(); ++i) {
        true_range[i] = bars[i].high - bars[i].low;
        if (i) true_range[i] = std::max({true_range[i],
            std::abs(bars[i].high - bars[i - 1].close),
            std::abs(bars[i - 1].close - bars[i].low)});
    }
    values["ATR"] = rolling_mean(true_range, n);
}


}  // namespace tdx::formula_calc_detail
