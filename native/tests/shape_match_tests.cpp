#include "tdx/common.hpp"
#include "tdx/shape_match.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string_view>

namespace fs = std::filesystem;

namespace {

constexpr std::size_t kHeaderSize = 4;
constexpr std::size_t kRecordSize = 94195;

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

bool close(double left, double right, double epsilon = 1e-6) {
    return std::abs(left - right) <= epsilon;
}

void write_u16(tdx::Bytes& data, std::size_t offset, std::uint16_t value) {
    data[offset] = static_cast<std::uint8_t>(value);
    data[offset + 1] = static_cast<std::uint8_t>(value >> 8U);
}

void write_u32(tdx::Bytes& data, std::size_t offset, std::uint32_t value) {
    for (std::size_t index = 0; index < 4; ++index)
        data[offset + index] =
            static_cast<std::uint8_t>(value >> (index * 8U));
}

void write_i32(tdx::Bytes& data, std::size_t offset, std::int32_t value) {
    write_u32(data, offset, static_cast<std::uint32_t>(value));
}

void write_float(tdx::Bytes& data, std::size_t offset, float value) {
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    write_u32(data, offset, bits);
}

void write_text(tdx::Bytes& data, std::size_t offset, std::size_t width,
                std::string_view text) {
    const auto encoded = tdx::encode_gbk(text);
    require(encoded.size() < width, "fixture text exceeds fixed field");
    std::copy(encoded.begin(), encoded.end(),
              data.begin() + static_cast<std::ptrdiff_t>(offset));
    data[offset + encoded.size()] = 0;
}

void write_bar(tdx::Bytes& data, std::size_t offset, int day,
               float open, float close, float volume) {
    write_u16(data, offset, 2026);
    data[offset + 2] = 8;
    data[offset + 3] = static_cast<std::uint8_t>(day);
    write_float(data, offset + 7, open);
    write_float(data, offset + 11, std::max(open, close));
    write_float(data, offset + 15, std::min(open, close));
    write_float(data, offset + 19, close);
    write_float(data, offset + 27, volume);
}

struct Fixture {
    fs::path root = fs::temp_directory_path() /
        ("tdx-shape-match-" + std::to_string(
            std::chrono::high_resolution_clock::now().time_since_epoch().count()));
    fs::path library = root / "T0002" / "shapematch.dat";
    fs::path candidate = root / "candidate.json";
    fs::path scan_input = root / "scan-input.json";

    Fixture() {
        fs::create_directories(library.parent_path());
        tdx::Bytes data(kHeaderSize + 2 * kRecordSize, 0);
        write_i32(data, 0, 0);

        const auto security = kHeaderSize;
        write_u32(data, security, 1700000000U);
        write_text(data, security + 4, 45, "平安模板");
        write_u16(data, security + 49, 0);
        write_u16(data, security + 51, 4);
        write_i32(data, security + 53, 4);
        write_u16(data, security + 57, 0);
        write_text(data, security + 59, 23, "000001");
        write_text(data, security + 82, 45, "平安银行");
        for (int index = 0; index < 4; ++index)
            write_bar(data, security + 127 + index * 35, index + 1,
                      static_cast<float>(index + 1),
                      static_cast<float>((index + 1) * 2),
                      static_cast<float>((index + 1) * 100));
        write_i32(data, security + 94131, 90);
        data[security + 94135] = 1;
        write_float(data, security + 94155, 1.0F);

        const auto drawing = kHeaderSize + kRecordSize;
        write_u32(data, drawing, 1700000001U);
        write_text(data, drawing + 4, 45, "手绘模板");
        write_u16(data, drawing + 49, 1);
        write_u16(data, drawing + 51, 4);
        write_i32(data, drawing + 53, 4);
        for (int index = 0; index < 4; ++index) {
            const auto point = drawing + 70131 + index * 12;
            write_i32(data, point, 10 + index);
            write_i32(data, point + 4, 20 + index);
            write_float(data, point + 8, static_cast<float>(index + 1));
        }
        write_i32(data, drawing + 94131, 90);
        tdx::atomic_write_bytes(library, data);

        tdx::Json document = tdx::Json::object();
        document["schema"] = "tdx-minute-v1";
        tdx::Json bars = tdx::Json::array();
        for (int index = 0; index < 4; ++index) {
            tdx::Json bar = tdx::Json::object();
            bar["open"] = static_cast<double>(index + 1);
            bar["close"] = static_cast<double>(index + 1);
            bar["volume"] = static_cast<double>((index + 1) * 10);
            bars.push_back(std::move(bar));
        }
        document["bars"] = std::move(bars);
        tdx::atomic_write_text(candidate, document.dump(2) + "\n");

        auto scan_row = [&](std::string market, std::string code,
                            std::string name,
                            const std::array<double, 4>& closes) {
            tdx::Json row = tdx::Json::object();
            row["market"] = std::move(market);
            row["code"] = std::move(code);
            row["name"] = std::move(name);
            tdx::Json scan_bars = tdx::Json::array();
            for (std::size_t index = 0; index < closes.size(); ++index) {
                tdx::Json bar = tdx::Json::object();
                bar["open"] = static_cast<double>(index + 1);
                bar["close"] = closes[index];
                bar["volume"] = static_cast<double>((index + 1) * 10);
                scan_bars.push_back(std::move(bar));
            }
            row["bars"] = std::move(scan_bars);
            return row;
        };
        tdx::Json bundle = tdx::Json::object();
        tdx::Json securities = tdx::Json::array();
        securities.push_back(scan_row(
            "sz", "000001", "命中", {1.0, 2.0, 3.0, 4.0}));
        securities.push_back(scan_row(
            "sh", "600000", "反向", {4.0, 3.0, 2.0, 1.0}));
        securities.push_back(scan_row(
            "bj", "920001", "平坦", {3.0, 3.0, 3.0, 3.0}));
        bundle["securities"] = std::move(securities);
        tdx::atomic_write_text(scan_input, bundle.dump(2) + "\n");
    }

