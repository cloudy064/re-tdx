#include "corporate_internal.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>

namespace tdx::corporate_detail {
const Json* object_value(const Json& object, std::string_view key) {
    if (!object.is_object()) return nullptr;
    const auto found = object.as_object().find(key);
    return found == object.as_object().end() ? nullptr : &found->second;
}

double number_or(const Json& object, std::string_view key, double fallback = 0.0) {
    const auto* value = object_value(object, key);
    return value && value->is_number() ? value->as_number() : fallback;
}

std::string normalized_date(std::string value, bool required) {
    value = trim(std::move(value));
    value.erase(std::remove(value.begin(), value.end(), '-'), value.end());
    if (value.empty() && !required) return {};
    if (value.size() != 8 ||
        !std::all_of(value.begin(), value.end(), [](char ch) { return ch >= '0' && ch <= '9'; }))
        throw Error("adjustment anchor date must be YYYY-MM-DD or YYYYMMDD");
    return value.substr(0, 4) + "-" + value.substr(4, 2) + "-" + value.substr(6, 2);
}

struct DividendTerms {
    double cash_per_share{};
    double bonus_per_share{};
    double rights_per_share{};
    double rights_consideration_per_share{};
    int event_count{};
};

struct AdjustmentEvent {
    std::string date;
    double previous_close{};
    double ratio{};
    DividendTerms terms;
};

std::vector<const Json*> capital_records(const Json& document) {
    std::vector<const Json*> result;
    if (const auto* blocks = object_value(document, "blocks"); blocks && blocks->is_array()) {
        for (const auto& block : blocks->as_array()) {
            const auto* records = object_value(block, "records");
            if (!records || !records->is_array()) continue;
            for (const auto& record : records->as_array()) result.push_back(&record);
        }
        return result;
    }
    if (const auto* records = object_value(document, "records"); records && records->is_array())
        for (const auto& record : records->as_array()) result.push_back(&record);
    return result;
}

std::vector<AdjustmentEvent> adjustment_events(const Json& daily_document,
                                                const Json& capital_document,
                                                Json& ignored) {
    const auto* daily_bars = object_value(daily_document, "bars");
    if (!daily_bars || !daily_bars->is_array()) throw Error("daily K-line document has no bars");
    std::map<std::string, double> closes;
    for (const auto& bar : daily_bars->as_array()) {
        const auto* date = object_value(bar, "date");
        const auto* close = object_value(bar, "close");
        if (!date || !date->is_string() || !close || !close->is_number()) continue;
        closes[date->as_string()] = close->as_number();
    }
    if (closes.empty()) throw Error("daily K-line document has no usable closes");

    std::map<std::string, DividendTerms> grouped;
    for (const auto* record : capital_records(capital_document)) {
        const auto* category = object_value(*record, "category");
        const auto* date = object_value(*record, "date");
        if (!category || !category->is_number() || category->as_number() != 1 ||
            !date || !date->is_string()) continue;
        const auto* details = object_value(*record, "details");
        if (!details || !details->is_object()) continue;
        auto& terms = grouped[date->as_string()];
        const double cash = number_or(*details, "dividend_per_share_yuan");
        const double bonus = number_or(*details, "bonus_transfer_per_10_shares") / 10.0;
        const double rights = number_or(*details, "rights_per_10_shares") / 10.0;
        const double rights_price = number_or(*details, "rights_price_yuan");
        if (!std::isfinite(cash) || !std::isfinite(bonus) || !std::isfinite(rights) ||
            !std::isfinite(rights_price)) throw Error("capital adjustment terms are not finite");
        terms.cash_per_share += cash;
        terms.bonus_per_share += bonus;
        terms.rights_per_share += rights;
        terms.rights_consideration_per_share += rights * rights_price;
        ++terms.event_count;
    }

    std::vector<AdjustmentEvent> result;
    for (const auto& [event_date, terms] : grouped) {
        Json failure = Json::object();
        failure["date"] = event_date;
        if (event_date > closes.rbegin()->first) {
            failure["reason"] = "event is later than the newest daily bar";
            ignored.push_back(std::move(failure));
            continue;
        }
        const auto on_or_after = closes.lower_bound(event_date);
        if (on_or_after == closes.begin()) {
            failure["reason"] = "no previous trading-day close";
            ignored.push_back(std::move(failure));
            continue;
        }
        const auto previous = std::prev(on_or_after);
        const double denominator = previous->second *
            (1.0 + terms.bonus_per_share + terms.rights_per_share);
        const double theoretical = previous->second - terms.cash_per_share +
                                   terms.rights_consideration_per_share;
        const double ratio = theoretical / denominator;
        if (!(ratio > 0.0) || !std::isfinite(ratio)) {
            failure["reason"] = "computed factor is invalid";
            ignored.push_back(std::move(failure));
            continue;
        }
        result.push_back(AdjustmentEvent{event_date, previous->second, ratio, terms});
    }
    std::sort(result.begin(), result.end(), [](const auto& left, const auto& right) {
        return left.date < right.date;
    });
    return result;
}

double qfq_factor_for(const std::vector<AdjustmentEvent>& events, const std::string& date) {
    double factor = 1.0;
    for (const auto& event : events)
        if (event.date > date) factor *= event.ratio;
    if (!(factor > 0.0) || !std::isfinite(factor)) throw Error("cumulative adjustment factor is invalid");
    return factor;
}

}  // namespace tdx::corporate_detail

