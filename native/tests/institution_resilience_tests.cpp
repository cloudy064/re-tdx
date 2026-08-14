#include "tdx/common.hpp"
#include "tdx/institution.hpp"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <string>

namespace {

void require(bool value, const char* message) {
    if (!value) throw tdx::Error(message);
}

tdx::Bytes bytes(std::string value) {
    return tdx::Bytes(value.begin(), value.end());
}

std::string request_mode(const tdx::Bytes& payload) {
    const auto request = tdx::Json::parse(
        std::string(payload.begin(), payload.end()));
    return request.at("Params").as_array().front().as_string();
}

tdx::Bytes history_response() {
    return bytes(
        R"({"ErrorCode":0,"ResultSets":[{"ColName":["T001","bh","cnt","rq","sc","zqdm","zqjc","T006","T007","stype","T012","T008","T009"],"Content":[["row1","1",1,"2026-07-23",1,"688657","浩辰软件",889637,1.4903,"1","流通A股","新进",1]]}]})");
}

tdx::Bytes detail_response() {
    return bytes(
        R"({"ErrorCode":0,"ResultSets":[{"ColName":["rq","T006","T007","T012","T009","T008","stype"],"Content":[["2026-03-31",1359308,0.56,"流通A股",3,"523500","2"]]}]})");
}

}  // namespace

int main() {
    try {
        namespace fs = std::filesystem;
        const auto cache_root = fs::temp_directory_path() /
            ("tdx-institution-resilience-" + std::to_string(
                std::chrono::steady_clock::now().time_since_epoch().count()));
        fs::remove_all(cache_root);
        bool fail_history = false;
        bool fail_detail = false;
        bool stable_history_error = false;
        int history_calls = 0;
        int detail_calls = 0;
        tdx::InstitutionService service({},
            [&](const std::string&, const tdx::Bytes& payload, int) {
                const auto mode = request_mode(payload);
                if (mode == "gdjc") {
                    ++history_calls;
                    if (stable_history_error)
                        return bytes(R"({"ErrorCode":5,"ErrorInfo":"stable"})");
                    if (fail_history)
                        return bytes(R"({"ErrorCode":4,"ErrorInfo":"busy"})");
                    return history_response();
                }
                require(mode == "gdjcxq", "unexpected holder request mode");
                ++detail_calls;
                if (fail_detail)
                    return bytes(R"({"ErrorCode":4,"ErrorInfo":"busy"})");
                return detail_response();
            }, cache_root);

        tdx::HolderQuery query;
        query.holder_id = "GD_TEST";
        query.reference_code = "000001";
        query.stock_code = "688657";
        query.cache_ttl_seconds = 0;
        query.detail_cache_ttl_seconds = 0;
        query.timeout_ms = 1000;

        const auto live = service.query_holder(query);
        require(live.at("availability").as_string() == "live" &&
                    live.at("records").size() == 1 &&
                    live.at("stock_detail").at("records").size() == 1 &&
                    live.at("cache").at("history_attempts").as_number() == 1 &&
                    live.at("cache").at("detail_attempts").as_number() == 1,
                "holder live fetch did not populate both caches");

        history_calls = 0;
        detail_calls = 0;
        fail_detail = true;
        const auto stale_detail = service.query_holder(query);
        require(stale_detail.at("availability").as_string() == "stale-cache" &&
                    !stale_detail.at("cache").at("history_stale").as_bool() &&
                    stale_detail.at("cache").at("detail_stale").as_bool() &&
                    stale_detail.at("cache").at("detail_attempts").as_number() == 3 &&
                    stale_detail.at("stock_detail").at("records").size() == 1 &&
                    history_calls == 1 && detail_calls == 3 &&
                    stale_detail.at("cache").at("detail_upstream_error")
                        .as_string().find("ErrorCode 4") != std::string::npos,
                "holder detail did not fall back to a complete stale cache");

        fail_detail = false;
        stable_history_error = true;
        history_calls = 0;
        query.stock_code.clear();
        bool stable_failed = false;
        try {
            (void)service.query_holder(query);
        } catch (const tdx::Error& error) {
            stable_failed = std::string(error.what()).find("ErrorCode 5") !=
                std::string::npos;
        }
        require(stable_failed && history_calls == 1,
                "stable holder business errors must not retry or use stale data");

        int restored_calls = 0;
        tdx::InstitutionService restored({},
            [&](const std::string&, const tdx::Bytes&, int) {
                ++restored_calls;
                return bytes(R"({"ErrorCode":4,"ErrorInfo":"busy"})");
            }, cache_root);
        query.stock_code = "688657";
        const auto persisted = restored.query_holder(query);
        require(persisted.at("availability").as_string() == "stale-cache" &&
                    persisted.at("records").size() == 1 &&
                    persisted.at("stock_detail").at("records").size() == 1 &&
                    persisted.at("cache").at("persistent_enabled").as_bool() &&
                    persisted.at("cache").at("history_persistent_restored").as_bool() &&
                    persisted.at("cache").at("detail_persistent_restored").as_bool() &&
                    persisted.at("cache").at("history_attempts").as_number() == 3 &&
                    persisted.at("cache").at("detail_attempts").as_number() == 3 &&
                    restored_calls == 6,
                "holder cache did not survive service reconstruction");

        int cold_calls = 0;
        tdx::InstitutionService cold({},
            [&](const std::string&, const tdx::Bytes&, int) {
                ++cold_calls;
                return bytes(R"({"ErrorCode":4,"ErrorInfo":"busy"})");
            });
        bool cold_failed = false;
        try {
            (void)cold.query_holder(query);
        } catch (const tdx::Error& error) {
            cold_failed = std::string(error.what()).find("ErrorCode 4") !=
                std::string::npos;
        }
        require(cold_failed && cold_calls == 3,
                "cold holder cache must fail after bounded transient retries");

        fs::remove_all(cache_root);

        std::cout << "institution resilience tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
