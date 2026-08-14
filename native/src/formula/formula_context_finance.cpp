#include "formula_context_finance_internal.hpp"

#include "tdx/common.hpp"
#include "tdx/disclosures.hpp"
#include "tdx/formula_context.hpp"
#include "tdx/professional_data.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <string_view>

namespace tdx {
namespace {

const Json* optional(const Json& object, std::string_view key) {
    if (!object.is_object()) return nullptr;
    const auto found = object.as_object().find(key);
    return found == object.as_object().end() ? nullptr : &found->second;
}

std::string text_or(const Json& object, std::string_view key,
                    std::string fallback = {}) {
    const auto* value = optional(object, key);
    return value && value->is_string() ? value->as_string() : std::move(fallback);
}

std::string compact_date(const std::string& value) {
    std::string result;
    for (const unsigned char ch : value)
        if (std::isdigit(ch)) result.push_back(static_cast<char>(ch));
    return result.size() >= 8 ? result.substr(0, 8) : std::string{};
}

struct CapitalEvent {
    std::string date;
    double before{};
    double after{};
};

std::vector<CapitalEvent> circulating_capital_events(const Json& document) {
    std::vector<CapitalEvent> result;
    const auto append = [&](const Json& records) {
        if (!records.is_array()) return;
        for (const auto& record : records.as_array()) {
            const auto date = text_or(record, "date");
            const auto* details = optional(record, "details");
            const auto* before = details
                ? optional(*details, "before_circulating_shares") : nullptr;
            const auto* after = details
                ? optional(*details, "after_circulating_shares") : nullptr;
            if (date.empty() || !before || !after || !before->is_number() ||
                !after->is_number())
                continue;
            const double before_value = before->as_number();
            const double after_value = after->as_number();
            if (!(before_value > 0.0) || !(after_value > 0.0) ||
                !std::isfinite(before_value) || !std::isfinite(after_value))
                continue;
            result.push_back({date, before_value, after_value});
        }
    };
    if (const auto* blocks = optional(document, "blocks");
        blocks && blocks->is_array())
        for (const auto& block : blocks->as_array())
            if (const auto* records = optional(block, "records")) append(*records);
    if (const auto* records = optional(document, "records")) append(*records);
    std::stable_sort(result.begin(), result.end(),
                     [](const auto& left, const auto& right) {
                         return left.date < right.date;
                     });
    return result;
}

Json historical_capital_points(const Json& kline,
                               const std::vector<CapitalEvent>& events,
                               double current_circulating_shares) {
    const auto* bars = optional(kline, "bars");
    if (!bars || !bars->is_array())
        throw Error("chip formula context needs K-line bars");
    if (events.empty() && (!(current_circulating_shares > 0.0) ||
                           !std::isfinite(current_circulating_shares)))
        throw Error("chip formula context has no circulating-capital history");
    Json points = Json::object();
    for (const auto& bar : bars->as_array()) {
        const auto date = text_or(bar, "date");
        const auto time = text_or(bar, "time");
        if (date.empty()) continue;
        double shares = current_circulating_shares;
        if (!events.empty()) {
            shares = events.front().before;
            for (const auto& event : events) {
                if (event.date > date) break;
                shares = event.after;
            }
        }
        if (shares > 0.0 && std::isfinite(shares))
            points[date + "|" + time] = shares / 100.0;
    }
    return points;
}

struct SplitEvent {
    std::string date;
    double cash_per_10{};
    double bonus_transfer_per_10{};
};

std::vector<SplitEvent> split_events(const Json& document) {
    std::vector<SplitEvent> result;
    const auto append = [&](const Json& records) {
        if (!records.is_array()) return;
        for (const auto& record : records.as_array()) {
            const auto* category = optional(record, "category");
            const auto* details = optional(record, "details");
            const auto date = text_or(record, "date");
            if (!category || !category->is_number() ||
                category->as_number() != 1.0 || !details ||
                !details->is_object() || date.empty())
                continue;
            const auto* cash =
                optional(*details, "dividend_per_10_shares_yuan");
            const auto* bonus =
                optional(*details, "bonus_transfer_per_10_shares");
            const double cash_value =
                cash && cash->is_number() ? cash->as_number() : 0.0;
            const double bonus_value =
                bonus && bonus->is_number() ? bonus->as_number() : 0.0;
            if (!std::isfinite(cash_value) || !std::isfinite(bonus_value))
                continue;
            result.push_back({date, cash_value, bonus_value});
        }
    };
    if (const auto* blocks = optional(document, "blocks");
        blocks && blocks->is_array())
        for (const auto& block : blocks->as_array())
            if (const auto* records = optional(block, "records")) append(*records);
    if (const auto* records = optional(document, "records")) append(*records);
    std::stable_sort(result.begin(), result.end(),
                     [](const auto& left, const auto& right) {
                         return left.date < right.date;
                     });
    return result;
}

bool split_event_matches(const SplitEvent& event, int type) {
    constexpr double epsilon = 0.00001;
    if (type == 0) return event.bonus_transfer_per_10 > epsilon;
    if (type == 1) return event.cash_per_10 > epsilon;
    return type == 2 &&
           event.cash_per_10 + event.bonus_transfer_per_10 > epsilon;
}

Json split_binding_points(
    const Json& kline, const std::vector<SplitEvent>& events,
    const formula_context_detail::SplitBinding& binding) {
    const auto* bars = optional(kline, "bars");
    if (!bars || !bars->is_array())
        throw Error("split formula context needs K-line bars");
    Json points = Json::object();
    for (const auto& bar : bars->as_array()) {
        const auto date = text_or(bar, "date");
        const auto time = text_or(bar, "time");
        if (date.empty()) continue;
        const SplitEvent* selected = nullptr;
        int occurrence = 0;
        for (auto event = events.rbegin(); event != events.rend(); ++event) {
            if (event->date > date || !split_event_matches(*event, binding.type))
                continue;
            if (occurrence++ == binding.occurrence) {
                selected = &*event;
                break;
            }
        }
        if (!selected) continue;
        double value = std::numeric_limits<double>::quiet_NaN();
        if (binding.name.rfind("SPLITBARS#", 0) == 0) {
            value = 0.0;
            for (const auto& candidate : bars->as_array()) {
                const auto candidate_date = text_or(candidate, "date");
                if (candidate_date > selected->date && candidate_date <= date)
                    value += 1.0;
            }
        } else if (binding.type == 0) {
            value = selected->bonus_transfer_per_10 /
                    (selected->bonus_transfer_per_10 + 10.0);
        } else if (binding.type == 1) {
            value = selected->cash_per_10 / 10.0;
        }
        if (std::isfinite(value)) points[date + "|" + time] = value;
    }
    return points;
}

struct DivFactorPoints {
    Json front{Json::object()};
    Json back{Json::object()};
    std::uint64_t matched_bars{};
    std::uint64_t event_dates{};
};

DivFactorPoints divfactor_points(const Json& kline,
                                 const std::vector<SplitEvent>& events) {
    const auto* bars = optional(kline, "bars");
    if (!bars || !bars->is_array())
        throw Error("DIVFACTOR formula context needs K-line bars");
    std::map<std::string, float> factors_by_date;
    for (const auto& event : events) {
        const float raw = static_cast<float>(event.bonus_transfer_per_10);
        if (raw > 0.00001F)
            factors_by_date[event.date] =
                static_cast<float>((raw + 10.0F) / 10.0F);
    }
    std::vector<float> event_factors(bars->as_array().size(), 0.0F);
    std::vector<float> front(event_factors.size(), 1.0F);
    std::vector<float> back(event_factors.size(), 1.0F);
    DivFactorPoints result;
    result.event_dates = static_cast<std::uint64_t>(factors_by_date.size());
    for (std::size_t index = 0; index < bars->as_array().size(); ++index) {
        const auto found = factors_by_date.find(
            text_or(bars->as_array()[index], "date"));
        if (found == factors_by_date.end()) continue;
        event_factors[index] = found->second;
        ++result.matched_bars;
    }
    for (std::size_t index = 0; index < event_factors.size(); ++index) {
        const float factor = event_factors[index];
        if (!(factor > 0.00001F)) continue;
        for (std::size_t prior = 0; prior < index; ++prior)
            front[prior] = static_cast<float>(front[prior] / factor);
    }
    for (std::size_t reverse = event_factors.size(); reverse > 0; --reverse) {
        const std::size_t index = reverse - 1;
        const float factor = event_factors[index];
        if (!(factor > 0.00001F)) continue;
        for (std::size_t after = index; after < back.size(); ++after)
            back[after] = static_cast<float>(back[after] * factor);
    }
    for (std::size_t index = 0; index < bars->as_array().size(); ++index) {
        const auto& bar = bars->as_array()[index];
        const auto key = text_or(bar, "date") + "|" + text_or(bar, "time");
        result.front[key] = static_cast<double>(front[index]);
        result.back[key] = static_cast<double>(back[index]);
    }
    return result;
}

}  // namespace

namespace formula_context_detail {

void bind_capital_history_context(
    Json& context, const Json& kline_document, const Json& capital_document,
    double current_circulating_shares) {
    const auto events = circulating_capital_events(capital_document);
    if (!context.as_object().count("series")) context["series"] = Json::object();
    context["series"]["CAPITAL"] = historical_capital_points(
        kline_document, events, current_circulating_shares);
    context["capital_series_mode"] = "tdx-0x000f-historical";
    context["capital_series_event_count"] =
        static_cast<std::uint64_t>(events.size());
    if (!events.empty()) {
        context["capital_series_oldest_event"] = events.front().date;
        context["capital_series_newest_event"] = events.back().date;
    }
}

void bind_split_context(
    Json& context, const Json& kline_document, const Json& capital_document,
    const std::vector<SplitBinding>& bindings) {
    const auto events = split_events(capital_document);
    if (!context.as_object().count("series")) context["series"] = Json::object();
    for (const auto& binding : bindings)
        context["series"][binding.name] =
            split_binding_points(kline_document, events, binding);
    context["split_series_mode"] =
        "tcalc-type164-category1-date-bounded-occurrence-series";
    context["split_series_event_count"] =
        static_cast<std::uint64_t>(events.size());
    context["split_series_binding_count"] =
        static_cast<std::uint64_t>(bindings.size());
    if (!events.empty()) {
        context["split_series_oldest_event"] = events.front().date;
        context["split_series_newest_event"] = events.back().date;
    }
}

void bind_point_in_time_finance_context(
    Json& context, const std::filesystem::path& root,
    const std::string& market, const std::string& code,
    const Json& kline_document, const std::set<int>& finance_bindings,
    const std::set<int>& finvalue_bindings, int timeout_ms) {
    const auto archive_path = default_disclosure_archive_path(root);
    if (!std::filesystem::is_regular_file(archive_path))
        throw Error("point-in-time finance archive is missing; run 'tdx-tool market "
                    "disclosures --archive --root <TDX>' first: " +
                    path_utf8(archive_path));
    const auto archive = Json::parse(read_text_utf8(archive_path));
    if (!archive.is_object() || !archive.as_object().count("schema") ||
        !archive.at("schema").is_string() ||
        archive.at("schema").as_string() !=
            "tdx-disclosure-availability-archive-v1" ||
        !archive.as_object().count("entries") ||
        !archive.at("entries").is_array())
        throw Error("point-in-time finance archive has an unsupported schema: " +
                    path_utf8(archive_path));
    const auto normalized_market = lower_ascii(
        market == "0" ? "sz" : market == "1" ? "sh" :
        market == "2" ? "bj" : market);
    std::string latest_bar_date;
    if (const auto* bars = optional(kline_document, "bars");
        bars && bars->is_array())
        for (const auto& bar : bars->as_array())
            latest_bar_date =
                std::max(latest_bar_date, compact_date(text_or(bar, "date")));
    if (latest_bar_date.empty())
        throw Error("point-in-time finance context needs dated K-line bars");

    struct Event {
        std::string report_period;
        std::string available_from;
    };
    std::map<std::string, Event> events_by_period;
    for (const auto& entry : archive.at("entries").as_array()) {
        if (!entry.is_object() ||
            lower_ascii(text_or(entry, "market")) != normalized_market ||
            text_or(entry, "code") != code)
            continue;
        const auto period = compact_date(text_or(entry, "report_period"));
        const auto available =
            compact_date(text_or(entry, "report_available_from"));
        if (period.empty() || available.empty() || available >= latest_bar_date)
            continue;
        auto& event = events_by_period[period];
        event.report_period = period;
        if (event.available_from.empty() || available < event.available_from)
            event.available_from = available;
    }
    if (events_by_period.empty())
        throw Error("point-in-time finance archive has no actual full-report disclosure "
                    "before the requested K-line endpoint for " + normalized_market +
                    ":" + code);

    std::vector<Event> events;
    for (const auto& [period, event] : events_by_period) {
        (void)period;
        events.push_back(event);
    }
    std::sort(events.begin(), events.end(),
              [](const Event& left, const Event& right) {
                  return left.report_period > right.report_period;
              });
    const bool truncated = events.size() > 80;
    if (truncated) events.resize(80);
    std::reverse(events.begin(), events.end());

    const int market_id = normalized_market == "sh" ? 1 :
                          normalized_market == "bj" ? 2 : 0;
    const auto cache_directory =
        root / "T0002" / "tdx-tool" / "professional-finance-cache";
    Json reports = Json::array();
    Json failures = Json::array();
    for (const auto& event : events) {
        try {
            const auto report_date =
                static_cast<std::uint32_t>(std::stoul(event.report_period));
            const auto data = fetch_professional_finance_data(
                report_date, cache_directory, timeout_ms, false);
            const auto found = data.records.find({market_id, code});
            if (found == data.records.end())
                throw Error("security absent from report package");
            Json report = Json::object();
            report["report_period"] = event.report_period;
            report["available_from"] = event.available_from;
            Json finance = Json::object();
            Json finvalue = Json::object();
            for (const int id : finance_bindings)
                if (const auto value =
                        professional_finance_growth_value(found->second, id))
                    finance[std::to_string(id)] = *value;
            for (const int id : finvalue_bindings) {
                if (id == 0) continue;
                if (id > 0 &&
                    static_cast<std::size_t>(id) <= found->second.fields.size() &&
                    found->second.fields[static_cast<std::size_t>(id - 1)])
                    finvalue[std::to_string(id)] =
                        *found->second.fields[static_cast<std::size_t>(id - 1)];
            }
            report["finance"] = std::move(finance);
            report["finvalue"] = std::move(finvalue);
            reports.push_back(std::move(report));
        } catch (const std::exception& error) {
            Json failure = Json::object();
            failure["report_period"] = event.report_period;
            failure["available_from"] = event.available_from;
            failure["error"] = error.what();
            failures.push_back(std::move(failure));
        }
    }
    if (!reports.size()) {
        std::string detail =
            "point-in-time finance could not load any archived report package";
        if (failures.size())
            detail += ": " +
                      failures.as_array().front().at("error").as_string();
        throw Error(detail);
    }
    auto generated = build_point_in_time_finance_series_document(
        kline_document, reports, finance_bindings, finvalue_bindings);
    if (!context.as_object().count("series")) context["series"] = Json::object();
    for (const auto& [name, points] : generated.at("series").as_object())
        context["series"][name] = points;
    context["finance_point_in_time_mode"] =
        "archived-actual-full-report-next-bar-no-current-fallback";
    context["finance_point_in_time_archive"] = path_utf8(archive_path);
    context["finance_point_in_time_cache"] = path_utf8(cache_directory);
    context["finance_point_in_time_archive_event_count"] =
        static_cast<std::uint64_t>(events_by_period.size());
    context["finance_point_in_time_loaded_report_count"] =
        static_cast<std::uint64_t>(reports.size());
    context["finance_point_in_time_failed_report_count"] =
        static_cast<std::uint64_t>(failures.size());
    context["finance_point_in_time_failures"] = std::move(failures);
    context["finance_point_in_time_truncated"] = truncated;
    context["finance_point_in_time_same_day_available"] = false;
    context["finance_point_in_time_first_available_from"] =
        generated.at("first_available_from");
    context["finance_point_in_time_last_available_from"] =
        generated.at("last_available_from");
}

}  // namespace formula_context_detail

Json build_point_in_time_finance_series_document(
    const Json& kline_document, const Json& available_reports,
    const std::set<int>& finance_bindings,
    const std::set<int>& finvalue_bindings) {
    const auto* bars = optional(kline_document, "bars");
    if (!bars || !bars->is_array())
        throw Error("point-in-time finance series needs K-line bars");
    if (!available_reports.is_array())
        throw Error("point-in-time finance series needs an available-report array");
    struct Report {
        std::string period;
        std::string available;
        const Json* finance{};
        const Json* finvalue{};
    };
    std::vector<Report> reports;
    for (const auto& row : available_reports.as_array()) {
        if (!row.is_object()) continue;
        const auto period = compact_date(text_or(row, "report_period"));
        const auto available = compact_date(text_or(row, "available_from"));
        if (period.empty() || available.empty()) continue;
        const auto* finance = optional(row, "finance");
        const auto* finvalue = optional(row, "finvalue");
        reports.push_back(
            {period, available,
             finance && finance->is_object() ? finance : nullptr,
             finvalue && finvalue->is_object() ? finvalue : nullptr});
    }
    std::sort(reports.begin(), reports.end(),
              [](const Report& left, const Report& right) {
                  if (left.period != right.period)
                      return left.period < right.period;
                  return left.available < right.available;
              });
    Json series = Json::object();
    for (const int id : finance_bindings)
        series["FINANCE#" + std::to_string(id)] = Json::object();
    for (const int id : finvalue_bindings)
        series["FINVALUE#" + std::to_string(id)] = Json::object();
    for (const auto& bar : bars->as_array()) {
        const auto bar_date = compact_date(text_or(bar, "date"));
        if (bar_date.empty()) continue;
        const Report* selected = nullptr;
        for (const auto& report : reports) {
            if (report.available >= bar_date) continue;
            if (!selected || report.period > selected->period ||
                (report.period == selected->period &&
                 report.available > selected->available))
                selected = &report;
        }
        if (!selected) continue;
        const auto point_key =
            text_or(bar, "date") + "|" + text_or(bar, "time");
        for (const int id : finance_bindings) {
            if (!selected->finance) continue;
            const auto found =
                selected->finance->as_object().find(std::to_string(id));
            if (found != selected->finance->as_object().end() &&
                found->second.is_number())
                series["FINANCE#" + std::to_string(id)][point_key] =
                    found->second;
        }
        for (const int id : finvalue_bindings) {
            if (id == 0) {
                series["FINVALUE#0"][point_key] =
                    static_cast<std::uint64_t>(std::stoul(selected->period));
                continue;
            }
            if (!selected->finvalue) continue;
            const auto found =
                selected->finvalue->as_object().find(std::to_string(id));
            if (found != selected->finvalue->as_object().end() &&
                found->second.is_number())
                series["FINVALUE#" + std::to_string(id)][point_key] =
                    found->second;
        }
    }
    Json result = Json::object();
    result["series"] = std::move(series);
    result["report_count"] = static_cast<std::uint64_t>(reports.size());
    if (reports.empty()) {
        result["first_available_from"] = Json(nullptr);
        result["last_available_from"] = Json(nullptr);
    } else {
        auto first = reports.front().available;
        auto last = reports.front().available;
        for (const auto& report : reports) {
            first = std::min(first, report.available);
            last = std::max(last, report.available);
        }
        result["first_available_from"] = first;
        result["last_available_from"] = last;
    }
    result["same_day_available"] = false;
    result["selection_rule"] =
        "largest report_period whose actual disclosure date is before the bar date";
    return result;
}

Json build_divfactor_series_document(const Json& kline_document,
                                     const Json& capital_document) {
    const auto events = split_events(capital_document);
    auto points = divfactor_points(kline_document, events);
    Json result = Json::object();
    result["schema"] = "tdx-formula-divfactor-series-v1";
    result["front"] = std::move(points.front);
    result["back"] = std::move(points.back);
    result["event_date_count"] = points.event_dates;
    result["matched_bar_count"] = points.matched_bars;
    result["mode"] =
        "TCalc-opcode1359-type164-category1-offset21-bonus-only-float32";
    return result;
}

}  // namespace tdx
