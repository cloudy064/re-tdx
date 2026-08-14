#include "tdx/shape_match.hpp"

#include "shape_match_internal.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace tdx {
namespace {

constexpr float kNativeFloor = 0.00009999999747378752F;

std::size_t leading_below_floor(const std::vector<float>& values,
                                std::size_t count) {
    std::size_t index = 0;
    while (index < count && values[index] < kNativeFloor) ++index;
    return index;
}

}  // namespace

float shape_series_similarity(const std::vector<float>& reference,
                              const std::vector<float>& candidate,
                              std::size_t window) {
    const auto count = std::min(reference.size(), candidate.size());
    if (window < 2 || window > count) return 0.0F;
    const auto first_valid = std::max(
        leading_below_floor(reference, count),
        leading_below_floor(candidate, count));
    const auto last = count - 1;
    if (last < first_valid + window - 1) return 0.0F;

    float reference_sum = 0.0F;
    float candidate_sum = 0.0F;
    for (std::size_t index = 0; index < window; ++index) {
        reference_sum += reference[last - index];
        candidate_sum += candidate[last - index];
    }
    const float reference_mean = reference_sum / static_cast<float>(window);
    const float candidate_mean = candidate_sum / static_cast<float>(window);
    float covariance = 0.0F;
    float reference_square = 0.0F;
    float candidate_square = 0.0F;
    for (std::size_t index = 0; index < window; ++index) {
        const float left = reference[last - index] - reference_mean;
        const float right = candidate[last - index] - candidate_mean;
        covariance += left * right;
        reference_square += left * left;
        candidate_square += right * right;
    }
    const float reference_std = std::sqrt(
        reference_square / static_cast<float>(window));
    const float candidate_std = std::sqrt(
        candidate_square / static_cast<float>(window));
    if (reference_std * candidate_std <= kNativeFloor) return 0.0F;
    return covariance / static_cast<float>(window) /
           reference_std / candidate_std;
}

