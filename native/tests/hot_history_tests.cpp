#include "tdx/common.hpp"
#include "tdx/hot_history.hpp"

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
        ("tdx-hot-history-" + std::to_string(
            std::chrono::high_resolution_clock::now().time_since_epoch().count()));

    Fixture() { fs::create_directories(root / "T0002" / "hq_cache"); }
    ~Fixture() {
        std::error_code ignored;
        fs::remove_all(root, ignored);
    }
    fs::path path() const {
        return root / "T0002" / "hq_cache" / "speczshot.txt";
    }
    fs::path historical_path() const {
        return root / "T0002" / "hq_cache" / "pttab.dat";
    }
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
        const std::string source =
            "1|603137|20260615|20260715|22|拟收购存储公司|202.80|202.80|存储器收购\r\n"
            "0|000798|20251114|20251124|7|水产品|94.84|95.50|客户端段|完整补充\r\n"
            "2|920001|20240102|20240108|5|北交所主题|40.00|48.00|北交所说明\r\n";
        tdx::atomic_write_bytes(fixture.path(), tdx::encode_gbk(source));
        tdx::atomic_write_bytes(
            fixture.historical_path(),
            tdx::encode_gbk("2,920001,北交所历史名称\r\n"));

        std::map<std::pair<int, std::string>, tdx::Security> securities;
        securities[{1, "603137"}] = {1, "sh", "上海", "603137", "恒尚节能"};
        securities[{0, "000798"}] = {0, "sz", "深圳", "000798", "中水渔业"};

        tdx::HotHistoryQuery all;
        const auto document = tdx::load_local_hot_history(
            fixture.root, all, securities);
        require(document.at("schema").as_string() ==
                    "tdx-market-hot-history-native-v1", "schema");
        require(document.at("match_count").as_number() == 3, "all rows");
        require(document.at("summary").at("security_count").as_number() == 3,
                "security count");
        require(document.at("summary").at("embedded_delimiter_records")
                    .as_number() == 1, "embedded delimiter count");
        require(document.at("summary").at("historical_name_fallback_records")
                    .as_number() == 1, "historical name fallback count");
        require(document.at("transport").at("network_requests").as_number() == 0,
                "offline transport");
        const auto& first = document.at("records").as_array().front();
        require(first.at("security").at("code").as_string() == "603137",
                "default descending order");
        require(first.at("security").at("name").as_string() == "恒尚节能",
                "security name");
        require(close(first.at("interval_return_pct").as_number(), 202.80),
                "interval return");

        tdx::HotHistoryQuery recovered;
        recovered.query = "完整补充";
        const auto recovered_document = tdx::load_local_hot_history(
            fixture.root, recovered, securities);
        require(recovered_document.at("returned").as_number() == 1,
                "search recovered analysis");
        const auto& recovered_row =
            recovered_document.at("records").as_array().front();
        require(recovered_row.at("analysis").as_string() == "客户端段|完整补充",
                "lossless analysis");
        require(recovered_row.at("native_client_analysis").as_string() ==
                    "客户端段", "native segment");
        require(recovered_row.at("analysis_contains_embedded_delimiter")
                    .as_bool(), "delimiter flag");

        tdx::HotHistoryQuery overlap;
        overlap.market = "sz";
        overlap.code = "000798";
        overlap.date_from = "20251120";
        overlap.date_to = "20251121";
        const auto overlap_document = tdx::load_local_hot_history(
            fixture.root, overlap, securities);
        require(overlap_document.at("returned").as_number() == 1,
                "overlap filter");
        require(overlap_document.at("mode").as_string() == "security",
                "security mode");

        tdx::HotHistoryQuery bj;
        bj.market = "2";
        const auto bj_document = tdx::load_local_hot_history(
            fixture.root, bj, securities);
        require(bj_document.at("returned").as_number() == 1, "Beijing filter");
        require(bj_document.at("records").as_array().front().at("security")
                    .at("name").as_string() == "北交所历史名称",
                "historical compatibility name");
        require(bj_document.at("records").as_array().front().at("security")
                    .at("name_source").as_string() == "historical-compatibility",
                "historical compatibility source");
        require(!bj_document.at("records").as_array().front().at("native_host_eligible")
                    .as_bool(), "unresolved native host eligibility");

        tdx::HotHistoryQuery paged;
        paged.sort = "peak";
        paged.order = "asc";
        paged.offset = 1;
        paged.limit = 1;
        const auto page = tdx::load_local_hot_history(
            fixture.root, paged, securities);
        require(page.at("returned").as_number() == 1, "page size");
        require(page.at("has_more").as_bool(), "has more");
        require(close(page.at("records").as_array().front().at("peak_return_pct")
                    .as_number(), 95.50), "peak sort");

        tdx::HotHistoryQuery bad_market;
        bad_market.market = "hk";
        require_error([&] {
            tdx::load_local_hot_history(fixture.root, bad_market, securities);
        }, "market must be");

        tdx::atomic_write_text(fixture.path(), "1|603137|broken\n");
        require_error([&] {
            tdx::load_local_hot_history(fixture.root, all, securities);
        }, "expected at least 9 columns");

        std::cout << "hot history tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
