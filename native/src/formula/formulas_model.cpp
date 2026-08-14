#include "formulas_internal.hpp"

#include <cmath>

namespace tdx::formulas_detail {

Json category_json(const Category& item) {
    Json value = Json::object();
    value["id"] = static_cast<std::uint64_t>(item.id);
    value["parent"] = static_cast<std::uint64_t>(item.parent);
    value["name"] = item.name;
    value["flags"] = static_cast<std::uint64_t>(item.flags);
    return value;
}

Json formula_json(const Formula& item) {
    Json value = Json::object();
    value["kind"] = item.kind;
    value["kind_key"] = item.kind_key;
    value["kind_name"] = item.kind_name;
    value["index"] = static_cast<std::uint64_t>(item.index);
    value["code"] = item.code;
    value["name"] = item.name;
    value["category_id"] = static_cast<int>(item.category_id);
    value["category_name"] = item.category_name;
    value["display_flags"] = static_cast<std::uint64_t>(item.display_flags);
    value["attribute_flags"] = static_cast<std::uint64_t>(item.attribute_flags);
    value["source"] = item.source;
    value["is_custom"] = item.is_custom;
    value["source_text_available"] = !item.source_text.empty();
    value["source_text_origin"] = item.source_text.empty() ? "unavailable"
        : !item.source_text_origin.empty() ? item.source_text_origin
        : item.source_rva ? "embedded"
        : item.source_text_recovered ? "native-recovered" : "unavailable";
    value["source_rva"] = item.source_rva
        ? Json(static_cast<std::uint64_t>(item.source_rva)) : Json(nullptr);
    value["source_text"] = !item.source_text.empty()
        ? Json(item.source_text) : Json(nullptr);
    Json parameters = Json::array();
    for (const auto& parameter : item.parameters) {
        Json row = Json::object();
        row["name"] = parameter.name;
        row["minimum"] = std::isfinite(parameter.minimum)
            ? Json(parameter.minimum) : Json(nullptr);
        row["maximum"] = std::isfinite(parameter.maximum)
            ? Json(parameter.maximum) : Json(nullptr);
        row["step"] = std::isfinite(parameter.step)
            ? Json(parameter.step) : Json(nullptr);
        row["default"] = std::isfinite(parameter.default_value)
            ? Json(parameter.default_value) : Json(nullptr);
        row["current"] = std::isfinite(parameter.current_value)
            ? Json(parameter.current_value) : Json(nullptr);
        parameters.push_back(std::move(row));
    }
    value["parameter_count"] =
        static_cast<std::uint64_t>(item.parameters.size());
    value["parameters"] = std::move(parameters);
    Json outputs = Json::array();
    for (const auto& output : item.outputs) outputs.push_back(output);
    value["output_count"] = static_cast<std::uint64_t>(item.outputs.size());
    value["outputs"] = std::move(outputs);
    return value;
}

}  // namespace tdx::formulas_detail
