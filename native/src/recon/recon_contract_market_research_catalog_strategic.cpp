#include "recon_contract_market_research_internal.hpp"

#include "tdx/common.hpp"

#include <cstdint>

namespace tdx::recon_contract_detail {
namespace {

// Resource presence flags scanned once from "sources"; both views assert on
// different subsets of them.
struct StrategicSourceFlags {
    const Json* sources{};
    bool g5{};
    bool defense{};
    bool defense_groups{};
    bool internet{};
    bool detail{};
};

// Shared preamble for the strategic-themes catalog and detail views.
StrategicSourceFlags assert_strategic_common(const Json& document, Json& result,
                                            bool detail_mode) {
    add_assertion(result, "schema",
                  string_is(member(document, "schema"),
                            "tdx-market-strategic-themes-native-v1"),
                  "tdx-market-strategic-themes-native-v1",
                  value_or_null(member(document, "schema")));
    add_assertion(result, "view",
                  string_is(member(document, "view"), detail_mode ? "theme" : "catalog"),
                  detail_mode ? "theme" : "catalog",
                  value_or_null(member(document, "view")));
    const auto* availability = member(document, "availability");
    const bool available = string_is(availability, "live") ||
                           string_is(availability, "stale-cache");
    add_assertion(result, "availability", available, "live or stale-cache",
                  value_or_null(availability));
    const auto* category_count = member_path(document, {"summary", "category_count"});
    const auto* theme_count = member_path(document, {"summary", "theme_count"});
    const auto* relations = member_path(
        document, {"summary", "category_theme_memberships"});
    const auto* mismatches = member_path(
        document, {"summary", "count_mismatch_count"});
    const auto* duplicates = member_path(
        document, {"summary", "duplicate_master_memberships"});
    add_assertion(result, "catalog_totals",
                  category_count && category_count->is_number() &&
                      category_count->as_number() == 26 &&
                      theme_count && theme_count->is_number() &&
                      theme_count->as_number() == 598 &&
                      relations && relations->is_number() &&
                      relations->as_number() == 655,
                  "26 categories, 598 themes, 655 category-theme relations",
                  value_or_null(theme_count));
    add_assertion(result, "master_consistency",
                  mismatches && mismatches->is_number() &&
                      mismatches->as_number() == 0 &&
                      duplicates && duplicates->is_number() &&
                      duplicates->as_number() == 9,
                  "zero count mismatches and nine duplicate memberships",
                  value_or_null(mismatches));
    const auto* categories = member(document, "categories");
    StrategicSourceFlags flags;
    flags.sources = member(document, "sources");
    if (flags.sources && flags.sources->is_array()) {
        for (const auto& source : flags.sources->as_array()) {
            const auto* resource = member(source, "resource");
            if (!resource || !resource->is_string()) continue;
            flags.g5 |= resource->as_string() == "list/func_5G101_1.jsn";
            flags.defense |= resource->as_string() == "list/func_gfjg101_1.jsn";
            flags.defense_groups |=
                resource->as_string() == "list/func_gfjg102_1.jsn";
            flags.internet |= resource->as_string() == "list/func_hlw101_1.jsn";
            flags.detail |= resource->as_string() == "zttzty/657.jsn";
        }
    }
    add_assertion(result, "category_hierarchy",
                  categories && categories->is_array() && categories->size() == 26,
                  "all 26 categories returned",
                  categories ? Json(static_cast<std::uint64_t>(categories->size()))
                             : Json(nullptr));
    return flags;
}

}  // namespace

void validate_strategic_themes_catalog_contract(const Json& document,
                                                Json& result) {
    const auto flags = assert_strategic_common(document, result, false);
    const auto* sources = flags.sources;
    const auto* themes = member(document, "themes");
    bool sorted = themes && themes->is_array() && themes->size() == 598;
    if (sorted) {
        for (std::size_t i = 1; i < themes->size(); ++i) {
            const auto& previous = themes->as_array()[i - 1];
            const auto& current = themes->as_array()[i];
            const auto* previous_name = member(previous, "name");
            const auto* current_name = member(current, "name");
            if (!previous_name || !current_name || !previous_name->is_string() ||
                !current_name->is_string() ||
                previous_name->as_string() > current_name->as_string()) {
                sorted = false;
                break;
            }
        }
    }
    const Json* first = themes && themes->is_array() && themes->size()
        ? &themes->as_array()[0] : nullptr;
    add_assertion(result, "theme_catalog", sorted,
                  "598 themes sorted by name",
                  themes ? Json(static_cast<std::uint64_t>(themes->size()))
                         : Json(nullptr));
    add_assertion(result, "theme_semantics",
                  first && member(*first, "theme_id") && member(*first, "name") &&
                      member(*first, "categories") &&
                      member(*first, "master_member_count") &&
                      member(*first, "detail_resource"),
                  "id/name/categories/member-count/detail-resource",
                  first ? *first : Json(nullptr));
    add_assertion(result, "master_sources",
                  sources && sources->is_array() && sources->size() == 26 &&
                      flags.g5 && flags.defense && flags.defense_groups &&
                      flags.internet,
                  "26 sources including 5G6G, defense groups and Internet+",
                  sources ? Json(static_cast<std::uint64_t>(sources->size()))
                          : Json(nullptr));
}

void validate_strategic_theme_detail_contract(const Json& document, Json& result) {
    const auto flags = assert_strategic_common(document, result, true);
    const auto* sources = flags.sources;
    const auto* selected = member(document, "selected_theme");
    const auto* members = member(document, "members");
    const auto* details = member(document, "details");
    add_assertion(result, "selected_theme",
                  selected && string_is(member(*selected, "theme_id"), "657") &&
                      string_is(member(*selected, "name"), "5G概念") &&
                      bool_is(member(*selected, "detail_available"), true) &&
                      member(*selected, "master_member_count") &&
                      member(*selected, "member_count"),
                  "theme 657 / 5G概念 with live detail",
                  value_or_null(selected));
    const auto* master_count = selected
        ? member(*selected, "master_member_count") : nullptr;
    const auto* member_count = selected ? member(*selected, "member_count") : nullptr;
    add_assertion(result, "detail_coverage",
                  master_count && master_count->is_number() &&
                      master_count->as_number() >= 440 &&
                      member_count && member_count->is_number() &&
                      member_count->as_number() >= 450 &&
                      members && members->is_array() && members->size() >= 450 &&
                      details && details->is_array() &&
                      details->size() == members->size(),
                  "master >= 440, detail/member >= 450 and aligned",
                  member_count ? *member_count : Json(nullptr));
    std::size_t resolved = 0, described = 0, raw_rows = 0;
    if (details && details->is_array()) {
        for (const auto& row : details->as_array()) {
            const auto* security = member(row, "security");
            if (security && bool_is(member(*security, "name_resolved"), true)) ++resolved;
            const auto* logic = member(row, "logic");
            if (logic && logic->is_string() && !logic->as_string().empty()) ++described;
            const auto* raw = member(row, "raw");
            if (raw && raw->is_object()) ++raw_rows;
        }
    }
    add_assertion(result, "detail_semantics",
                  resolved >= 440 && described >= 400 && raw_rows >= 450,
                  "resolved names, inclusion logic and auditable raw rows",
                  Json(static_cast<std::uint64_t>(described)));
    add_assertion(result, "detail_sources",
                  sources && sources->is_array() && sources->size() == 27 &&
                      flags.g5 && flags.defense && flags.defense_groups &&
                      flags.internet && flags.detail,
                  "26 masters plus zttzty/657.jsn",
                  sources ? Json(static_cast<std::uint64_t>(sources->size()))
                          : Json(nullptr));
}

}  // namespace tdx::recon_contract_detail
