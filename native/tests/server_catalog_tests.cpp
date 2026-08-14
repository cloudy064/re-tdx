#include "server_catalog_internal.hpp"
#include "server_market_internal.hpp"

#include "tdx/common.hpp"

#include <iostream>
#include <string>

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw tdx::Error(message);
}

const tdx::Json& feature_named(const tdx::Json& document,
                               const char* command) {
    for (const auto& feature : document.at("features").as_array()) {
        if (feature.at("command").as_string() == command) return feature;
    }
    throw tdx::Error("feature is absent from catalog: " + std::string(command));
}

bool has_feature_operation(const tdx::Json& feature,
                           const std::string& method,
                           const std::string& path) {
    for (const auto& operation : feature.at("api_operations").as_array()) {
        if (operation.at("method").as_string() == method &&
            operation.at("path").as_string() == path)
            return true;
    }
    return false;
}

bool has_openapi_method(const tdx::Json& paths, const std::string& path,
                        const std::string& method) {
    const auto found = paths.as_object().find(path);
    return found != paths.as_object().end() &&
           found->second.as_object().count(method) == 1;
}

void require_pbrpc_budget_rejected(const std::string& value) {
    tdx::server_detail::RequestTarget target;
    target.path = "/api/v1/pbrpc/query";
    target.query = {{"req_id", "1"}, {"max_assembled_bytes", value}};
    try {
        (void)tdx::server_detail::query_pbrpc_api(
            tdx::server_detail::ApiState{}, target);
    } catch (const tdx::Error& error) {
        require(std::string(error.what()).find(
                    "max_assembled_bytes must be in 1..134217728") !=
                    std::string::npos,
                "PBRPC HTTP budget reports its public range");
        return;
    }
    throw tdx::Error("PBRPC HTTP accepted an out-of-range assembly budget");
}

}  // namespace

