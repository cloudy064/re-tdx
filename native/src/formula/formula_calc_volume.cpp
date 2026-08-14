#include "formula_calc_internal.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <utility>
#include <vector>

namespace tdx::formula_calc_detail {

void calculate_vol(const std::vector<Bar>& bars, const std::map<std::string, int>& supplied,
                   Values& values, std::map<std::string, int>& actual) {
    reject_unknown(supplied, {"m1", "m2"});
    const int m1 = parameter(supplied, "m1", 5, 2, 500);
    const int m2 = parameter(supplied, "m2", 10, 2, 500);
    actual = {{"m1", m1}, {"m2", m2}};
    auto& volume = values["VOLUME"];
    volume.reserve(bars.size());
    for (const auto& bar : bars) volume.push_back(bar.formula_volume);
    values["MAVOL1"] = rolling_mean(volume, m1);
    values["MAVOL2"] = rolling_mean(volume, m2);
}

void calculate_obv(const std::vector<Bar>& bars, const std::map<std::string, int>& supplied,
                   Values& values, std::map<std::string, int>& actual) {
    reject_unknown(supplied, {"m"});
    const int m = parameter(supplied, "m", 30, 2, 100);
    actual = {{"m", m}};
    auto& obv = values["OBV"];
    obv.resize(bars.size(), 0.0);
    for (std::size_t i = 1; i < bars.size(); ++i) {
        const double signed_volume = bars[i].close > bars[i - 1].close ? bars[i].formula_volume
                                   : bars[i].close < bars[i - 1].close ? -bars[i].formula_volume
                                   : 0.0;
        obv[i] = obv[i - 1] + signed_volume;
    }
    values["MAOBV"] = rolling_mean(obv, m);
}

void calculate_psy(const std::vector<Bar>& bars, const std::map<std::string, int>& supplied,
                   Values& values, std::map<std::string, int>& actual) {
    reject_unknown(supplied, {"n", "m"});
    const int n = parameter(supplied, "n", 12, 2, 100);
    const int m = parameter(supplied, "m", 6, 2, 100);
    actual = {{"n", n}, {"m", m}};
    std::vector<double> advances(bars.size(), missing);
    for (std::size_t i = 1; i < bars.size(); ++i)
        advances[i] = bars[i].close > bars[i - 1].close ? 1.0 : 0.0;
    auto counts = rolling_sum(advances, n);
    auto& psy = values["PSY"];
    psy.resize(bars.size(), missing);
    for (std::size_t i = 0; i < bars.size(); ++i)
        if (std::isfinite(counts[i])) psy[i] = counts[i] * 100.0 / n;
    values["PSYMA"] = rolling_mean(psy, m);
}

void calculate_vr(const std::vector<Bar>& bars, const std::map<std::string, int>& supplied,
                  Values& values, std::map<std::string, int>& actual) {
    reject_unknown(supplied, {"n", "m"});
    const int n = parameter(supplied, "n", 26, 2, 100);
    const int m = parameter(supplied, "m", 6, 2, 100);
    actual = {{"n", n}, {"m", m}};
    std::vector<double> up(bars.size(), missing), down(bars.size(), missing),
                        flat(bars.size(), missing);
    for (std::size_t i = 1; i < bars.size(); ++i) {
        up[i] = bars[i].close > bars[i - 1].close ? bars[i].formula_volume : 0.0;
        down[i] = bars[i].close < bars[i - 1].close ? bars[i].formula_volume : 0.0;
        flat[i] = bars[i].close == bars[i - 1].close ? bars[i].formula_volume : 0.0;
    }
    const auto up_sum = rolling_sum(up, n);
    const auto down_sum = rolling_sum(down, n);
    const auto flat_sum = rolling_sum(flat, n);
    auto& vr = values["VR"];
    vr.resize(bars.size(), missing);
    for (std::size_t i = 0; i < bars.size(); ++i) {
        if (!std::isfinite(up_sum[i]) || !std::isfinite(down_sum[i]) ||
            !std::isfinite(flat_sum[i])) continue;
        const double denominator = 2.0 * down_sum[i] + flat_sum[i];
        if (denominator > 1e-12)
            vr[i] = (2.0 * up_sum[i] + flat_sum[i]) * 100.0 / denominator;
    }
    values["MAVR"] = rolling_mean(vr, m);
}

void calculate_brar(const std::vector<Bar>& bars, const std::map<std::string, int>& supplied,
                    Values& values, std::map<std::string, int>& actual) {
    reject_unknown(supplied, {"n"});
    const int n = parameter(supplied, "n", 26, 2, 120);
    actual = {{"n", n}};
    std::vector<double> ar_up, ar_down;
    ar_up.reserve(bars.size());
    ar_down.reserve(bars.size());
    std::vector<double> br_up(bars.size(), missing), br_down(bars.size(), missing);
    for (std::size_t i = 0; i < bars.size(); ++i) {
        ar_up.push_back(bars[i].high - bars[i].open);
        ar_down.push_back(bars[i].open - bars[i].low);
        if (i) {
            br_up[i] = std::max(0.0, bars[i].high - bars[i - 1].close);
            br_down[i] = std::max(0.0, bars[i - 1].close - bars[i].low);
        }
    }
    const auto ar_up_sum = rolling_sum(ar_up, n);
    const auto ar_down_sum = rolling_sum(ar_down, n);
    const auto br_up_sum = rolling_sum(br_up, n);
    const auto br_down_sum = rolling_sum(br_down, n);
    auto& ar = values["AR"];
    auto& br = values["BR"];
    ar.resize(bars.size(), missing);
    br.resize(bars.size(), missing);
    for (std::size_t i = 0; i < bars.size(); ++i) {
        if (std::isfinite(ar_down_sum[i]) && ar_down_sum[i] > 1e-12)
            ar[i] = ar_up_sum[i] * 100.0 / ar_down_sum[i];
        if (std::isfinite(br_down_sum[i]) && br_down_sum[i] > 1e-12)
            br[i] = br_up_sum[i] * 100.0 / br_down_sum[i];
    }
}

void calculate_bbi(const std::vector<Bar>& bars, const std::map<std::string, int>& supplied,
                   Values& values, std::map<std::string, int>& actual) {
    reject_unknown(supplied, {"m1", "m2", "m3", "m4"});
    const std::vector<std::pair<std::string, int>> specs{
        {"m1", 3}, {"m2", 6}, {"m3", 12}, {"m4", 24}};
    const auto input = closes(bars);
    std::vector<std::vector<double>> averages;
    for (const auto& [name, fallback] : specs) {
        const int period = parameter(supplied, name, fallback, 2, 100);
        actual[name] = period;
        averages.push_back(rolling_mean(input, period));
    }
    auto& bbi = values["BBI"];
    bbi.resize(bars.size(), missing);
    for (std::size_t i = 0; i < bars.size(); ++i) {
        if (std::all_of(averages.begin(), averages.end(), [i](const auto& series) {
                return std::isfinite(series[i]);
            })) {
            bbi[i] = (averages[0][i] + averages[1][i] + averages[2][i] + averages[3][i]) / 4.0;
        }
    }
}

void calculate_expma(const std::vector<Bar>& bars, const std::map<std::string, int>& supplied,
                     Values& values, std::map<std::string, int>& actual) {
    reject_unknown(supplied, {"m1", "m2"});
    const int m1 = parameter(supplied, "m1", 12, 2, 500);
    const int m2 = parameter(supplied, "m2", 50, 2, 500);
    actual = {{"m1", m1}, {"m2", m2}};
    const auto input = closes(bars);
    values["EXP1"] = ema(input, m1);
    values["EXP2"] = ema(input, m2);
}


}  // namespace tdx::formula_calc_detail
