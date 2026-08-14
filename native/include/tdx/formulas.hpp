#pragma once

#include "tdx/common.hpp"
#include "tdx/json.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace tdx {

struct FormulaIconSprite {
    Bytes bitmap;
    Bytes png;
    Json manifest;
};

Json extract_user_formulas_document(const std::filesystem::path& pri_gs,
                                    const std::string& kind = "all");
std::filesystem::path locate_formula_bundle(
    const std::filesystem::path& explicit_directory = {});
Json load_bundled_formula_library_document(
    const std::filesystem::path& bundle_directory = {},
    const std::string& kind = "all");
Json load_bundled_installed_formula_library_document(
    const std::filesystem::path& root = {}, bool include_user = false,
    const std::filesystem::path& bundle_directory = {});
FormulaIconSprite load_bundled_formula_icon_sprite(
    const std::filesystem::path& bundle_directory = {});
Json merge_user_formula_library_document(
    Json system, const std::filesystem::path& pri_gs,
    const std::string& kind = "all");
int command_formulas_extract(const std::vector<std::string>& args);
int command_formulas_user_library(const std::vector<std::string>& args);
int command_formulas_icons(const std::vector<std::string>& args);

}  // namespace tdx
