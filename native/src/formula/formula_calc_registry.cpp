#include "formula_calc_internal.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace tdx::formula_calc_detail {

const FormulaDefinition* find_formula(std::string_view name) {
    for (const auto& definition : formula_catalog)
        if (definition.name == name) return &definition;
    return nullptr;
}

std::vector<std::string> ordered_outputs(
    const FormulaDefinition& definition, const Values& values) {
    std::vector<std::string> result;
    for (std::size_t index = 0; index < definition.output_count; ++index) {
        const auto name = std::string(definition.outputs[index]);
        if (values.count(name)) result.push_back(name);
    }
    for (const auto& [name, series] : values) {
        (void)series;
        if (std::find(result.begin(), result.end(), name) == result.end())
            result.push_back(name);
    }
    return result;
}

}  // namespace tdx::formula_calc_detail

namespace tdx {

using namespace formula_calc_detail;

Json formula_calculation_catalog_document() {
    Json result = Json::object();
    result["schema_version"] = 1;
    result["engine"] = "tdx-native-compatible-v4";
    result["execution_mode"] = "native-cpp";
    result["dll_loaded"] = false;
    result["scope"] = "high-value standard indicators; not arbitrary TCalc bytecode";
    Json formulas = Json::array();
    for (const auto& definition : formula_catalog) {
        Json item = Json::object();
        item["code"] = std::string(definition.code);
        std::string outputs;
        for (std::size_t index = 0; index < definition.output_count; ++index) {
            if (!outputs.empty()) outputs += ", ";
            outputs += definition.outputs[index];
        }
        item["outputs"] = std::move(outputs);
        formulas.push_back(std::move(item));
    }
    result["count"] = static_cast<std::uint64_t>(formulas.size());
    result["formulas"] = std::move(formulas);
    return result;
}

Json calculate_formula_document(Json kline_document, const std::string& selected_formula,
                                const std::map<std::string, int>& parameters) {
    auto bars = read_bars(kline_document);
    if (bars.empty()) throw Error("formula calculation requires at least one K-line bar");
    const auto formula = lower_ascii(trim(selected_formula));
    Values values;
    std::map<std::string, int> actual;
    const auto* definition = find_formula(formula);
    if (!definition)
        throw Error("unsupported native formula: " + selected_formula);
    definition->handler(bars, parameters, values, actual);

    Json outputs = Json::array();
    for (const auto& name : ordered_outputs(*definition, values)) outputs.push_back(name);
    Json points = Json::array();
    for (std::size_t index = 0; index < bars.size(); ++index) {
        Json point = Json::object();
        point["date"] = bars[index].date;
        point["time"] = bars[index].time;
        point["open"] = bars[index].open;
        point["high"] = bars[index].high;
        point["low"] = bars[index].low;
        point["close"] = bars[index].close;
        point["amount"] = bars[index].amount;
        point["volume"] = bars[index].volume;
        Json row_values = Json::object();
        for (const auto& [name, series] : values)
            row_values[name] = std::isfinite(series[index]) ? Json(series[index]) : Json(nullptr);
        point["values"] = std::move(row_values);
        points.push_back(std::move(point));
    }

    Json result = Json::object();
    result["schema_version"] = 1;
    result["engine"] = "tdx-native-compatible-v4";
    result["execution_mode"] = "native-cpp";
    result["dll_loaded"] = false;
    result["formula"] = std::string(definition->code);
    result["parameters"] = parameter_json(actual);
    result["outputs"] = std::move(outputs);
    result["count"] = static_cast<std::uint64_t>(bars.size());
    result["points"] = std::move(points);
    for (const auto key : {"market", "code", "name", "source", "period", "period_id",
                           "endpoint", "server_name", "index_mode", "start", "next_start",
                           "has_more", "downloaded", "page_size"})
        if (const auto* value = optional(kline_document, key)) result[key] = *value;
    result["input_count"] = static_cast<std::uint64_t>(bars.size());
    result["chronological"] = true;
    const auto* index_value = optional(kline_document, "index_mode");
    const bool index_mode = index_value && index_value->is_bool() && index_value->as_bool();
    result["formula_volume_unit"] = index_mode ? "index-native" : "hand";
    result["formula_volume_divisor"] = index_mode ? 1 : 100;
    return result;
}

}  // namespace tdx

