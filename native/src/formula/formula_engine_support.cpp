#include "formula_engine_support_internal.hpp"
#include "formula_engine_internal.hpp"
#include "tdx/common.hpp"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <set>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace fs = std::filesystem;

namespace tdx::formula_engine_support {
using namespace formula_engine_detail;

const Json* optional(const Json& object, std::string_view key) {
    if (!object.is_object()) return nullptr;
    const auto found = object.as_object().find(key);
    return found == object.as_object().end() ? nullptr : &found->second;
}

std::string upper_ascii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return ch < 0x80 ? static_cast<char>(std::toupper(ch)) : static_cast<char>(ch);
    });
    return value;
}

fs::path formula_path_from_utf8(const std::string& value) {
#ifdef _WIN32
    return fs::path(utf8_to_wide(value));
#else
    return fs::path(value);
#endif
}

bool expansion_context_dependency(std::string_view dependency) {
    return dependency == "IVOLAT" || dependency == "IST0CODE" ||
           dependency == "ISSTCODE" || dependency == "ISQUITCODE" ||
           dependency == "ISQHQQCODE" || dependency == "ISJYDATE" ||
           dependency == "MULTIPLIER" ||
           dependency == "LOCALDAYNUM";
}


bool supported_external_security_dependency(std::string_view dependency) {
    constexpr std::string_view prefix = "EXTERNAL#";
    if (dependency.rfind(prefix, 0) != 0) return false;
    const auto dollar = dependency.rfind('$');
    if (dollar == std::string_view::npos || dollar <= prefix.size() ||
        dollar + 1 >= dependency.size()) return false;
    auto security = dependency.substr(prefix.size(), dollar - prefix.size());
    if (security.size() == 8 &&
        (security.substr(0, 2) == "SH" || security.substr(0, 2) == "SZ" ||
         security.substr(0, 2) == "BJ"))
        security.remove_prefix(2);
    if (security.size() != 6 ||
        !std::all_of(security.begin(), security.end(), [](unsigned char ch) {
            return std::isdigit(ch) != 0;
        })) return false;
    static const std::set<std::string_view> fields{
        "O", "OPEN", "H", "HIGH", "L", "LOW", "C", "CLOSE",
        "V", "VOL", "VOLUME", "A", "AMO", "AMOUNT"};
    return fields.count(dependency.substr(dollar + 1)) != 0;
}

bool supported_formula_reference_dependency(std::string_view dependency) {
    constexpr std::string_view prefix = "EXTERNAL#";
    if (dependency.rfind(prefix, 0) != 0 ||
        dependency.find('$', prefix.size()) != std::string_view::npos ||
        dependency.find('#', prefix.size()) != std::string_view::npos)
        return false;
    const auto reference = dependency.substr(prefix.size());
    const auto separator = reference.rfind('.');
    return separator != std::string_view::npos && separator > 0 &&
           separator + 1 < reference.size();
}

std::string parameterized_formula_reference_binding(
    std::string_view dependency, const std::vector<double>& arguments) {
    if (!supported_formula_reference_dependency(dependency))
        throw Error("invalid formula output reference: " +
                    std::string(dependency));
    std::string result(dependency);
    result += "#PARAMS";
    for (double value : arguments) {
        if (!std::isfinite(value))
            throw Error("formula reference parameter must be finite");
        if (value == 0.0) value = 0.0;  // Canonicalize negative zero.
        char buffer[64]{};
        const auto converted = std::to_chars(
            buffer, buffer + sizeof(buffer), value,
            std::chars_format::general,
            std::numeric_limits<double>::max_digits10);
        if (converted.ec != std::errc{})
            throw Error("cannot canonicalize formula reference parameter");
        result.push_back('#');
        result.append(buffer, converted.ptr);
    }
    return result;
}

