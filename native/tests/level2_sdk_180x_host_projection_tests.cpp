#include "tdx/common.hpp"
#include "tdx/level2.hpp"

#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <functional>
#include <iostream>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw tdx::Error(message);
}

void require_number(const tdx::Json& value, std::int64_t expected,
                    const char* message) {
    require(value.is_number() &&
                value.as_number() == static_cast<double>(expected),
            message);
}

void require_throws(const std::function<void()>& action,
                    const char* expected_fragment) {
    try {
        action();
    } catch (const tdx::Error& error) {
        require(std::string(error.what()).find(expected_fragment) !=
                    std::string::npos,
                "error includes expected contract detail");
        return;
    }
    throw tdx::Error("expected contract validation failure");
}

void put_u32(tdx::Bytes& body, std::size_t offset, std::uint32_t value) {
    for (std::size_t index = 0; index < 4; ++index)
        body[offset + index] =
            static_cast<std::uint8_t>(value >> (8U * index));
}

void put_u64(tdx::Bytes& body, std::size_t offset, std::uint64_t value) {
    put_u32(body, offset, static_cast<std::uint32_t>(value));
    put_u32(body, offset + 4, static_cast<std::uint32_t>(value >> 32U));
}

void put_i64(tdx::Bytes& body, std::size_t offset, std::int64_t value) {
    std::uint64_t bits{};
    std::memcpy(&bits, &value, sizeof(bits));
    put_u64(body, offset, bits);
}

void put_f64(tdx::Bytes& body, std::size_t offset, double value) {
    std::uint64_t bits{};
    std::memcpy(&bits, &value, sizeof(bits));
    put_u64(body, offset, bits);
}

std::int64_t local_epoch_ms(int hour, int minute, int second = 0) {
    std::tm value{};
    value.tm_year = 126;
    value.tm_mon = 0;
    value.tm_mday = 15;
    value.tm_hour = hour;
    value.tm_min = minute;
    value.tm_sec = second;
    value.tm_isdst = -1;
    const auto stamp = std::mktime(&value);
    if (stamp == static_cast<std::time_t>(-1))
        throw tdx::Error("test cannot construct local timestamp");
    return static_cast<std::int64_t>(stamp) * 1000;
}

tdx::Bytes transaction_record(std::int64_t epoch_ms = 0,
                              double price = 0.0,
                              std::int64_t volume = 0,
                              std::uint32_t first_raw = 0,
                              std::uint32_t second_raw = 0,
                              std::uint32_t qualifier_raw = 0) {
    tdx::Bytes result(52, 0);
    put_i64(result, 0, epoch_ms);
    put_f64(result, 8, price);
    put_i64(result, 16, volume);
    put_u32(result, 36, first_raw);
    put_u32(result, 40, second_raw);
    put_u32(result, 48, qualifier_raw);
    return result;
}

tdx::Bytes order_record(std::int64_t epoch_ms = 0,
                        double price = 0.0,
                        std::int64_t volume = 0,
                        std::uint32_t order_id_raw = 0,
                        std::uint32_t record_type_raw = 0) {
    tdx::Bytes result(40, 0);
    put_i64(result, 0, epoch_ms);
    put_f64(result, 8, price);
    put_i64(result, 16, volume);
    put_u32(result, 24, order_id_raw);
    put_u32(result, 36, record_type_raw);
    return result;
}

tdx::Json project_1801(std::uint16_t market, std::string code,
                       tdx::Bytes body) {
    tdx::Level2Sdk1801HostProjectionRequest request;
    request.market_id = market;
    request.code = std::move(code);
    request.body = std::move(body);
    return tdx::project_level2_sdk_1801_host_projection(request);
}

tdx::Json project_1802(std::uint16_t market, std::string code,
                       tdx::Bytes body) {
    tdx::Level2Sdk1802HostProjectionRequest request;
    request.market_id = market;
    request.code = std::move(code);
    request.body = std::move(body);
    return tdx::project_level2_sdk_1802_host_projection(request);
}

