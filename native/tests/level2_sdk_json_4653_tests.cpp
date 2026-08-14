#include "tdx/level2.hpp"

#include "tdx/common.hpp"

#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <functional>
#include <iostream>
#include <limits>
#include <string>
#include <system_error>
#include <utility>

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw tdx::Error(message);
}

void require_near(const tdx::Json& value, float expected,
                  const char* message) {
    require(value.is_number() &&
                std::fabs(value.as_number() - static_cast<double>(expected)) <
                    1.0e-6,
            message);
}

void require_throws(const std::function<void()>& action,
                    const char* expected_fragment) {
    try {
        action();
    } catch (const tdx::Error& error) {
        require(std::string(error.what()).find(expected_fragment) !=
                    std::string::npos,
                "error includes expected 4653 contract detail");
        return;
    }
    throw tdx::Error("expected SDK 4653 contract validation failure");
}

tdx::Json valid_document() {
    return tdx::Json::parse(R"json({"Data":[
      {"datetime":"202608131459","closePrice":11248.459,"averagePrice":11229.999,"tradeVolume":4294967295,"reference_price":11200},
      {"datetime":"7","closePrice":-1250,"averagePrice":0,"tradeVolume":0}
    ]})json");
}

void test_layout_scaling_time_and_boundary() {
    const auto result = tdx::normalize_level2_sdk_json_4653(
        valid_document(), 1);
    require(result.at("schema").as_string() ==
                    "tdx-level2-sdk-json-4653-v1" &&
                result.at("format").as_string() == "sdk-json-4653" &&
                result.at("function_id").as_number() == 4653.0,
            "4653 schema and format are stable");
    require(result.at("native_header_size").as_number() == 35.0 &&
                result.at("native_record_size").as_number() == 18.0 &&
                result.at("recovered_base_layout_size").as_number() == 71.0 &&
                result.at("optional_attachment_size").as_number() == 120.0 &&
                !result.at("optional_attachment_projected").as_bool() &&
                !result.at("native_body_built").as_bool(),
            "4653 reports the recovered layout without building a native body");

    const auto& header = result.at("header");
    require(header.at("record_count_u16_raw").as_number() == 2.0 &&
                header.at("record_count_native_offset").as_number() == 33.0 &&
                header.at("reference_price_native_offset").as_number() == 29.0 &&
                header.at("reference_price_source_index").as_number() == 0.0 &&
                !header.at("request_context_fields_projected").as_bool() &&
                !header.at("cache_reference_fallback_projected").as_bool(),
            "4653 header contains only response-derived semantics");
    require_near(header.at("reference_price"),
                 static_cast<float>(11200.0) / 1000.0F,
                 "4653 first reference price is f32-scaled by 1000");

    require(result.at("count").as_number() == 2.0 &&
                result.at("returned").as_number() == 1.0 &&
                result.at("truncated").as_bool() &&
                result.at("source_order").as_string() == "first-to-last",
            "4653 limit bounds output without changing source order");
    const auto& record = result.at("records").as_array().front();
    require(record.at("index").as_number() == 0.0 &&
                record.at("source_index").as_number() == 0.0 &&
                record.at("native_offset").as_number() == 35.0 &&
                record.at("datetime_raw").as_string() == "202608131459" &&
                record.at("datetime_right4_raw").as_string() == "1459" &&
                record.at("time_u16_raw").as_number() == 61.0 * 59.0 &&
                record.at("trade_volume_u32_raw").as_number() ==
                    4294967295.0,
            "4653 record preserves evidence-named time and u32 volume fields");
    require_near(record.at("close_price"),
                 static_cast<float>(11248.459) / 1000.0F,
                 "4653 close price follows the native f32 divide boundary");
    require_near(record.at("average_price"),
                 static_cast<float>(11229.999) / 1000.0F,
                 "4653 average price follows the native f32 divide boundary");
    require(result.at("time_transform").as_string().find("CString::Right") !=
                    std::string::npos &&
                !result.at("time_semantics_resolved").as_bool(),
            "4653 time transform remains raw and evidence-named");

    require(!result.at("input_retained").as_bool() &&
                !result.at("input_document_retained").as_bool() &&
                !result.at("file_retained").as_bool() &&
                !result.at("source_file_retained").as_bool() &&
                !result.at("sdk_called").as_bool() &&
                !result.at("sdk_callback_invoked").as_bool() &&
                !result.at("wire_bytes_built").as_bool() &&
                !result.at("network_request_bytes").as_bool() &&
                !result.at("network_request_bytes_built").as_bool() &&
                result.at("network_requests").as_number() == 0.0 &&
                !result.at("request_built").as_bool() &&
                !result.at("request_sent").as_bool() &&
                !result.at("subscription_sent").as_bool() &&
                result.at("offline").as_bool() &&
                !result.at("entitlement_bypass").as_bool(),
            "4653 normalization has an explicit zero-side-effect boundary");

    const auto all = tdx::normalize_level2_sdk_json_4653(valid_document(), 2);
    const auto& short_time = all.at("records").as_array()[1];
    require(short_time.at("datetime_right4_raw").as_string() == "7" &&
                short_time.at("time_u16_raw").as_number() == 61.0 * 7.0,
            "CString Right(4) evidence also covers strings shorter than four bytes");
    const auto generic = tdx::normalize_level2_sdk_json(
        4653, valid_document(), 1);
    require(generic.at("schema").as_string() ==
                "tdx-level2-sdk-json-4653-v1",
            "generic SDK JSON public API dispatches 4653 to its strict domain");
}

