#include "formula_calc_internal.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <utility>
#include <vector>

namespace tdx::formula_calc_detail {

void calculate_dmi(const std::vector<Bar>& bars, const std::map<std::string, int>& supplied,
                   Values& values, std::map<std::string, int>& actual) {
    reject_unknown(supplied, {"n", "m"});
    const int n = parameter(supplied, "n", 14, 2, 90);
    const int m = parameter(supplied, "m", 6, 2, 60);
    actual = {{"n", n}, {"m", m}};
    std::vector<double> true_range(bars.size(), missing), positive(bars.size(), missing),
                        negative(bars.size(), missing);
    for (std::size_t i = 1; i < bars.size(); ++i) {
        true_range[i] = std::max({bars[i].high - bars[i].low,
            std::abs(bars[i].high - bars[i - 1].close),
            std::abs(bars[i - 1].close - bars[i].low)});
        const double high_delta = bars[i].high - bars[i - 1].high;
        const double low_delta = bars[i - 1].low - bars[i].low;
        positive[i] = high_delta > 0.0 && high_delta > low_delta ? high_delta : 0.0;
        negative[i] = low_delta > 0.0 && low_delta > high_delta ? low_delta : 0.0;
    }
    const auto range_sum = rolling_sum(true_range, n);
    const auto positive_sum = rolling_sum(positive, n);
    const auto negative_sum = rolling_sum(negative, n);
    auto& pdi = values["PDI"];
    auto& mdi = values["MDI"];
    pdi.resize(bars.size(), missing);
    mdi.resize(bars.size(), missing);
    std::vector<double> directional_ratio(bars.size(), missing);
    for (std::size_t i = 0; i < bars.size(); ++i) {
        if (!std::isfinite(range_sum[i]) || range_sum[i] <= 1e-12) continue;
        pdi[i] = positive_sum[i] * 100.0 / range_sum[i];
        mdi[i] = negative_sum[i] * 100.0 / range_sum[i];
        const double denominator = pdi[i] + mdi[i];
        if (denominator > 1e-12)
            directional_ratio[i] = std::abs(mdi[i] - pdi[i]) * 100.0 / denominator;
    }
    values["ADX"] = rolling_mean(directional_ratio, m);
    auto& adxr = values["ADXR"];
    adxr.resize(bars.size(), missing);
    for (std::size_t i = static_cast<std::size_t>(m); i < bars.size(); ++i)
        if (std::isfinite(values["ADX"][i]) && std::isfinite(values["ADX"][i - m]))
            adxr[i] = (values["ADX"][i] + values["ADX"][i - m]) / 2.0;
}

void calculate_wvad(const std::vector<Bar>& bars, const std::map<std::string, int>& supplied,
                    Values& values, std::map<std::string, int>& actual) {
    reject_unknown(supplied, {"n", "m"});
    const int n = parameter(supplied, "n", 24, 2, 120);
    const int m = parameter(supplied, "m", 6, 2, 60);
    actual = {{"n", n}, {"m", m}};
    std::vector<double> contribution(bars.size(), missing);
    for (std::size_t i = 0; i < bars.size(); ++i) {
        const double range = bars[i].high - bars[i].low;
        if (range > 1e-12)
            contribution[i] = (bars[i].close - bars[i].open) *
                              bars[i].formula_volume / range;
    }
    auto& wvad = values["WVAD"];
    wvad = rolling_sum(contribution, n);
    for (auto& value : wvad) if (std::isfinite(value)) value /= 10000.0;
    values["MAWVAD"] = rolling_mean(wvad, m);
}

void calculate_emv(const std::vector<Bar>& bars, const std::map<std::string, int>& supplied,
                   Values& values, std::map<std::string, int>& actual) {
    reject_unknown(supplied, {"n", "m"});
    const int n = parameter(supplied, "n", 14, 2, 90);
    const int m = parameter(supplied, "m", 9, 2, 60);
    actual = {{"n", n}, {"m", m}};
    std::vector<double> volume, range;
    volume.reserve(bars.size());
    range.reserve(bars.size());
    for (const auto& bar : bars) {
        volume.push_back(bar.formula_volume);
        range.push_back(bar.high - bar.low);
    }
    const auto average_volume = rolling_mean(volume, n);
    const auto average_range = rolling_mean(range, n);
    std::vector<double> raw(bars.size(), missing);
    for (std::size_t i = 1; i < bars.size(); ++i) {
        const double price_sum = bars[i].high + bars[i].low;
        if (!std::isfinite(average_volume[i]) || !std::isfinite(average_range[i]) ||
            std::abs(volume[i]) <= 1e-12 || std::abs(price_sum) <= 1e-12 ||
            std::abs(average_range[i]) <= 1e-12) continue;
        const double volume_ratio = average_volume[i] / volume[i];
        const double midpoint_move = 100.0 *
            (price_sum - bars[i - 1].high - bars[i - 1].low) / price_sum;
        raw[i] = midpoint_move * volume_ratio * range[i] / average_range[i];
    }
    values["EMV"] = rolling_mean(raw, n);
    values["MAEMV"] = rolling_mean(values["EMV"], m);
}

void calculate_cho(const std::vector<Bar>& bars, const std::map<std::string, int>& supplied,
                   Values& values, std::map<std::string, int>& actual) {
    reject_unknown(supplied, {"n1", "n2", "m"});
    const int n1 = parameter(supplied, "n1", 10, 2, 60);
    const int n2 = parameter(supplied, "n2", 20, 2, 100);
    const int m = parameter(supplied, "m", 6, 2, 60);
    actual = {{"n1", n1}, {"n2", n2}, {"m", m}};
    std::vector<double> mid(bars.size(), missing);
    double cumulative = 0.0;
    bool valid = true;
    for (std::size_t i = 0; i < bars.size(); ++i) {
        const double denominator = bars[i].high + bars[i].low;
        if (std::abs(denominator) <= 1e-12) valid = false;
        if (valid) {
            cumulative += bars[i].formula_volume *
                (2.0 * bars[i].close - bars[i].high - bars[i].low) / denominator;
            mid[i] = cumulative;
        }
    }
    const auto short_average = rolling_mean(mid, n1);
    const auto long_average = rolling_mean(mid, n2);
    auto& cho = values["CHO"];
    cho.resize(bars.size(), missing);
    for (std::size_t i = 0; i < bars.size(); ++i)
        if (std::isfinite(short_average[i]) && std::isfinite(long_average[i]))
            cho[i] = short_average[i] - long_average[i];
    values["MACHO"] = rolling_mean(cho, m);
}

void calculate_adtm(const std::vector<Bar>& bars, const std::map<std::string, int>& supplied,
                    Values& values, std::map<std::string, int>& actual) {
    reject_unknown(supplied, {"n", "m"});
    const int n = parameter(supplied, "n", 23, 2, 100);
    const int m = parameter(supplied, "m", 8, 2, 100);
    actual = {{"n", n}, {"m", m}};
    std::vector<double> buying(bars.size(), missing), selling(bars.size(), missing);
    for (std::size_t i = 1; i < bars.size(); ++i) {
        buying[i] = bars[i].open <= bars[i - 1].open ? 0.0 :
            std::max(bars[i].high - bars[i].open, bars[i].open - bars[i - 1].open);
        selling[i] = bars[i].open >= bars[i - 1].open ? 0.0 :
            std::max(bars[i].open - bars[i].low, bars[i - 1].open - bars[i].open);
    }
    const auto buying_sum = rolling_sum(buying, n);
    const auto selling_sum = rolling_sum(selling, n);
    auto& adtm = values["ADTM"];
    adtm.resize(bars.size(), missing);
    for (std::size_t i = 0; i < bars.size(); ++i) {
        if (!std::isfinite(buying_sum[i]) || !std::isfinite(selling_sum[i])) continue;
        if (buying_sum[i] > selling_sum[i] && buying_sum[i] > 1e-12)
            adtm[i] = (buying_sum[i] - selling_sum[i]) / buying_sum[i];
        else if (buying_sum[i] < selling_sum[i] && selling_sum[i] > 1e-12)
            adtm[i] = (buying_sum[i] - selling_sum[i]) / selling_sum[i];
        else adtm[i] = 0.0;
    }
    values["MAADTM"] = rolling_mean(adtm, m);
}

void calculate_dkx(const std::vector<Bar>& bars, const std::map<std::string, int>& supplied,
                   Values& values, std::map<std::string, int>& actual) {
    reject_unknown(supplied, {"m"});
    const int m = parameter(supplied, "m", 10, 2, 250);
    actual = {{"m", m}};
    std::vector<double> mid;
    mid.reserve(bars.size());
    for (const auto& bar : bars)
        mid.push_back((3.0 * bar.close + bar.low + bar.open + bar.high) / 6.0);
    auto& dkx = values["DKX"];
    dkx.resize(bars.size(), missing);
    for (std::size_t i = 20; i < bars.size(); ++i) {
        double weighted = 20.0 * mid[i];
        for (std::size_t offset = 1; offset <= 18; ++offset)
            weighted += (20.0 - offset) * mid[i - offset];
        // The recovered system formula intentionally skips REF(MID,19).
        weighted += mid[i - 20];
        dkx[i] = weighted / 210.0;
    }
    values["MADKX"] = rolling_mean(dkx, m);
}

}  // namespace tdx::formula_calc_detail