const tdx::Json& state(const tdx::Json& result) {
    return result.at("projected_state");
}

const tdx::Json& record(const tdx::Json& result, std::size_t index = 0) {
    return state(result).at("records").as_array().at(index);
}

void append(tdx::Bytes& target, const tdx::Bytes& source) {
    target.insert(target.end(), source.begin(), source.end());
}

void test_complete_security_classifier_and_lot_divisor() {
    struct Case {
        std::uint16_t market;
        const char* code;
        int security_class_raw;
        int lot_divisor_raw;
    };
    const std::vector<Case> cases{
        {0, "002000", 8, 100}, {0, "000000", 0, 100},
        {0, "030000", 1, 100}, {0, "080000", 1, 100},
        {0, "010000", 10, 100}, {0, "100000", 2, 10},
        {0, "190000", 2, 10}, {0, "110000", 3, 10},
        {0, "140000", 3, 10}, {0, "120000", 3, 10},
        {0, "121400", 4, 10}, {0, "121500", 3, 10},
        {0, "131000", 5, 10}, {0, "130000", 3, 10},
        {0, "150000", 6, 100}, {0, "200000", 7, 100},
        {0, "230000", 1, 100}, {0, "300000", 9, 100},
        {0, "380000", 1, 100}, {0, "500000", 3, 10},
        {0, "560000", 2, 10}, {0, "530000", 10, 100},
        {1, "000999", 20, 100}, {1, "001000", 13, 10},
        {1, "100000", 14, 10}, {1, "110000", 15, 10},
        {1, "112000", 14, 10}, {1, "120000", 14, 10},
        {1, "200000", 16, 10}, {1, "230000", 13, 10},
        {1, "240000", 14, 10}, {1, "580000", 12, 100},
        {1, "582000", 12, 100}, {1, "581000", 17, 100},
        {1, "600000", 11, 100}, {1, "688000", 19, 100},
        {1, "689000", 19, 100}, {1, "680000", 11, 100},
        {1, "750000", 13, 10}, {1, "770000", 13, 10},
        {1, "700000", 20, 100}, {1, "900000", 18, 100},
        {1, "901000", 20, 100}, {1, "300000", 20, 100},
        {2, "430000", 21, 100}, {2, "400000", 24, 100},
        {2, "830000", 21, 100}, {2, "870000", 21, 100},
        {2, "810000", 22, 10}, {2, "821000", 23, 10},
        {2, "820000", 24, 100}, {2, "920000", 21, 100},
        {2, "921000", 24, 100}, {2, "910000", 24, 100},
    };
    for (const auto& fixture : cases) {
        const auto result = project_1802(
            fixture.market, fixture.code, order_record());
        require_number(result.at("security_class_raw"),
                       fixture.security_class_raw,
                       "security classifier reproduces sub_5960B0");
        require_number(result.at("lot_divisor_raw"),
                       fixture.lot_divisor_raw,
                       "lot divisor reproduces sub_594680");
    }
}

