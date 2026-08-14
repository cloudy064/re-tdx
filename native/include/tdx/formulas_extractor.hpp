#pragma once

#include "tdx/formulas.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace tdx {

// Optional forensic tooling. These functions parse TCalc.dll as an inert PE
// image; they are intentionally not linked into the standalone tdx-tool.
Json extract_formulas_document(const std::filesystem::path& dll,
                               const std::string& kind = "all");
FormulaIconSprite extract_formula_icon_sprite(const std::filesystem::path& dll);
int command_tcalc_formulas_extract(const std::vector<std::string>& args);
int command_tcalc_formulas_icons(const std::vector<std::string>& args);

}  // namespace tdx