namespace tdx {

using namespace corporate_detail;
std::string normalize_kline_adjustment_mode(const std::string& requested_mode) {
    auto mode = lower_ascii(trim(requested_mode));
    if (mode.empty() || mode == "raw") mode = "none";
    if (mode == "front") mode = "qfq";
    if (mode == "back") mode = "hfq";
    if (mode != "none" && mode != "qfq" && mode != "hfq" &&
        mode != "fixed_qfq" && mode != "fixed_hfq")
        throw Error("adjust must be none, qfq, hfq, fixed_qfq, or fixed_hfq");
    return mode;
}

Json apply_kline_adjustment(Json document, const Json& daily_document,
                            const Json& capital_document, const std::string& requested_mode,
                            const std::string& anchor_date) {
    const auto mode = normalize_kline_adjustment_mode(requested_mode);
    if (mode == "none") {
        document["adjustment_mode"] = "none";
        return document;
    }
    const bool fixed = mode == "fixed_qfq" || mode == "fixed_hfq";
    const auto anchor = normalized_date(anchor_date, fixed);
    Json ignored = Json::array();
    const auto events = adjustment_events(daily_document, capital_document, ignored);
    const auto* daily_bars = object_value(daily_document, "bars");
    if (!daily_bars || daily_bars->as_array().empty())
        throw Error("daily K-line document is empty");
    std::string earliest_date;
    for (const auto& bar : daily_bars->as_array()) {
        const auto* value = object_value(bar, "date");
        if (value && value->is_string() &&
            (earliest_date.empty() || value->as_string() < earliest_date))
            earliest_date = value->as_string();
    }
    if (earliest_date.empty()) throw Error("daily K-line bars have no dates");
    double normalization = 1.0;
    if (mode == "hfq") normalization = qfq_factor_for(events, earliest_date);
    else if (fixed) normalization = qfq_factor_for(events, anchor);
    if (!(normalization > 0.0) || !std::isfinite(normalization))
        throw Error("adjustment normalization is invalid");

    auto& bars = document.as_object().at("bars").as_array();
    double minimum_factor = std::numeric_limits<double>::infinity();
    double maximum_factor = 0.0;
    for (auto& bar : bars) {
        const auto* date = object_value(bar, "date");
        if (!date || !date->is_string()) throw Error("K-line bar has no date");
        const double factor = qfq_factor_for(events, date->as_string()) / normalization;
        minimum_factor = std::min(minimum_factor, factor);
        maximum_factor = std::max(maximum_factor, factor);
        auto& values = bar.as_object();
        for (const auto* key : {"open", "high", "low", "close"}) {
            auto found = values.find(key);
            if (found == values.end() || !found->second.is_number())
                throw Error(std::string("K-line bar has no numeric ") + key);
            found->second = found->second.as_number() * factor;
        }
        values["adjustment_factor"] = factor;
    }
    if (bars.empty()) minimum_factor = maximum_factor = 1.0;

    Json applied = Json::array();
    for (const auto& event : events) {
        Json value = Json::object();
        value["date"] = event.date;
        value["previous_close"] = event.previous_close;
        value["event_factor"] = event.ratio;
        value["cash_per_share_yuan"] = event.terms.cash_per_share;
        value["bonus_per_share"] = event.terms.bonus_per_share;
        value["rights_per_share"] = event.terms.rights_per_share;
        value["rights_consideration_per_share"] =
            event.terms.rights_consideration_per_share;
        value["source_event_count"] = event.terms.event_count;
        applied.push_back(std::move(value));
    }
    Json metadata = Json::object();
    metadata["mode"] = mode;
    metadata["source_command"] = "0x000F";
    metadata["method"] = "local-corporate-action-factor-v1";
    if (const auto* source_mode = object_value(capital_document, "source_mode"))
        metadata["source_mode"] = *source_mode;
    if (const auto* source_format = object_value(capital_document, "source_format"))
        metadata["source_format"] = *source_format;
    metadata["anchor_date"] = anchor.empty() ? Json(nullptr) : Json(anchor);
    metadata["normalization"] = normalization;
    metadata["minimum_factor"] = minimum_factor;
    metadata["maximum_factor"] = maximum_factor;
    metadata["applied_event_count"] = static_cast<std::uint64_t>(events.size());
    metadata["applied_events"] = std::move(applied);
    metadata["ignored_events"] = std::move(ignored);
    document["adjustment_mode"] = mode;
    document["adjustment"] = std::move(metadata);
    return document;
}

Json summarize_kline_adjustments(const std::vector<Json>& documents,
                                 const std::string& requested_mode) {
    const auto mode = normalize_kline_adjustment_mode(requested_mode);
    Json summary = Json::object();
    summary["mode"] = mode;
    summary["security_scope"] = "per-security";
    if (mode == "none") return summary;
    summary["source_command"] = "0x000F";
    summary["method"] = "local-corporate-action-factor-v1";
    summary["anchor_date"] = Json(nullptr);
    Json cache_counts = Json::object();
    bool anchor_selected = false;
    for (const auto& document : documents) {
        const auto* adjustment = object_value(document, "adjustment");
        if (!adjustment || !adjustment->is_object()) continue;
        if (!anchor_selected) {
            if (const auto* anchor = object_value(*adjustment, "anchor_date")) {
                summary["anchor_date"] = *anchor;
                anchor_selected = true;
            }
        }
        const auto* cache = object_value(*adjustment, "input_cache");
        const auto* status = cache ? object_value(*cache, "status") : nullptr;
        if (!status || !status->is_string()) continue;
        auto& values = cache_counts.as_object();
        const auto found = values.find(status->as_string());
        values[status->as_string()] = found != values.end() && found->second.is_number()
            ? found->second.as_number() + 1.0 : 1.0;
    }
    summary["input_cache_counts"] = std::move(cache_counts);
    summary["shared_cache"] = kline_adjustment_cache_document();
    return summary;
}

std::string uniform_kline_adjustment_mode(
    const std::vector<Json>& documents,
    const std::string& fallback_mode) {
    std::string result;
    for (const auto& document : documents) {
        const auto* value = object_value(document, "adjustment_mode");
        std::string mode = "none";
        if (value && value->is_string())
            mode = normalize_kline_adjustment_mode(value->as_string());
        else if (value && value->is_number()) {
            const auto numeric = value->as_number();
            if (numeric == 0) mode = "none";
            else if (numeric == 1) mode = "qfq";
            else if (numeric == 2) mode = "hfq";
            else throw Error("K-line adjustment_mode must be 0, 1, or 2");
        } else if (value) {
            throw Error("K-line adjustment_mode must be a string or integer");
        }
        if (result.empty()) result = mode;
        else if (result != mode)
            throw Error("K-line documents must use one uniform adjustment mode");
    }
    return result.empty() ? normalize_kline_adjustment_mode(fallback_mode) : result;
}

}  // namespace tdx
