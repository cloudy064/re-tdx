#include "recon_contract_market_research_internal.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace tdx::recon_contract_detail {

namespace {

using IntelligenceContractValidator = void (*)(const Json& document, Json& result);

void validate_intelligence_value_attention(const Json& document, Json& result) {
    add_assertion(result, "schema",
                  string_is(member(document, "schema"),
                            "tdx-market-intelligence-native-v1"),
                  "tdx-market-intelligence-native-v1",
                  value_or_null(member(document, "schema")));
    add_assertion(result, "selected_category",
                  string_is(member(document, "view"), "value-attention") &&
                      string_is(member_path(document,
                                            {"selected_value_attention", "category_id"}),
                                "3109"),
                  "value-attention / 3109",
                  value_or_null(member(document, "selected_value_attention")));
    const auto categories = numeric_value(member_path(
        document, {"value_attention_summary", "categories"}));
    const auto relationships = numeric_value(member_path(
        document, {"value_attention_summary", "relationships"}));
    const auto securities = numeric_value(member_path(
        document, {"value_attention_summary", "unique_securities"}));
    add_assertion(result, "master_population",
                  categories && *categories == 8 && relationships &&
                      *relationships >= 1000 && securities && *securities >= 800,
                  "8 categories, 1000+ relationships and 800+ securities",
                  value_or_null(member(document, "value_attention_summary")));
    const auto* records = member(document, "records");
    bool typed = records && records->is_array() && !records->as_array().empty();
    if (typed) for (const auto& record : records->as_array()) {
        const auto* security = member(record, "security");
        typed = typed && string_is(member(record, "category_id"), "3109") &&
            security && security->is_object() &&
            nonempty_string(member(*security, "code")) &&
            member(record, "anchor_price_yuan") &&
            member(record, "adjusted_anchor_price_yuan") &&
            member(record, "three_month_adjusted_close_yuan") &&
            member(record, "breach_depth_pct") &&
            member(record, "breach_depth_pct")->is_null() &&
            member(record, "raw") && member(record, "raw")->is_object();
    }
    add_assertion(result, "typed_dynamic_details", typed,
                  "security, three static prices, null live-price formula and raw evidence",
                  typed);
    const auto* reconciliation = member(document, "value_attention_reconciliation");
    add_assertion(result, "reconciliation_shape",
                  reconciliation && reconciliation->is_object() &&
                      member(*reconciliation, "inline_member_count") &&
                      member(*reconciliation, "inline_member_count")->is_number() &&
                      member(*reconciliation, "dynamic_member_count") &&
                      member(*reconciliation, "dynamic_member_count")->is_number() &&
                      member(*reconciliation, "exact_match") &&
                      member(*reconciliation, "exact_match")->is_bool() &&
                      member(*reconciliation, "inline_only") &&
                      member(*reconciliation, "inline_only")->is_array() &&
                      member(*reconciliation, "dynamic_only") &&
                      member(*reconciliation, "dynamic_only")->is_array(),
                  "auditable inline/dynamic set comparison",
                  value_or_null(reconciliation));
    const bool exact_sources =
        source_exists(document, "list/func_jzgz101_1.jsn") &&
        source_exists(document, "jzgz1/3109.jsn") &&
        member(document, "sources") && member(document, "sources")->is_array() &&
        member(document, "sources")->size() == 2;
    add_assertion(result, "exact_sources", exact_sources,
                  "JZGZ master plus selected dynamic detail", exact_sources);
}

void validate_intelligence_discredited(const Json& document, Json& result) {
    add_assertion(result, "schema",
                  string_is(member(document, "schema"),
                            "tdx-market-intelligence-native-v1"),
                  "tdx-market-intelligence-native-v1",
                  value_or_null(member(document, "schema")));
    add_assertion(result, "view_category",
                  string_is(member(document, "view"), "risks") &&
                      string_is(member(document, "category"), "discredited"),
                  "risks/discredited", value_or_null(member(document, "category")));
    const auto* rows = member(document, "records");
    bool typed = rows && rows->is_array() && rows->size() >= 8;
    bool descending = true;
    if (typed) {
        for (std::size_t index = 0; index < rows->size(); ++index) {
            const auto& row = rows->as_array()[index];
            const auto* security = member(row, "security");
            const auto occurrences = numeric_value(
                member(row, "occurrences_past_year"));
            const auto* raw = member(row, "raw");
            typed = typed && security && security->is_object() &&
                nonempty_string(member(*security, "code")) &&
                string_is(member(row, "category"), "discredited") &&
                string_is(member(row, "category_label"), "失信被执行") &&
                nonempty_string(member(row, "announcement_date")) &&
                nonempty_string(member(row, "involved_subject")) &&
                nonempty_string(member(row, "object_type")) && occurrences &&
                *occurrences >= 1 && member(row, "safety_score") &&
                member(row, "safety_score")->is_null() && raw && raw->is_object();
            if (index) {
                const auto* previous = member(rows->as_array()[index - 1],
                                              "announcement_date");
                const auto* current = member(row, "announcement_date");
                descending = descending && previous && current &&
                    previous->is_string() && current->is_string() &&
                    previous->as_string() >= current->as_string();
            }
        }
    }
    add_assertion(result, "typed_discredited_rows", typed,
                  "8+ rows with security, subject, type, date, count and raw evidence",
                  typed);
    add_assertion(result, "descending_announcement_date", descending,
                  true, descending);
    const bool exact_source =
        source_exists(document, "list/func_sxbzx101_1.jsn");
    add_assertion(result, "exact_source", exact_source,
                  "list/func_sxbzx101_1.jsn", exact_source);
}

void validate_intelligence_highlights(const Json& document, Json& result) {
    add_assertion(result, "schema",
                  string_is(member(document, "schema"),
                            "tdx-market-intelligence-native-v1"),
                  "tdx-market-intelligence-native-v1",
                  value_or_null(member(document, "schema")));
    add_assertion(result, "view",
                  string_is(member(document, "view"), "highlights"),
                  "highlights", value_or_null(member(document, "view")));
    const auto* availability = member(document, "availability");
    const bool available = string_is(availability, "live") ||
                           string_is(availability, "stale-cache");
    add_assertion(result, "availability", available, "live or stale-cache",
                  value_or_null(availability));
    const auto* securities = member_path(document, {"highlight_summary", "securities"});
    const auto* resolved = member_path(document, {"highlight_summary", "names_resolved"});
    const auto* type_count = member_path(document, {"highlight_summary", "type_count"});
    const bool coverage = securities && securities->is_number() &&
        securities->as_number() >= 250 && resolved && resolved->is_number() &&
        resolved->as_number() == securities->as_number() && type_count &&
        type_count->is_number() && type_count->as_number() >= 10;
    add_assertion(result, "highlight_coverage", coverage,
                  "at least 250 resolved securities and 10 types", coverage);
    const auto* records = member(document, "records");
    const Json* first = records && records->is_array() && records->size() >= 2
        ? &records->as_array()[0] : nullptr;
    const Json* second = records && records->is_array() && records->size() >= 2
        ? &records->as_array()[1] : nullptr;
    add_assertion(result, "records_present", first && second, "at least two rows",
                  records ? Json(static_cast<std::uint64_t>(records->size())) : Json(nullptr));
    const auto* security = first ? member(*first, "security") : nullptr;
    const auto* code = security ? member(*security, "code") : nullptr;
    const auto* name_resolved = security ? member(*security, "name_resolved") : nullptr;
    add_assertion(result, "security_identity",
                  security && security->is_object() && code && code->is_string() &&
                      code->as_string().size() == 6 && name_resolved &&
                      name_resolved->is_bool() && name_resolved->as_bool(),
                  "resolved six-digit security", value_or_null(security));
    const auto* first_count = first ? member(*first, "highlight_count") : nullptr;
    const auto* second_count = second ? member(*second, "highlight_count") : nullptr;
    add_assertion(result, "descending_highlight_count",
                  first_count && second_count && first_count->is_number() &&
                      second_count->is_number() &&
                      first_count->as_number() >= second_count->as_number(), true,
                  first_count && second_count && first_count->is_number() &&
                          second_count->is_number()
                      ? Json(first_count->as_number() >= second_count->as_number())
                      : Json(nullptr));
    const auto* type = first ? member(*first, "primary_highlight_type") : nullptr;
    const auto* detail = first ? member(*first, "highlight_detail") : nullptr;
    add_assertion(result, "highlight_semantics",
                  type && type->is_string() && !type->as_string().empty() &&
                      detail && detail->is_string() && !detail->as_string().empty(),
                  "non-empty type and detail", type && detail ? Json(true) : Json(false));
    const auto* raw = first ? member(*first, "raw") : nullptr;
    add_assertion(result, "raw_row_retained", raw && raw->is_object(), "object",
                  value_or_null(raw));
    const bool exact_source = source_exists(document, "list/func_ldph101_1.jsn");
    add_assertion(result, "exact_source", exact_source,
                  "list/func_ldph101_1.jsn", exact_source);
    const auto* sources = member(document, "sources");
    const Json* source = sources && sources->is_array() && !sources->as_array().empty()
        ? &sources->as_array().front() : nullptr;
    const auto* attempts = source ? member(*source, "attempts") : nullptr;
    const auto* stale = source ? member(*source, "stale") : nullptr;
    add_assertion(result, "source_health",
                  attempts && attempts->is_number() && attempts->as_number() >= 1 &&
                      stale && stale->is_bool(), true,
                  attempts && stale ? Json(true) : Json(false));
}

void validate_stock_intelligence_highlight(const Json& document, Json& result) {
    add_assertion(result, "schema",
                  string_is(member(document, "schema"),
                            "tdx-market-intelligence-native-v1"),
                  "tdx-market-intelligence-native-v1",
                  value_or_null(member(document, "schema")));
    add_assertion(result, "view", string_is(member(document, "view"), "security"),
                  "security", value_or_null(member(document, "view")));
    const auto* security = member(document, "security");
    add_assertion(result, "selected_security",
                  security && security->is_object() &&
                      string_is(member(*security, "market"), "sh") &&
                      string_is(member(*security, "code"), "600519"),
                  "SH600519", value_or_null(security));
    const auto* highlights = member(document, "highlights");
    const Json* row = highlights && highlights->is_array() && highlights->size() == 1
        ? &highlights->as_array().front() : nullptr;
    add_assertion(result, "selected_highlight", row != nullptr,
                  "one current highlight row",
                  highlights ? Json(static_cast<std::uint64_t>(highlights->size()))
                             : Json(nullptr));
    const auto* type = row ? member(*row, "primary_highlight_type") : nullptr;
    const auto* detail = row ? member(*row, "highlight_detail") : nullptr;
    add_assertion(result, "selected_highlight_semantics",
                  type && type->is_string() && !type->as_string().empty() &&
                      detail && detail->is_string() && !detail->as_string().empty(),
                  "non-empty type and detail", type && detail ? Json(true) : Json(false));
    const bool exact_source = source_exists(document, "list/func_ldph101_1.jsn");
    add_assertion(result, "exact_source", exact_source,
                  "list/func_ldph101_1.jsn", exact_source);
}

struct IntelligenceContract {
    std::string_view id;
    IntelligenceContractValidator validate;
};

constexpr std::array<IntelligenceContract, 4> intelligence_contracts{{
    {"intelligence-value-attention-live", validate_intelligence_value_attention},
    {"intelligence-discredited-live", validate_intelligence_discredited},
    {"intelligence-highlights-live", validate_intelligence_highlights},
    {"stock-intelligence-highlight-live", validate_stock_intelligence_highlight},
}};

constexpr bool unique_contract_ids() {
    for (std::size_t left = 0; left < intelligence_contracts.size(); ++left)
        for (std::size_t right = left + 1; right < intelligence_contracts.size(); ++right)
            if (intelligence_contracts[left].id == intelligence_contracts[right].id) return false;
    return true;
}

static_assert(unique_contract_ids());

}  // namespace

bool validate_market_research_intelligence_contract(const std::string& contract_id,
    const Json& document, const Json& context, Json& result) {
    (void)context;
    for (const auto& contract : intelligence_contracts) {
        if (contract.id != contract_id) continue;
        contract.validate(document, result);
        return true;
    }
    return false;
}

}  // namespace tdx::recon_contract_detail