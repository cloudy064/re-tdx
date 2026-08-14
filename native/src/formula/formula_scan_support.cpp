#include "formula_scan_internal.hpp"

namespace tdx::formula_scan_detail {
namespace fs = std::filesystem;

const Json* optional(const Json& object, std::string_view key) {
    if (!object.is_object()) return nullptr;
    const auto found = object.as_object().find(key);
    return found == object.as_object().end() ? nullptr : &found->second;
}

fs::path from_utf8(const std::string& value) {
#ifdef _WIN32
    return fs::path(utf8_to_wide(value));
#else
    return fs::path(value);
#endif
}

std::string upper_ascii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return ch < 0x80 ? static_cast<char>(std::toupper(ch)) : static_cast<char>(ch);
    });
    return value;
}

const Json& find_formula(const Json& library, const std::string& code) {
    const auto wanted = upper_ascii(trim(code));
    for (const auto& formula : library.at("formulas").as_array())
        if (upper_ascii(formula.at("code").as_string()) == wanted) return formula;
    throw Error("formula not found in library: " + code);
}

std::map<std::string, double> parse_parameters(const std::vector<std::string>& items) {
    std::map<std::string, double> result;
    for (const auto& item : items) {
        const auto separator = item.find('=');
        if (separator == std::string::npos || separator == 0 || separator + 1 == item.size())
            throw Error("--param must use NAME=NUMBER");
        const auto name = upper_ascii(trim(item.substr(0, separator)));
        const auto text = trim(item.substr(separator + 1));
        std::size_t used = 0; double value = 0.0;
        try { value = std::stod(text, &used); }
        catch (...) { throw Error("--param value must be numeric: " + item); }
        if (used != text.size() || !std::isfinite(value)) throw Error("--param value must be numeric: " + item);
        if (!result.emplace(name, value).second) throw Error("duplicate formula parameter: " + name);
    }
    return result;
}

int integer_option(Args& args, std::string_view name, int fallback, int minimum, int maximum) {
    const auto text = args.take_option(name, std::to_string(fallback));
    std::size_t used = 0; int value = 0;
    try { value = std::stoi(text, &used); } catch (...) { throw Error(std::string(name) + " must be an integer"); }
    if (used != text.size() || value < minimum || value > maximum) throw Error(std::string(name) + " is outside the safe range");
    return value;
}


bool has_finance_dependency(const Json& analysis) {
    const auto* dependencies = optional(analysis, "external_dependencies");
    if (!dependencies || !dependencies->is_array()) return false;
    for (const auto& dependency : dependencies->as_array())
        if (dependency.is_string() &&
            (dependency.as_string() == "FINANCE" || dependency.as_string() == "FINVALUE"))
            return true;
    return false;
}

Json analyze_selected_formula(const Json& formula) {
    const auto* source = optional(formula, "source_text");
    if (!source || !source->is_string())
        throw Error("selected formula source text is unavailable");
    std::vector<std::string> parameter_names;
    if (const auto* parameters = optional(formula, "parameters");
        parameters && parameters->is_array()) {
        for (const auto& parameter : parameters->as_array())
            if (const auto* name = optional(parameter, "name"); name && name->is_string())
                parameter_names.push_back(name->as_string());
    }
    return analyze_formula_source(source->as_string(), parameter_names);
}

}  // namespace tdx::formula_scan_detail