void test_empty_and_exact_shape_contracts() {
    const auto empty = tdx::normalize_level2_sdk_json_4653(
        tdx::Json::parse(R"json({"Data":[]})json"), 20);
    require(empty.at("count").as_number() == 0.0 &&
                empty.at("records").as_array().empty() &&
                empty.at("header").at("reference_price").is_null() &&
                empty.at("header").at("reference_price_source_index").is_null(),
            "empty Data produces an explicit unavailable reference header");

    require_throws(
        [] { (void)tdx::normalize_level2_sdk_json_4653(tdx::Json::array()); },
        "must be an object");
    require_throws(
        [] {
            auto document = valid_document();
            document["extra"] = 1;
            (void)tdx::normalize_level2_sdk_json_4653(document);
        },
        "unknown field extra");
    require_throws(
        [] {
            (void)tdx::normalize_level2_sdk_json_4653(
                tdx::Json::parse(R"json({"Data":"[]"})json"));
        },
        "Data must be an array");
    require_throws(
        [] {
            auto document = valid_document();
            document["Data"].as_array()[0]["extra"] = 1;
            (void)tdx::normalize_level2_sdk_json_4653(document);
        },
        "unknown field extra");
    require_throws(
        [] {
            auto document = valid_document();
            document["Data"].as_array()[0].as_object().erase("reference_price");
            (void)tdx::normalize_level2_sdk_json_4653(document);
        },
        "requires reference_price");
    require_throws(
        [] {
            auto document = valid_document();
            document["Data"].as_array()[0].as_object().erase("closePrice");
            (void)tdx::normalize_level2_sdk_json_4653(document);
        },
        "requires closePrice");
    require_throws(
        [] {
            auto document = valid_document();
            document["Data"].as_array()[0]["datetime"] = "12x4";
            (void)tdx::normalize_level2_sdk_json_4653(document);
        },
        "ASCII digit string");
}