std::map<std::string, double> effective_formula_parameters(
    const Json& formula, const std::map<std::string, double>& overrides) {
    std::map<std::string, double> result;
    if (const auto* defaults = optional(formula, "parameters");
        defaults && defaults->is_array()) {
        for (const auto& parameter : defaults->as_array()) {
            const auto* name = optional(parameter, "name");
            const auto* value = optional(parameter, "default");
            if (name && name->is_string() && value && value->is_number())
                result[upper_ascii(name->as_string())] = value->as_number();
        }
    }
    for (const auto& [name, value] : overrides)
        result[upper_ascii(name)] = value;
    return result;
}



Json strings_json(const std::set<std::string>& values) {
    Json result = Json::array(); for (const auto& value : values) result.push_back(value); return result;
}

Json strings_json(const std::vector<std::string>& values) {
    Json result = Json::array(); for (const auto& value : values) result.push_back(value); return result;
}

std::vector<std::string> formula_parameter_names(const Json& formula) {
    std::vector<std::string> result;
    const auto* parameters = optional(formula, "parameters");
    if (!parameters || !parameters->is_array()) return result;
    for (const auto& parameter : parameters->as_array()) {
        const auto* name = optional(parameter, "name");
        if (name && name->is_string()) result.push_back(upper_ascii(name->as_string()));
    }
    return result;
}

void merge_formula_context(Json& target, const Json& supplied) {
    if (!supplied.is_object()) throw Error("formula context file root must be an object");
    if (!target.is_object()) target = Json::object();
    for (const auto& [key, value] : supplied.as_object()) {
        auto found = target.as_object().find(key);
        if (found != target.as_object().end() && found->second.is_object() &&
            value.is_object()) {
            merge_formula_context(found->second, value);
        } else {
            target[key] = value;
        }
    }
}

namespace {

std::size_t trusted_tcalc_materialized_bar_count(const Json& context) {
    const auto* schema = optional(context, "schema");
    const auto* callback_type = optional(context, "tdx_callback_type");
    const auto* metadata = optional(context, "_formula_context");
    if (!schema || !schema->is_string() ||
        schema->as_string() != "tdx-level2-tcalc-order-flow-v1" ||
        !callback_type || !callback_type->is_number() ||
        callback_type->as_number() != 31.0 ||
        !metadata || !metadata->is_object())
        return 0;

    const auto* context_schema = optional(*metadata, "schema");
    const auto* materialization_schema = optional(
        *metadata, "materialization_schema");
    const auto* materialized = optional(*metadata, "materialized");
    const auto* complete = optional(*metadata, "context_complete");
    const auto* count = optional(*metadata, "materialized_bar_count");
    if (!context_schema || !context_schema->is_string() ||
        context_schema->as_string() !=
            "tdx-formula-explicit-context-from-tcalc-l2-v1" ||
        !materialization_schema || !materialization_schema->is_string() ||
        materialization_schema->as_string() !=
            "tdx-formula-tcalc-l2-kline-context-v1" ||
        !materialized || !materialized->is_bool() ||
        !materialized->as_bool() ||
        !complete || !complete->is_bool() || !complete->as_bool() ||
        !count || !count->is_number())
        return 0;

    const double value = count->as_number();
    if (!std::isfinite(value) || value < 1.0 || std::floor(value) != value ||
        value > static_cast<double>(std::numeric_limits<std::size_t>::max()))
        return 0;
    return static_cast<std::size_t>(value);
}

std::size_t trusted_capture_materialized_bar_count(const Json& context) {
    const auto* schema = optional(context, "schema");
    const auto* metadata = optional(context, "_capture_import");
    if (!schema || !schema->is_string() ||
        schema->as_string() != "tdx-formula-explicit-context-v1" ||
        !metadata || !metadata->is_object())
        return 0;
    const auto* import_schema = optional(*metadata, "schema");
    const auto* complete = optional(*metadata, "complete");
    const auto* missing = optional(*metadata, "missing_series_point_count");
    const auto* count = optional(*metadata, "materialized_bar_count");
    if (!import_schema || !import_schema->is_string() ||
        import_schema->as_string() != "tdx-formula-context-capture-import-v1" ||
        !complete || !complete->is_bool() || !complete->as_bool() ||
        !missing || !missing->is_number() || missing->as_number() != 0.0 ||
        !count || !count->is_number())
        return 0;
    const double value = count->as_number();
    if (!std::isfinite(value) || value < 1.0 || std::floor(value) != value ||
        value > static_cast<double>(std::numeric_limits<std::size_t>::max()))
        return 0;
    return static_cast<std::size_t>(value);
}

bool complete_tcalc_missing_series(const Json* values,
                                   std::size_t materialized_bar_count) {
    if (!materialized_bar_count || !values || !values->is_object() ||
        values->size() != materialized_bar_count)
        return false;
    for (const auto& [stamp, value] : values->as_object()) {
        if (stamp.empty() || (!value.is_number() && !value.is_null()))
            return false;
    }
    return true;
}

}  // namespace

