#include "formula_context_plan_internal.hpp"

#include "formula_engine_support_internal.hpp"
#include "formula_nested_internal.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <initializer_list>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace tdx::formula_context_detail {
namespace {

const Json* optional(const Json& object, std::string_view key) {
    if (!object.is_object()) return nullptr;
    const auto found = object.as_object().find(key);
    return found == object.as_object().end() ? nullptr : &found->second;
}

std::set<int> numbered_bindings(const Json& analysis, std::string_view name) {
    std::set<int> result;
    const auto* values = optional(analysis, "context_bindings_required");
    if (!values || !values->is_array()) return result;
    const std::string prefix = std::string(name) + "#";
    for (const auto& value : values->as_array()) {
        if (!value.is_string() || value.as_string().rfind(prefix, 0) != 0) continue;
        try { result.insert(std::stoi(value.as_string().substr(prefix.size()))); }
        catch (...) { /* Analyzer only emits numeric IDs; ignore malformed external input. */ }
    }
    return result;
}

std::vector<std::string> string_bindings(
    const Json& analysis, std::string_view name) {
    std::vector<std::string> result;
    const auto* values = optional(analysis, "context_bindings_required");
    if (!values || !values->is_array()) return result;
    const std::string prefix = std::string(name) + "#";
    for (const auto& value : values->as_array()) {
        if (!value.is_string() || value.as_string().rfind(prefix, 0) != 0)
            continue;
        result.push_back(value.as_string());
    }
    return result;
}

std::vector<HorcalcBinding> horcalc_bindings(const Json& analysis) {
    std::vector<HorcalcBinding> result;
    const auto* values = optional(analysis, "context_bindings_required");
    if (!values || !values->is_array()) return result;
    constexpr std::string_view prefix = "HORCALC#";
    for (const auto& value : values->as_array()) {
        if (!value.is_string() || value.as_string().rfind(prefix, 0) != 0)
            continue;
        const auto& binding = value.as_string();
        const auto weight_separator = binding.rfind('#');
        const auto calculation_separator = weight_separator == std::string::npos
            ? std::string::npos : binding.rfind('#', weight_separator - 1);
        const auto item_separator = calculation_separator == std::string::npos
            ? std::string::npos : binding.rfind('#', calculation_separator - 1);
        if (item_separator == std::string::npos ||
            item_separator <= prefix.size())
            continue;
        try {
            HorcalcBinding parsed;
            parsed.name = binding;
            parsed.block_name = binding.substr(
                prefix.size(), item_separator - prefix.size());
            parsed.item = std::stoi(binding.substr(
                item_separator + 1,
                calculation_separator - item_separator - 1));
            parsed.calculation = std::stoi(binding.substr(
                calculation_separator + 1,
                weight_separator - calculation_separator - 1));
            parsed.weight = std::stoi(binding.substr(weight_separator + 1));
            if (!parsed.block_name.empty() && parsed.item >= 100 &&
                parsed.item <= 106 && parsed.calculation >= 0 &&
                parsed.calculation <= 2 && parsed.weight >= 0 &&
                parsed.weight <= 4)
                result.push_back(std::move(parsed));
        } catch (...) { /* Analyzer emits validated constants. */ }
    }
    return result;
}

std::vector<IndicatorAggregateBinding> indicator_aggregate_bindings(
    const Json& analysis) {
    std::vector<IndicatorAggregateBinding> result;
    const auto* values = optional(analysis, "context_bindings_required");
    if (!values || !values->is_array()) return result;
    for (const auto& value : values->as_array()) {
        if (!value.is_string()) continue;
        const auto parts = split(value.as_string(), '#');
        if (parts.size() != 5 ||
            (parts[0] != "INSORT" && parts[0] != "INSUM"))
            continue;
        try {
            IndicatorAggregateBinding binding;
            binding.name = value.as_string();
            binding.function = parts[0];
            binding.block_name = parts[1];
            binding.formula_name = parts[2];
            binding.output = std::stoi(parts[3]);
            binding.mode = std::stoi(parts[4]);
            if (!binding.block_name.empty() && !binding.formula_name.empty() &&
                binding.output >= 1 &&
                (binding.function == "INSORT"
                     ? binding.mode >= 0 && binding.mode <= 1
                     : binding.mode >= 0 && binding.mode <= 5))
                result.push_back(std::move(binding));
        } catch (...) { /* Analyzer emits validated constants. */ }
    }
    return result;
}

std::vector<TradingBinding> trading_bindings(const Json& analysis, std::string_view name) {
    std::vector<TradingBinding> result;
    const auto* values = optional(analysis, "context_bindings_required");
    if (!values || !values->is_array()) return result;
    const std::string prefix = std::string(name) + "#";
    for (const auto& value : values->as_array()) {
        if (!value.is_string() || value.as_string().rfind(prefix, 0) != 0) continue;
        const auto parts = split(value.as_string(), '#');
        if (parts.size() != 4) continue;
        try {
            result.push_back({value.as_string(), std::stoi(parts[1]),
                              std::stoi(parts[2]), std::stoi(parts[3])});
        } catch (...) { /* Analyzer emits validated integers; ignore malformed external input. */ }
    }
    return result;
}

std::vector<OnePointBinding> one_point_bindings(
    const Json& analysis, std::string_view name) {
    std::vector<OnePointBinding> result;
    const auto* values = optional(analysis, "context_bindings_required");
    if (!values || !values->is_array()) return result;
    const std::string prefix = std::string(name) + "#";
    for (const auto& value : values->as_array()) {
        if (!value.is_string() || value.as_string().rfind(prefix, 0) != 0) continue;
        const auto parts = split(value.as_string(), '#');
        const bool finance = name == "FINONE";
        const bool local = name == "GPONEDAT";
        if (parts.size() != (local ? 2u : finance ? 4u : 5u)) continue;
        try {
            OnePointBinding binding;
            binding.name = value.as_string();
            binding.id = std::stoi(parts[1]);
            if (finance) {
                binding.year = std::stoi(parts[2]);
                binding.mmdd = std::stoi(parts[3]);
            } else if (!local) {
                binding.field = std::stoi(parts[2]);
                binding.year = std::stoi(parts[3]);
                binding.mmdd = std::stoi(parts[4]);
            }
            result.push_back(std::move(binding));
        } catch (...) { /* Analyzer emits validated integers; ignore malformed external input. */ }
    }
    return result;
}

std::vector<SplitBinding> split_bindings(const Json& analysis) {
    std::vector<SplitBinding> result;
    const auto* values = optional(analysis, "context_bindings_required");
    if (!values || !values->is_array()) return result;
    for (const auto& value : values->as_array()) {
        if (!value.is_string()) continue;
        const auto parts = split(value.as_string(), '#');
        if (parts.size() != 3 || (parts[0] != "SPLIT" && parts[0] != "SPLITBARS")) continue;
        try {
            result.push_back({value.as_string(), std::stoi(parts[1]), std::stoi(parts[2])});
        } catch (...) { /* Analyzer emits validated integers; ignore malformed external input. */ }
    }
    return result;
}

std::vector<FormulaReferenceRequest> formula_reference_bindings(
    const Json& analysis) {
    std::vector<FormulaReferenceRequest> result;
    const auto* uses = optional(analysis, "formula_reference_uses");
    if (uses && uses->is_array()) {
        for (const auto& use : uses->as_array()) {
            const auto* binding = optional(use, "binding");
            const auto* arguments = optional(use, "arguments");
            const auto* scalar = optional(use, "scalar_arguments");
            if (!binding || !binding->is_string() ||
                !formula_engine_support::supported_formula_reference_dependency(
                    binding->as_string()) ||
                (scalar && scalar->is_bool() && !scalar->as_bool()))
                continue;
            FormulaReferenceRequest request;
            request.binding = binding->as_string();
            if (arguments && arguments->is_array())
                for (const auto& argument : arguments->as_array())
                    if (argument.is_string())
                        request.argument_expressions.push_back(
                            argument.as_string());
            result.push_back(std::move(request));
        }
        return result;
    }
    const auto* values = optional(analysis, "external_dependencies");
    if (!values || !values->is_array()) return result;
    for (const auto& value : values->as_array())
        if (value.is_string() &&
            formula_engine_support::supported_formula_reference_dependency(
                value.as_string()))
            result.push_back(FormulaReferenceRequest{value.as_string(), {}});
    return result;
}


} // namespace