void test_numeric_and_resource_bounds() {
    require_throws(
        [] {
            auto document = valid_document();
            document["Data"].as_array()[0]["closePrice"] = "11200";
            (void)tdx::normalize_level2_sdk_json_4653(document);
        },
        "must be a JSON number");
    require_throws(
        [] {
            auto document = valid_document();
            document["Data"].as_array()[0]["averagePrice"] =
                std::numeric_limits<double>::infinity();
            (void)tdx::normalize_level2_sdk_json_4653(document);
        },
        "must be finite");
    require_throws(
        [] {
            auto document = valid_document();
            document["Data"].as_array()[0]["reference_price"] =
                std::numeric_limits<double>::max();
            (void)tdx::normalize_level2_sdk_json_4653(document);
        },
        "finite f32 range");
    require_throws(
        [] {
            auto document = valid_document();
            document["Data"].as_array()[0]["tradeVolume"] = -1;
            (void)tdx::normalize_level2_sdk_json_4653(document);
        },
        "0..4294967295");
    require_throws(
        [] {
            auto document = valid_document();
            document["Data"].as_array()[0]["tradeVolume"] = 1.5;
            (void)tdx::normalize_level2_sdk_json_4653(document);
        },
        "0..4294967295");
    require_throws(
        [] {
            auto document = valid_document();
            document["Data"].as_array()[0]["tradeVolume"] = 4294967296.0;
            (void)tdx::normalize_level2_sdk_json_4653(document);
        },
        "0..4294967295");
    require_throws(
        [] { (void)tdx::normalize_level2_sdk_json_4653(valid_document(), -1); },
        "limit must be 0..10000");
    require_throws(
        [] { (void)tdx::normalize_level2_sdk_json_4653(valid_document(), 10001); },
        "limit must be 0..10000");

    require_throws(
        [] {
            auto document = valid_document();
            const auto record = document.at("Data").as_array().front();
            auto& records = document["Data"].as_array();
            records.clear();
            for (std::size_t index = 0; index < 10001; ++index)
                records.push_back(record);
            (void)tdx::normalize_level2_sdk_json_4653(document);
        },
        "record count exceeds");
    require_throws(
        [] {
            auto document = valid_document();
            document["Data"].as_array()[0]["datetime"] =
                std::string(384U * 1024U, '1');
            (void)tdx::normalize_level2_sdk_json_4653(document);
        },
        "compact JSON document exceeds 384 KiB");
}

void test_cli_file_boundary_and_utf8() {
    namespace fs = std::filesystem;
    const auto stamp = std::chrono::steady_clock::now()
                           .time_since_epoch()
                           .count();
    const auto prefix = fs::temp_directory_path() /
                        ("tdx-level2-sdk-json-4653-" +
                         std::to_string(stamp));
    auto input = prefix;
    input += ".json";
    auto output = prefix;
    output += ".out.json";
    auto invalid = prefix;
    invalid += ".invalid.json";
    auto oversized = prefix;
    oversized += ".oversized.json";

    tdx::atomic_write_text(input, valid_document().dump(-1));
    const auto status = tdx::command_level2_decode(
        {"--format", "sdk-json-4653", "--input", tdx::path_utf8(input),
         "--limit", "1", "--output", tdx::path_utf8(output), "--compact"});
    const auto report = tdx::Json::parse(tdx::read_text_utf8(output));
    require(status == 0 &&
                report.at("schema").as_string() ==
                    "tdx-level2-sdk-json-4653-v1" &&
                report.at("returned").as_number() == 1.0,
            "level2 decode sdk-json-4653 consumes a bounded UTF-8 file");

    tdx::atomic_write_bytes(
        invalid,
        tdx::Bytes{'{', '"', 'D', 'a', 't', 'a', '"', ':', '[', '"',
                   0xc0, 0xaf, '"', ']', '}'});
    require_throws(
        [&] {
            (void)tdx::command_level2_decode(
                {"--format", "sdk-json-4653", "--input",
                 tdx::path_utf8(invalid)});
        },
        "valid UTF-8");

    tdx::atomic_write_bytes(oversized,
                            tdx::Bytes(384U * 1024U + 1U, ' '));
    require_throws(
        [&] {
            (void)tdx::command_level2_decode(
                {"--format", "sdk-json-4653", "--input",
                 tdx::path_utf8(oversized)});
        },
        "no larger than 384 KiB");
    require_throws(
        [&] {
            (void)tdx::command_level2_decode(
                {"--format", "sdk-json-4653", "--input",
                 tdx::path_utf8(input), "--encoding", "raw"});
        },
        "accepts only --limit");
    require_throws(
        [&] {
            (void)tdx::command_level2_decode(
                {"--format", "sdk-json-4653", "--input",
                 tdx::path_utf8(input), "--depth", "5"});
        },
        "only valid for sdk-json-4680");

    std::error_code cleanup_error;
    fs::remove(input, cleanup_error);
    cleanup_error.clear();
    fs::remove(output, cleanup_error);
    cleanup_error.clear();
    fs::remove(invalid, cleanup_error);
    cleanup_error.clear();
    fs::remove(oversized, cleanup_error);
}

}  // namespace

int main() {
    try {
        test_layout_scaling_time_and_boundary();
        test_empty_and_exact_shape_contracts();
        test_numeric_and_resource_bounds();
        test_cli_file_boundary_and_utf8();
        std::cout << "level2 SDK JSON 4653 tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