namespace shape_match_detail {
namespace {

using SeriesSet = std::array<std::vector<float>, 4>;

SeriesSet template_features(const ShapeTemplate& shape) {
    SeriesSet result;
    for (const auto& bar : shape.bars) {
        result[0].push_back(bar.close);
        result[1].push_back(bar.volume);
        result[2].push_back((bar.close - bar.open) / bar.open);
        result[3].push_back(bar.open >= bar.close ? -1.0F : 1.0F);
    }
    return result;
}

SeriesSet candidate_features(const std::vector<CandidateBar>& bars) {
    SeriesSet result;
    for (const auto& bar : bars) {
        result[0].push_back(bar.close);
        result[1].push_back(bar.volume);
        result[2].push_back((bar.close - bar.open) / bar.open);
        result[3].push_back(bar.open >= bar.close ? -1.0F : 1.0F);
    }
    return result;
}

bool finite_series(const std::vector<float>& values) {
    return std::all_of(values.begin(), values.end(),
                       [](float value) { return std::isfinite(value); });
}

Json ineligible(const ShapeTemplate& shape, std::size_t candidate_count,
                const std::string& diagnostic) {
    Json result = Json::object();
    result["eligible"] = false;
    result["matched"] = false;
    result["score"] = Json(nullptr);
    result["threshold"] = static_cast<double>(shape.threshold_percent) / 100.0;
    result["required_bar_count"] = shape.point_count;
    result["candidate_bar_count"] =
        static_cast<std::uint64_t>(candidate_count);
    result["diagnostic"] = diagnostic;
    return result;
}

Json score_security_template(const ShapeTemplate& shape,
                             const std::vector<CandidateBar>& input) {
    const auto required = static_cast<std::size_t>(shape.point_count);
    if (required < 2 || shape.bars.size() != required)
        return ineligible(shape, input.size(),
                          "security template requires at least two stored bars");
    if (input.size() < required)
        return ineligible(shape, input.size(),
                          "candidate has fewer bars than the template");
    std::vector<CandidateBar> candidate(
        input.end() - static_cast<std::ptrdiff_t>(required), input.end());
    const auto reference = template_features(shape);
    const auto target = candidate_features(candidate);
    static constexpr std::array<const char*, 4> names{
        "close", "volume", "candle-body-return", "candle-direction"};
    float total = 0.0F;
    Json components = Json::array();
    std::size_t active_count = 0;
    for (std::size_t index = 0; index < shape.active.size(); ++index) {
        Json component = Json::object();
        component["name"] = names[index];
        component["active"] = shape.active[index];
        component["weight"] = std::isfinite(shape.weights[index])
            ? Json(shape.weights[index]) : Json(nullptr);
        if (shape.active[index]) {
            ++active_count;
            if (!std::isfinite(shape.weights[index]) ||
                !finite_series(reference[index]) || !finite_series(target[index]))
                return ineligible(shape, input.size(),
                                  "active component contains a non-finite value");
            const auto similarity = shape_series_similarity(
                reference[index], target[index], required);
            component["similarity"] = similarity;
            component["weighted_score"] = similarity * shape.weights[index];
            total += similarity * shape.weights[index];
        } else {
            component["similarity"] = Json(nullptr);
            component["weighted_score"] = Json(nullptr);
        }
        components.push_back(std::move(component));
    }
    if (!active_count)
        return ineligible(shape, input.size(),
                          "template has no active matching component");
    const float threshold = static_cast<float>(shape.threshold_percent) / 100.0F;
    Json result = Json::object();
    result["eligible"] = std::isfinite(total);
    result["matched"] = std::isfinite(total) && total >= threshold;
    result["score"] = std::isfinite(total) ? Json(total) : Json(nullptr);
    result["threshold"] = threshold;
    result["required_bar_count"] = shape.point_count;
    result["candidate_bar_count"] =
        static_cast<std::uint64_t>(input.size());
    result["components"] = std::move(components);
    result["algorithm"] =
        "TDXDeep weighted trailing Pearson correlation over close, volume, candle body return and direction";
    return result;
}

Json score_drawing_template(const ShapeTemplate& shape,
                            const std::vector<CandidateBar>& input) {
    const auto required = static_cast<std::size_t>(shape.point_count);
    if (required < 2 || shape.drawing_points.size() != required)
        return ineligible(shape, input.size(),
                          "hand-drawn template requires at least two slots");
    if (input.size() < required)
        return ineligible(shape, input.size(),
                          "candidate has fewer bars than the template");
    const auto candidate_begin = input.size() - required;
    std::vector<float> reference;
    std::vector<float> candidate;
    for (std::size_t index = 0; index < required; ++index) {
        const auto& point = shape.drawing_points[index];
        if (!point.x || !point.y) continue;
        reference.push_back(point.value);
        candidate.push_back(input[candidate_begin + index].close);
    }
    if (reference.size() < 2 || !finite_series(reference) ||
        !finite_series(candidate))
        return ineligible(shape, input.size(),
                          "hand-drawn template has fewer than two finite active points");
    const auto similarity = shape_series_similarity(
        reference, candidate, reference.size());
    const float threshold = static_cast<float>(shape.threshold_percent) / 100.0F;
    Json result = Json::object();
    result["eligible"] = std::isfinite(similarity);
    result["matched"] = std::isfinite(similarity) && similarity >= threshold;
    result["score"] = std::isfinite(similarity) ? Json(similarity) : Json(nullptr);
    result["threshold"] = threshold;
    result["required_bar_count"] = shape.point_count;
    result["candidate_bar_count"] =
        static_cast<std::uint64_t>(input.size());
    result["active_drawing_points"] =
        static_cast<std::uint64_t>(reference.size());
    result["algorithm"] =
        "TDXDeep trailing Pearson correlation over active hand-drawn close points";
    return result;
}

}  // namespace

Json score_template(const ShapeTemplate& shape,
                    const std::vector<CandidateBar>& candidate) {
    if (shape.threshold_percent < 0 || shape.threshold_percent > 100)
        return ineligible(shape, candidate.size(),
                          "template threshold is outside 0..100 percent");
    if (shape.template_kind == 0)
        return score_security_template(shape, candidate);
    if (shape.template_kind == 1)
        return score_drawing_template(shape, candidate);
    return ineligible(shape, candidate.size(), "unknown template kind");
}

}  // namespace shape_match_detail
}  // namespace tdx
