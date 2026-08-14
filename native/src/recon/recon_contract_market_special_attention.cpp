#include "recon_contract_market_corporate_internal.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include <vector>

namespace tdx::recon_contract_detail {

void validate_market_special_attention_contract(
    const Json& document, Json& result) {
    add_assertion(result, "schema",
                  string_is(member(document, "schema"),
                            "tdx-market-special-attention-native-v1"),
                  "tdx-market-special-attention-native-v1",
                  value_or_null(member(document, "schema")));
    add_assertion(result, "view_mode",
                  string_is(member(document, "view"), "all") &&
                      string_is(member(document, "mode"), "catalog") &&
                      string_is(member(document, "availability"), "live"),
                  "all/catalog/live", value_or_null(member(document, "mode")));
    const auto* dispersion = member_path(document, {"summary", "equity_dispersion"});
    const auto* st = member_path(document, {"summary", "st_risk"});
    const auto* removal = member_path(document, {"summary", "star_cap_removal"});
    const auto* investigations = member_path(document, {"summary", "investigations"});
    const auto* goodwill = member_path(document, {"summary", "goodwill_risk"});
    const auto* matched = member(document, "match_count");
    const bool population = dispersion && dispersion->is_number() &&
            dispersion->as_number() >= 160 &&
        st && st->is_number() && st->as_number() >= 130 &&
        removal && removal->is_number() && removal->as_number() >= 70 &&
        investigations && investigations->is_number() &&
            investigations->as_number() >= 450 &&
        goodwill && goodwill->is_number() && goodwill->as_number() >= 290 &&
        matched && matched->is_number();
    add_assertion(result, "five_family_population", population,
                  "160 dispersion, 130 ST, 70 removal, 450 investigation and 290 goodwill rows",
                  population);
    const bool reconciliation = population &&
        dispersion->as_number() + st->as_number() + removal->as_number() +
            investigations->as_number() + goodwill->as_number() ==
                matched->as_number();
    add_assertion(result, "summary_reconciliation", reconciliation,
                  "five family counts equal match_count", reconciliation);

    bool households = false, removal_units = false, goodwill_formula = false;
    bool investigation_identity = false, raw_audit = true;
    const auto* records = member(document, "records");
    if (records && records->is_array()) {
        for (const auto& row : records->as_array()) {
            const auto* raw = member(row, "raw");
            raw_audit = raw_audit && raw && raw->is_object() &&
                member(row, "source_resource") &&
                member(row, "source_resource")->is_string();
            if (!raw || !raw->is_object()) continue;
            if (string_is(member(row, "kind"), "st-risk")) {
                const auto source = numeric_value(member(*raw, "zxgdhs"));
                const auto normalized = numeric_value(
                    member(row, "shareholder_households_10k"));
                households = households || (source && normalized &&
                    std::abs(*normalized - *source * 1e-4) < 0.000001);
            } else if (string_is(member(row, "kind"), "star-cap-removal")) {
                const auto source = numeric_value(member(*raw, "jlr1"));
                const auto normalized = numeric_value(
                    member(row, "current_net_profit_yuan"));
                removal_units = removal_units || (source && normalized &&
                    std::abs(*normalized - *source * 1e4) < 0.01);
            } else if (string_is(member(row, "kind"), "goodwill-risk")) {
                const auto current = numeric_value(member(*raw, "sy1"));
                const auto prior = numeric_value(member(*raw, "sy2"));
                const auto change = numeric_value(member(row, "goodwill_change_yuan"));
                goodwill_formula = goodwill_formula || (current && prior && change &&
                    std::abs(*change - (*current - *prior)) < 0.01);
            } else if (string_is(member(row, "kind"), "investigations")) {
                const auto* security = member(row, "security");
                investigation_identity = investigation_identity ||
                    (security && security->is_object() &&
                     string_is(member(*security, "code"),
                               member(*raw, "$ZQDM1") &&
                                       member(*raw, "$ZQDM1")->is_string()
                                   ? member(*raw, "$ZQDM1")->as_string()
                                   : std::string{}));
            }
        }
    } else raw_audit = false;
    add_assertion(result, "display_unit_boundaries",
                  households && removal_units,
                  "households converted to 万户 and removal profits from 万元 to yuan",
                  households && removal_units);
    add_assertion(result, "goodwill_formula", goodwill_formula,
                  "goodwill change equals current minus prior", goodwill_formula);
    add_assertion(result, "investigation_security_identity", investigation_identity,
                  "$ZQDM1/$SC1 identify the investigated security",
                  investigation_identity);
    add_assertion(result, "raw_audit", raw_audit,
                  "all normalized rows retain source resource and raw fields", raw_audit);

    const std::vector<std::string> required_sources{
        "list/func_tbgz102_1.jsn", "list/func_tbgz103_1.jsn",
        "list/func_tbgz104_1.jsn", "list/func_tbgz106_1.jsn",
        "list/func_tbgz110_1.jsn"};
    const auto* sources = member(document, "sources");
    bool exact_sources = sources && sources->is_array() &&
        sources->size() == required_sources.size();
    bool local_first = exact_sources;
    for (const auto& source : required_sources)
        exact_sources = exact_sources && source_exists(document, source);
    if (sources && sources->is_array())
        for (const auto& source : sources->as_array()) {
            const auto* endpoint = member(source, "endpoint");
            local_first = local_first && endpoint && endpoint->is_string() &&
                endpoint->as_string().rfind("local-jsn:", 0) == 0;
        }
    else local_first = false;
    add_assertion(result, "exact_sources", exact_sources,
                  static_cast<std::uint64_t>(required_sources.size()), exact_sources);
    add_assertion(result, "local_first_boundary", local_first,
                  "normal API query reads the mirrored JSN files", local_first);
}

}  // namespace tdx::recon_contract_detail
