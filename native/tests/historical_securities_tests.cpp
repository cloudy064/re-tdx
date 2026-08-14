#include "tdx/common.hpp"
#include "tdx/historical_securities.hpp"

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
        ("tdx-historical-securities-" + std::to_string(
            std::chrono::high_resolution_clock::now().time_since_epoch().count()));
    Fixture() { fs::create_directories(root / "T0002" / "hq_cache"); }
    ~Fixture() {
        std::error_code ignored;
        fs::remove_all(root, ignored);
    }
    fs::path path() const {
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
        tdx::atomic_write_bytes(
            fixture.path(), tdx::encode_gbk(
                "0,000003,深金田A\r\n"
                "1,600001,邯郸钢铁\r\n"
                "2,920305,云创数据历史名\r\n"));

        tdx::SecurityCatalog current;
        current[{2, "920305"}] =
            {2, "bj", "北京", "920305", "云创数据"};

        tdx::HistoricalSecurityQuery all;
        const auto document = tdx::load_local_historical_securities(
            fixture.root, all, current);
        require(document.at("schema").as_string() ==
                    "tdx-market-historical-securities-native-v1", "schema");
        require(document.at("match_count").as_number() == 3, "row count");
        require(document.at("summary").at("current_directory_present")
                    .as_number() == 1, "current count");
        require(document.at("summary").at("absent_from_current_directory")
                    .as_number() == 2, "absent count");
        require(document.at("summary").at("name_differs_from_current")
                    .as_number() == 1, "renamed count");
        require(document.at("transport").at("network_requests").as_number() == 0,
                "offline transport");

        tdx::HistoricalSecurityQuery absent;
        absent.presence = "absent";
        absent.query = "钢铁";
        const auto absent_document = tdx::load_local_historical_securities(
            fixture.root, absent, current);
        require(absent_document.at("returned").as_number() == 1,
                "absent search");
        require(absent_document.at("records").as_array().front()
                    .at("security").at("security_id").as_string() == "SH600001",
                "security identity");

        tdx::HistoricalSecurityQuery current_only;
        current_only.market = "bj";
        current_only.presence = "current";
        const auto current_document = tdx::load_local_historical_securities(
            fixture.root, current_only, current);
        const auto& row = current_document.at("records").as_array().front();
        require(row.at("compatibility_name").as_string() == "云创数据历史名",
                "compatibility name");
        require(row.at("current_name").as_string() == "云创数据",
                "current name");
        require(row.at("name_differs_from_current").as_bool(),
                "name difference");

        tdx::HistoricalSecurityQuery bad;
        bad.presence = "deleted";
        require_error([&] {
            tdx::load_local_historical_securities(fixture.root, bad, current);
        }, "presence must be");

        tdx::atomic_write_text(fixture.path(), "0,123,broken\n");
        require_error([&] {
            tdx::load_local_historical_security_names(fixture.root);
        }, "six digits");

        std::cout << "historical securities tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