    ~Fixture() {
        std::error_code ignored;
        fs::remove_all(root, ignored);
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
        const std::vector<float> ascending{1, 2, 3, 4};
        const std::vector<float> scaled{2, 4, 6, 8};
        const std::vector<float> reversed{4, 3, 2, 1};
        const std::vector<float> flat{3, 3, 3, 3};
        const std::vector<float> leading_zero{0, 0, 3, 4};
        require(close(tdx::shape_series_similarity(
                          ascending, ascending, ascending.size()), 1.0),
                "identical Pearson vector");
        require(close(tdx::shape_series_similarity(
                          ascending, scaled, ascending.size()), 1.0),
                "scaled Pearson vector");
        require(close(tdx::shape_series_similarity(
                          ascending, reversed, ascending.size()), -1.0),
                "reversed Pearson vector");
        require(close(tdx::shape_series_similarity(
                          flat, flat, flat.size()), 0.0),
                "zero variance preserves zero output");
        require(close(tdx::shape_series_similarity(
                          leading_zero, leading_zero, 2), 1.0),
                "native leading floor and trailing window");

        Fixture fixture;
        const auto library = tdx::load_shape_match(
            fixture.root, tdx::ShapeMatchQuery{});
        require(library.at("schema").as_string() ==
                    "tdx-shape-match-native-v1", "schema");
        require(!library.at("network_used").as_bool(), "library is offline");
        require(library.at("scope").as_string() == "all-a-shares", "scope");
        require(library.at("summary").at("template_count").as_number() == 2,
                "template count");
        require(library.at("summary").at(
                    "security_template_count").as_number() == 1,
                "security template count");
        require(library.at("summary").at(
                    "hand_drawn_template_count").as_number() == 1,
                "drawing template count");
        const auto& first = library.at("templates").as_array().front();
        require(first.at("name").as_string() == "平安模板", "GBK name");
        require(first.at("period").as_string() == "day", "period mapping");
        require(first.at("start").as_string() == "2026-08-01",
                "first bar date");
        require(first.at("end").as_string() == "2026-08-04",
                "last bar date");

        tdx::ShapeMatchQuery score;
        score.view = "score";
        score.template_index = 0;
        score.candidate_path = fixture.candidate;
        const auto security_score = tdx::load_shape_match(fixture.root, score);
        require(security_score.at("score").at("eligible").as_bool(),
                "security score eligible");
        require(security_score.at("score").at("matched").as_bool(),
                "security score match");
        require(close(security_score.at("score").at("score").as_number(), 1.0),
                "security close correlation");
        require(!security_score.at("network_used").as_bool(),
                "candidate JSON is offline");

        tdx::ShapeMatchQuery scan;
        scan.view = "scan";
        scan.template_index = 0;
        scan.scan_input_path = fixture.scan_input;
        scan.source = "local";
        const auto matched_scan = tdx::load_shape_match(fixture.root, scan);
        require(!matched_scan.at("network_used").as_bool(),
                "embedded scan is offline");
        require(matched_scan.at("scan").at("summary").at(
                    "candidate_count").as_number() == 3,
                "scan candidate count");
        require(matched_scan.at("scan").at("summary").at(
                    "matched_count").as_number() == 1,
                "scan matched count");
        const auto& matched_rows = matched_scan.at("scan").at(
            "results").as_array();
        require(matched_rows.size() == 1, "matched-only result count");
        require(matched_rows.front().at("security").at("code").as_string() ==
                    "000001", "scan rank winner");

        scan.include_unmatched = true;
        scan.result_limit = 2;
        const auto diagnostic_scan = tdx::load_shape_match(fixture.root, scan);
        const auto& diagnostic_rows = diagnostic_scan.at("scan").at(
            "results").as_array();
        require(diagnostic_rows.size() == 2, "scan result limit");
        require(diagnostic_scan.at("scan").at("summary").at(
                    "truncated").as_bool(), "scan truncation flag");

        tdx::ShapeMatchQuery guarded_scan;
        guarded_scan.view = "scan";
        guarded_scan.template_index = 0;
        guarded_scan.source = "network";
        guarded_scan.securities = {"sz:000001", "sh:600000"};
        guarded_scan.max_network_requests = 1;
        require_error([&] {
            tdx::load_shape_match(fixture.root, guarded_scan);
        }, "exceeds --max-network-requests");

        score.template_index = 1;
        const auto drawing_score = tdx::load_shape_match(fixture.root, score);
        require(drawing_score.at("score").at("matched").as_bool(),
                "hand-drawn score match");
        require(close(drawing_score.at("score").at("score").as_number(), 1.0),
                "hand-drawn close correlation");

        const auto empty_root = fixture.root / "empty";
        fs::create_directories(empty_root / "T0002");
        const auto empty = tdx::load_shape_match(
            empty_root, tdx::ShapeMatchQuery{});
        require(!empty.at("library_available").as_bool(),
                "absent library is a clean empty state");
        require(empty.at("summary").at("template_count").as_number() == 0,
                "empty template count");

        const auto malformed = fixture.root / "malformed.dat";
        tdx::atomic_write_bytes(malformed, {0, 0, 0, 0, 1});
        tdx::ShapeMatchQuery malformed_query;
        malformed_query.library_path = malformed;
        require_error([&] {
            tdx::load_shape_match(fixture.root, malformed_query);
        }, "4 + N * 94195");

        std::cout << "Shape-match tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
