#pragma once

#include "tdx/json.hpp"

#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace tdx::formula_engine_support {

const Json* optional(const Json& object, std::string_view key);
std::string upper_ascii(std::string value);
std::filesystem::path formula_path_from_utf8(const std::string& value);

bool expansion_context_dependency(std::string_view dependency);
bool supported_external_security_dependency(std::string_view dependency);
bool supported_formula_reference_dependency(std::string_view dependency);
std::string parameterized_formula_reference_binding(
    std::string_view dependency,
    const std::vector<double>& arguments);

std::map<std::string, double> effective_formula_parameters(
    const Json& formula,
    const std::map<std::string, double>& overrides = {});

Json strings_json(const std::set<std::string>& values);
Json strings_json(const std::vector<std::string>& values);
std::vector<std::string> formula_parameter_names(const Json& formula);

void merge_formula_context(Json& target, const Json& supplied);
bool explicit_formula_context_ready(const Json& analysis, const Json* context);
bool needs_automatic_formula_context(const Json& analysis);
bool automatic_formula_context_enabled(const Json* context);

}  // namespace tdx::formula_engine_support
