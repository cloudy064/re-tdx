#include "formulas_internal.hpp"

#include "tdx/common.hpp"

#include <algorithm>

namespace tdx::formulas_detail {
namespace {

constexpr std::array<FormulaKindDefinition, 5> kinds{{
    {0, "technical", "技术指标公式"},
    {1, "selection", "条件选股公式"},
    {2, "expert", "专家系统公式"},
    {3, "color-k", "五彩K线公式"},
    {4, "reserved", "内部保留集合"},
}};

}  // namespace

const std::array<FormulaKindDefinition, 5>& formula_kind_definitions() {
    return kinds;
}

std::string formula_kind_key(int kind) {
    const auto found = std::find_if(kinds.begin(), kinds.end(),
        [kind](const FormulaKindDefinition& item) { return item.id == kind; });
    if (found == kinds.end()) throw Error("unknown formula kind id");
    return std::string(found->key);
}

std::string formula_kind_name(int kind) {
    const auto found = std::find_if(kinds.begin(), kinds.end(),
        [kind](const FormulaKindDefinition& item) { return item.id == kind; });
    if (found == kinds.end()) throw Error("unknown formula kind id");
    return std::string(found->name);
}

std::set<int> selected_kinds(const std::string& value) {
    if (value == "all") return {0, 1, 2, 3};
    const auto found = std::find_if(kinds.begin(), kinds.end(),
        [&](const FormulaKindDefinition& item) { return item.key == value; });
    if (found == kinds.end()) throw Error("unknown formula kind: " + value);
    return {found->id};
}

}  // namespace tdx::formulas_detail