void test_1801_exact_record_and_time_qualifier_boundaries() {
    auto result = project_1801(
        0, "110000",
        transaction_record(local_epoch_ms(9, 27), 1.2345, -101,
                           0x11223344U, 0x55667788U, 1));
    require(result.at("schema").as_string() ==
                "tdx-level2-sdk-1801-host-projection-v1" &&
                result.at("format").as_string() ==
                    "sdk-1801-host-projection",
            "1801 projection schema is stable");
    require_number(record(result).at("time_offset_seconds_raw"), 12420,
                   "localtime offset is measured from local 06:00");
    require_number(record(result).at("local_minute_classifier_raw"), 567,
                   "09:27 enters the qualifier source branch");
    require_number(record(result).at("price_x10000_i64_raw"), 12345,
                   "x87 price path applies four-decimal scaling and bias");
    require_number(record(result).at("volume_lot_quotient_i64_raw"), -10,
                   "class three uses signed division by ten");
    require_number(record(result).at("volume_lot_remainder_raw"), -1,
                   "signed remainder follows dividend sign");
    require_number(record(result).at("qualifier_raw"), 0,
                   "source qualifier one maps to raw zero from 09:27");
    require(record(result).at("host_record_hex").as_string() ==
                "843039300000f6ffffffff004433221188776655",
            "1801 projected host record is byte-exact and 20 bytes");
    require_number(record(result).at("first_raw"), 0x11223344U,
                   "1801 first raw dword is retained");
    require_number(record(result).at("second_raw"), 0x55667788U,
                   "1801 second raw dword is retained");

    result = project_1801(
        0, "110000",
        transaction_record(local_epoch_ms(9, 26, 59), 1.0, 1,
                           0, 0, 2));
    require_number(record(result).at("qualifier_raw"), 2,
                   "before 09:27 the qualifier is raw two");

    result = project_1801(
        1, "688000",
        transaction_record(local_epoch_ms(15, 2, 59), 1.0, 1,
                           0, 0, 2));
    require_number(record(result).at("qualifier_raw"), 1,
                   "class 19 still uses the normal path at minute 902");
    result = project_1801(
        1, "688000",
        transaction_record(local_epoch_ms(15, 3), 1.0, 1,
                           0, 0, 2));
    require_number(record(result).at("qualifier_raw"), 5,
                   "class 19 uses raw five after minute 902");
}

void test_signed_epoch_and_x87_integer_indefinite() {
    tdx::Bytes epochs;
    append(epochs, order_record(-1, -0.00005, -21));
    append(epochs, order_record(0, std::numeric_limits<double>::max(), -21));
    const auto result = project_1802(2, "810000", std::move(epochs));
    require_number(record(result, 0).at("time_offset_seconds_raw"),
                   static_cast<std::int64_t>(
                       record(result, 1).at("time_offset_seconds_raw")
                           .as_number()),
                   "signed -1 ms truncates toward the local Unix epoch");
    require(record(result, 0).at("source")
                .at("epoch_ms_raw_i64_decimal").as_string() == "-1",
            "signed epoch is parsed directly and retained losslessly");
    require_number(record(result, 0).at("price_x10000_i64_raw"), 0,
                   "negative half-tick boundary keeps the positive bias");
    require(!record(result, 0).at("price_x87_integer_indefinite").as_bool(),
            "finite in-range x87 conversion is not indefinite");
    require(record(result, 1).at("source").at("price").is_number(),
            "finite source price remains reportable");
    require(record(result, 1).at("price_x87_integer_indefinite").as_bool() &&
                record(result, 1).at("price_x10000_i64_decimal").as_string() ==
                    "-9223372036854775808" &&
                record(result, 1).at("price_x10000_i32_raw").as_number() == 0.0,
            "finite out-of-range x87 FISTP uses integer-indefinite and stores low zero");
}

void test_1802_record_type_mapping_and_exact_records() {
    tdx::Bytes body;
    for (std::uint32_t selector = 0; selector <= 4; ++selector)
        append(body, order_record(local_epoch_ms(10, 0), 2.0, -21,
                                  0xA1B2C3D4U, selector));
    const auto result = project_1802(2, "810000", std::move(body));
    require(result.at("schema").as_string() ==
                "tdx-level2-sdk-1802-host-projection-v1" &&
                state(result).at("records").size() == 5,
            "1802 counted body produces a complete replacement state");
    const std::vector<std::pair<int, int>> expected{
        {0, 0}, {0, 'B'}, {0, 'S'}, {'B', 'C'}, {'S', 'C'}};
    for (std::size_t index = 0; index < expected.size(); ++index) {
        require_number(record(result, index)
                           .at("side_or_cancel_qualifier_raw"),
                       expected[index].first,
                       "1802 side/cancel qualifier byte retains raw mapping");
        require_number(record(result, index).at("action_raw"),
                       expected[index].second,
                       "1802 action byte retains raw mapping");
        require_number(record(result, index).at("order_id_raw"),
                       0xA1B2C3D4U,
                       "1802 order id raw dword is retained");
        require_number(record(result, index)
                           .at("volume_lot_quotient_i64_raw"),
                       -2, "1802 signed quotient uses class-22 divisor ten");
        require_number(record(result, index).at("volume_lot_remainder_raw"),
                       -1, "1802 signed remainder is retained");
        require(record(result, index).at("host_record_hex")
                    .as_string().size() == 40,
                "each 1802 projected record is exactly 20 bytes");
    }
    require(record(result, 3).at("host_record_hex").as_string() ==
                "4038204e0000feffffffff4243000000d4c3b2a1",
            "1802 record-type three host record is byte-exact");
    require(state(result).at("replacement_semantics").as_string().find(
                "clears") != std::string::npos,
            "each callback document explicitly replaces prior logical state");
}

