#include "recon_contract_formula_foundation_internal.hpp"

#include "recon_contract_internal.hpp"

#include <array>
#include <string_view>

namespace tdx::recon_contract_detail {
namespace {

constexpr std::array<std::string_view, 28> kRequiredFeatures{{
    "market fund-analytics", "market abnormal-details", "market roadshows",
    "market speed", "market watch", "recon api-contracts", "recon cloud-variants",
    "recon jsn-variants", "recon jsn-discovery", "jsn candidates", "formulas watch",
    "formulas strategy", "formulas context-template", "formulas icons",
    "formulas extern-signals", "formulas extdata-user", "pool evaluate",
    "market panorama", "market institution-analysis", "market thematic-opportunities",
    "market stock-connect", "market limit-review", "market session-turnover",
    "market convertible-bonds", "market block-rotation", "market limit-ladder",
    "market threshold-stocks", "market tender-offers",
}};

struct FeatureApiMapping {
    std::string_view command;
    std::string_view endpoint;
};

constexpr std::array<FeatureApiMapping, 3> kFeatureApiMappings{{
    {"formulas context-template", "/api/v1/formulas/context-template"},
    {"formulas icons", "/api/v1/formulas/icons"},
    {"pool evaluate", "/api/v1/pools/evaluate"},
}};

}  // namespace

void validate_service_health_contract(const Json& document, Json& result) {
    add_assertion(result, "ok", bool_is(member(document, "ok"), true), true,
                  value_or_null(member(document, "ok")));
    add_assertion(result, "native_cpp",
                  bool_is(member(document, "native_cpp"), true), true,
                  value_or_null(member(document, "native_cpp")));
    add_assertion(result, "python_runtime",
                  bool_is(member(document, "python_runtime"), false), false,
                  value_or_null(member(document, "python_runtime")));
    add_assertion(result, "service",
                  string_is(member(document, "service"), "tdx-tool"), "tdx-tool",
                  value_or_null(member(document, "service")));
}

void validate_feature_catalog_contract(const Json& document, Json& result) {
    const auto* features = member(document, "features");
    const auto* count = member(document, "count");
    const bool shape = features && features->is_array() && count && count->is_number() &&
                       static_cast<std::size_t>(count->as_number()) == features->size();
    add_assertion(result, "count_matches_features", shape, true, shape);
    for (const auto command : kRequiredFeatures) {
        const bool found = feature_exists(document, command);
        add_assertion(result, std::string("feature:") + std::string(command),
                      found, true, found);
    }
    for (const auto& mapping : kFeatureApiMappings) {
        const bool mapped = feature_api_is(document, mapping.command, mapping.endpoint);
        add_assertion(result, "feature_api:" + std::string(mapping.command),
                      mapped, std::string(mapping.endpoint),
                      mapped ? Json(std::string(mapping.endpoint)) : Json(nullptr));
    }
}

}  // namespace tdx::recon_contract_detail
