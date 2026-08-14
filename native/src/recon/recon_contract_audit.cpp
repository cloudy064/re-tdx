#include "tdx/recon.hpp"
#include "tdx/time.hpp"

#include "recon_contract_internal.hpp"
#include "tdx/common.hpp"
#include "tdx/http.hpp"
#include "tdx/utf8.hpp"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <set>
#include <thread>

namespace tdx {
namespace {

using namespace recon_contract_detail;

std::string now_text() {
    return local_timestamp_text();
}

std::string body_text(const Bytes& body) {
    return std::string(body.begin(), body.end());
}

std::string contract_path(const ApiContractSpec& spec, const Json& cache_context) {
    if (std::string_view(spec.id) != "report-cache-explicit") return spec.path;
    const auto* requested = member(cache_context, "requested_report_date");
    if (!requested || !requested->is_string()) return {};
    auto compact = requested->as_string();
    compact.erase(std::remove(compact.begin(), compact.end(), '-'), compact.end());
    return "/api/v1/market/fund-analytics?view=reported-holding-securities&fund_code="
           "000326&report_date=" + url_encode(compact);
}

void discover_strong_stock_interval(const std::string& base_url,
                                    int timeout_ms,
                                    std::size_t& requests,
                                    std::string& path) {
    try {
        ++requests;
        const auto response = http_get(
            base_url + "/api/v1/market/strong-stocks?view=intervals&limit=1",
            {{"Accept", "application/json"}}, timeout_ms, 16 * 1024 * 1024);
        if (response.status != 200) return;
        const auto document = Json::parse(body_text(response.body));
        const auto* records = member(document, "records");
        if (!records || !records->is_array() || !records->size()) return;
        const auto* interval = member(records->as_array().front(), "interval_id");
        if (interval && interval->is_string())
            path = "/api/v1/market/strong-stocks?view=detail&interval_id=" +
                   url_encode(interval->as_string()) + "&limit=20";
    } catch (...) {
        // The fixed catalog path remains the diagnostic fallback.
    }
}

HttpResult request_contract(const std::string& base_url,
                            const std::string& path,
                            const ApiContractSpec& spec,
                            int timeout_ms,
                            int& attempts,
                            std::size_t& requests) {
    HttpResult response;
    for (int attempt = 1; attempt <= 3; ++attempt) {
        ++attempts;
        ++requests;
        if (const auto* post = api_contract_post_request(spec.id)) {
            response = http_post(
                base_url + path, Bytes(post->body.begin(), post->body.end()),
                {{"Accept", "application/json"},
                 {"Content-Type", "application/json"},
                 {"X-TDX-Action", std::string(post->action)}},
                timeout_ms, 16 * 1024 * 1024);
        } else {
            const bool html = std::string_view(spec.id) == "homepage" ||
                              std::string_view(spec.id) == "formula-workbench-route";
            response = http_get(base_url + path,
                {{"Accept", html ? "text/html" : "application/json"}},
                timeout_ms, 16 * 1024 * 1024);
        }
        const bool retryable = response.status == 429 || response.status == 502 ||
                               response.status == 503 || response.status == 504;
        if (!retryable || attempt == 3) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(250 * attempt));
    }
    return response;
}

Json dependency_failure(const ApiContractSpec& spec) {
    Json check = Json::object();
    check["id"] = spec.id;
    check["title"] = spec.title;
    check["path"] = Json(nullptr);
    check["passed"] = false;
    check["error"] = "default report-date dependency did not return a date";
    check["assertions"] = Json::array();
    return check;
}

}  // namespace

