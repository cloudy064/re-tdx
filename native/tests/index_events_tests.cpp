#include "tdx/common.hpp"
#include "tdx/index_events.hpp"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace fs = std::filesystem;

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

struct Fixture {
    fs::path root = fs::temp_directory_path() /
        ("tdx-index-events-" + std::to_string(
            std::chrono::high_resolution_clock::now().time_since_epoch().count()));
    Fixture() { fs::create_directories(root / "T0002" / "hq_cache"); }
    ~Fixture() {
        std::error_code ignored;
        fs::remove_all(root, ignored);
    }
    fs::path cache() const { return root / "T0002" / "hq_cache"; }
};

template <typename Callback>
void require_error(Callback&& callback, const char* needle) {
    try {
        callback();
    } catch (const std::exception& error) {
        require(std::string(error.what()).find(needle) != std::string::npos,
                "unexpected error text");
        return;
    }
    throw std::runtime_error("expected error");
}

}  // namespace

int main() {
    try {
        Fixture fixture;
        tdx::atomic_write_bytes(
            fixture.cache() / "speczsevent.txt",
            tdx::encode_gbk(
                "1231|20240102|跨年事件|100\r\n"
                "0103|20240103|普通事件|101\r\n"));
        tdx::atomic_write_bytes(
            fixture.cache() / "speczsevent_ds.txt",
            tdx::encode_gbk(
                "0106|20240108|港股事件|200|1\r\n"
                "0105|20240105|纳指事件|300|2\r\n"));
        tdx::atomic_write_bytes(
            fixture.cache() / "neednote.dat",
            tdx::encode_gbk(
                "[URL]\r\n"
                "ZSEventUrl=http://www.treeid/dlghttp://example.test/event/%d\r\n"));

        tdx::IndexEventQuery all;
        const auto document = tdx::load_local_index_events(fixture.root, all);
        require(document.at("schema").as_string() ==
                    "tdx-market-index-events-native-v1", "schema");
        require(document.at("match_count").as_number() == 4, "row count");
        require(document.at("summary").at("shanghai_composite").as_number() == 2,
                "Shanghai count");
        require(document.at("summary").at("hang_seng").as_number() == 1,
                "Hang Seng count");
        require(document.at("summary").at("nasdaq_composite").as_number() == 1,
                "Nasdaq count");
        require(document.at("summary").at("chart_date_adjusted").as_number() == 2,
                "adjusted chart dates");
        require(document.at("transport").at("network_requests").as_number() == 0,
                "offline transport");

        tdx::IndexEventQuery event;
        event.event_id = "100";
        const auto selected = tdx::load_local_index_events(fixture.root, event);
        require(selected.at("returned").as_number() == 1, "event id filter");
        const auto& row = selected.at("records").as_array().front();
        require(row.at("occurrence_date").as_string() == "20231231",
                "cross-year occurrence date");
        require(row.at("chart_date").as_string() == "20240102",
                "chart date");
        require(row.at("chart_date_adjusted").as_bool(), "adjustment flag");
        require(row.at("detail_url").as_string() ==
                    "http://example.test/event/100", "browser detail URL");
        require(row.at("native_detail_target").as_string() ==
                    "http://www.treeid/dlghttp://example.test/event/100",
                "native detail target");

        tdx::IndexEventQuery occurrence;
        occurrence.date_basis = "occurrence";
        occurrence.date_from = "20231231";
        occurrence.date_to = "20231231";
        const auto occurrence_page = tdx::load_local_index_events(
            fixture.root, occurrence);
        require(occurrence_page.at("returned").as_number() == 1,
                "occurrence date filter");

        tdx::IndexEventQuery chart;
        chart.benchmark = "hk";
        chart.date_from = "20240108";
        chart.date_to = "20240108";
        const auto chart_page = tdx::load_local_index_events(fixture.root, chart);
        require(chart_page.at("returned").as_number() == 1,
                "chart date and benchmark filters");
        require(chart_page.at("records").as_array().front().at("native_kind")
                    .as_number() == 1, "native kind");

        tdx::IndexEventQuery bad;
        bad.benchmark = "dax";
        require_error([&] {
            tdx::load_local_index_events(fixture.root, bad);
        }, "benchmark must be");

        tdx::atomic_write_text(
            fixture.cache() / "speczsevent_ds.txt", "0101|20240101|broken\n");
        require_error([&] {
            tdx::load_local_index_events(fixture.root, all);
        }, "expected 5 columns");

        std::cout << "index events tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
