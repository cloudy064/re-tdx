#include "hk_actions_internal.hpp"

#include "tdx/corporate.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cctype>
#include <cstdint>
#include <limits>
#include <map>
#include <string_view>

namespace tdx {
namespace {

const Json* value(const Json& object, std::string_view key) {
    if (!object.is_object()) return nullptr;
    const auto found = object.as_object().find(std::string(key));
    return found == object.as_object().end() ? nullptr : &found->second;
}

std::string compact_date(std::string_view input, bool required = true) {
    std::string result;
    for (const unsigned char ch : input)
        if (std::isdigit(ch)) result.push_back(static_cast<char>(ch));
    if (result.empty() && !required) return {};
    if (result.size() != 8)
        throw Error("adjustment date must be YYYY-MM-DD or YYYYMMDD");
    return result;
}

std::string display_date(const std::string& compact) {
    return compact.empty() ? std::string{} :
        compact.substr(0, 4) + "-" + compact.substr(4, 2) + "-" +
        compact.substr(6, 2);
}

struct Step {
    const hk_actions_detail::ActionRecord* record{};
    double forward_scale{};
    double forward_offset{};
    double backward_scale{};
    double backward_offset{};
};

std::vector<Step> steps_for(
    const std::vector<hk_actions_detail::ActionRecord>& records,
    const std::string& code) {
    std::vector<Step> result;
    for (const auto& record : records) {
        if (record.code != code) continue;
        const float previous_multiplier =
            static_cast<float>(record.previous_multiplier);
        const float cumulative_multiplier =
            static_cast<float>(record.cumulative_multiplier);
        const float previous_offset = static_cast<float>(record.previous_offset);
        const float cumulative_offset = static_cast<float>(record.cumulative_offset);
        if (!(previous_multiplier > 0.0F) || !(cumulative_multiplier > 0.0F))
            throw Error("HK action contains an invalid native float32 multiplier");
        result.push_back(Step{
            &record,
            static_cast<double>(previous_multiplier) / cumulative_multiplier,
            -static_cast<double>(cumulative_offset - previous_offset) /
                cumulative_multiplier,
            static_cast<double>(cumulative_multiplier) / previous_multiplier,
            static_cast<double>(cumulative_offset - previous_offset) /
                previous_multiplier,
        });
    }
    return result;
}

void compose(double scale, double offset,
             double& total_scale, double& total_offset) {
    total_offset = total_offset * scale + offset;
    total_scale *= scale;
}

float transformed_price(double input, const std::vector<const Step*>& selected,
                        bool forward) {
    float result = static_cast<float>(input);
    for (const auto* step : selected) {
        const auto scale = forward ? step->forward_scale : step->backward_scale;
        const auto offset = forward ? step->forward_offset : step->backward_offset;
        result = static_cast<float>(static_cast<double>(result) * scale + offset);
    }
    return result;
}

Json event_json(const Step& step, std::size_t affected_bars) {
    const auto& record = *step.record;
    Json result = Json::object();
    result["date"] = record.date;
    result["description"] = record.description;
    result["previous_cumulative_multiplier"] = record.previous_multiplier;
    result["previous_cumulative_offset"] = record.previous_offset;
    result["cumulative_multiplier"] = record.cumulative_multiplier;
    result["cumulative_offset"] = record.cumulative_offset;
    result["event_share_multiplier"] = step.backward_scale;
    result["event_additive_adjustment"] = step.backward_offset;
    result["affected_bar_count"] = static_cast<std::uint64_t>(affected_bars);
    result["source_file"] = record.source_file;
    return result;
}

}  // namespace

bool is_hk_action_market(std::string_view input) {
    auto market = lower_ascii(trim(std::string(input)));
    return market == "hk" || market == "31" || market == "48" ||
           market == "kh" || market == "kg";
}

Json apply_hk_kline_adjustment(
    Json document, const std::filesystem::path& root,
    const std::string& raw_code, const std::string& requested_mode,
    const std::string& anchor_date) {
    const auto mode = normalize_kline_adjustment_mode(requested_mode);
    if (mode == "none") {
        document["adjustment_mode"] = "none";
        return document;
    }
    const auto code = trim(raw_code);
    if (code.size() != 5 || !std::all_of(code.begin(), code.end(), [](char ch) {
            return ch >= '0' && ch <= '9';
        }))
        throw Error("HK adjustment code must contain exactly five digits");
    if (!document.is_object() || !document.as_object().count("bars") ||
        !document.at("bars").is_array())
        throw Error("HK adjustment needs a K-line bars array");
    const bool fixed = mode == "fixed_qfq" || mode == "fixed_hfq";
    const auto anchor = compact_date(anchor_date, fixed);
    const auto record_set = hk_actions_detail::load_records(root);
    const auto steps = steps_for(*record_set, code);

    auto& bars = document.as_object().at("bars").as_array();
    std::string first_date, last_date;
    for (const auto& bar : bars) {
        const auto* date = value(bar, "date");
        if (!date || !date->is_string())
            throw Error("K-line bar has no date");
        const auto compact = compact_date(date->as_string());
        if (first_date.empty() || compact < first_date) first_date = compact;
        if (last_date.empty() || compact > last_date) last_date = compact;
    }

    std::map<const Step*, std::size_t> affected;
    double minimum_scale = std::numeric_limits<double>::infinity();
    double maximum_scale = 0.0;
    double minimum_offset = std::numeric_limits<double>::infinity();
    double maximum_offset = -std::numeric_limits<double>::infinity();
    for (auto& bar : bars) {
        const auto date = compact_date(value(bar, "date")->as_string());
        std::vector<const Step*> selected;
        bool forward = mode == "qfq";
        for (const auto& step : steps) {
            const auto& event_date = step.record->date;
            bool use = false;
            if (mode == "qfq")
                use = event_date >= first_date && event_date <= last_date &&
                      date < event_date;
            else if (mode == "hfq")
                use = event_date >= first_date && event_date <= last_date &&
                      date >= event_date;
            else if (date < anchor) {
                forward = true;
                use = date < event_date && event_date <= anchor;
            } else if (date > anchor) {
                forward = false;
                use = anchor < event_date && event_date <= date;
            }
            if (use) {
                selected.push_back(&step);
                ++affected[&step];
            }
        }

        double scale = 1.0;
        double offset = 0.0;
        for (const auto* step : selected)
            compose(forward ? step->forward_scale : step->backward_scale,
                    forward ? step->forward_offset : step->backward_offset,
                    scale, offset);
        minimum_scale = std::min(minimum_scale, scale);
        maximum_scale = std::max(maximum_scale, scale);
        minimum_offset = std::min(minimum_offset, offset);
        maximum_offset = std::max(maximum_offset, offset);

        auto& values = bar.as_object();
        for (const auto* key : {"open", "high", "low", "close"}) {
            const auto found = values.find(key);
            if (found == values.end() || !found->second.is_number())
                throw Error(std::string("K-line bar has no numeric ") + key);
            found->second = static_cast<double>(transformed_price(
                found->second.as_number(), selected, forward));
        }
        values["adjustment_factor"] = scale;
        values["adjustment_scale"] = scale;
        values["adjustment_offset"] = offset;
    }
    if (bars.empty()) {
        minimum_scale = maximum_scale = 1.0;
        minimum_offset = maximum_offset = 0.0;
    }

    Json applied = Json::array();
    for (const auto& step : steps) {
        const auto count = affected[&step];
        if (count) applied.push_back(event_json(step, count));
    }
    Json metadata = Json::object();
    metadata["mode"] = mode;
    metadata["source_command"] = "local-hkqxinfo";
    metadata["source_mode"] = "local";
    metadata["source_format"] = "hkqxinfo2.dat+hkqxinfo.dat";
    metadata["method"] = "tdx-hk-native-affine-v1";
    metadata["anchor_date"] = anchor.empty()
        ? Json(nullptr) : Json(display_date(anchor));
    metadata["minimum_factor"] = minimum_scale;
    metadata["maximum_factor"] = maximum_scale;
    metadata["minimum_offset"] = minimum_offset;
    metadata["maximum_offset"] = maximum_offset;
    metadata["applied_event_count"] =
        static_cast<std::uint64_t>(applied.size());
    metadata["available_event_count"] =
        static_cast<std::uint64_t>(steps.size());
    metadata["applied_events"] = std::move(applied);
    Json cache = Json::object();
    cache["status"] = "file-stamp-cache";
    cache["shared"] = true;
    metadata["input_cache"] = std::move(cache);
    document["adjustment_mode"] = mode;
    document["adjustment"] = std::move(metadata);
    return document;
}

Json build_hk_divfactor_series_document(
    const std::filesystem::path& root, const std::string& raw_code,
    const Json& kline_document) {
    const auto code = trim(raw_code);
    if (code.size() != 5)
        throw Error("HK DIVFACTOR code must contain exactly five digits");
    const auto* bars = value(kline_document, "bars");
    if (!bars || !bars->is_array())
        throw Error("HK DIVFACTOR formula context needs K-line bars");
    const auto record_set = hk_actions_detail::load_records(root);
    const auto steps = steps_for(*record_set, code);
    std::map<std::string, float> factors_by_date;
    for (const auto& step : steps) {
        const float factor = static_cast<float>(step.backward_scale);
        if (std::abs(factor - 1.0F) <= 0.00001F) continue;
        auto& combined = factors_by_date[step.record->date];
        combined = combined == 0.0F
            ? factor : static_cast<float>(combined * factor);
    }

    std::vector<float> event_factors(bars->as_array().size(), 0.0F);
    std::vector<float> front(event_factors.size(), 1.0F);
    std::vector<float> back(event_factors.size(), 1.0F);
    std::uint64_t matched = 0;
    for (std::size_t index = 0; index < bars->as_array().size(); ++index) {
        const auto* date = value(bars->as_array()[index], "date");
        if (!date || !date->is_string())
            throw Error("HK DIVFACTOR K-line bar has no date");
        const auto found = factors_by_date.find(compact_date(date->as_string()));
        if (found == factors_by_date.end()) continue;
        event_factors[index] = found->second;
        ++matched;
    }
    for (std::size_t index = 0; index < event_factors.size(); ++index) {
        const auto factor = event_factors[index];
        if (!(factor > 0.00001F)) continue;
        for (std::size_t prior = 0; prior < index; ++prior)
            front[prior] = static_cast<float>(front[prior] / factor);
    }
    for (std::size_t reverse = event_factors.size(); reverse > 0; --reverse) {
        const auto index = reverse - 1;
        const auto factor = event_factors[index];
        if (!(factor > 0.00001F)) continue;
        for (std::size_t after = index; after < back.size(); ++after)
            back[after] = static_cast<float>(back[after] * factor);
    }

    Json front_points = Json::object();
    Json back_points = Json::object();
    for (std::size_t index = 0; index < bars->as_array().size(); ++index) {
        const auto& bar = bars->as_array()[index];
        const auto* date = value(bar, "date");
        const auto* time = value(bar, "time");
        const auto key = date->as_string() + "|" +
            (time && time->is_string() ? time->as_string() : std::string{});
        front_points[key] = static_cast<double>(front[index]);
        back_points[key] = static_cast<double>(back[index]);
    }
    Json result = Json::object();
    result["schema"] = "tdx-formula-divfactor-series-v1";
    result["front"] = std::move(front_points);
    result["back"] = std::move(back_points);
    result["event_date_count"] =
        static_cast<std::uint64_t>(factors_by_date.size());
    result["matched_bar_count"] = matched;
    result["mode"] =
        "TCalc-DIVFACTOR-HK-hkqxinfo-native-share-multiplier-float32";
    return result;
}

}  // namespace tdx
