#include "formula_context_aggregate_internal.hpp"

#include "formula_catalog_internal.hpp"

#include "tdx/common.hpp"
#include "tdx/formula_engine.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <optional>

namespace tdx::formula_context_detail {

void bind_indicator_aggregates(
    Json& context, const std::filesystem::path& root, const BlockData& data,
    const Json& target, int target_market_id, const std::string& target_code,
    const std::vector<IndicatorAggregateBinding>& bindings,
    const Json& formula_library) {
    if (bindings.empty()) return;
    if (lower_ascii(aggregate_text_or(target, "period", "day")) != "day")
        throw Error("INSORT/INSUM automatic context currently requires daily K-line bars");
    const auto adjustment = lower_ascii(
        aggregate_text_or(target, "adjustment_mode", "none"));
    if (adjustment != "none" && adjustment != "raw" && adjustment != "0")
        throw Error("INSORT/INSUM automatic context currently requires unadjusted daily K-line bars");
    const auto* target_rows = aggregate_optional(target, "bars");
    if (!target_rows || !target_rows->is_array() || target_rows->as_array().empty())
        throw Error("INSORT/INSUM requires dated daily K-line bars");
    const auto* formulas = aggregate_optional(formula_library, "formulas");
    if (!formulas || !formulas->is_array())
        throw Error("INSORT/INSUM requires a recovered TCalc formula library");

    struct TargetBar {
        int date{};
        std::string key;
        DailyBar bar;
    };
    const auto target_number = [&](const Json& row, std::string_view name) {
        const auto* value = aggregate_optional(row, name);
        return value && value->is_number() ? value->as_number() : 0.0;
    };
    std::vector<TargetBar> target_bars;
    target_bars.reserve(target_rows->as_array().size());
    for (const auto& row : target_rows->as_array()) {
        const auto compact = aggregate_compact_date(
            aggregate_text_or(row, "date"));
        if (compact.empty())
            throw Error("INSORT/INSUM requires valid bar dates");
        const int date = std::stoi(compact);
        const auto display_date = aggregate_text_or(row, "date");
        const auto time = aggregate_text_or(row, "time");
        target_bars.push_back(TargetBar{
            date, display_date + "|" + time,
            DailyBar{date, target_number(row, "open"),
                     target_number(row, "high"), target_number(row, "low"),
                     target_number(row, "close"), target_number(row, "amount"),
                     static_cast<std::uint32_t>(std::max(
                         0.0, target_number(row, "volume")))},
        });
    }

    using formula_context_detail::FormulaCatalogSelection;
    using formula_context_detail::FormulaSelectionPolicy;
    std::map<std::pair<std::string, int>, FormulaCatalogSelection> selections;
    const auto select_formula = [&](const std::string& requested, int output)
        -> const FormulaCatalogSelection& {
        const auto key = std::make_pair(lower_ascii(requested), output);
        const auto cached = selections.find(key);
        if (cached != selections.end()) return cached->second;
        auto selection = formula_context_detail::select_technical_formula(
            formula_library, requested, output,
            FormulaSelectionPolicy::context_free_ohlcv, "INSORT/INSUM");
        return selections.emplace(key, std::move(selection)).first->second;
    };

    const auto catalog = load_aggregate_universe_catalog(root);
    std::vector<AggregateUniverseResolution> resolutions;
    std::vector<const FormulaCatalogSelection*> selected_formulas;
    resolutions.reserve(bindings.size());
    selected_formulas.reserve(bindings.size());
    for (const auto& binding : bindings) {
        const auto special = lower_ascii(binding.block_name);
        if (special == "cloudgps" ||
            (binding.function == "INSORT" &&
             (special == "hyinsort" ||
              aggregate_starts_with(special, "gninsort.") ||
              aggregate_starts_with(special, "fginsort."))) ||
            (binding.function == "INSUM" &&
             (special == "mygnbk" || special == "myfgbk"))) {
            throw Error(binding.function +
                        " Level2/cloud special universe is intentionally unavailable: " +
                        binding.block_name);
        }
        selected_formulas.push_back(
            &select_formula(binding.formula_name, binding.output));
        resolutions.push_back(resolve_aggregate_universe(
            root, data, catalog, binding.block_name,
            AggregateMemberPolicy::indicator));
    }
    std::uint64_t member_file_count = 0;
    auto daily = load_aggregate_daily(root, resolutions, member_file_count);

    const auto format_date = [](int date) {
        auto raw = std::to_string(date);
        if (raw.size() != 8) return raw;
        return raw.substr(0, 4) + "-" + raw.substr(4, 2) + "-" +
            raw.substr(6, 2);
    };
    const int last_target_date = target_bars.back().date;
    const std::size_t window_size = target_bars.size() + 100;
    const auto make_kline = [&](const AggregateSecurityKey& security,
                                std::vector<DailyBar> bars) {
        bars.erase(std::remove_if(bars.begin(), bars.end(), [&](const auto& bar) {
            return bar.date > last_target_date;
        }), bars.end());
        if (bars.size() > window_size) {
            bars.erase(bars.begin(), bars.end() -
                       static_cast<std::ptrdiff_t>(window_size));
        }
        Json document = Json::object();
        document["market"] = aggregate_market_name(security.first);
        document["market_id"] = security.first;
        document["code"] = security.second;
        document["period"] = "day";
        document["adjustment_mode"] = "none";
        document["bars"] = Json::array();
        for (const auto& bar : bars) {
            Json row = Json::object();
            row["date"] = format_date(bar.date);
            row["time"] = "";
            row["open"] = bar.open;
            row["high"] = bar.high;
            row["low"] = bar.low;
            row["close"] = bar.close;
            row["amount"] = bar.amount;
            row["volume"] = static_cast<std::uint64_t>(bar.volume);
            document["bars"].push_back(std::move(row));
        }
        return document;
    };

    const AggregateSecurityKey target_security{target_market_id, target_code};
    std::vector<DailyBar> expanded_target;
    if (const auto found = daily.find(target_security); found != daily.end())
        expanded_target = found->second;
    else
        expanded_target = load_daily_bars(root, target_market_id, target_code);
    std::map<int, DailyBar> target_by_date;
    for (const auto& bar : expanded_target)
        if (bar.date <= last_target_date) target_by_date[bar.date] = bar;
    for (const auto& bar : target_bars) target_by_date[bar.date] = bar.bar;
    expanded_target.clear();
    for (const auto& [date, bar] : target_by_date) {
        (void)date;
        expanded_target.push_back(bar);
    }

    using DateSeries = std::map<int, float>;
    std::map<std::string, DateSeries> evaluated_cache;
    std::uint64_t evaluation_count = 0;
    const auto evaluate_selected = [&](const AggregateSecurityKey& security,
                                       const FormulaCatalogSelection& formula,
                                       int output, bool target_series)
        -> const DateSeries& {
        const auto cache_key = formula.code + "#" + std::to_string(output) + "#" +
            std::to_string(security.first) + "#" + security.second +
            (target_series ? "#TARGET" : "#MEMBER");
        const auto cached = evaluated_cache.find(cache_key);
        if (cached != evaluated_cache.end()) return cached->second;
        std::vector<DailyBar> bars;
        if (target_series) {
            bars = expanded_target;
        } else if (const auto found = daily.find(security); found != daily.end()) {
            bars = found->second;
        }
        DateSeries values;
        if (!bars.empty()) {
            const auto evaluated = evaluate_formula_document(
                make_kline(security, std::move(bars)), *formula.definition);
            if (const auto* points = aggregate_optional(evaluated, "points");
                points && points->is_array()) {
                for (const auto& point : points->as_array()) {
                    const auto date = aggregate_compact_date(
                        aggregate_text_or(point, "date"));
                    const auto* point_values = aggregate_optional(point, "values");
                    const auto* value = point_values
                        ? aggregate_optional(*point_values, formula.output_name)
                        : nullptr;
                    if (!date.empty() && value && value->is_number() &&
                        std::isfinite(value->as_number())) {
                        values[std::stoi(date)] =
                            static_cast<float>(value->as_number());
                    }
                }
            }
            ++evaluation_count;
        }
        return evaluated_cache.emplace(cache_key, std::move(values)).first->second;
    };

    if (!context.as_object().count("series")) context["series"] = Json::object();
    Json documents = Json::array();
    std::uint64_t insort_count = 0;
    std::uint64_t insum_count = 0;
    constexpr float epsilon = 0.00001f;
    for (std::size_t resolution_index = 0;
         resolution_index < resolutions.size(); ++resolution_index) {
        const auto& binding = bindings[resolution_index];
        const auto& resolution = resolutions[resolution_index];
        const auto& formula = *selected_formulas[resolution_index];
        Json points = Json::object();
        std::uint64_t used_members = 0;
        if (binding.function == "INSORT") {
            ++insort_count;
            const auto& selected = evaluate_selected(
                target_security, formula, binding.output, true);
            std::vector<float> ranks(target_bars.size(), 1.0f);
            std::vector<bool> target_valid(target_bars.size(), false);
            for (std::size_t index = 0; index < target_bars.size(); ++index)
                target_valid[index] = selected.count(target_bars[index].date) != 0;
            for (const auto& member : resolution.members) {
                if (member == target_security) continue;
                const auto& values = evaluate_selected(
                    member, formula, binding.output, false);
                if (values.empty()) continue;
                ++used_members;
                std::optional<float> previous;
                for (std::size_t index = 0; index < target_bars.size(); ++index) {
                    const auto found = values.find(target_bars[index].date);
                    if (found != values.end()) previous = found->second;
                    if (!target_valid[index] || !previous) continue;
                    const float target_value = selected.at(target_bars[index].date);
                    if ((binding.mode == 0 &&
                         target_value + epsilon < *previous) ||
                        (binding.mode != 0 &&
                         target_value > *previous + epsilon)) {
                        ranks[index] = static_cast<float>(ranks[index] + 1.0f);
                    }
                }
            }
            for (std::size_t index = 0; index < target_bars.size(); ++index)
                if (target_valid[index])
                    points[target_bars[index].key] =
                        static_cast<double>(ranks[index]);
        } else {
            ++insum_count;
            std::vector<float> sum(target_bars.size(), 0.0f);
            std::vector<float> maximum(target_bars.size(), 0.0f);
            std::vector<float> minimum(target_bars.size(), 0.0f);
            std::vector<int> maximum_index(target_bars.size(), 0);
            std::vector<int> minimum_index(target_bars.size(), 0);
            std::vector<bool> valid(target_bars.size(), false);
            for (std::size_t member_index = 0;
                 member_index < resolution.members.size(); ++member_index) {
                const auto& member = resolution.members[member_index];
                const auto& values = evaluate_selected(
                    member, formula, binding.output,
                    member == target_security);
                if (values.empty()) continue;
                ++used_members;
                for (std::size_t index = 0; index < target_bars.size(); ++index) {
                    const auto found = values.find(target_bars[index].date);
                    if (found == values.end()) continue;
                    const float value = found->second;
                    if (!valid[index]) {
                        valid[index] = true;
                        sum[index] = value;
                        maximum[index] = value;
                        minimum[index] = value;
                        maximum_index[index] = static_cast<int>(member_index + 1);
                        minimum_index[index] = static_cast<int>(member_index + 1);
                        continue;
                    }
                    sum[index] = static_cast<float>(sum[index] + value);
                    if (value > maximum[index] + epsilon)
                        maximum_index[index] = static_cast<int>(member_index + 1);
                    if (value + epsilon < minimum[index])
                        minimum_index[index] = static_cast<int>(member_index + 1);
                    if (value > maximum[index]) maximum[index] = value;
                    if (value < minimum[index]) minimum[index] = value;
                }
            }
            const float member_count =
                static_cast<float>(resolution.members.size());
            for (std::size_t index = 0; index < target_bars.size(); ++index) {
                if (!valid[index]) continue;
                float value = 0.0f;
                if (binding.mode == 0) value = sum[index];
                else if (binding.mode == 1 && member_count > 0.0f)
                    value = static_cast<float>(sum[index] / member_count);
                else if (binding.mode == 2) value = maximum[index];
                else if (binding.mode == 3) value = minimum[index];
                else if (binding.mode == 4)
                    value = static_cast<float>(maximum_index[index]);
                else if (binding.mode == 5)
                    value = static_cast<float>(minimum_index[index]);
                points[target_bars[index].key] = static_cast<double>(value);
            }
        }
        context["series"][binding.name] = std::move(points);

        Json item = Json::object();
        item["binding"] = binding.name;
        item["function"] = binding.function;
        item["requested_name"] = binding.block_name;
        item["lookup_name"] = resolution.lookup_name;
        item["found"] = resolution.found;
        item["family"] = resolution.found ? Json(resolution.family) : Json(nullptr);
        item["block_code"] = resolution.found
            ? Json(resolution.block_code) : Json(nullptr);
        item["source"] = resolution.found ? Json(resolution.source) : Json(nullptr);
        item["formula_code"] = formula.code;
        item["formula_name"] = formula.name;
        item["formula_output"] = binding.output;
        item["formula_output_name"] = formula.output_name;
        item["mode"] = binding.mode;
        item["member_count"] =
            static_cast<std::uint64_t>(resolution.members.size());
        item["member_series_count"] = used_members;
        documents.push_back(std::move(item));
    }
    context["indicator_aggregate_resolutions"] = std::move(documents);
    context["insort_binding_count"] = insort_count;
    context["insum_binding_count"] = insum_count;
    context["indicator_aggregate_member_file_count"] = member_file_count;
    context["indicator_aggregate_member_series_count"] =
        static_cast<std::uint64_t>(daily.size());
    context["indicator_aggregate_formula_evaluation_count"] = evaluation_count;
    context["indicator_aggregate_warmup_bars"] = 100;
    context["indicator_aggregate_industry_mode"] = catalog.industry_mode;
    context["indicator_aggregate_custom_catalog_source"] =
        catalog.custom.catalog_source;
    context["indicator_aggregate_mode"] =
        "tcalc-opcodes1246-1247-active-technical-indicator-local-day-horizontal-evaluation";
}

}  // namespace tdx::formula_context_detail
