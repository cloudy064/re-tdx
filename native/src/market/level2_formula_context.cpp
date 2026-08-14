#include "tdx/level2.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace tdx {
namespace {

struct TcalcContextRecord {
    std::string date;
    const Json* bindings{};
    const Json* document{};
};

struct TcalcKlineBar {
    std::string date;
    std::string stamp;
    std::optional<float> low;
    std::optional<float> high;
};

using TcalcRawFields = std::array<std::optional<float>, 45>;

constexpr std::size_t field_index(std::size_t byte_offset) {
    return (byte_offset - 4) / 4;
}

const Json* member(const Json& value, const std::string& name) {
    if (!value.is_object()) return nullptr;
    const auto found = value.as_object().find(name);
    return found == value.as_object().end() ? nullptr : &found->second;
}

bool is_tcalc_order_flow_context(const Json& context) {
    const auto* metadata = member(context, "_formula_context");
    const auto* schema = metadata ? member(*metadata, "schema") : nullptr;
    return schema && schema->is_string() &&
           schema->as_string() == "tdx-formula-explicit-context-from-tcalc-l2-v1";
}

std::vector<TcalcContextRecord> read_context_records(const Json& context) {
    const auto* records = member(context, "records");
    if (!records || !records->is_array())
        throw Error("TCalc Level2 formula context requires decoded records");
    std::vector<TcalcContextRecord> result;
    result.reserve(records->size());
    for (const auto& record : records->as_array()) {
        const auto* stamp = member(record, "formula_context_stamp");
        const auto* bindings = member(record, "formula_bindings");
        if (!stamp || !stamp->is_string() || stamp->as_string().size() < 10 ||
            !bindings || !bindings->is_object())
            throw Error("TCalc Level2 formula context contains an invalid date record");
        result.push_back({stamp->as_string().substr(0, 10), bindings, &record});
    }
    std::sort(result.begin(), result.end(), [](const auto& left, const auto& right) {
        return left.date < right.date;
    });
    for (std::size_t index = 1; index < result.size(); ++index)
        if (result[index - 1].date == result[index].date)
            throw Error("TCalc Level2 formula context contains duplicate dates");
    return result;
}

std::optional<float> optional_float(const Json* value,
                                    const std::string& label) {
    if (!value || value->is_null()) return std::nullopt;
    if (!value->is_number() || !std::isfinite(value->as_number()) ||
        value->as_number() < -std::numeric_limits<float>::max() ||
        value->as_number() > std::numeric_limits<float>::max())
        throw Error("TCalc Level2 context requires a finite number for " + label);
    return static_cast<float>(value->as_number());
}

std::vector<TcalcKlineBar> read_bars(const Json& kline_document,
                                     bool require_prices) {
    const auto* bars = member(kline_document, "bars");
    if (!bars || !bars->is_array() || bars->size() == 0)
        throw Error("TCalc Level2 context materialization requires K-line bars");
    std::vector<TcalcKlineBar> result;
    result.reserve(bars->size());
    for (const auto& bar : bars->as_array()) {
        const auto* date = member(bar, "date");
        const auto* time = member(bar, "time");
        if (!date || !date->is_string() || date->as_string().size() != 10 ||
            !time || !time->is_string() || time->as_string().empty())
            throw Error("TCalc Level2 context requires date/time on every K-line bar");
        const auto low = optional_float(member(bar, "low"), "K-line low");
        const auto high = optional_float(member(bar, "high"), "K-line high");
        if (require_prices && (!low || !high))
            throw Error("TCalc Level2 week/month context requires low/high on every K-line bar");
        result.push_back({date->as_string(),
                          date->as_string() + "|" + time->as_string(),
                          low, high});
    }
    for (std::size_t index = 1; index < result.size(); ++index)
        if (result[index - 1].date >= result[index].date)
            throw Error("TCalc Level2 context requires chronological unique K-line dates");
    return result;
}

std::optional<float> matrix_value(const Json& record, const std::string& name,
                                  std::size_t row, std::size_t column,
                                  std::size_t rows, std::size_t columns) {
    const auto* matrix = member(record, name);
    if (!matrix || !matrix->is_array() || matrix->size() != rows)
        throw Error("TCalc Level2 context contains an invalid " + name + " matrix");
    const auto& row_value = matrix->as_array()[row];
    if (!row_value.is_array() || row_value.size() != columns)
        throw Error("TCalc Level2 context contains an invalid " + name + " matrix row");
    return optional_float(&row_value.as_array()[column], name);
}

TcalcRawFields read_raw_fields(const TcalcContextRecord& source) {
    const auto& record = *source.document;
    TcalcRawFields result;
    result[field_index(4)] = optional_float(
        member(record, "host_auxiliary_f32"), "host_auxiliary_f32");
    for (std::size_t row = 0; row < 4; ++row)
        for (std::size_t column = 0; column < 4; ++column) {
            const auto element = row * 4 + column;
            result[field_index(8) + element] = matrix_value(
                record, "l2_vol", row, column, 4, 4);
            result[field_index(72) + element] = matrix_value(
                record, "l2_amo", row, column, 4, 4);
        }
    for (std::size_t row = 0; row < 2; ++row)
        for (std::size_t column = 0; column < 2; ++column)
            result[field_index(136) + row * 2 + column] = matrix_value(
                record, "l2_volnum_raw", row, column, 2, 2);
    for (const auto& [offset, name] :
         std::map<std::size_t, std::string>{
             {152, "BIDORDERVOL"}, {156, "BIDCANCELVOL"},
             {160, "OFFERORDERVOL"}, {164, "OFFERCANCELVOL"},
             {168, "AVGBIDPX"}, {172, "AVGOFFERPX"},
             {176, "CUR_BUYORDER"}, {180, "CUR_SELLORDER"}})
        result[field_index(offset)] = optional_float(
            member(*source.bindings, name), name);
    return result;
}

std::optional<float> add_values(std::optional<float> left,
                                std::optional<float> right) {
    if (!left || !right) return std::nullopt;
    const auto value = static_cast<float>(*left + *right);
    return std::isfinite(value) ? std::optional<float>(value) : std::nullopt;
}

std::optional<float> add_values(std::optional<float> first,
                                std::optional<float> second,
                                std::optional<float> third) {
    return add_values(add_values(first, second), third);
}

TcalcRawFields zero_raw_fields() {
    TcalcRawFields result;
    for (auto& value : result) value = 0.0F;
    return result;
}

void aggregate_raw_fields(TcalcRawFields& target,
                          const TcalcRawFields& source,
                          const TcalcKlineBar& bar) {
    const auto previous_buy = target[field_index(176)];
    const auto previous_sell = target[field_index(180)];
    for (std::size_t index = field_index(4);
         index <= field_index(164); ++index) {
        if (index == field_index(156) || index == field_index(164)) continue;
        target[index] = add_values(target[index], source[index]);
    }
    target[field_index(156)] = add_values(
        source[field_index(156)], previous_buy, target[field_index(156)]);
    target[field_index(164)] = add_values(
        source[field_index(164)], previous_sell, target[field_index(164)]);
    for (const auto offset : {168U, 172U, 176U, 180U})
        target[field_index(offset)] = source[field_index(offset)];
    auto& average_bid = target[field_index(168)];
    if (!average_bid || *average_bid <= 1.0e-5F) average_bid = bar.low;
    auto& average_offer = target[field_index(172)];
    if (!average_offer || *average_offer <= 1.0e-5F) average_offer = bar.high;
}

Json numeric(std::optional<float> value) {
    return value ? Json(static_cast<double>(*value)) : Json(nullptr);
}

std::optional<float> sum_fields(const TcalcRawFields& fields,
                                std::initializer_list<std::size_t> offsets) {
    std::optional<float> result = 0.0F;
    for (const auto offset : offsets)
        result = add_values(result, fields[field_index(offset)]);
    return result;
}

Json count_value(std::optional<float> value) {
    if (!value) return nullptr;
    const auto adjusted = static_cast<double>(*value) + 0.503000020980835;
    if (adjusted < static_cast<double>(std::numeric_limits<int>::min()) ||
        adjusted > static_cast<double>(std::numeric_limits<int>::max()))
        return nullptr;
    return static_cast<double>(static_cast<int>(adjusted));
}

Json raw_bindings(const TcalcRawFields& fields) {
    Json bindings = Json::object();
    for (std::size_t size_class = 0; size_class < 4; ++size_class)
        for (std::size_t direction = 0; direction < 4; ++direction) {
            const auto suffix = "#" + std::to_string(size_class) + "#" +
                                std::to_string(direction);
            bindings["L2_VOL" + suffix] = numeric(fields[
                field_index(8) + size_class * 4 + direction]);
            bindings["L2_AMO" + suffix] = numeric(fields[
                field_index(72) + size_class * 4 + direction]);
        }
    for (std::size_t first = 0; first < 2; ++first)
        for (std::size_t second = 0; second < 2; ++second) {
            const auto suffix = "#" + std::to_string(first) + "#" +
                                std::to_string(second);
            bindings["L2_VOLNUM" + suffix] = count_value(fields[
                field_index(136) + first * 2 + second]);
        }
    bindings["ACTINVOL"] = numeric(sum_fields(fields, {16, 32, 48, 64}));
    bindings["ACTOUTVOL"] = numeric(sum_fields(fields, {20, 36, 52, 68}));
    bindings["LARGEINTRDVOL"] = numeric(sum_fields(fields, {8, 24}));
    bindings["LARGEOUTTRDVOL"] = numeric(sum_fields(fields, {12, 28}));
    for (const auto& [offset, name] :
         std::map<std::size_t, std::string>{
             {152, "BIDORDERVOL"}, {156, "BIDCANCELVOL"},
             {160, "OFFERORDERVOL"}, {164, "OFFERCANCELVOL"},
             {168, "AVGBIDPX"}, {172, "AVGOFFERPX"},
             {176, "CUR_BUYORDER"}, {180, "CUR_SELLORDER"}})
        bindings[name] = numeric(fields[field_index(offset)]);
    bindings["TRADENUM"] = count_value(fields[field_index(4)]);
    bindings["TRADEINNUM"] = count_value(sum_fields(fields, {136, 144}));
    bindings["TRADEOUTNUM"] = count_value(sum_fields(fields, {140, 148}));
    bindings["LARGETRDINNUM"] = count_value(fields[field_index(136)]);
    bindings["LARGETRDOUTNUM"] = count_value(fields[field_index(140)]);
    return bindings;
}

Json null_bindings(const std::set<std::string>& names) {
    Json result = Json::object();
    for (const auto& name : names) result[name] = nullptr;
    return result;
}

void append_series(Json& series, const std::set<std::string>& names,
                   const Json& bindings, const std::string& stamp) {
    for (const auto& name : names) {
        const auto* value = member(bindings, name);
        series[name][stamp] = value ? *value : Json(nullptr);
    }
}

}  // namespace

