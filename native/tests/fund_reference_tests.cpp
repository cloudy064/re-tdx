#include "tdx/common.hpp"
#include "tdx/fund_reference.hpp"

#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <map>
#include <stdexcept>

namespace fs = std::filesystem;

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

bool close(double left, double right, double epsilon = 1e-8) {
    return std::abs(left - right) <= epsilon;
}

struct Fixture {
    fs::path root = fs::temp_directory_path() /
        ("tdx-fund-reference-" + std::to_string(
            std::chrono::high_resolution_clock::now().time_since_epoch().count()));

    Fixture() {
        fs::create_directories(root / "T0002" / "hq_cache");
    }

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
        tdx::atomic_write_text(
            fixture.cache() / "specjjdata.txt",
            "158006,0,,20260807,9404.60,1.0477,1.0359\r\n"
            "589890,1,,20260807,8998.70,1.6157,1.5642\r\n");
        tdx::atomic_write_text(
            fixture.cache() / "specetfdata.txt",
            "0,159004,,0,jjjl0000060,,20130513,20130513\r\n"
            "1,510210,000001,1,jjjl0000040,,20110105,20110124\r\n"
            "0,159518,IXIC,12,jjjl0000037,,20261016,20261027\r\n");
        tdx::atomic_write_text(
            fixture.cache() / "speclofdata.txt",
            "0,160105,000001,1,jjjl0000033,\r\n"
            "0,160125,HSI,27,jjjl0000033,\r\n"
            "1,506001,000171,62,jjjl0000050,\r\n");

        std::map<std::pair<int, std::string>, tdx::Security> securities;
        securities[{0, "158006"}] = {0, "sz", "深圳", "158006", "化工ETF博时"};
        securities[{1, "510210"}] = {1, "sh", "上海", "510210", "综指ETF"};

        tdx::FundReferenceQuery all;
        all.as_of_date = "20260812";
        const auto document = tdx::load_local_fund_reference(
            fixture.root, all, securities);
        require(document.at("schema").as_string() ==
                    "tdx-market-fund-reference-native-v1", "schema");
        require(document.at("match_count").as_number() == 8, "all row count");
        require(document.at("summary").at("fund_snapshots").as_number() == 2,
                "snapshot summary");
        require(document.at("summary").at("etf_mappings").as_number() == 3,
                "mapping summary");
        require(document.at("summary").at("lof_mappings").as_number() == 3,
                "LOF mapping summary");
        require(document.at("transport").at("network_requests").as_number() == 0,
                "offline transport");

        const auto& snapshot = document.at("records").as_array().front();
        require(snapshot.at("security").at("name").as_string() == "化工ETF博时",
                "security name resolution");
        require(close(snapshot.at("fund_units").as_number(), 94046000.0),
                "ten-thousand fund units conversion");
        require(close(snapshot.at("unit_reference_value").as_number(), 1.0477),
                "native unit reference value");
        require(close(snapshot.at("unit_nav").as_number(), 1.0359),
                "published unit NAV");
        require(snapshot.at("native_semantics").at(
                    "published_unit_nav_consumed_by_host").as_bool() == false,
                "host ignores seventh column");

        tdx::FundReferenceQuery mapping;
        mapping.view = "etf-mapping";
        mapping.market = "sh";
        mapping.code = "510210";
        mapping.as_of_date = "20110110";
        const auto mapped = tdx::load_local_fund_reference(
            fixture.root, mapping, securities);
        require(mapped.at("returned").as_number() == 1, "mapping filter");
        const auto& row = mapped.at("records").as_array().front();
        require(row.at("reference_instrument").at("source_code").as_string() ==
                    "000001", "source reference retained");
        require(row.at("reference_instrument").at("code").as_string() ==
                    "999999", "Shanghai composite alias");
        require(row.at("native_lifecycle_status").as_number() == 2,
                "native lifecycle in-window status");

        tdx::FundReferenceQuery alias;
        alias.view = "etf-mapping";
        alias.query = "A_IXIC";
        alias.as_of_date = "20260812";
        const auto aliases = tdx::load_local_fund_reference(
            fixture.root, alias, securities);
        require(aliases.at("returned").as_number() == 1, "alias search");
        require(aliases.at("records").as_array().front().at("reference_instrument")
                    .at("code").as_string() == "A_IXIC", "US index alias");
        require(aliases.at("records").as_array().front()
                    .at("native_lifecycle_status").as_number() == 1,
                "native lifecycle before-window status");

        tdx::atomic_write_text(
            fixture.cache() / "specetfdata.txt",
            "0,159025,,0,jjjl0000032,,,\r\n"
            "0,158011,,0,jjjl0000034,,20200101,\r\n");
        tdx::FundReferenceQuery undated;
        undated.view = "etf-mapping";
        undated.as_of_date = "20260813";
        const auto undated_document = tdx::load_local_fund_reference(
            fixture.root, undated, securities);
        require(undated_document.at("returned").as_number() == 2,
                "native undated ETF rows remain available");
        require(undated_document.at("summary").at("native_status_unset")
                    .as_number() == 2,
                "undated ETF rows are counted without inventing lifecycle state");
        const auto& undated_rows = undated_document.at("records").as_array();
        require(undated_rows[0].at("dates").at("window_start").is_null() &&
                    undated_rows[0].at("dates").at("window_end").is_null() &&
                    undated_rows[0].at("native_lifecycle_status").is_null() &&
                    undated_rows[0].at("lifecycle_phase").is_null(),
                "empty native lifecycle dates stay explicitly unresolved");
        require(undated_rows[1].at("dates").at("window_start").as_string() ==
                    "20200101" &&
                    undated_rows[1].at("dates").at("window_end").is_null() &&
                    undated_rows[1].at("native_lifecycle_status").is_null(),
                "one-sided native lifecycle date remains unresolved");

        tdx::atomic_write_text(
            fixture.cache() / "specetfdata.txt",
            "0,159025,,0,jjjl0000032,,2020010,\r\n");
        require_error([&] {
            tdx::load_local_fund_reference(
                fixture.root, undated, securities);
        }, "empty or YYYYMMDD");

        tdx::FundReferenceQuery lof;
        lof.view = "lof-mapping";
        lof.code = "160125";
        const auto lof_document = tdx::load_local_fund_reference(
            fixture.root, lof, securities);
        require(lof_document.at("returned").as_number() == 1, "LOF filter");
        const auto& lof_row = lof_document.at("records").as_array().front();
        require(lof_row.at("reference_instrument").at("native_market_id")
                    .as_number() == 27, "LOF non-A-share reference market");
        require(lof_row.at("reference_instrument").at("code").as_string() == "HSI",
                "LOF reference code");
        require(lof_row.at("native_semantics").at("record_size").as_number() == 40,
                "LOF native record size");

        tdx::FundReferenceQuery bad_market;
        bad_market.market = "bj";
        require_error([&] {
            tdx::load_local_fund_reference(fixture.root, bad_market, securities);
        }, "market must be");

        tdx::atomic_write_text(
            fixture.cache() / "specjjdata.txt", "158006,0,broken\n");
        tdx::FundReferenceQuery malformed;
        malformed.view = "snapshot";
        require_error([&] {
            tdx::load_local_fund_reference(fixture.root, malformed, securities);
        }, "expected 7 columns");

        std::cout << "fund reference tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
