#include "recon_contract_market_corporate_internal.hpp"

#include "tdx/common.hpp"

#include <set>
#include <string>

namespace tdx::recon_contract_detail {

void validate_jsn_discovery_contract(const Json& document, const Json&,
                                     Json& result) {
    add_assertion(result, "schema",
                  string_is(member(document, "schema"),
                            "tdx-jsn-discovery-native-v1"),
                  "tdx-jsn-discovery-native-v1",
                  value_or_null(member(document, "schema")));
    const auto* current = member_path(document, {"summary", "current_file_count"});
    const bool populated = current && current->is_number() &&
                           current->as_number() >= 300;
    add_assertion(result, "downloaded_inventory", populated, "number >= 300",
                  value_or_null(current));
    add_assertion(result, "parse_errors_zero",
                  number_is(member_path(document,
                      {"summary", "parse_error_file_count"}), 0), 0,
                  value_or_null(member_path(document,
                      {"summary", "parse_error_file_count"})));
    add_assertion(result, "unrecognized_zero",
                  number_is(member_path(document,
                      {"summary", "unrecognized_file_count"}), 0), 0,
                  value_or_null(member_path(document,
                      {"summary", "unrecognized_file_count"})));
    const auto* files = member(document, "files");
    bool profiles = false;
    if (files && files->is_array()) {
        for (const auto& file : files->as_array()) {
            const auto* value = member(file, "column_profiles");
            if (value && value->is_object() && !value->as_object().empty()) {
                profiles = true;
                break;
            }
        }
    }
    add_assertion(result, "field_profiles", profiles, true, profiles);
    const auto* available = member_path(document, {"baseline", "available"});
    add_assertion(result, "baseline_state_boolean",
                  available && available->is_bool(), "boolean",
                  value_or_null(available));
}

void validate_jsn_candidates_contract(const Json& document, const Json&,
                                      Json& result) {
    add_assertion(result, "schema",
                  string_is(member(document, "schema"),
                            "tdx-jsn-candidates-native-v1"),
                  "tdx-jsn-candidates-native-v1",
                  value_or_null(member(document, "schema")));
    add_assertion(result, "offline_api",
                  bool_is(member(document, "network_used"), false), false,
                  value_or_null(member(document, "network_used")));
    add_assertion(result, "one_dynamic_template",
                  number_is(member_path(document,
                      {"summary", "dynamic_template_count"}), 1), 1,
                  value_or_null(member_path(document,
                      {"summary", "dynamic_template_count"})));
    const auto* count = member_path(document, {"summary", "candidate_count"});
    const auto* missing = member_path(document,
        {"summary", "unique_missing_resource_count"});
    const bool populated = count && count->is_number() &&
        count->as_number() >= 1000 && missing && missing->is_number() &&
        missing->as_number() >= 1000;
    add_assertion(result, "gqzy_candidate_population", populated,
                  "at least 1000 page-scoped candidates and local misses",
                  value_or_null(member(document, "summary")));
    std::set<std::string> master_resources;
    const auto* templates = member(document, "templates");
    if (templates && templates->is_array())
        for (const auto& item : templates->as_array()) {
            if (!string_is(member(item, "template"),
                           "gqzy/$$$SC$$$$$ZQDM$$.jsn")) continue;
            const auto* masters = member(item, "master_resources");
            if (masters && masters->is_array())
                for (const auto& master : masters->as_array())
                    if (master.is_string()) master_resources.insert(master.as_string());
        }
    const std::set<std::string> expected_masters{
        "list/func_gqzy101_1.jsn", "list/func_gqzy102_1.jsn",
        "list/func_gqzy103_1.jsn", "list/func_gqzy109_1.jsn"};
    Json actual_masters = Json::array();
    for (const auto& master : master_resources) actual_masters.push_back(master);
    add_assertion(result, "page_scoped_refunit_masters",
                  master_resources == expected_masters,
                  "four GQZY masters linked by CFG family/client pages",
                  std::move(actual_masters));
    const auto* candidates = member(document, "candidates");
    bool evidence = candidates && candidates->is_array() && candidates->size() == 5;
    if (evidence) {
        for (const auto& candidate : candidates->as_array()) {
            const auto* resource = member(candidate, "resource");
            const auto* sources = member(candidate, "source_resources");
            if (!resource || !resource->is_string() ||
                resource->as_string().rfind("gqzy/", 0) != 0 ||
                !bool_is(member(candidate, "probe_eligible"), true) ||
                !sources || !sources->is_array() || sources->size() == 0) {
                evidence = false;
                break;
            }
        }
    }
    add_assertion(result, "typed_refunit_evidence", evidence, true, evidence);
}

void validate_cloud_variants_fixed_contract(const Json& document, const Json&,
                                            Json& result) {
    add_assertion(result, "schema",
                  string_is(member(document, "schema"),
                            "tdx-cloud-variant-coverage-native-v1"),
                  "tdx-cloud-variant-coverage-native-v1",
                  value_or_null(member(document, "schema")));
    add_assertion(result, "fully_fixed",
                  bool_is(member_path(document, {"summary", "fully_fixed"}), true),
                  true, value_or_null(member_path(document,
                      {"summary", "fully_fixed"})));
    add_assertion(result, "generic_only_zero",
                  number_is(member_path(document,
                      {"summary", "generic_only_variant_count"}), 0), 0,
                  value_or_null(member_path(document,
                      {"summary", "generic_only_variant_count"})));
    add_assertion(result, "parse_errors_zero",
                  number_is(member_path(document,
                      {"summary", "parse_error_count"}), 0), 0,
                  value_or_null(member_path(document,
                      {"summary", "parse_error_count"})));
    for (const auto& transport : {std::string("tqlex"), std::string("pbrpc")}) {
        const auto* total = member_path(document,
            {"summary", transport + "_request_id_count"});
        const auto* fixed = member_path(document,
            {"summary", "fixed_" + transport + "_request_id_count"});
        const bool complete = total && fixed && total->is_number() && fixed->is_number() &&
                              total->as_number() > 0 &&
                              total->as_number() == fixed->as_number();
        add_assertion(result, transport + "_request_ids_fixed", complete,
                      value_or_null(total), value_or_null(fixed));
    }
}

void validate_report_cache_default_contract(const Json& document, const Json&,
                                            Json& result) {
    const auto* requested = member_path(document, {"parameters", "requested_report_date"});
    const auto* resolved = member_path(document, {"parameters", "resolved_report_date"});
    const auto* fallback = member_path(document,
        {"parameters", "latest_report_fallback_used"});
    add_assertion(result, "requested_report_date",
                  requested && requested->is_string() && !requested->as_string().empty(),
                  "non-empty date", value_or_null(requested));
    add_assertion(result, "resolved_report_date",
                  resolved && resolved->is_string() && !resolved->as_string().empty(),
                  "non-empty date", value_or_null(resolved));
    add_assertion(result, "fallback_boolean", fallback && fallback->is_bool(),
                  "boolean", value_or_null(fallback));
}

void validate_report_cache_explicit_contract(const Json& document,
                                             const Json& context, Json& result) {
    const auto* expected = member(context, "requested_report_date");
    const auto* requested = member_path(document, {"parameters", "requested_report_date"});
    const auto* resolved = member_path(document, {"parameters", "resolved_report_date"});
    const bool same_requested = expected && expected->is_string() && requested &&
        requested->is_string() && requested->as_string() == expected->as_string();
    const bool same_resolved = expected && expected->is_string() && resolved &&
        resolved->is_string() && resolved->as_string() == expected->as_string();
    add_assertion(result, "requested_date_preserved", same_requested,
                  value_or_null(expected), value_or_null(requested));
    add_assertion(result, "resolved_date_preserved", same_resolved,
                  value_or_null(expected), value_or_null(resolved));
    add_assertion(result, "fallback_disabled",
                  bool_is(member_path(document,
                      {"parameters", "latest_report_fallback_used"}), false), false,
                  value_or_null(member_path(document,
                      {"parameters", "latest_report_fallback_used"})));
}

}  // namespace tdx::recon_contract_detail
