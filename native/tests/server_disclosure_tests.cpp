#include "server_core_internal.hpp"
#include "server_market_events_internal.hpp"

#include "tdx/common.hpp"
#include "tdx/disclosures.hpp"

#include <chrono>
#include <filesystem>
#include <functional>
#include <iostream>
#include <string>
#include <utility>

namespace fs = std::filesystem;

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw tdx::Error(message);
}

void require_error(const std::function<void()>& action,
                   const std::string& expected) {
    try {
        action();
    } catch (const std::exception& error) {
        if (std::string(error.what()).find(expected) != std::string::npos)
            return;
        throw tdx::Error("unexpected disclosure server error: " +
                         std::string(error.what()));
    }
    throw tdx::Error("expected disclosure server error containing: " + expected);
}

struct TemporaryTdxRoot {
    fs::path path = fs::temp_directory_path() /
        ("tdx-server-disclosure-" + std::to_string(
            std::chrono::high_resolution_clock::now()
                .time_since_epoch().count()));

    TemporaryTdxRoot() { fs::create_directories(path); }
    ~TemporaryTdxRoot() {
        std::error_code ignored;
        fs::remove_all(path, ignored);
    }
};

tdx::Json disclosure_observation() {
    auto row = tdx::Json::object();
    row["kind"] = "recent";
    row["status"] = "disclosed";
    row["report_period"] = "20260331";
    row["available_from"] = "20260424";
    auto security = tdx::Json::object();
    security["market"] = "sz";
    security["market_id"] = 0;
    security["code"] = "000001";
    security["security_id"] = "SZ000001";
    security["name"] = "平安银行";
    row["security"] = std::move(security);

    auto result = tdx::Json::object();
    result["rows"] = tdx::Json::array();
    result["rows"].push_back(std::move(row));
    return result;
}

}  // namespace

int main() {
    try {
        auto empty = tdx::Json::object();
        const auto defaults =
            tdx::server_detail::parse_disclosure_archive_request(empty);
        require(!defaults.backfill_announcements && !defaults.refresh &&
                    defaults.market.empty() && defaults.code.empty() &&
                    defaults.cache_ttl_seconds == 300 &&
                    defaults.timeout_ms == 15000,
                "archive request defaults are bounded and read no path");

        auto backfill = tdx::Json::object();
        backfill["backfill_announcements"] = true;
        backfill["market"] = "SH";
        backfill["code"] = "600000";
        backfill["refresh"] = true;
        backfill["cache_ttl_seconds"] = 0;
        backfill["timeout_ms"] = 60000;
        const auto parsed =
            tdx::server_detail::parse_disclosure_archive_request(backfill);
        require(parsed.backfill_announcements && parsed.market == "sh" &&
                    parsed.code == "600000" && parsed.refresh &&
                    parsed.cache_ttl_seconds == 0 &&
                    parsed.timeout_ms == 60000,
                "archive request accepts only bounded backfill controls");

        auto path_injection = empty;
        path_injection["archive_path"] = "C:/outside.json";
        require_error([&] {
            (void)tdx::server_detail::parse_disclosure_archive_request(
                path_injection);
        }, "does not accept field: archive_path");

        auto ambiguous_security = empty;
        ambiguous_security["market"] = "sz";
        ambiguous_security["code"] = "000001";
        require_error([&] {
            (void)tdx::server_detail::parse_disclosure_archive_request(
                ambiguous_security);
        }, "only when backfill_announcements is true");

        auto invalid_timeout = empty;
        invalid_timeout["timeout_ms"] = 99;
        require_error([&] {
            (void)tdx::server_detail::parse_disclosure_archive_request(
                invalid_timeout);
        }, "100..60000");

        TemporaryTdxRoot fixture;
        tdx::server_detail::ApiState unavailable_state;
        unavailable_state.root = fixture.path;
        tdx::server_detail::RequestTarget legacy_get;
        legacy_get.path = "/api/v1/market/disclosures";
        legacy_get.query["archive"] = "0";
        require_error([&] {
            (void)tdx::server_detail::query_market_disclosures(
                unavailable_state, legacy_get);
        }, "require confirmed POST");
        require(!fs::exists(tdx::default_disclosure_archive_path(fixture.path)),
                "legacy GET archive parameters cannot create an archive");

        tdx::server_detail::RequestTarget archive_get;
        archive_get.path = "/api/v1/market/disclosures/archive";
        require_error([&] {
            (void)tdx::server_detail::route(
                unavailable_state, archive_get, false, nullptr);
        }, "requires a confirmed POST body");
        require(!fs::exists(tdx::default_disclosure_archive_path(fixture.path)),
                "archive route without a confirmed body cannot write");

        const auto observation = disclosure_observation();
        const auto metadata =
            tdx::server_detail::persist_disclosure_archive_observation(
                fixture.path, observation, "2026-08-12T19:00:00+0800");
        const auto expected = tdx::default_disclosure_archive_path(
            fs::weakly_canonical(fixture.path));
        require(fs::is_regular_file(expected) &&
                    metadata.at("path").as_string() == tdx::path_utf8(expected) &&
                    metadata.at("fixed_root_path").as_bool() &&
                    metadata.at("summary").at("entry_count").as_number() == 1,
                "archive writer uses only the fixed path below the supplied root");
        const auto stored = tdx::Json::parse(tdx::read_text_utf8(expected));
        require(stored.at("schema").as_string() ==
                    "tdx-disclosure-availability-archive-v1" &&
                    stored.at("entries").size() == 1,
                "archive writer persists the merged disclosure schema");

        const auto repeated =
            tdx::server_detail::persist_disclosure_archive_observation(
                fixture.path, observation, "2026-08-12T19:05:00+0800");
        require(repeated.at("summary").at("entry_count").as_number() == 1,
                "archive writer incrementally merges its fixed existing file");

        std::cout << "server disclosure tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