bool includes_any(const std::set<int>& values,
                  std::initializer_list<int> wanted) {
    return std::any_of(wanted.begin(), wanted.end(),
                       [&](int value) { return values.count(value) != 0; });
}

FormulaContextPlan build_formula_context_plan(const Json& analysis,
                                              bool point_in_time_finance) {
    FormulaContextPlan plan;
    for (const auto& value : analysis.at("external_dependencies").as_array())
        plan.dependencies.insert(value.as_string());
    plan.finance_bindings = numbered_bindings(analysis, "FINANCE");
    plan.professional_finance_bindings = numbered_bindings(analysis, "FINVALUE");
    plan.dynamic_quote =
        dynamic_quote_requirements(analysis, plan.dependencies);
    plan.stock_trading_bindings = trading_bindings(analysis, "GPJYVALUE");
    plan.board_trading_bindings = trading_bindings(analysis, "BKJYVALUE");
    plan.market_trading_bindings = trading_bindings(analysis, "SCJYVALUE");
    plan.finone_bindings = one_point_bindings(analysis, "FINONE");
    plan.stock_one_bindings = one_point_bindings(analysis, "GPJYONE");
    plan.board_one_bindings = one_point_bindings(analysis, "BKJYONE");
    plan.market_one_bindings = one_point_bindings(analysis, "SCJYONE");
    plan.local_one_bindings = one_point_bindings(analysis, "GPONEDAT");
    plan.blocksetnum_bindings = string_bindings(analysis, "BLOCKSETNUM");
    plan.horizontal_bindings = horcalc_bindings(analysis);
    plan.aggregate_bindings = indicator_aggregate_bindings(analysis);
    plan.calcstockindex_bindings =
        string_bindings(analysis, "CALCSTOCKINDEX");
    plan.formula_reference_bindings = formula_reference_bindings(analysis);
    plan.split_context_bindings = split_bindings(analysis);
    plan.external_series_bindings =
        string_bindings(analysis, "EXTDATA_USER");
    plan.local_signal_bindings = string_bindings(analysis, "SIGNALS_SYS");
    const auto user_signal_bindings =
        string_bindings(analysis, "SIGNALS_USER");
    plan.local_signal_bindings.insert(plan.local_signal_bindings.end(),
                                      user_signal_bindings.begin(),
                                      user_signal_bindings.end());
    plan.main_quote_bindings = string_bindings(analysis, "MAINZSHQ");

    const auto& dependencies = plan.dependencies;
    plan.chip_context =
        dependencies.count("WINNER") || dependencies.count("COST") ||
        dependencies.count("COSTEX") || dependencies.count("PWINNER") ||
        dependencies.count("LWINNER") || dependencies.count("PPART") ||
        dependencies.count("TDXMCST") || dependencies.count("TDXPAV") ||
        dependencies.count("TDXPAVE") || dependencies.count("TDXSSRP");
    plan.capital_history_context =
        plan.chip_context || dependencies.count("LFS");
    plan.security_status_context =
        dependencies.count("IST0CODE") || dependencies.count("ISSTCODE") ||
        dependencies.count("ISQUITCODE") || dependencies.count("ISQHQQCODE");
    plan.contract_multiplier_context = dependencies.count("MULTIPLIER");
    plan.host_calendar_context =
        dependencies.count("ISJYDATE") || dependencies.count("LOCALDAYNUM");
    plan.current_finance = current_finance_requirements(
        dependencies, plan.finance_bindings, point_in_time_finance,
        plan.dynamic_quote.requires_finance(), plan.capital_history_context);
    plan.relation_blocks = relation_block_requirements(dependencies);
    return plan;
}

