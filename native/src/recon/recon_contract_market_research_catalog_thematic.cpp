#include "recon_contract_market_research_internal.hpp"

#include "tdx/common.hpp"

#include <cstdint>
#include <string>
#include <string_view>

namespace tdx::recon_contract_detail {

void assert_thematic_common(const Json& document, Json& result,
                            std::string_view expected_view) {
    add_assertion(result, "schema",
                  string_is(member(document, "schema"),
                            "tdx-market-thematic-opportunities-native-v1"),
                  "tdx-market-thematic-opportunities-native-v1",
                  value_or_null(member(document, "schema")));
    add_assertion(result, "view",
                  string_is(member(document, "view"), expected_view),
                  std::string(expected_view), value_or_null(member(document, "view")));
    const auto* availability = member(document, "availability");
    add_assertion(result, "availability",
                  string_is(availability, "live") ||
                      string_is(availability, "stale-cache"),
                  "live or stale-cache", value_or_null(availability));
    const auto* groups_total = member_path(document, {"summary", "group_count"});
    const auto* industry_total = member_path(
        document, {"summary", "industry_group_count"});
    const auto* region_total = member_path(
        document, {"summary", "region_group_count"});
    const auto* memberships = member_path(
        document, {"summary", "group_memberships"});
    const auto* securities = member_path(document, {"summary", "security_count"});
    const auto* legacy_total = member_path(
        document, {"summary", "legacy_client_theme_count"});
    const auto* semantic_mismatches = member_path(
        document, {"summary", "semantic_mismatch_count"});
    add_assertion(result, "opportunity_population",
                  number_is(groups_total, 15) && number_is(industry_total, 6) &&
                      number_is(region_total, 8) && number_is(legacy_total, 1) &&
                      number_is(semantic_mismatches, 1) && memberships &&
                      memberships->is_number() && memberships->as_number() >= 700 &&
                      securities && securities->is_number() &&
                      securities->as_number() >= 500,
                  "15 groups (6 industry + 8 region + 1 legacy), one semantic mismatch, >=700 memberships and >=500 securities",
                  value_or_null(member(document, "summary")));
    bool master_sources = true;
    for (const auto* resource : {"list/func_ydyl101_1.jsn",
                                 "list/func_ydyl102_1.jsn",
                                 "list/func_rdhs101_1.jsn",
                                 "list/func_rdhs102_1.jsn",
                                 "list/func_xnxs101_1.jsn"})
        master_sources = master_sources && source_exists(document, resource);
    add_assertion(result, "master_sources", master_sources, true, master_sources);
}

void validate_thematic_opportunities_catalog_contract(const Json& document,
                                                      Json& result) {
    assert_thematic_common(document, result, "catalog");
    const auto* groups = member(document, "groups");
    bool shape = groups && groups->is_array() && groups->size() == 15;
    std::uint64_t industry = 0, region = 0, legacy = 0;
    if (shape) for (const auto& group : groups->as_array()) {
        const auto* type = member(group, "type");
        if (string_is(type, "industry")) ++industry;
        else if (string_is(type, "region")) ++region;
        else if (string_is(type, "legacy-client-theme")) ++legacy;
        else shape = false;
        shape = shape && member(group, "group_id") &&
            member(group, "name") && member(group, "member_count") &&
            member(group, "detail_resource");
    }
    add_assertion(result, "group_hierarchy",
                  shape && industry == 6 && region == 8 && legacy == 1,
                  "15 typed groups with detail resources", shape);
}

void validate_thematic_opportunity_detail_contract(const Json& document,
                                                   Json& result) {
    assert_thematic_common(document, result, "group");
    const auto* selected = member(document, "selected_group");
    add_assertion(result, "selected_group",
                  selected && string_is(member(*selected, "group_id"), "hy57") &&
                      string_is(member(*selected, "name"), "IGCC技术") &&
                      string_is(member(*selected, "type"), "industry") &&
                      bool_is(member(*selected, "detail_available"), true),
                  "hy57 / IGCC技术 / industry with detail",
                  value_or_null(selected));
    const auto* details = member(document, "details");
    const Json* first = details && details->is_array() && details->size() >= 8
        ? &details->as_array().front() : nullptr;
    const auto* security = first ? member(*first, "security") : nullptr;
    const auto* logic = first ? member(*first, "logic") : nullptr;
    const auto* three_month = first
        ? member(*first, "three_month_adjusted_close") : nullptr;
    const auto* year_start = first
        ? member(*first, "year_start_adjusted_close") : nullptr;
    add_assertion(result, "detail_semantics",
                  first && security && security->is_object() &&
                      bool_is(member(*security, "name_resolved"), true) &&
                      logic && logic->is_string() && !logic->as_string().empty() &&
                      three_month && three_month->is_number() &&
                      year_start && year_start->is_number() &&
                      member(*first, "raw") && member(*first, "raw")->is_object(),
                  "resolved security, logic, two adjusted reference closes and raw row",
                  first ? *first : Json(nullptr));
    const bool detail_source = source_exists(document, "ydyl1/hy57.jsn");
    add_assertion(result, "detail_source", detail_source,
                  "ydyl1/hy57.jsn", detail_source);
}

void validate_thematic_legacy_client_theme_contract(const Json& document,
                                                    Json& result) {
    assert_thematic_common(document, result, "group");
    const auto* selected = member(document, "selected_group");
    const bool selected_ok = selected &&
        string_is(member(*selected, "group_id"), "299") &&
        string_is(member(*selected, "name"), "快中子反应堆") &&
        string_is(member(*selected, "category"), "内容应用") &&
        string_is(member(*selected, "type"), "legacy-client-theme") &&
        string_is(member(*selected, "page_declared_name"), "虚拟现实") &&
        bool_is(member(*selected, "semantic_mismatch"), true) &&
        bool_is(member(*selected, "detail_available"), true) &&
        string_is(member(*selected, "detail_resource"), "xnxs/299.jsn");
    add_assertion(result, "legacy_theme_identity", selected_ok,
                  "299 fast-neutron reactor with explicit virtual-reality page conflict",
                  value_or_null(selected));
    const auto* details = member(document, "details");
    bool detail_ok = details && details->is_array() && details->size() == 3;
    if (detail_ok) for (const auto& row : details->as_array()) {
        const auto* security = member(row, "security");
        const auto* logic = member(row, "logic");
        const auto* close3 = member(row, "reference_close_3d");
        const auto* close5 = member(row, "reference_close_5d");
        const auto* close20 = member(row, "reference_close_20d");
        const auto* close3m = member(row, "three_month_adjusted_close");
        detail_ok = detail_ok && security && security->is_object() &&
            bool_is(member(*security, "name_resolved"), true) &&
            logic && logic->is_string() && !logic->as_string().empty() &&
            close3 && close3->is_number() && close5 && close5->is_number() &&
            close20 && close20->is_number() && close3m && close3m->is_number() &&
            string_is(member(row, "detail_variant"), "legacy-client-theme") &&
            member(row, "raw") && member(row, "raw")->is_object();
    }
    add_assertion(result, "legacy_theme_details", detail_ok,
                  "three resolved securities with 3/5/20d/3m reference closes and logic",
                  detail_ok);
    const bool detail_source = source_exists(document, "xnxs/299.jsn");
    add_assertion(result, "legacy_theme_detail_source", detail_source,
                  "xnxs/299.jsn", detail_source);
}

}  // namespace tdx::recon_contract_detail
