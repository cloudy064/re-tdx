#include "formula_calc_internal.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace tdx::formula_calc_detail {

const Json* optional(const Json& object, std::string_view key) {
    if (!object.is_object()) return nullptr;
    const auto found = object.as_object().find(key);
    return found == object.as_object().end() ? nullptr : &found->second;
}

double number(const Json& object, std::string_view key) {
    const auto* value = optional(object, key);
    if (!value || !value->is_number()) throw Error("K-line bar is missing numeric field " + std::string(key));
    return value->as_number();
}

std::string text(const Json& object, std::string_view key) {
    const auto* value = optional(object, key);
    if (!value || !value->is_string()) throw Error("K-line bar is missing string field " + std::string(key));
    return value->as_string();
}

std::vector<Bar> read_bars(const Json& document) {
    const auto* rows = optional(document, "bars");
    if (!rows || !rows->is_array()) throw Error("K-line document does not contain a bars array");
    std::vector<Bar> result;
    result.reserve(rows->size());
    const auto* index_value = optional(document, "index_mode");
    const bool index_mode = index_value && index_value->is_bool() && index_value->as_bool();
    const double volume_divisor = index_mode ? 1.0 : 100.0;
    for (const auto& row : rows->as_array()) {
        const double volume = number(row, "volume");
        result.push_back(Bar{text(row, "date"), text(row, "time"),
            number(row, "open"), number(row, "high"), number(row, "low"),
            number(row, "close"), number(row, "amount"), volume,
            volume / volume_divisor});
    }
    std::stable_sort(result.begin(), result.end(), [](const Bar& left, const Bar& right) {
        return std::tie(left.date, left.time) < std::tie(right.date, right.time);
    });
    result.erase(std::unique(result.begin(), result.end(), [](const Bar& left, const Bar& right) {
        return left.date == right.date && left.time == right.time;
    }), result.end());
    return result;
}

int parameter(const std::map<std::string, int>& supplied, std::string_view name,
              int fallback, int minimum, int maximum) {
    const auto found = supplied.find(std::string(name));
    const int value = found == supplied.end() ? fallback : found->second;
    if (value < minimum || value > maximum)
        throw Error("formula parameter " + std::string(name) + " must be between " +
                    std::to_string(minimum) + " and " + std::to_string(maximum));
    return value;
}

void reject_unknown(const std::map<std::string, int>& supplied,
                    std::initializer_list<std::string_view> allowed) {
    std::set<std::string, std::less<>> names;
    for (const auto name : allowed) names.emplace(name);
    for (const auto& [name, value] : supplied) {
        (void)value;
        if (!names.count(name)) throw Error("unsupported parameter for selected formula: " + name);
    }
}

std::vector<double> closes(const std::vector<Bar>& bars) {
    std::vector<double> result;
    result.reserve(bars.size());
    for (const auto& bar : bars) result.push_back(bar.close);
    return result;
}

std::vector<double> rolling_mean(const std::vector<double>& source, int period) {
    std::vector<double> result(source.size(), missing);
    double sum = 0.0;
    int finite_count = 0;
    for (std::size_t index = 0; index < source.size(); ++index) {
        if (std::isfinite(source[index])) { sum += source[index]; ++finite_count; }
        if (index >= static_cast<std::size_t>(period)) {
            const auto expired = source[index - period];
            if (std::isfinite(expired)) { sum -= expired; --finite_count; }
        }
        if (index + 1 >= static_cast<std::size_t>(period) && finite_count == period)
            result[index] = sum / period;
    }
    return result;
}

std::vector<double> rolling_sum(const std::vector<double>& source, int period) {
    std::vector<double> result(source.size(), missing);
    double sum = 0.0;
    int finite_count = 0;
    for (std::size_t index = 0; index < source.size(); ++index) {
        if (std::isfinite(source[index])) { sum += source[index]; ++finite_count; }
        if (index >= static_cast<std::size_t>(period)) {
            const auto expired = source[index - period];
            if (std::isfinite(expired)) { sum -= expired; --finite_count; }
        }
        if (index + 1 >= static_cast<std::size_t>(period) && finite_count == period)
            result[index] = sum;
    }
    return result;
}

int parameter_alias(const std::map<std::string, int>& supplied,
                    std::string_view canonical, std::string_view alias,
                    int fallback, int minimum, int maximum) {
    const auto primary = supplied.find(std::string(canonical));
    const auto legacy = supplied.find(std::string(alias));
    if (primary != supplied.end() && legacy != supplied.end())
        throw Error("formula parameters " + std::string(canonical) + " and " +
                    std::string(alias) + " are aliases; supply only one");
    const int value = primary != supplied.end() ? primary->second
                    : legacy != supplied.end() ? legacy->second : fallback;
    if (value < minimum || value > maximum)
        throw Error("formula parameter " + std::string(canonical) + " must be between " +
                    std::to_string(minimum) + " and " + std::to_string(maximum));
    return value;
}

std::vector<double> ema(const std::vector<double>& source, int period) {
    std::vector<double> result(source.size(), missing);
    if (source.empty()) return result;
    const double alpha = 2.0 / (period + 1.0);
    result[0] = source[0];
    for (std::size_t index = 1; index < source.size(); ++index)
        result[index] = alpha * source[index] + (1.0 - alpha) * result[index - 1];
    return result;
}

std::vector<double> tdx_sma(const std::vector<double>& source, int period,
                            int weight, double seed) {
    std::vector<double> result(source.size(), missing);
    if (source.empty()) return result;
    double previous = seed;
    for (std::size_t index = 0; index < source.size(); ++index) {
        previous = (weight * source[index] + (period - weight) * previous) / period;
        result[index] = previous;
    }
    return result;
}

Json parameter_json(const std::map<std::string, int>& parameters) {
    Json result = Json::object();
    for (const auto& [name, value] : parameters) result[name] = value;
    return result;
}


}  // namespace tdx::formula_calc_detail