Json materialize_tcalc_level2_formula_context(Json context,
                                              const Json& kline_document) {
    if (!is_tcalc_order_flow_context(context)) return context;
    const auto& metadata = context.at("_formula_context");
    const auto* complete = member(metadata, "context_complete");
    if (!complete || !complete->is_bool() || !complete->as_bool())
        throw Error("TCalc Level2 formula context is truncated; decode every type-31 record before evaluation");
    const auto* invalid = member(metadata, "invalid_date_record_count");
    if (invalid && invalid->is_number() && invalid->as_number() != 0.0)
        throw Error("TCalc Level2 formula context contains invalid record dates");
    const auto* period = member(kline_document, "period");
    if (!period || !period->is_string())
        throw Error("TCalc Level2 context requires a K-line period");
    const auto period_name = lower_ascii(period->as_string());
    if (period_name != "day" && period_name != "week" &&
        period_name != "month")
        throw Error("TCalc Level2 type-31 context supports day, week, or month K-lines only");

    const auto records = read_context_records(context);
    const auto bars = read_bars(kline_document, period_name != "day");
    std::set<std::string> binding_names;
    for (const auto& record : records)
        for (const auto& [name, value] : record.bindings->as_object()) {
            (void)value;
            binding_names.insert(name);
        }

    Json series = Json::object();
    std::size_t record_index = 0;
    std::size_t exact_matches = 0;
    std::size_t trailing_inherited = 0;
    std::size_t leading_cropped = 0;
    std::size_t aggregate_inputs = 0;
    std::size_t discarded_partial_inputs = 0;
    std::size_t missing_exact = 0;
    if (period_name == "day") {
        const Json* previous_output = nullptr;
        for (const auto& bar : bars) {
            while (record_index < records.size() &&
                   records[record_index].date < bar.date) {
                ++record_index;
                ++leading_cropped;
            }
            const Json* selected = nullptr;
            if (record_index < records.size() &&
                records[record_index].date == bar.date) {
                selected = records[record_index].bindings;
                previous_output = selected;
                ++record_index;
                ++exact_matches;
            } else if (record_index == records.size() && previous_output) {
                selected = previous_output;
                ++trailing_inherited;
            }
            if (selected) append_series(series, binding_names, *selected, bar.stamp);
            else append_series(series, binding_names,
                               null_bindings(binding_names), bar.stamp);
        }
    } else {
        Json previous_output = null_bindings(binding_names);
        bool have_previous_output = false;
        for (std::size_t bar_index = 0; bar_index < bars.size(); ++bar_index) {
            const auto& bar = bars[bar_index];
            auto aggregate = zero_raw_fields();
            std::size_t partial_inputs = 0;
            while (record_index < records.size() &&
                   records[record_index].date < bar.date) {
                if (bar_index == 0) {
                    ++leading_cropped;
                } else {
                    aggregate_raw_fields(
                        aggregate, read_raw_fields(records[record_index]), bar);
                    ++aggregate_inputs;
                    ++partial_inputs;
                }
                ++record_index;
            }

            Json selected;
            if (record_index < records.size() &&
                records[record_index].date == bar.date) {
                aggregate_raw_fields(
                    aggregate, read_raw_fields(records[record_index]), bar);
                ++record_index;
                ++aggregate_inputs;
                ++exact_matches;
                selected = raw_bindings(aggregate);
            } else if (record_index == records.size()) {
                discarded_partial_inputs += partial_inputs;
                selected = have_previous_output
                    ? previous_output : null_bindings(binding_names);
                if (have_previous_output) ++trailing_inherited;
            } else {
                discarded_partial_inputs += partial_inputs;
                selected = null_bindings(binding_names);
                ++missing_exact;
            }
            append_series(series, binding_names, selected, bar.stamp);
            previous_output = std::move(selected);
            have_previous_output = true;
        }
    }
    context["series"] = std::move(series);
    auto& output_metadata = context["_formula_context"];
    output_metadata["materialized"] = true;
    output_metadata["materialization_schema"] =
        "tdx-formula-tcalc-l2-kline-context-v1";
    output_metadata["materialized_period"] = period_name;
    output_metadata["materialized_bar_count"] =
        static_cast<std::uint64_t>(bars.size());
    output_metadata["exact_match_count"] =
        static_cast<std::uint64_t>(exact_matches);
    output_metadata["trailing_inherited_count"] =
        static_cast<std::uint64_t>(trailing_inherited);
    output_metadata["leading_cropped_record_count"] =
        static_cast<std::uint64_t>(leading_cropped);
    if (period_name == "day") {
        output_metadata["materialization_rule"] =
            "TCalc daily exact-date merge, crop, then trailing inheritance";
    } else {
        output_metadata["aggregate_input_record_count"] =
            static_cast<std::uint64_t>(aggregate_inputs);
        output_metadata["discarded_partial_record_count"] =
            static_cast<std::uint64_t>(discarded_partial_inputs);
        output_metadata["missing_exact_end_date_count"] =
            static_cast<std::uint64_t>(missing_exact);
        output_metadata["materialization_rule"] =
            "TCalc native week/month daily type-31 aggregation: leading crop, exact end-date close, then trailing inheritance";
    }
    output_metadata["stamp_source"] = "target K-line date and time";
    return context;
}

}  // namespace tdx
