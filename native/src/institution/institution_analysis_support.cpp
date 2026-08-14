#include "institution_analysis_internal.hpp"

#include "tdx/common.hpp"
#include "tdx/jsn.hpp"
#include "tdx/jsn_data.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace tdx::institution_analysis_detail {

const Json* value_ptr(const Json& object, std::string_view name) {
    if (!object.is_object()) return nullptr;
    const auto found = object.as_object().find(name);
    return found == object.as_object().end() ? nullptr : &found->second;
}

Json copy_value(const Json& object, std::string_view name) {
    const auto* value = value_ptr(object, name);
    return value ? *value : Json(nullptr);
}

std::string text_value(const Json& object, std::string_view name) {
    const auto* value = value_ptr(object, name);
    return value ? trim(jsn_scalar_text(*value)) : std::string{};
}

bool parse_number(const Json& object, std::string_view name, double& result) {
    const auto text = text_value(object, name);
    if (text.empty()) return false;
    try {
        std::size_t used = 0;
        result = std::stod(text, &used);
        return used == text.size() && std::isfinite(result);
    } catch (...) { return false; }
}

Json number_or_null(const Json& object, std::string_view name, double scale) {
    double value = 0;
    return parse_number(object, name, value) ? Json(value * scale) : Json(nullptr);
}

Json quotient_or_null(double numerator, double denominator) {
    return std::isfinite(numerator) && std::isfinite(denominator) && denominator != 0.0
        ? Json(numerator / denominator) : Json(nullptr);
}

int market_id(const std::string& value) {
    const auto normalized = lower_ascii(trim(value));
    if (normalized == "0" || normalized == "sz") return 0;
    if (normalized == "1" || normalized == "sh") return 1;
    if (normalized == "2" || normalized == "44" || normalized == "bj") return 2;
    throw Error("market must be sz/sh/bj or 0/1/2");
}

std::string market_name(int value) {
    return value == 0 ? "sz" : value == 1 ? "sh" : value == 2 ? "bj"
                                                               : "m" + std::to_string(value);
}

std::string market_prefix(int value) {
    return value == 0 ? "SZ" : value == 1 ? "SH" : value == 2 ? "BJ"
                                                               : "M" + std::to_string(value);
}

bool six_digits(const std::string& value) {
    return value.size() == 6 && std::all_of(value.begin(), value.end(), [](char ch) {
        return ch >= '0' && ch <= '9';
    });
}

const Json& document_for(const Json& documents, std::string_view resource) {
    if (!documents.is_array()) throw Error("institution-analysis sources must be an array");
    for (const auto& document : documents.as_array())
        if (text_value(document, "resource") == resource) return document;
    throw Error("institution-analysis source is missing: " + std::string(resource));
}

}  // namespace tdx::institution_analysis_detail
