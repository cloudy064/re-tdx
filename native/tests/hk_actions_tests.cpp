#include "tdx/common.hpp"
#include "tdx/hk_actions.hpp"

#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string_view>

namespace fs = std::filesystem;

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

bool close(double left, double right, double epsilon = 1e-8) {
    return std::abs(left - right) <= epsilon;
}

std::uint8_t nibble(char value) {
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    throw std::runtime_error("invalid fixture hex");
}

tdx::Bytes from_hex(std::string_view value) {
    require(value.size() % 2 == 0, "fixture hex length");
    tdx::Bytes result;
    result.reserve(value.size() / 2);
    for (std::size_t index = 0; index < value.size(); index += 2)
        result.push_back(static_cast<std::uint8_t>(
            (nibble(value[index]) << 4U) | nibble(value[index + 1])));
    return result;
}

tdx::Json kline_fixture() {
    tdx::Json bars = tdx::Json::array();
    for (const auto* date : {
             "1999-12-31", "2000-06-01", "2001-01-01", "2002-01-01"}) {
        tdx::Json bar = tdx::Json::object();
        bar["date"] = date;
        bar["time"] = "15:00";
        bar["open"] = 10.0;
        bar["high"] = 11.0;
        bar["low"] = 9.0;
        bar["close"] = 10.0;
        bars.push_back(std::move(bar));
    }
    tdx::Json result = tdx::Json::object();
    result["market"] = "31";
    result["code"] = "00001";
    result["bars"] = std::move(bars);
    return result;
}

