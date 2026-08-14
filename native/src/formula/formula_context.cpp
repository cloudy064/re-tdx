#include "tdx/formula_context.hpp"

#include "tdx/common.hpp"
#include "tdx/corporate.hpp"
#include "tdx/formula_engine.hpp"
#include "tdx/formulas.hpp"
#include "tdx/hk_actions.hpp"

#include "formula_catalog_internal.hpp"
#include "formula_context_aggregate_internal.hpp"
#include "formula_context_blocks_internal.hpp"
#include "formula_context_current_finance_internal.hpp"
#include "formula_context_dynamic_internal.hpp"
#include "formula_context_finance_extensions_internal.hpp"
#include "formula_context_finance_internal.hpp"
#include "formula_context_host_internal.hpp"
#include "formula_context_local_internal.hpp"
#include "formula_context_market_summary_internal.hpp"
#include "formula_context_plan_internal.hpp"
#include "formula_context_professional_internal.hpp"
#include "formula_context_relations_internal.hpp"
#include "formula_context_support_internal.hpp"
#include "formula_engine_support_internal.hpp"
#include "formula_nested_internal.hpp"
#include "formula_reference_internal.hpp"

#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <utility>

namespace tdx {

using formula_context_detail::formula_market_id;

Json build_formula_market_context_document(const std::filesystem::path& root,
                                           const std::string& market,
                                           const std::string& code,
                                           const Json& analysis,
                                           int timeout_ms,
                                           const BlockData* block_data,
                                           const Json* kline_document,
                                           bool point_in_time_finance,
                                           const Json* formula_library,
                                           const std::filesystem::path& jsn_root,
                                           const Json* nested_state,
                                           const std::map<std::string, double>* formula_parameters) {
    const auto plan =
        formula_context_detail::build_formula_context_plan(
            analysis, point_in_time_finance);
    formula_context_detail::validate_formula_context_plan(
        plan, kline_document, point_in_time_finance);
    const auto& dependencies = plan.dependencies;
    const auto& professional_finance_bindings =
        plan.professional_finance_bindings;
    const auto& dynamic_quote = plan.dynamic_quote;
    const auto& stock_trading_bindings = plan.stock_trading_bindings;
    const auto& board_trading_bindings = plan.board_trading_bindings;
    const auto& market_trading_bindings = plan.market_trading_bindings;
    const auto& finone_bindings = plan.finone_bindings;
    const auto& stock_one_bindings = plan.stock_one_bindings;
    const auto& board_one_bindings = plan.board_one_bindings;
    const auto& market_one_bindings = plan.market_one_bindings;
    const auto& local_one_bindings = plan.local_one_bindings;
    const auto& blocksetnum_bindings = plan.blocksetnum_bindings;
    const auto& horizontal_bindings = plan.horizontal_bindings;
    const auto& aggregate_bindings = plan.aggregate_bindings;
    const auto& calcstockindex_bindings = plan.calcstockindex_bindings;
    const auto& formula_reference_bindings = plan.formula_reference_bindings;
    const auto& split_context_bindings = plan.split_context_bindings;
    const auto& main_quote_bindings = plan.main_quote_bindings;
    const bool capital_history_context = plan.capital_history_context;
    const auto& current_finance_requirements = plan.current_finance;
    const auto& relation_blocks = plan.relation_blocks;
    const int status_market_id = formula_market_id(market);
    const int formula_security_type =
        formula_context_detail::formula_security_type(
            market, code, kline_document);
    if ((plan.security_status_context || plan.contract_multiplier_context ||
         plan.host_calendar_context) && status_market_id < 0)
        throw Error("formula host context has an invalid market");
    Json context = Json::object(), finance_values = Json::object(),
         professional_finance_values = Json::object();
    std::optional<BlockData> owned_block_data;
    if (plan.needs_block_data(status_market_id) && !block_data) {
        owned_block_data = load_blocks(root, plan.block_families(root));
        block_data = &*owned_block_data;
    }
    Json symbols = Json::object();
    formula_context_detail::bind_static_security_relations(
        context, symbols, root, jsn_root, market, code, kline_document,
        dependencies, block_data, formula_security_type, timeout_ms);
    formula_context_detail::bind_security_stat_functions(
        context, symbols, root, status_market_id, code,
        dependencies, timeout_ms);
    formula_context_detail::bind_public_market_summary_functions(
        context, root, status_market_id, code, dependencies,
        main_quote_bindings, block_data, timeout_ms);
    formula_context_detail::bind_host_formula_context(
        context, symbols, root, market, code, plan, status_market_id,
        formula_security_type, kline_document, block_data, timeout_ms);
    auto current_finance =
        formula_context_detail::build_current_finance_context(
            symbols, root, market, code, current_finance_requirements,
            status_market_id, formula_security_type, timeout_ms);
    finance_values = std::move(current_finance.values);
    if (current_finance.metadata.is_object())
        for (const auto& [name, value] :
             current_finance.metadata.as_object())
            context[name] = value;
    const Json* finance_record = current_finance.record();
    const double current_circulating_shares =
        current_finance.circulating_shares;
    if (current_finance_requirements.region)
        formula_context_detail::bind_region_text(
            context, symbols, current_finance.province_id);
    if (dependencies.count("GNBLOCK")) {
        if (!block_data) throw Error("formula concept text context requires block data");
        formula_context_detail::bind_concept_text(
            context, symbols, *block_data, market, code);
    }
    formula_context_detail::bind_local_security_metadata(
        context, symbols, root, market, code, dependencies);
    if (relation_blocks.metadata) {
        if (!block_data) throw Error("formula block metadata context requires block data");
        formula_context_detail::bind_block_metadata(
            context, symbols, *block_data, market, code, dependencies);
    }
    if (relation_blocks.code_functions) {
        if (!block_data)
            throw Error("formula block-code context requires block data");
        formula_context_detail::bind_block_code_functions(
            context, *block_data, root, market, code, dependencies);
    }
    if (!blocksetnum_bindings.empty()) {
        if (!block_data)
            throw Error("BLOCKSETNUM formula context requires local block data");
        formula_context_detail::bind_blocksetnum(
            context, root, *block_data, blocksetnum_bindings);
    }
    if (!horizontal_bindings.empty()) {
        if (!block_data)
            throw Error("HORCALC formula context requires local block data");
        formula_context_detail::bind_horcalc(
            context, root, *block_data, *kline_document,
            horizontal_bindings, timeout_ms);
    }
    std::optional<Json> owned_formula_library;
    if (plan.needs_formula_library()) {
        if (!formula_library) {
            owned_formula_library = analyze_formula_library_document(
                load_bundled_formula_library_document());
            formula_library = &*owned_formula_library;
        }
    }
    if (!aggregate_bindings.empty()) {
        if (!block_data)
            throw Error("INSORT/INSUM formula context requires local block data");
        formula_context_detail::bind_indicator_aggregates(
            context, root, *block_data, *kline_document, status_market_id,
            code, aggregate_bindings, *formula_library);
    }
    if (!calcstockindex_bindings.empty()) {
        formula_context_detail::bind_calcstockindex_context(
            context, root, market, code, *kline_document,
            calcstockindex_bindings, *formula_library, timeout_ms, block_data,
            jsn_root, nested_state);
    }
    if (!formula_reference_bindings.empty()) {
        std::map<std::string, double> caller_parameters;
        if (const auto* defaults = formula_engine_support::optional(
                analysis, "parameter_defaults");
            defaults && defaults->is_object())
            for (const auto& [name, value] : defaults->as_object())
                if (value.is_number())
                    caller_parameters[formula_engine_support::upper_ascii(name)] =
                        value.as_number();
        if (formula_parameters)
            for (const auto& [name, value] : *formula_parameters)
                caller_parameters[formula_engine_support::upper_ascii(name)] =
                    value;
        formula_context_detail::bind_formula_reference_context(
            context, root, market, code, *kline_document,
            formula_reference_bindings, *formula_library, timeout_ms,
            block_data, jsn_root, nested_state, caller_parameters);
    }
    const bool hk_actions = is_hk_action_market(market);
    Json capital_document;
    if (!hk_actions &&
        (capital_history_context || !split_context_bindings.empty() ||
         dependencies.count("DIVFACTOR")))
        capital_document = fetch_capital_changes_document(
            {market + ":" + code}, {}, timeout_ms, false);
    if (hk_actions &&
        (capital_history_context || !split_context_bindings.empty()))
        throw Error("CAPITAL/SPLIT formula context is unavailable for HK actions; "
                    "only native DIVFACTOR share multipliers are mapped");
    if (capital_history_context) {
        if (!kline_document)
            throw Error("historical-capital formula context requires a K-line document");
        formula_context_detail::bind_capital_history_context(
            context, *kline_document, capital_document,
            current_circulating_shares);
    }
    if (!split_context_bindings.empty()) {
        if (!kline_document) throw Error("split formula context requires a K-line document");
        formula_context_detail::bind_split_context(
            context, *kline_document, capital_document,
            split_context_bindings);
    }
    if (dependencies.count("DIVFACTOR")) {
        if (!kline_document)
            throw Error("DIVFACTOR formula context requires a K-line document");
        auto points = hk_actions
            ? build_hk_divfactor_series_document(root, code, *kline_document)
            : build_divfactor_series_document(*kline_document, capital_document);
        if (!context.as_object().count("series")) context["series"] = Json::object();
        context["series"]["__DIVFACTOR_FRONT"] = points.at("front");
        context["series"]["__DIVFACTOR_BACK"] = points.at("back");
        context["divfactor_mode"] = points.at("mode");
        context["divfactor_front_semantics"] =
            "divide all bars strictly before each matched ex-date";
        context["divfactor_back_semantics"] =
            "multiply each matched ex-date and every later bar";
        context["divfactor_event_date_count"] = points.at("event_date_count");
        context["divfactor_matched_bar_count"] = points.at("matched_bar_count");
    }
    formula_context_detail::bind_dynamic_quote_context(
        context, symbols, root, market, code, kline_document,
        dynamic_quote, finance_record, formula_security_type,
        timeout_ms, block_data);
    const int type = formula_security_type;
    formula_context_detail::bind_formula_finance_extensions(
        context, finance_values, professional_finance_values, root, market,
        code, kline_document, plan, point_in_time_finance, type, timeout_ms);
    if (kline_document)
        formula_context_detail::bind_security_relation_series(
            context, root, *kline_document, market, code, dependencies,
            block_data, timeout_ms);
    if ((!stock_trading_bindings.empty() || !board_trading_bindings.empty() ||
         !market_trading_bindings.empty() || !professional_finance_bindings.empty()) &&
        !kline_document)
        throw Error("professional formula context requires a K-line document");
    if (kline_document)
        formula_context_detail::bind_professional_series_context(
            context, *kline_document, market, code,
            stock_trading_bindings, board_trading_bindings,
            market_trading_bindings, professional_finance_bindings,
            professional_finance_values, point_in_time_finance, type,
            timeout_ms);
    std::optional<formula_context_detail::ProfessionalBoardTarget>
        professional_board_target;
    if (!board_one_bindings.empty())
        professional_board_target =
            formula_context_detail::resolve_professional_board_target(
                market, code, formula_security_type, block_data);
    formula_context_detail::bind_professional_one_points(
        context, root, market, code, finone_bindings, stock_one_bindings,
        board_one_bindings, market_one_bindings, local_one_bindings,
        timeout_ms, professional_board_target);
    context["finance"] = std::move(finance_values);
    context["finvalue"] = std::move(professional_finance_values);
    const auto bound_symbols = context.as_object().find("symbols");
    if (bound_symbols != context.as_object().end() &&
        bound_symbols->second.is_object())
        for (const auto& [name, value] : bound_symbols->second.as_object())
            symbols[name] = value;
    context["symbols"] = std::move(symbols);
    return context;
}

} // namespace tdx
