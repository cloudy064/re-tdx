#include "server_core_internal.hpp"
#include "server_catalog_internal.hpp"
#include "server_formula_internal.hpp"
#include "server_level2_internal.hpp"
#include "server_market_internal.hpp"
#include "server_market_events_internal.hpp"
#include "server_pool_internal.hpp"

#include "tdx/cloud_routes.hpp"
#include "tdx/cloud_variants.hpp"
#include "tdx/cloud_workflow.hpp"
#include "tdx/common.hpp"
#include "tdx/jsn.hpp"
#include "tdx/jsn_data.hpp"
#include "tdx/jsn_variants.hpp"
#include "tdx/level2.hpp"
#include "tdx/market_stream.hpp"
#include "tdx/registry.hpp"
#include "tdx/recon.hpp"
#include "tdx/tpool.hpp"

#include <cstdint>
#include <limits>
#include <memory>
#include <mutex>

namespace tdx::server_detail {
HttpResponse route(const ApiState& state, const RequestTarget& target,
                   bool allow_write, const Json* request_body) {
    if (target.path == "/api/v1/health") return json_response(health_document(state));
    if (target.path == "/api/v1/features") return json_response(feature_document());
    if (target.path == "/api/v1/openapi.json") return json_response(openapi_document());
    if (target.path == "/api/v1/blocks") return json_response(query_blocks(state, target));
    if (target.path == "/api/v1/securities")
        return json_response(query_securities(state, target));
    if (target.path == "/api/v1/securities/blocks")
        return json_response(query_security_blocks(state, target));
    if (target.path == "/api/v1/formulas")
        return json_response(query_formulas(formula_http_state(state), target));
    if (target.path == "/api/v1/formulas/icons")
        return json_response(state.formula_icons.manifest);
    if (target.path == "/api/v1/formulas/drawicon-strip.png")
        return HttpResponse{200, "OK", "image/png",
            std::string(reinterpret_cast<const char*>(state.formula_icons.png.data()),
                        state.formula_icons.png.size())};
    if (target.path == "/api/v1/formulas/drawicon-strip.bmp")
        return HttpResponse{200, "OK", "image/bmp",
            std::string(reinterpret_cast<const char*>(state.formula_icons.bitmap.data()),
                        state.formula_icons.bitmap.size())};
    if (target.path == "/api/v1/formulas/signal-image")
        return formula_signal_image_response(state, target);
    if (target.path == "/api/v1/formulas/coverage")
        return json_response(query_formula_coverage(formula_http_state(state), target));
    if (target.path == "/api/v1/formulas/context-template")
        return json_response(query_formula_context_template(formula_http_state(state), target));
    if (target.path == "/api/v1/formulas/context-import") {
        if (!request_body)
            throw Error("formula context-import requires a confirmed POST body");
        return json_response(query_formula_context_import(*request_body));
    }
    if (target.path == "/api/v1/formulas/audit")
        return json_response(query_formula_audit(formula_http_state(state), target));
    if (target.path == "/api/v1/formulas/cloud-calc")
        return json_response(request_body
            ? query_cloud_calc_execution(formula_http_state(state), *request_body)
            : query_cloud_calc_audit(formula_http_state(state), target));
    if (target.path == "/api/v1/formulas/cloud-calc/template")
        return json_response(query_cloud_calc_template(formula_http_state(state), target));
    if (target.path == "/api/v1/formulas/cloud-calc/batch") {
        if (!request_body) throw Error("cloud-calc batch requires a confirmed POST body");
        return json_response(query_cloud_calc_batch_execution(
            formula_http_state(state), *request_body));
    }
    if (target.path == "/api/v1/formulas/evaluate")
        return json_response(request_body
            ? query_post_formula_execution(
                  formula_http_state(state), target, *request_body)
            : query_formula_execution(formula_http_state(state), target));
    if (target.path == "/api/v1/formulas/backtest")
        return json_response(request_body
            ? query_inline_formula_backtest(
                  formula_http_state(state), target, *request_body)
            : query_formula_backtest(
                  formula_http_state(state), target, state.formulas));
    if (target.path == "/api/v1/formulas/scan")
        return json_response(request_body
            ? query_inline_formula_scan(
                  formula_http_state(state), target, *request_body)
            : query_formula_scan(formula_http_state(state), target));
    if (target.path == "/api/v1/formulas/strategy/scan") {
        if (!request_body) throw Error("formula strategy scan requires a JSON body");
        return json_response(query_formula_strategy(
            formula_http_state(state), target, *request_body, false));
    }
    if (target.path == "/api/v1/formulas/strategy/backtest") {
        if (!request_body) throw Error("formula strategy backtest requires a JSON body");
        return json_response(query_formula_strategy(
            formula_http_state(state), target, *request_body, true));
    }
    if (target.path == "/api/v1/formulas/calculate")
        return json_response(query_formula_calculation(target));
    if (target.path == "/api/v1/pools")
        return json_response(query_tpool_catalog(state.root, target));
    // History is a root-scoped read-only scan; its HTTP projection strips
    // absolute server paths before returning the domain document.
    if (target.path == "/api/v1/pools/history")
        return json_response(query_tpool_history(state.root, target));
    if (target.path == "/api/v1/pools/evaluate") {
        if (request_body)
            return json_response(query_inline_tpool_evaluation(
                formula_http_state(state), *request_body));
        return json_response(query_tpool_file_evaluation(
            state.root, target, state.formulas));
    }
    if (target.path == "/api/v1/cloud/routes")
        return json_response(cloud_routes_document(
            state.root, query_value(target, "entry"), query_value(target, "source")));
    if (target.path == "/api/v1/cloud/variants")
        return json_response(cloud_variant_coverage_document(
            state.root, query_bool(target, "gaps_only")));
    if (target.path == "/api/v1/level2/status") {
        const int pid = parse_bounded(query_value(target, "pid", "0"), "pid", 0,
                                      std::numeric_limits<int>::max());
        return json_response(level2_session_preflight_document(
            query_value(target, "process", "TdxW.exe"), static_cast<std::uint32_t>(pid)));
    }
    if (target.path == "/api/v1/level2/build") {
        if (!request_body)
            throw Error("Level2 build requires a confirmed POST body");
        return json_response(query_level2_build(*request_body));
    }
    if (target.path == "/api/v1/level2/decode") {
        if (!request_body)
            throw Error("Level2 decode requires a confirmed POST body");
        return json_response(query_level2_decode(*request_body));
    }
    if (target.path == "/api/v1/level2/project") {
        if (!request_body)
            throw Error("Level2 project requires a confirmed POST body");
        return json_response(query_level2_project(*request_body));
    }
    if (target.path == "/api/v1/market/disclosures/archive") {
        if (!request_body)
            throw Error("disclosure archive requires a confirmed POST body");
        return json_response(query_market_disclosure_archive(
            state, *request_body));
    }
    if (target.path == "/api/v1/market/stream/status") {
        if (!state.market_stream_hub)
            throw Error("market stream hub is unavailable");
        return json_response(state.market_stream_hub->status());
    }
    if (const auto response = route_registered_market_api(state, target))
        return *response;
    if (target.path == "/api/v1/minute") return json_response(query_minute(state, target));
    if (target.path == "/api/v1/kline") return json_response(query_minute(state, target));
    if (target.path == "/api/v1/security/profile")
        return json_response(query_security_profile(state, target));
    if (target.path == "/api/v1/cloud/workflows")
        return json_response(cloud_workflows_document());
    if (target.path == "/api/v1/cloud/workflow")
        return json_response(query_cloud_workflow_api(state, target));
    if (target.path == "/api/v1/pbrpc/configs")
        return json_response(pbrpc_configs_document(state.root));
    if (target.path == "/api/v1/pbrpc/query")
        return json_response(query_pbrpc_api(state, target));
    if (target.path == "/api/v1/tqlex/configs")
        return json_response(tqlex_configs_document(state.root));
    if (target.path == "/api/v1/tqlex/query")
        return json_response(query_tqlex_api(state, target));
    if (target.path == "/api/v1/jsn/security")
        return json_response(query_jsn_security(state, target));
    if (target.path == "/api/v1/jsn/catalog") {
        if (!state.jsn_index) throw Error("JSN data directory is unavailable; pass --jsn-root");
        return json_response(state.jsn_index->catalog());
    }
    if (target.path == "/api/v1/jsn/variants")
        return json_response(jsn_variant_coverage_document(
            state.root, state.jsn_root, query_bool(target, "gaps_only"),
            parse_bounded(query_value(target, "top", "50"), "top", 0, 1000)));
    if (target.path == "/api/v1/jsn/discovery") {
        if (state.jsn_root.empty())
            throw Error("JSN data directory is unavailable; pass --jsn-root");
        const bool capture = lower_ascii(query_value(target, "action")) == "capture";
        if (capture && !allow_write)
            throw Error("capturing a JSN discovery baseline requires confirmed POST");
        const auto baseline = state.jsn_root.parent_path() /
            "tdx-jsn-discovery-baseline.json";
        const int top = parse_bounded(query_value(target, "top", "100"),
                                      "top", 0, 1000);
        const bool refresh = query_bool(target, "refresh");
        if (capture) {
            auto report = jsn_discovery_document(
                state.root, state.jsn_root, baseline, true, top);
            if (state.jsn_discovery_mutex) {
                std::lock_guard<std::mutex> lock(*state.jsn_discovery_mutex);
                state.jsn_discovery_cache.reset();
            }
            return json_response(std::move(report));
        }
        if (!state.jsn_discovery_mutex)
            throw Error("JSN discovery cache is unavailable");
        std::lock_guard<std::mutex> lock(*state.jsn_discovery_mutex);
        if (!state.jsn_discovery_cache || refresh)
            state.jsn_discovery_cache = std::make_shared<Json>(jsn_discovery_document(
                state.root, state.jsn_root, baseline, false, 1000));
        auto report = *state.jsn_discovery_cache;
        auto& priority = report["priority_changes"].as_array();
        if (priority.size() > static_cast<std::size_t>(top))
            priority.resize(static_cast<std::size_t>(top));
        return json_response(std::move(report));
    }
    if (target.path == "/api/v1/jsn/candidates") {
        if (state.jsn_root.empty())
            throw Error("JSN data directory is unavailable; pass --jsn-root");
        if (!state.jsn_discovery_mutex)
            throw Error("JSN candidate cache is unavailable");
        const auto family = lower_ascii(trim(query_value(target, "family")));
        const bool missing_only = query_bool(target, "missing_only", true);
        const bool refresh = query_bool(target, "refresh");
        const int limit = parse_bounded(query_value(target, "limit", "200"),
                                        "limit", 0, 2000);
        const auto cache_key = family + "|" + (missing_only ? "1" : "0") +
            "|" + std::to_string(limit);
        std::lock_guard<std::mutex> lock(*state.jsn_discovery_mutex);
        const auto found = state.jsn_candidate_cache.find(cache_key);
        if (found != state.jsn_candidate_cache.end() && !refresh)
            return json_response(*found->second);
        auto report = std::make_shared<Json>(jsn_candidate_document(
            state.root, state.jsn_root, family, missing_only, limit,
            false, 10, 10000));
        state.jsn_candidate_cache[cache_key] = report;
        return json_response(*report);
    }
    if (target.path == "/api/v1/jsn/resource")
        return json_response(query_jsn_resource(state, target, allow_write));
    if (target.path == "/api/v1/industry/tree")
        return json_response(query_industry_tree(state, target));
    if (target.path == "/api/v1/install")
        return json_response(inventory_document(state.root, false, false));
    if (target.path.rfind("/api/", 0) == 0) {
        Json error = Json::object();
        error["error"] = "not_found";
        error["path"] = target.path;
        return json_response(std::move(error), 404, "Not Found");
    }
    return static_response(state, target);
}

}  // namespace tdx::server_detail