void test_domain_bounds_and_identity_validation() {
    require_throws([] {
        (void)project_1801(0, "000001", {});
    }, "clear-only");
    require_throws([] {
        (void)project_1801(0, "000001", tdx::Bytes(51, 0));
    }, "multiple of 52");
    require_throws([] {
        (void)project_1802(0, "000001", tdx::Bytes(41, 0));
    }, "multiple of 40");
    require_throws([] {
        (void)project_1802(0, "000001", tdx::Bytes(393240, 0));
    }, "384 KiB");
    require_throws([] {
        (void)project_1802(3, "000001", order_record());
    }, "market must be 0, 1, or 2");
    require_throws([] {
        (void)project_1802(0, "00001", order_record());
    }, "exactly 6 digits");
    require_throws([] {
        (void)project_1802(0, "00A001", order_record());
    }, "exactly 6 digits");
}

std::string body_hex(const tdx::Bytes& body) {
    constexpr char digits[] = "0123456789abcdef";
    std::string result;
    result.reserve(body.size() * 2);
    for (const auto byte : body) {
        result.push_back(digits[byte >> 4U]);
        result.push_back(digits[byte & 0x0fU]);
    }
    return result;
}

void require_offline_boundary(const tdx::Json& result) {
    require(!result.at("input_body_retained").as_bool() &&
                !result.at("sdk_called").as_bool() &&
                !result.at("sdk_callback_invoked").as_bool() &&
                !result.at("callback_executed").as_bool() &&
                !result.at("host_storage_call_attempted").as_bool() &&
                !result.at("host_storage_mutated").as_bool() &&
                !result.at("sub_68C170_called").as_bool() &&
                !result.at("sub_68C200_called").as_bool() &&
                !result.at("host_message_dispatch_attempted").as_bool() &&
                result.at("host_messages_sent").as_number() == 0.0 &&
                !result.at("wire_bytes_built").as_bool() &&
                !result.at("network_accessed").as_bool() &&
                !result.at("network_request_bytes_built").as_bool() &&
                result.at("network_requests").as_number() == 0.0 &&
                !result.at("send_attempted").as_bool() &&
                !result.at("request_sent").as_bool() &&
                !result.at("entitlement_bypass").as_bool() &&
                result.at("offline").as_bool(),
            "all SDK/callback/storage/message/network/send/bypass boundaries hold");
}

