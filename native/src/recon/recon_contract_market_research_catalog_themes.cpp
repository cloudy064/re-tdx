#include "recon_contract_market_research_internal.hpp"

#include "tdx/common.hpp"

#include <cstdint>

namespace tdx::recon_contract_detail {
namespace {

// Shared preamble for the theme-library catalog and detail views.
void assert_theme_library_common(const Json& document, Json& result,
                                 bool detail_mode) {
    add_assertion(result, "schema",
                  string_is(member(document, "schema"),
                            "tdx-market-theme-library-native-v1"),
                  "tdx-market-theme-library-native-v1",
                  value_or_null(member(document, "schema")));
    add_assertion(result, "view",
                  string_is(member(document, "view"), detail_mode ? "theme" : "catalog"),
                  detail_mode ? "theme" : "catalog",
                  value_or_null(member(document, "view")));
    const auto* availability = member(document, "availability");
    add_assertion(result, "availability",
                  string_is(availability, "live") ||
                      string_is(availability, "stale-cache"),
                  "live or stale-cache", value_or_null(availability));
    const auto* source_count = member_path(document, {"summary", "source_count"});
    const auto* snapshots = member_path(document, {"summary", "snapshot_count"});
    const auto* unique_ids = member_path(document, {"summary", "unique_theme_id_count"});
    const auto* mismatches = member_path(document, {"summary", "count_mismatch_count"});
    add_assertion(result, "catalog_totals",
                  source_count && source_count->is_number() &&
                      source_count->as_number() == 5 &&
                      snapshots && snapshots->is_number() &&
                       snapshots->as_number() >= 1102 &&
                       unique_ids && unique_ids->is_number() &&
                       unique_ids->as_number() >= 927,
                   "5 sources, at least 1102 snapshots and 927 unique theme ids",
                  value_or_null(snapshots));
    add_assertion(result, "master_consistency",
                  mismatches && mismatches->is_number() &&
                      mismatches->as_number() == 0,
                  0, value_or_null(mismatches));
}

}  // namespace

void validate_theme_library_catalog_contract(const Json& document, Json& result) {
    assert_theme_library_common(document, result, false);
    const auto* themes = member(document, "themes");
    bool latest = false;
    if (themes && themes->is_array())
        for (const auto& theme : themes->as_array())
            latest |= string_is(member(theme, "theme_id"), "3137") &&
                      string_is(member(theme, "name"), "实验猴") &&
                      string_is(member(theme, "created_date"), "20260716") &&
                      number_is(member(theme, "member_count"), 4);
    add_assertion(result, "latest_theme", latest,
                  "3137 实验猴 / 20260716 / 4 members", latest);
    add_assertion(result, "general_count",
                  themes && themes->is_array() && themes->size() >= 850,
                  "at least 850", themes ? Json(static_cast<std::uint64_t>(themes->size()))
                              : Json(nullptr));
}

void validate_theme_library_detail_contract(const Json& document, Json& result) {
    assert_theme_library_common(document, result, true);
    const auto* selected = member(document, "selected_theme");
    const auto* members = member(document, "members");
    const auto* details = member(document, "details");
    const auto* chart = member(document, "chart");
    add_assertion(result, "selected_theme",
                  selected && selected->is_object() &&
                      string_is(member(*selected, "theme_id"), "3137") &&
                      string_is(member(*selected, "name"), "实验猴") &&
                      number_is(member(*selected, "active_member_count"), 4),
                  "3137 实验猴 / 4 active members",
                  value_or_null(selected));
    std::size_t described = 0;
    if (details && details->is_array())
        for (const auto& row : details->as_array()) {
            const auto* description = member(row, "description");
            if (description && description->is_string() &&
                !description->as_string().empty()) ++described;
        }
    add_assertion(result, "member_evidence",
                  members && members->is_array() && members->size() == 4 &&
                      details && details->is_array() && details->size() == 4 &&
                      described == 4,
                  "4 members with 4 inclusion descriptions",
                  Json(static_cast<std::uint64_t>(described)));
    bool base = false;
    if (chart && chart->is_array() && chart->size() >= 18)
        for (const auto& point : chart->as_array())
            base |= string_is(member(point, "date"), "20260715") &&
                    number_is(member(point, "value"), 1000);
    add_assertion(result, "theme_index", base,
                  "at least 18 points including 20260715=1000",
                  chart ? Json(static_cast<std::uint64_t>(chart->size()))
                        : Json(nullptr));
}

}  // namespace tdx::recon_contract_detail
