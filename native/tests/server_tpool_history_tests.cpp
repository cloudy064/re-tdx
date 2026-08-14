#include "server_core_internal.hpp"
#include "server_pool_internal.hpp"

#include "tdx/common.hpp"

#include <chrono>
#include <filesystem>
#include <functional>
#include <iostream>
#include <map>
#include <string>
#include <utility>

namespace fs = std::filesystem;

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) throw tdx::Error(message);
}

void require_error(const std::function<void()>& action,
                   const std::string& expected) {
    try {
        action();
    } catch (const std::exception& error) {
        if (std::string(error.what()).find(expected) != std::string::npos)
            return;
        throw tdx::Error("unexpected TPool history server error: " +
                         std::string(error.what()));
    }
    throw tdx::Error("expected TPool history error containing: " + expected);
}

struct TemporaryTdxRoot {
    fs::path path = fs::temp_directory_path() /
        ("tdx-server-tpool-history-" + std::to_string(
            std::chrono::high_resolution_clock::now()
                .time_since_epoch().count()));

    TemporaryTdxRoot() { fs::create_directories(path); }
    ~TemporaryTdxRoot() {
        std::error_code ignored;
        fs::remove_all(path, ignored);
    }
};

void write_fixture(const fs::path& path, const std::string& contents) {
    fs::create_directories(path.parent_path());
    tdx::atomic_write_text(path, contents);
}

tdx::server_detail::RequestTarget request(
    std::map<std::string, std::string> query = {}) {
    tdx::server_detail::RequestTarget target;
    target.path = "/api/v1/pools/history";
    target.query = std::move(query);
    return target;
}

tdx::server_detail::RequestTarget catalog_request(
    std::map<std::string, std::string> query = {}) {
    tdx::server_detail::RequestTarget target;
    target.path = "/api/v1/pools";
    target.query = std::move(query);
    return target;
}

tdx::server_detail::RequestTarget evaluation_request(
    std::map<std::string, std::string> query) {
    tdx::server_detail::RequestTarget target;
    target.path = "/api/v1/pools/evaluate";
    target.query = std::move(query);
    return target;
}

}  // namespace