void test_cli_raw_hex_and_bounds() {
    namespace fs = std::filesystem;
    const auto suffix = std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count());
    const auto directory = fs::temp_directory_path() /
        fs::path("tdx-level2-sdk-180x-host-projection-cli-" + suffix);
    fs::create_directories(directory);
    struct Cleanup {
        fs::path path;
        ~Cleanup() {
            std::error_code ignored;
            fs::remove_all(path, ignored);
        }
    } cleanup{directory};

    const auto transaction = transaction_record(
        local_epoch_ms(9, 27), 1.2345, -101,
        0x11223344U, 0x55667788U, 1);
    const auto order = order_record(
        local_epoch_ms(10, 0), 2.0, -21, 0xA1B2C3D4U, 3);
    const auto raw_path = directory / "transaction.bin";
    const auto hex_path = directory / "order.hex";
    const auto output_path = directory / "output.json";
    tdx::atomic_write_bytes(raw_path, transaction);
    tdx::atomic_write_text(hex_path, body_hex(order));

    require(tdx::command_level2_project({
                "--format", "sdk-1801-host-projection", "--input",
                tdx::path_utf8(raw_path), "--encoding", "raw", "--market",
                "0", "--code", "110000", "--output",
                tdx::path_utf8(output_path), "--compact"}) == 0,
            "raw CLI 1801 projection succeeds");
    auto result = tdx::Json::parse(tdx::read_text_utf8(output_path));
    require(result.at("format").as_string() ==
                "sdk-1801-host-projection" &&
                record(result).at("host_record_hex").as_string() ==
                    "843039300000f6ffffffff004433221188776655",
            "raw CLI emits the exact 1801 host record");
    require_offline_boundary(result);

    require(tdx::command_level2_project({
                "--format", "sdk-1802-host-projection", "--input",
                tdx::path_utf8(hex_path), "--encoding", "hex", "--market",
                "2", "--code", "810000", "--output",
                tdx::path_utf8(output_path), "--compact"}) == 0,
            "hex CLI 1802 projection succeeds");
    result = tdx::Json::parse(tdx::read_text_utf8(output_path));
    require(result.at("format").as_string() ==
                "sdk-1802-host-projection" &&
                record(result).at("host_record_hex").as_string() ==
                    "4038204e0000feffffffff4243000000d4c3b2a1",
            "hex CLI emits the exact 1802 host record");
    require_offline_boundary(result);

    const auto short_path = directory / "short.bin";
    const auto empty_path = directory / "empty.bin";
    const auto oversized_path = directory / "oversized.bin";
    tdx::atomic_write_bytes(short_path, tdx::Bytes(51, 0));
    tdx::atomic_write_bytes(empty_path, {});
    tdx::atomic_write_bytes(oversized_path, tdx::Bytes(384U * 1024U + 1U, 0));
    require_throws([&] {
        (void)tdx::command_level2_project({
            "--format", "sdk-1801-host-projection", "--input",
            tdx::path_utf8(short_path), "--encoding", "raw", "--market",
            "0", "--code", "110000"});
    }, "multiple of 52");
    require_throws([&] {
        (void)tdx::command_level2_project({
            "--format", "sdk-1802-host-projection", "--input",
            tdx::path_utf8(empty_path), "--encoding", "raw", "--market",
            "0", "--code", "000001"});
    }, "clear-only");
    require_throws([&] {
        (void)tdx::command_level2_project({
            "--format", "sdk-1802-host-projection", "--input",
            tdx::path_utf8(oversized_path), "--encoding", "raw", "--market",
            "0", "--code", "000001"});
    }, "384 KiB");
    require_throws([&] {
        (void)tdx::command_level2_project({
            "--format", "sdk-1801-host-projection", "--input",
            tdx::path_utf8(raw_path), "--encoding", "raw", "--code",
            "110000"});
    }, "requires --market");
    require_throws([&] {
        (void)tdx::command_level2_project({
            "--format", "sdk-1801-host-projection", "--input",
            tdx::path_utf8(raw_path), "--encoding", "raw", "--market",
            "0", "--code", "110000", "--url", "https://example.invalid"});
    }, "unknown argument");
}

}  // namespace

int main() {
    try {
        test_complete_security_classifier_and_lot_divisor();
        test_1801_exact_record_and_time_qualifier_boundaries();
        test_signed_epoch_and_x87_integer_indefinite();
        test_1802_record_type_mapping_and_exact_records();
        test_domain_bounds_and_identity_validation();
        test_cli_raw_hex_and_bounds();
        std::cout << "level2 SDK 1801/1802 host projection tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "level2 SDK 1801/1802 host projection tests failed: "
                  << error.what() << '\n';
        return 1;
    }
}