struct Fixture {
    fs::path root = fs::temp_directory_path() /
        ("tdx-hk-actions-" + std::to_string(
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
            fixture.cache() / "hkqxinfo2.dat",
            from_hex(
                "d243703763612dcdd69baa15098dbbdeff01d43538602fed31f5fcbf741779be"
                "7e187647d953c87e0858b600038c7d5ab971a6c12a0764632880b549727ab774"
                "5f49c778c0b5897250c332f6c159591d5ab477f232c69ca79f9ca7c62fa6ca7"
                "cc73e83460b5da522426312a3ea1dfe12fce34bb24129968c300d0a"));
        tdx::atomic_write_bytes(
            fixture.cache() / "hkqxinfo.dat",
            from_hex(
                "d243703763612dcdb1d69a739775288cc76e4ac8494850e4925fcc3992d753a4"
                "7f29bd8a92d8667c3cf6e4aa03d35a59a075d1f2829874d8fd8fba64ad84d38"
                "76c196bf63c4edaaedc388a9aedebc4ab14a6e9749420ac55bbd8d5592a803ea"
                "9f08cddf51fe37a005c7b370f4897c78b920c79ffc6e532dbf61fa96d4b47b4"
                "da11593d2cbbf94ab830300d0a"));

        tdx::HkActionQuery all;
        all.order = "asc";
        const auto document = tdx::load_local_hk_actions(fixture.root, all);
        require(document.at("schema").as_string() ==
                    "tdx-market-hk-actions-native-v1", "schema");
        require(document.at("match_count").as_number() == 4, "all rows");
        require(document.at("summary").at("securities").as_number() == 2,
                "security summary");
        require(document.at("summary").at("earliest_date").as_string() ==
                    "20000101", "earliest date");
        require(document.at("summary").at("latest_date").as_string() ==
                    "20071213", "latest date");
        require(document.at("transport").at("network_requests").as_number() == 0,
                "offline transport");

        tdx::HkActionQuery security;
        security.code = "00001";
        security.order = "asc";
        const auto actions = tdx::load_local_hk_actions(fixture.root, security);
        require(actions.at("returned").as_number() == 3, "security rows");
        const auto& rows = actions.at("rows").as_array();
        require(rows[0].at("description").as_string() ==
                    "年度股息每股0.23港元", "GBK description decoding");
        require(close(rows[0].at("factors").at(
                    "event_additive_adjustment").as_number(), 0.23),
                "first cash adjustment");
        require(rows[1].at("kind").as_string() == "bonus", "bonus kind");
        require(close(rows[1].at("factors").at(
                    "event_share_multiplier").as_number(), 1.1),
                "bonus multiplier");
        require(close(rows[2].at("factors").at(
                    "previous_cumulative_multiplier").as_number(), 1.1),
                "cross-file previous multiplier");
        require(close(rows[2].at("factors").at(
                    "event_additive_adjustment").as_number(), 0.3),
                "mixed event additive adjustment");
        require(rows[2].at("kind").as_string() == "mixed", "mixed kind");

        tdx::HkActionQuery dividends;
        dividends.kind = "dividend";
        require(tdx::load_local_hk_actions(fixture.root, dividends)
                    .at("match_count").as_number() == 2,
                "kind flag includes mixed events");

        tdx::HkActionQuery rights;
        rights.kind = "rights";
        const auto rights_rows = tdx::load_local_hk_actions(fixture.root, rights);
        require(rights_rows.at("returned").as_number() == 1, "rights filter");
        require(close(rights_rows.at("rows").as_array().front().at("factors")
                    .at("event_additive_adjustment").as_number(), -2.12),
                "negative rights adjustment retained");

        tdx::HkActionQuery page;
        page.order = "asc";
        page.offset = 1;
        page.limit = 1;
        const auto paged = tdx::load_local_hk_actions(fixture.root, page);
        require(paged.at("match_count").as_number() == 4, "page match count");
        require(paged.at("returned").as_number() == 1, "page size");
        require(paged.at("rows").as_array().front().at("date").as_string() ==
                    "20010101", "page offset");

        const auto qfq = tdx::apply_hk_kline_adjustment(
            kline_fixture(), fixture.root, "00001", "qfq");
        const auto& qfq_bars = qfq.at("bars").as_array();
        require(close(qfq_bars[0].at("close").as_number(), 7.801653, 1e-5),
                "native HK qfq applies chronological affine steps");
        require(close(qfq_bars[2].at("close").as_number(), 8.818182, 1e-5),
                "native HK qfq keeps the ex-date on its new basis");
        require(close(qfq_bars[3].at("close").as_number(), 10.0),
                "native HK qfq keeps the latest basis unchanged");
        require(qfq.at("adjustment").at("method").as_string() ==
                    "tdx-hk-native-affine-v1", "HK adjustment provenance");

        const auto hfq = tdx::apply_hk_kline_adjustment(
            kline_fixture(), fixture.root, "00001", "hfq");
        const auto& hfq_bars = hfq.at("bars").as_array();
        require(close(hfq_bars[0].at("close").as_number(), 10.0),
                "native HK hfq keeps the earliest basis unchanged");
        require(close(hfq_bars[2].at("close").as_number(), 11.253, 1e-5),
                "native HK hfq starts at each ex-date");
        require(close(hfq_bars[3].at("close").as_number(), 12.6783, 1e-4),
                "native HK hfq composes chronological affine steps");

        const auto fixed = tdx::apply_hk_kline_adjustment(
            kline_fixture(), fixture.root, "00001", "fixed_qfq", "20010101");
        const auto& fixed_bars = fixed.at("bars").as_array();
        require(close(fixed_bars[2].at("close").as_number(), 10.0),
                "fixed HK adjustment keeps the anchor basis");
        require(close(fixed_bars[3].at("close").as_number(), 11.3, 1e-5),
                "fixed HK adjustment maps later bars backward to anchor basis");

        const auto divfactor = tdx::build_hk_divfactor_series_document(
            fixture.root, "00001", kline_fixture());
        require(divfactor.at("event_date_count").as_number() == 2,
                "HK DIVFACTOR share-change dates");
        require(divfactor.at("matched_bar_count").as_number() == 2,
                "HK DIVFACTOR matched K-line dates");
        require(close(divfactor.at("front").at("1999-12-31|15:00")
                    .as_number(), 1.0 / 1.21, 1e-6),
                "HK DIVFACTOR front float32 series");
        require(close(divfactor.at("back").at("2002-01-01|15:00")
                    .as_number(), 1.21, 1e-6),
                "HK DIVFACTOR back float32 series");

        tdx::HkActionQuery invalid;
        invalid.code = "0001";
        require_error([&] {
            tdx::load_local_hk_actions(fixture.root, invalid);
        }, "five digits");

        std::cout << "HK action tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
