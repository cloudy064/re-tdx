#include "recon_contract_formula_foundation_internal.hpp"

#include "recon_contract_internal.hpp"
#include "tdx/common.hpp"
#include "tdx/registry.hpp"

#include <array>
#include <string_view>

namespace tdx::recon_contract_detail {
namespace {

constexpr std::array<std::string_view, 35> kRequiredPaths{{
    "/api/v1/health", "/api/v1/market/fund-analytics",
    "/api/v1/market/abnormal-details", "/api/v1/market/funds",
    "/api/v1/market/roadshows", "/api/v1/market/speed",
    "/api/v1/market/stream", "/api/v1/cloud/variants",
    "/api/v1/jsn/variants", "/api/v1/jsn/discovery",
    "/api/v1/jsn/candidates", "/api/v1/market/panorama",
    "/api/v1/market/institution-analysis", "/api/v1/market/stock-connect",
    "/api/v1/market/limit-review", "/api/v1/market/session-turnover",
    "/api/v1/market/convertible-bonds", "/api/v1/market/thematic-opportunities",
    "/api/v1/market/block-rotation", "/api/v1/market/limit-ladder",
    "/api/v1/market/threshold-stocks", "/api/v1/market/tender-offers",
    "/api/v1/formulas/audit", "/api/v1/formulas/icons",
    "/api/v1/formulas/drawicon-strip.png", "/api/v1/formulas/drawicon-strip.bmp",
    "/api/v1/formulas/cloud-calc", "/api/v1/formulas/cloud-calc/template",
    "/api/v1/formulas/cloud-calc/batch", "/api/v1/tqlex/query",
    "/api/v1/pbrpc/query", "/api/v1/pools/evaluate",
    "/api/v1/level2/build", "/api/v1/level2/decode",
    "/api/v1/level2/project",
}};

constexpr std::array<std::string_view, 11> kRequiredPostPaths{{
    "/api/v1/formulas/evaluate", "/api/v1/formulas/cloud-calc",
    "/api/v1/formulas/cloud-calc/batch", "/api/v1/formulas/scan",
    "/api/v1/formulas/backtest", "/api/v1/formulas/strategy/scan",
    "/api/v1/formulas/strategy/backtest", "/api/v1/pools/evaluate",
    "/api/v1/level2/build", "/api/v1/level2/decode",
    "/api/v1/level2/project",
}};

}  // namespace

void validate_openapi_catalog_contract(const Json& document, Json& result) {
    const auto* paths = member(document, "paths");
    add_assertion(result, "paths_object", paths && paths->is_object(), true,
                  paths ? Json(paths->is_object()) : Json(nullptr));
    add_assertion(result, "no_cli_only_path",
                  paths && paths->is_object() && !object_has(paths, "CLI only"),
                  true,
                  paths ? Json(!object_has(paths, "CLI only")) : Json(nullptr));
    for (const auto& command : command_registry()) {
        for (auto endpoint : split(command.api_endpoint, ',')) {
            endpoint = trim(std::move(endpoint));
            if (endpoint.empty() || endpoint.front() != '/') continue;
            const bool found = object_has(paths, endpoint);
            add_assertion(result, "feature_path:" + command.name + ":" + endpoint,
                          found, endpoint,
                          found ? Json(endpoint) : Json(nullptr));
        }
    }
    for (const auto path : kRequiredPaths) {
        const bool found = object_has(paths, path);
        add_assertion(result, "path:" + std::string(path), found, true, found);
    }
    for (const auto path : kRequiredPostPaths) {
        const auto* formula_path = paths && paths->is_object()
            ? member(*paths, path) : nullptr;
        add_assertion(result, "post_operation:" + std::string(path),
                      formula_path && object_has(formula_path, "post"), true,
                      formula_path ? Json(object_has(formula_path, "post"))
                                   : Json(nullptr));
    }
}

}  // namespace tdx::recon_contract_detail