void validate_formula_context_plan(const FormulaContextPlan& plan,
                                   const Json* kline_document,
                                   bool point_in_time_finance) {
    if (point_in_time_finance) {
        if (!kline_document)
            throw Error("point-in-time finance context requires a K-line document");
        for (const int id : plan.finance_bindings)
            if (id != 43 && id != 44)
                throw Error("point-in-time finance currently supports FINANCE(43/44); "
                            "unsupported FINANCE(" + std::to_string(id) +
                            ") would use current data");
    }
    if (plan.host_calendar_context && !kline_document)
        throw Error("formula host-calendar context requires a K-line document");
    if (!plan.horizontal_bindings.empty() && !kline_document)
        throw Error("HORCALC formula context requires a K-line document");
    if (!plan.aggregate_bindings.empty() && !kline_document)
        throw Error("INSORT/INSUM formula context requires a K-line document");
    if (!plan.calcstockindex_bindings.empty() && !kline_document)
        throw Error("CALCSTOCKINDEX formula context requires a K-line document");
    if (!plan.formula_reference_bindings.empty() && !kline_document)
        throw Error("formula output references require a K-line document");
}

bool FormulaContextPlan::needs_block_data(int status_market_id) const {
    return relation_blocks.any() || !blocksetnum_bindings.empty() ||
           !horizontal_bindings.empty() || !aggregate_bindings.empty() ||
           !board_one_bindings.empty() ||
           (security_status_context && status_market_id <= 2);
}

bool FormulaContextPlan::needs_formula_library() const {
    return !aggregate_bindings.empty() || !calcstockindex_bindings.empty() ||
           !formula_reference_bindings.empty();
}

bool FormulaContextPlan::needs_professional_kline() const {
    return !stock_trading_bindings.empty() || !board_trading_bindings.empty() ||
           !market_trading_bindings.empty() ||
           !professional_finance_bindings.empty();
}

std::set<std::string> FormulaContextPlan::block_families(
    const std::filesystem::path& root) const {
    std::set<std::string> families = relation_blocks.families;
    const bool full_catalog = !blocksetnum_bindings.empty() ||
                              !horizontal_bindings.empty() ||
                              !aggregate_bindings.empty();
    if (full_catalog) {
        families.insert(tdx_user_industry_mode(root) == 2
                            ? "research-industry" : "industry");
        families.insert("concept");
        families.insert("style");
        families.insert("index");
    }
    if (!board_one_bindings.empty()) {
        families.insert("industry");
        families.insert("research-industry");
    }
    return families;
}

} // namespace tdx::formula_context_detail