Json run_api_contract_audit(const std::string& raw_base_url,
                            const std::string& raw_profile,
                            const std::vector<std::string>& selected_cases,
                            int timeout_ms) {
    using namespace recon_contract_detail;
    auto base_url = trim(raw_base_url);
    while (!base_url.empty() && base_url.back() == '/') base_url.pop_back();
    if (base_url.rfind("http://", 0) != 0 && base_url.rfind("https://", 0) != 0)
        throw Error("base URL must start with http:// or https://");
    const auto profile = lower_ascii(trim(raw_profile));
    if (profile != "quick" && profile != "full")
        throw Error("profile must be quick or full");
    if (timeout_ms < 100 || timeout_ms > 60000)
        throw Error("timeout must be in 100..60000 milliseconds");

    std::set<std::string> selected;
    for (auto value : selected_cases) {
        value = lower_ascii(trim(std::move(value)));
        if (!value.empty()) selected.insert(std::move(value));
    }
    std::set<std::string> known;
    for (const auto& spec : api_contract_specs()) known.insert(spec.id);
    for (const auto& value : selected)
        if (!known.count(value)) throw Error("unknown API contract case: " + value);
    if (selected.count("report-cache-explicit")) selected.insert("report-cache-default");

    Json report = Json::object();
    report["schema"] = "tdx-api-contract-audit-native-v1";
    report["generated_at"] = now_text();
    report["base_url"] = base_url;
    report["profile"] = profile;
    report["selected_cases"] = Json::array();
    for (const auto& value : selected) report["selected_cases"].push_back(value);
    report["checks"] = Json::array();
    Json cache_context = Json::object();
    std::size_t passed = 0, failed = 0, requests = 0;

    for (const auto& spec : api_contract_specs()) {
        if (!selected.empty() && !selected.count(spec.id)) continue;
        if (selected.empty() && profile == "quick" && spec.full_only) continue;
        std::string path = contract_path(spec, cache_context);
        if (path.empty()) {
            report["checks"].push_back(dependency_failure(spec));
            ++failed;
            continue;
        }
        if (std::string_view(spec.id) == "strong-stocks-detail-live")
            discover_strong_stock_interval(base_url, timeout_ms, requests, path);

        const auto started = std::chrono::steady_clock::now();
        Json check;
        try {
            int attempts = 0;
            const auto response = request_contract(base_url, path, spec, timeout_ms,
                                                   attempts, requests);
            const auto body = body_text(response.body);
            check = evaluate_api_contract_response(spec.id, response.status,
                                                   response.content_type, body,
                                                   cache_context);
            check["request_attempts"] = attempts;
            if (!check.at("passed").as_bool())
                check["response_excerpt"] = utf8_prefix(body, 4096);
            if (std::string_view(spec.id) == "report-cache-default") {
                try {
                    const auto document = Json::parse(body);
                    const auto* requested = member_path(
                        document, {"parameters", "requested_report_date"});
                    if (requested && requested->is_string())
                        cache_context["requested_report_date"] = *requested;
                } catch (...) {}
            }
        } catch (const std::exception& error) {
            check = Json::object();
            check["id"] = spec.id;
            check["passed"] = false;
            check["error"] = error.what();
            check["assertions"] = Json::array();
        }
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - started).count();
        check["title"] = spec.title;
        check["path"] = path;
        check["duration_ms"] = static_cast<std::uint64_t>(
            std::max<std::int64_t>(0, elapsed));
        if (check.at("passed").as_bool()) ++passed;
        else ++failed;
        report["checks"].push_back(std::move(check));
    }

    Json summary = Json::object();
    summary["total"] = static_cast<std::uint64_t>(passed + failed);
    summary["passed"] = static_cast<std::uint64_t>(passed);
    summary["failed"] = static_cast<std::uint64_t>(failed);
    summary["network_requests"] = static_cast<std::uint64_t>(requests);
    report["summary"] = std::move(summary);
    report["ok"] = failed == 0;
    report["semantics"] =
        "quick checks local service shape and argument rejection; full also verifies stable empty relations, upstream resilience metadata, intraday funds, and report-date cache isolation; retryable HTTP responses are attempted at most three times";
    return report;
}

}  // namespace tdx