int main() {
    try {
        const auto features = tdx::server_detail::feature_document();
        require(features.at("schema").as_string() == "tdx-tool-features-v1",
                "feature schema remains backward compatible");
        require(features.at("count").as_number() ==
                    static_cast<double>(features.at("features").size()),
                "feature count matches rows");

        for (const auto& feature : features.at("features").as_array()) {
            const auto& surface = feature.at("surface").as_string();
            const auto& operations = feature.at("api_operations");
            require(operations.is_array(), "feature operations are an array");
            require(surface == "http" || surface == "cli-only",
                    "feature surface is recognized");
            require((surface == "http") == !operations.as_array().empty(),
                    "HTTP surface has operations and CLI-only does not");
            for (const auto& operation : operations.as_array()) {
                const auto& method = operation.at("method").as_string();
                const auto& path = operation.at("path").as_string();
                require(method == "get" || method == "post",
                        "feature operation method is supported");
                require(!path.empty() && path.front() == '/',
                        "feature operation path is absolute");
            }
        }
        const auto& cli_only = feature_named(features, "formulas user-library");
        require(cli_only.at("api_endpoint").as_string() == "CLI only" &&
                    cli_only.at("surface").as_string() == "cli-only" &&
                    cli_only.at("api_operations").as_array().empty(),
                "legacy CLI-only label is retained without a fake operation");

        const auto& strategy = feature_named(features, "formulas strategy");
        require(strategy.at("api_operations").size() == 2 &&
                    has_feature_operation(strategy, "post",
                        "/api/v1/formulas/strategy/scan") &&
                    has_feature_operation(strategy, "post",
                        "/api/v1/formulas/strategy/backtest"),
                "strategy feature operations are POST-only");
        const auto& context_template =
            feature_named(features, "formulas context-template");
        require(context_template.at("title").as_string().find("上下文") !=
                    std::string::npos &&
                    context_template.at("description").as_string().find(
                        "宿主 raw 标量") != std::string::npos &&
                    context_template.at("description").as_string().find(
                        "跳过 K 线获取") != std::string::npos &&
                    context_template.at("description").as_string().find(
                        "不获取、推导或伪造") != std::string::npos,
                "formula context-template catalog exposes scalar and authorization boundaries");
        const auto& context_import =
            feature_named(features, "formulas context-import");
        require(context_import.at("api_endpoint").as_string() ==
                    "/api/v1/formulas/context-import" &&
                    has_feature_operation(context_import, "post",
                        "/api/v1/formulas/context-import") &&
                    context_import.at("description").as_string().find(
                        "合法持有") != std::string::npos &&
                    context_import.at("description").as_string().find(
                        "不调用 SDK") != std::string::npos,
                "formula context-import catalog exposes ownership and offline boundaries");
        for (const auto* command : {
                 "level2 build", "level2 decode", "level2 project"}) {
            const auto& feature = feature_named(features, command);
            require(feature.at("api_operations").size() == 1 &&
                        feature.at("api_operations").as_array().front()
                                .at("method").as_string() == "post",
                    "Level2 feature operation is POST-only");
        }
        require(feature_named(features, "level2 build")
                    .at("description").as_string().find("fnSubscribeData") !=
                    std::string::npos,
                "Level2 feature catalog advertises the offline batch plan");
        const auto& level2_decode_feature =
            feature_named(features, "level2 decode").at("description")
                .as_string();
        require(level2_decode_feature.find("sdk-json-4653") !=
                    std::string::npos &&
                level2_decode_feature.find("tpbus-115") !=
                    std::string::npos &&
                level2_decode_feature.find("SHA-256") !=
                    std::string::npos &&
                level2_decode_feature.find("不回显") != std::string::npos &&
                level2_decode_feature.find("不调用 SDK") !=
                    std::string::npos &&
                level2_decode_feature.find("不联网") != std::string::npos,
                "Level2 feature catalog exposes SDK JSON and tpbus-115 offline boundaries");
        const auto& disclosures = feature_named(features, "market disclosures");
        require(disclosures.at("api_operations").size() == 2 &&
                    has_feature_operation(disclosures, "get",
                        "/api/v1/market/disclosures") &&
                    has_feature_operation(disclosures, "post",
                        "/api/v1/market/disclosures/archive"),
                "disclosure read and archive operations are separated");
        const auto& pool_history = feature_named(features, "pool history");
        require(pool_history.at("api_endpoint").as_string() ==
                    "/api/v1/pools/history" &&
                    pool_history.at("surface").as_string() == "http" &&
                    pool_history.at("api_operations").size() == 1 &&
                    has_feature_operation(pool_history, "get",
                        "/api/v1/pools/history"),
                "TPool history is exposed as one read-only GET operation");
        const auto& pool_inspect = feature_named(features, "pool inspect");
        require(pool_inspect.at("description").as_string().find(
                    "TDX 根相对路径") != std::string::npos &&
                    pool_inspect.at("description").as_string().find(
                    "不回显服务器绝对根目录") != std::string::npos,
                "TPool catalog feature documents its root-relative HTTP projection");
        for (const auto* command : {"market hk-actions", "market hk-finance"}) {
            const auto& feature = feature_named(features, command);
            require(feature.at("surface").as_string() == "http" &&
                        feature.at("description").as_string().find(
                            "TDX根相对路径") != std::string::npos,
                    "HK local cache feature documents root-relative sources");
        }

        const auto openapi = tdx::server_detail::openapi_document();
        const auto& paths = openapi.at("paths");
        require(paths.as_object().count("CLI only") == 0,
                "OpenAPI omits CLI-only sentinel");
        for (const auto& [path, operations] : paths.as_object()) {
            require(!path.empty() && path.front() == '/',
                    "OpenAPI contains only absolute HTTP paths");
            require(operations.is_object() && !operations.as_object().empty(),
                    "OpenAPI path contains at least one operation");
        }
        for (const auto& feature : features.at("features").as_array()) {
            for (const auto& operation : feature.at("api_operations").as_array()) {
                require(has_openapi_method(
                            paths, operation.at("path").as_string(),
                            operation.at("method").as_string()),
                        "feature operation is present in OpenAPI");
            }
        }

        for (const auto* path : {
                 "/api/v1/formulas/strategy/scan",
                 "/api/v1/formulas/strategy/backtest",
                 "/api/v1/formulas/context-import",
                 "/api/v1/level2/build",
                 "/api/v1/level2/decode",
                 "/api/v1/level2/project",
                 "/api/v1/market/disclosures/archive",
                 "/api/v1/formulas/cloud-calc/batch"}) {
            require(has_openapi_method(paths, path, "post") &&
                        !has_openapi_method(paths, path, "get"),
                    "body-only OpenAPI operation is POST-only");
        }
        const auto& tqlex_summary = paths.at("/api/v1/tqlex/query")
            .at("get").at("summary").as_string();
        require(tqlex_summary.find("page_size=1..5000") != std::string::npos &&
                    tqlex_summary.find("max_pages=1..20") != std::string::npos &&
                    tqlex_summary.find("50000") != std::string::npos &&
                    tqlex_summary.find("单页固定执行 1 页") != std::string::npos,
                "TQLEX OpenAPI summary exposes the HTTP resource budget");
        const auto& formula_context_summary =
            paths.at("/api/v1/formulas/context-template")
                .at("get").at("summary").as_string();
        require(formula_context_summary.find("宿主 raw 标量") !=
                    std::string::npos &&
                    formula_context_summary.find("跳过 K 线获取") !=
                    std::string::npos &&
                    formula_context_summary.find("不获取、推导或伪造") !=
                    std::string::npos,
                "formula context-template OpenAPI summary exposes scalar placeholders only");
        const auto& formula_context_import_summary =
            paths.at("/api/v1/formulas/context-import")
                .at("post").at("summary").as_string();
        require(formula_context_import_summary.find("allow_partial") !=
                    std::string::npos &&
                    formula_context_import_summary.find("不接受路径") !=
                    std::string::npos &&
                    formula_context_import_summary.find("不保留捕获封套") !=
                    std::string::npos &&
                    formula_context_import_summary.find("不调用 SDK") !=
                    std::string::npos,
                "formula context-import OpenAPI summary exposes strict offline semantics");
        const auto& formula_evaluate_summary =
            paths.at("/api/v1/formulas/evaluate")
                .at("post").at("summary").as_string();
        require(formula_evaluate_summary.find("future_replay") !=
                    std::string::npos &&
                    formula_evaluate_summary.find("max_observations=1..64") !=
                    std::string::npos &&
                    formula_evaluate_summary.find("max_events=1..10000") !=
                    std::string::npos &&
                    formula_evaluate_summary.find("禁止扫描、回测、账户、委托、SDK 和订阅") !=
                    std::string::npos,
                "formula evaluate OpenAPI exposes bounded read-only future replay");
        const auto& pbrpc_operation = paths.at("/api/v1/pbrpc/query").at("get");
        const auto& pbrpc_budget =
            pbrpc_operation.at("parameters").as_array().front();
        require(pbrpc_budget.at("name").as_string() ==
                    "max_assembled_bytes" &&
                    pbrpc_budget.at("in").as_string() == "query" &&
                    !pbrpc_budget.at("required").as_bool() &&
                    pbrpc_budget.at("schema").at("minimum").as_number() == 1 &&
                    pbrpc_budget.at("schema").at("maximum").as_number() ==
                        134217728 &&
                    pbrpc_budget.at("schema").at("default").as_number() ==
                        134217728 &&
                    pbrpc_operation.at("summary").as_string().find(
                        "组装字节预算") != std::string::npos,
                "PBRPC OpenAPI publishes the downscalable assembly budget");
        require_pbrpc_budget_rejected("0");
        require_pbrpc_budget_rejected("134217729");
        const auto& level2_build_summary = paths.at("/api/v1/level2/build")
            .at("post").at("summary").as_string();
        require(level2_build_summary.find("sdk-fnreqdata-plan") !=
                    std::string::npos &&
                level2_build_summary.find("sdk-fnreqdata-18031-plan") !=
                    std::string::npos &&
                level2_build_summary.find("sdk-callback-route-plan") !=
                    std::string::npos &&
                level2_build_summary.find("不调用 SDK") !=
                    std::string::npos &&
                level2_build_summary.find("不投递消息") !=
                    std::string::npos,
                "Level2 build catalog exposes fnReqData and callback route boundaries");
        const auto& level2_decode_summary = paths.at("/api/v1/level2/decode")
            .at("post").at("summary").as_string();
        require(level2_decode_summary.find("sdk-callback-invocation") !=
                    std::string::npos &&
                level2_decode_summary.find("sdk-json-4653") !=
                    std::string::npos &&
                level2_decode_summary.find("tpbus-115") !=
                    std::string::npos &&
                level2_decode_summary.find("SHA-256") !=
                    std::string::npos &&
                level2_decode_summary.find("不回显") != std::string::npos &&
                level2_decode_summary.find("不调用 SDK") !=
                    std::string::npos &&
                level2_decode_summary.find("不执行 SDK 回调") !=
                    std::string::npos &&
                level2_decode_summary.find("不投递宿主消息") !=
                    std::string::npos &&
                level2_decode_summary.find("不联网") != std::string::npos,
                "Level2 decode catalog exposes callback, SDK JSON, and tpbus-115 boundaries");
        const auto& level2_project_summary = paths.at(
            "/api/v1/level2/project").at("post").at("summary").as_string();
        require(level2_project_summary.find("sdk-quote-transition") !=
                    std::string::npos &&
                level2_project_summary.find("sdk-1801-host-projection") !=
                    std::string::npos &&
                level2_project_summary.find("sdk-1802-host-projection") !=
                    std::string::npos &&
                level2_project_summary.find(
                    "sdk-1803-depth-record-projection") !=
                    std::string::npos &&
                level2_project_summary.find(
                    "sdk-18031-queue-record-projection") !=
                    std::string::npos &&
                level2_project_summary.find("sdk-1804-host-projection") !=
                    std::string::npos &&
                level2_project_summary.find(
                    "sdk-4654-dual-snapshot-transition") !=
                    std::string::npos &&
                level2_project_summary.find(
                    "sdk-4651-dual-snapshot-transition") !=
                    std::string::npos &&
                level2_project_summary.find(
                    "sdk-4655-companion-raw-transition") !=
                    std::string::npos &&
                level2_project_summary.find("显式内联双快照") !=
                    std::string::npos &&
                level2_project_summary.find("1807/18071") !=
                    std::string::npos &&
                level2_project_summary.find("20 B") !=
                    std::string::npos &&
                level2_project_summary.find("13 B") !=
                    std::string::npos &&
                level2_project_summary.find("6 B") !=
                    std::string::npos &&
                level2_project_summary.find("105") !=
                    std::string::npos &&
                level2_project_summary.find("不调用 SDK") !=
                    std::string::npos &&
                level2_project_summary.find("不投递宿主消息") !=
                    std::string::npos &&
                level2_project_summary.find("不联网") !=
                    std::string::npos &&
                level2_project_summary.find("不绕过授权") !=
                    std::string::npos,
                "Level2 project catalog exposes the offline quote transition boundary");
        for (const auto* path : {
                 "/api/v1/formulas/evaluate",
                 "/api/v1/formulas/cloud-calc",
                 "/api/v1/formulas/scan",
                 "/api/v1/formulas/backtest",
                 "/api/v1/pools/evaluate"}) {
            require(has_openapi_method(paths, path, "get") &&
                        has_openapi_method(paths, path, "post"),
                    "dual-method OpenAPI operation remains dual-method");
        }
        require(has_openapi_method(paths,
                    "/api/v1/market/disclosures", "get") &&
                    !has_openapi_method(paths,
                    "/api/v1/market/disclosures", "post"),
                "disclosure query remains GET-only");
        require(has_openapi_method(paths, "/api/v1/pools/history", "get") &&
                    !has_openapi_method(paths,
                        "/api/v1/pools/history", "post"),
                "TPool history OpenAPI operation remains GET-only");
        require(paths.at("/api/v1/pools").at("get").at("summary")
                        .as_string().find("TDX 根相对") !=
                    std::string::npos &&
                    paths.at("/api/v1/pools/evaluate").at("get")
                        .at("summary").as_string().find(
                            "只接受并返回 TDX 根相对") !=
                    std::string::npos &&
                    paths.at("/api/v1/pools/evaluate").at("post")
                        .at("summary").as_string().find(
                            "不接受服务器文件路径") !=
                    std::string::npos,
                "TPool catalog/evaluate OpenAPI separates relative GET and inline POST paths");
        for (const auto* path : {
                 "/api/v1/market/hk-actions",
                 "/api/v1/market/hk-finance"}) {
            require(has_openapi_method(paths, path, "get") &&
                        paths.at(path).at("get").at("summary").as_string()
                            .find("TDX根相对路径") != std::string::npos,
                    "HK local cache OpenAPI documents root-relative sources");
        }

        std::cout << "server catalog tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
