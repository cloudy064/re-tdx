#include "recon_contract_market_research_internal.hpp"

#include "tdx/common.hpp"

#include <cmath>

namespace tdx::recon_contract_detail {

void validate_thematic_hype_completed_contract(const Json& document, Json& result) {
    assert_thematic_common(document, result, "hype-completed");
    const auto* rows = member(document, "completed_hype");
    const Json* first = rows && rows->is_array() && !rows->as_array().empty()
        ? &rows->as_array().front() : nullptr;
    const auto* leader = first ? member(*first, "leader") : nullptr;
    const auto* interval_return = first
        ? member(*first, "interval_return_pct") : nullptr;
    const auto* analysis = first ? member(*first, "analysis") : nullptr;
    add_assertion(result, "completed_hype_semantics",
                  first && string_is(member(*first, "status"), "completed") &&
                      member(*first, "block_name") &&
                      member(*first, "start_date") && member(*first, "end_date") &&
                      member(*first, "limit_pattern") && leader &&
                      leader->is_object() &&
                      bool_is(member(*leader, "name_resolved"), true) &&
                      interval_return && interval_return->is_number() &&
                      analysis && analysis->is_string() &&
                      !analysis->as_string().empty() && member(*first, "raw") &&
                      member(*first, "raw")->is_object(),
                  "block, resolved leader, interval return, pattern, analysis and raw row",
                  first ? *first : Json(nullptr));
}

void validate_thematic_hype_active_contract(const Json& document, Json& result) {
    assert_thematic_common(document, result, "hype-active");
    const auto* rows = member(document, "active_hype");
    const Json* first = rows && rows->is_array() && rows->size() >= 3
        ? &rows->as_array().front() : nullptr;
    const auto* stock_return = first
        ? member(*first, "stock_return_pct") : nullptr;
    const auto* index_return = first
        ? member(*first, "shanghai_index_return_pct") : nullptr;
    const auto* relative = first ? member(*first, "relative_return_pct") : nullptr;
    const bool formula = stock_return && stock_return->is_number() &&
        index_return && index_return->is_number() && relative &&
        relative->is_number() && std::abs(relative->as_number() -
            (stock_return->as_number() - index_return->as_number())) < 0.000001;
    add_assertion(result, "active_hype_semantics",
                  first && string_is(member(*first, "status"), "active") &&
                      member(*first, "security") &&
                      member(*first, "security")->is_object() &&
                      member(*first, "interval_stat") && formula &&
                      member(*first, "raw") && member(*first, "raw")->is_object(),
                  "at least three active stocks with stock-index relative-return formula",
                  first ? *first : Json(nullptr));
}

}  // namespace tdx::recon_contract_detail