bool explicit_formula_context_ready(const Json& analysis, const Json* context) {
    const auto* bindable = optional(analysis, "explicit_context_bindable");
    const auto* required = optional(analysis, "explicit_context_bindings_required");
    if (!bindable || !bindable->is_bool() || !bindable->as_bool() ||
        !required || !required->is_array() || required->as_array().empty() ||
        !context || !context->is_object()) return false;
    const auto materialized_bar_count = std::max(
        trusted_tcalc_materialized_bar_count(*context),
        trusted_capture_materialized_bar_count(*context));
    const auto* series = optional(*context, "series");
    const auto* scalars = optional(*context, "formula_scalar_bindings");
    for (const auto& binding : required->as_array()) {
        if (!binding.is_string()) return false;
        const Json* values = nullptr;
        if (series && series->is_object())
            for (const auto& [name, candidate] : series->as_object())
                if (upper_ascii(name) == binding.as_string()) {
                    values = &candidate;
                    break;
                }
        bool numeric = false;
        if (values && values->is_object())
            for (const auto& [stamp, value] : values->as_object()) {
                (void)stamp;
                if (value.is_number()) { numeric = true; break; }
            }
        const bool complete_native_missing = !numeric &&
            complete_tcalc_missing_series(values, materialized_bar_count);
        if (!numeric && !complete_native_missing && scalars && scalars->is_object())
            for (const auto& [name, value] : scalars->as_object())
                if (upper_ascii(name) == binding.as_string() && value.is_number()) {
                    numeric = true;
                    break;
                }
        if (!numeric && !complete_native_missing) return false;
    }
    return true;
}

bool needs_automatic_formula_context(const Json& analysis) {
    if (const auto* automatic = optional(analysis, "automatic_context_dependencies");
        automatic && automatic->is_array())
        return !automatic->as_array().empty();
    const auto* dependencies = optional(analysis, "external_dependencies");
    if (!dependencies || !dependencies->is_array()) return false;
    for (const auto& dependency : dependencies->as_array()) {
        if (!dependency.is_string()) continue;
        const auto& name = dependency.as_string();
        if (context_external_dependencies.count(name) ||
            supported_external_security_dependency(name) ||
            supported_formula_reference_dependency(name)) return true;
    }
    return false;
}

bool automatic_formula_context_enabled(const Json* context) {
    if (!context || !context->is_object()) return false;
    const auto* marker = optional(*context, "automatic_market_context");
    // Preserve the public C++ API's historical behavior: an unmarked context
    // is assumed to be a complete caller-built market context.  The CLI marks
    // explicit-only files false so they cannot accidentally enable unrelated
    // FINANCE/DYNAINFO/index formulas during a library audit.
    return !marker || !marker->is_bool() || marker->as_bool();
}

}  // namespace tdx::formula_engine_support

namespace tdx {

bool formula_explicit_context_ready(const Json& analysis, const Json* context) {
    return formula_engine_support::explicit_formula_context_ready(
        analysis, context);
}

}  // namespace tdx