int main() {
    try {
        TemporaryTdxRoot fixture;
        const auto snapshot = fixture.path / "tpool" / "demo-pool" /
            "snapshot-cell" / "20260810.dat";
        const auto entry_11 = fixture.path / "tpool" / "demo-pool" /
            "entry-cell" / "20260811.log";
        const auto entry_12 = fixture.path / "tpool" / "demo-pool" /
            "entry-cell" / "20260812.log";
        const auto pool_file = fixture.path / "T0002" / "tpool" /
            "relative-pool.xml";
        write_fixture(snapshot, R"xml(<root><data>
<stk market="0" code="000001" indate="20260810" intime="093001"
 inprice="10.25" income="1.50" now="10.40" rise="1.46" volume="123400"
 maxrate="2.75" maxperiod="7" maxtime="20260810" maxprice="10.53"
 idaynum="2" />
</data></root>)xml");
        write_fixture(entry_11, R"xml(<root><data>
<stk market="1" code="600000" indate="20260811" intime="100500"
 inprice="9.80" />
</data></root>)xml");
        write_fixture(entry_12, R"xml(<root><data>
<stk market="2" code="430047" indate="20260812" intime="101500"
 inprice="12.56" />
</data></root>)xml");
        write_fixture(pool_file,
            R"xml(<root><cells><cell id="empty"></cell></cells></root>)xml");

        const std::map<fs::path, std::string> before{
            {snapshot, tdx::sha256_file(snapshot)},
            {entry_11, tdx::sha256_file(entry_11)},
            {entry_12, tdx::sha256_file(entry_12)},
            {pool_file, tdx::sha256_file(pool_file)}};

        tdx::server_detail::ApiState state;
        state.root = fixture.path;
        const auto catalog_response = tdx::server_detail::route(
            state, catalog_request());
        require(catalog_response.status == 200 &&
                    catalog_response.content_type.find("application/json") == 0,
                "TPool catalog route is a read-only JSON endpoint");
        const auto catalog = tdx::Json::parse(catalog_response.body);
        require(catalog.at("schema").as_string() ==
                    "tdx-tpool-catalog-v1" &&
                    catalog.at("path_scope").as_string() ==
                    "tdx-root-relative" &&
                    catalog.as_object().count("root") == 0 &&
                    catalog.at("pool_count").as_number() == 1,
                "catalog HTTP projection removes the configured root");
        require(catalog.at("pools").as_array().front()
                        .at("source").as_string() ==
                    "T0002/tpool/relative-pool.xml" &&
                    catalog_response.body.find(
                        fixture.path.filename().string()) ==
                        std::string::npos,
                "catalog exposes only a stable root-relative pool source");
        for (const auto& directory : catalog.at("directories").as_array()) {
            const auto& path = directory.at("path").as_string();
            require(!path.empty() && path.front() != '/' &&
                        path.find(':') == std::string::npos &&
                        path.find('\\') == std::string::npos,
                    "catalog directory paths are root-relative URLs");
        }

        const auto evaluation_response = tdx::server_detail::route(
            state, evaluation_request({
                {"source", "T0002/tpool/relative-pool.xml"},
                {"limit", "1"}, {"pages", "1"},
                {"page_size", "1"}}));
        require(evaluation_response.status == 200,
                "root-relative pool source round-trips through evaluation");
        const auto evaluation = tdx::Json::parse(evaluation_response.body);
        require(evaluation.at("source").as_string() ==
                    "T0002/tpool/relative-pool.xml" &&
                    evaluation.at("path_scope").as_string() ==
                    "tdx-root-relative" &&
                    evaluation.at("inspection").at("source").as_string() ==
                    "T0002/tpool/relative-pool.xml" &&
                    evaluation_response.body.find(
                        fixture.path.filename().string()) ==
                        std::string::npos,
                "evaluation response keeps both source fields root-relative");

        const auto response = tdx::server_detail::route(
            state, request({{"pool", "demo-pool"}, {"kind", "all"},
                            {"from", "20260810"}, {"to", "20260812"},
                            {"limit", "2"}}));
        require(response.status == 200 &&
                    response.content_type.find("application/json") == 0,
                "TPool history route is a read-only JSON endpoint (status " +
                    std::to_string(response.status) + ", content type " +
                    response.content_type + ")");
        const auto document = tdx::Json::parse(response.body);
        require(document.at("schema").as_string() ==
                    "tdx-tpool-history-catalog-v1" &&
                    document.at("path_scope").as_string() ==
                    "tdx-root-relative" &&
                    document.as_object().count("root") == 0,
                "HTTP projection keeps the domain schema without exposing root");
        require(document.at("matched_file_count").as_number() == 3 &&
                    document.at("file_count").as_number() == 2 &&
                    document.at("truncated").as_bool(),
                "limit applies after date/kind/root discovery");
        const auto& first = document.at("files").as_array().front();
        require(first.at("source").as_string() ==
                    "tpool/demo-pool/entry-cell/20260812.log" &&
                    first.at("pool").as_string() == "demo-pool" &&
                    first.at("cell").as_string() == "entry-cell" &&
                    first.at("history_date").as_number() == 20260812 &&
                    first.at("sha256").as_string() ==
                        tdx::lower_ascii(before.at(entry_12)),
                "files retain stable relative identity, date and source hash");
        require(response.body.find(fixture.path.filename().string()) ==
                    std::string::npos,
                "HTTP response never exposes the temporary absolute root");
        for (const auto& directory : document.at("directories").as_array()) {
            const auto& path = directory.at("path").as_string();
            require(!path.empty() && path.front() != '/' &&
                        path.find(':') == std::string::npos &&
                        path.find('\\') == std::string::npos,
                    "directory catalog paths are stable root-relative URLs");
        }

        const auto snapshot_only = tdx::server_detail::query_tpool_history(
            fixture.path,
            request({{"pool", "demo-pool"}, {"cell", "snapshot-cell"},
                     {"kind", "snapshot"}, {"from", "20260810"},
                     {"to", "20260810"}, {"limit", "10"}}));
        require(snapshot_only.at("file_count").as_number() == 1 &&
                    snapshot_only.at("files").as_array().front()
                        .at("kind").as_string() == "daily-snapshot" &&
                    snapshot_only.at("record_count").as_number() == 1,
                "snapshot, cell and inclusive date filters compose");

        const auto entry_only = tdx::server_detail::query_tpool_history(
            fixture.path,
            request({{"kind", "entry"}, {"from", "20260811"},
                     {"to", "20260811"}, {"limit", "10"}}));
        require(entry_only.at("file_count").as_number() == 1 &&
                    entry_only.at("files").as_array().front()
                        .at("source").as_string().find("20260811.log") !=
                        std::string::npos,
                "entry kind and exact date return the expected log");

        const auto empty = tdx::server_detail::query_tpool_history(
            fixture.path, request({{"pool", "absent"}, {"limit", "10"}}));
        require(empty.at("file_count").as_number() == 0 &&
                    empty.at("record_count").as_number() == 0 &&
                    !empty.at("truncated").as_bool(),
                "an unmatched filter returns a real empty catalog");

        require_error([&] {
            (void)tdx::server_detail::query_tpool_history(
                fixture.path, request({{"kind", "queue"}}));
        }, "kind must be all, snapshot or entry");
        require_error([&] {
            (void)tdx::server_detail::query_tpool_history(
                fixture.path, request({{"from", "2026-08-10"}}));
        }, "must be YYYYMMDD");
        require_error([&] {
            (void)tdx::server_detail::query_tpool_history(
                fixture.path, request({{"from", "20260230"}}));
        }, "must be YYYYMMDD");
        require_error([&] {
            (void)tdx::server_detail::query_tpool_history(
                fixture.path,
                request({{"from", "20260812"}, {"to", "20260810"}}));
        }, "from date exceeds to date");
        for (const auto* value : {"0", "10001"}) {
            require_error([&] {
                (void)tdx::server_detail::query_tpool_history(
                    fixture.path, request({{"limit", value}}));
            }, "limit must be in 1..10000");
        }
        for (const auto* name : {"input", "path", "url", "file"}) {
            require_error([&] {
                (void)tdx::server_detail::query_tpool_history(
                    fixture.path, request({{name, "C:/outside.dat"}}));
            }, std::string("does not accept parameter: ") + name);
        }
        require_error([&] {
            (void)tdx::server_detail::query_tpool_history(
                fixture.path, request({{"pool", "../outside"}}));
        }, "not a path");
        require_error([&] {
            (void)tdx::server_detail::query_tpool_catalog(
                fixture.path, catalog_request({{"path", "C:/outside"}}));
        }, "does not accept query parameters");
        require_error([&] {
            (void)tdx::server_detail::query_tpool_file_evaluation(
                fixture.path,
                evaluation_request({{"source", tdx::path_utf8(pool_file)}}),
                state.formulas);
        }, "root-relative XML path");
        require_error([&] {
            (void)tdx::server_detail::query_tpool_file_evaluation(
                fixture.path,
                evaluation_request({{"source", "../outside.xml"}}),
                state.formulas);
        }, "existing root-relative XML file");
        require_error([&] {
            (void)tdx::server_detail::query_tpool_file_evaluation(
                fixture.path,
                evaluation_request({
                    {"source", "T0002/tpool/relative-pool.xml"},
                    {"url", "https://example.invalid/pool.xml"}}),
                state.formulas);
        }, "does not accept parameter: url");

        for (const auto& [path, hash] : before)
            require(tdx::sha256_file(path) == hash,
                    "read-only history query preserves source hash");

        std::cout << "server TPool surface tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
