#include "tdx/level2.hpp"

#include "tdx/common.hpp"

#include <cmath>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <exception>
#include <filesystem>
#include <functional>
#include <iostream>
#include <limits>
#include <string>
#include <utility>

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw tdx::Error(message);
}

void require_number(const tdx::Json& value, double expected,
                    const char* message) {
    require(value.is_number() &&
                std::fabs(value.as_number() - expected) < 1.0e-4,
            message);
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

void put_f32(tdx::Bytes& body, std::size_t offset, float value) {
    std::uint32_t raw{};
    std::memcpy(&raw, &value, sizeof(raw));
    put_u32(body, offset, raw);
}

void put_f64(tdx::Bytes& body, std::size_t offset, double value) {
    std::uint64_t raw{};
    std::memcpy(&raw, &value, sizeof(raw));
    put_u64(body, offset, raw);
}

std::uint64_t local_epoch_ms(int hour, int minute, int second) {
    std::tm value{};
    value.tm_year = 126;
    value.tm_mon = 7;
    value.tm_mday = 12;
    value.tm_hour = hour;
    value.tm_min = minute;
    value.tm_sec = second;
    value.tm_isdst = -1;
    const auto timestamp = std::mktime(&value);
    require(timestamp >= 0, "test fixture local timestamp is representable");
    return static_cast<std::uint64_t>(timestamp) * 1000U;
}

tdx::Bytes quote_body(int hour = 9, int minute = 31, int second = 2,
                      std::uint64_t volume = 105, double last = 10.5,
                      double bid1 = 10.4, double ask1 = 10.6) {
    tdx::Bytes body(380, 0);
    put_u64(body, 0, local_epoch_ms(hour, minute, second));
    put_f64(body, 8, last);
    put_f64(body, 16, 10.1);
    put_f64(body, 24, 10.8);
    put_f64(body, 32, 9.8);
    put_f64(body, 40, 12345.5);
    put_u64(body, 48, volume);
    put_u64(body, 56, 998877);
    put_u32(body, 64, 11);
    put_u32(body, 68, 0x11223344U);
    put_u32(body, 72, 88);
    put_f64(body, 76, 9.9);
    for (std::size_t index = 0; index < 10; ++index) {
        put_f64(body, 108 + 8 * index,
                index == 0 ? ask1 : 10.6 + 0.1 * index);
        put_f32(body, 188 + 4 * index,
                static_cast<float>(101 + index));
        put_f64(body, 228 + 8 * index,
                index == 0 ? bid1 : 10.4 - 0.1 * index);
        put_f32(body, 308 + 4 * index,
                static_cast<float>(201 + index));
    }
    put_f64(body, 348, 10.2);
    put_f32(body, 356, 1200.0F);
    put_f64(body, 364, 10.7);
    put_f32(body, 372, 1300.0F);
    return body;
}

tdx::Json levels(double first_price, double base_price, double base_quantity) {
    auto result = tdx::Json::array();
    for (std::size_t index = 0; index < 10; ++index) {
        auto row = tdx::Json::object();
        row["level"] = static_cast<std::uint64_t>(index + 1);
        row["price"] = index == 0 ? first_price : base_price + index;
        row["quantity_raw"] = base_quantity + index;
        result.push_back(std::move(row));
    }
    return result;
}

tdx::Json previous_state(std::uint32_t time = 93000,
                         std::uint32_t volume = 100,
                         double bid1 = 10.0, double ask1 = 10.2,
                         double last = 10.1) {
    auto base = tdx::Json::object();
    base["pre_close_price"] = 9.7;
    base["open_price"] = 9.8;
    base["high_price"] = 10.3;
    base["low_price"] = 9.6;
    base["last_price"] = last;
    base["time_hhmmss_raw"] = static_cast<std::uint64_t>(time);
    base["special_volume_projected_raw"] = 7.0;
    base["cumulative_volume_raw"] = static_cast<std::uint64_t>(volume);
    base["last_positive_volume_delta_raw"] = 3;
    base["amount_raw"] = 1000.0;
    base["first_volume_bucket_raw"] = 10;
    base["second_volume_bucket_raw"] = 20;
    base["host_status_flags_raw"] = 0xC0;
    base["host_auxiliary_142_raw"] = 44;

    auto aggregate = tdx::Json::object();
    aggregate["average_bid_price"] = 8.1;
    aggregate["total_bid_quantity_raw"] = 800.0;
    aggregate["average_ask_price"] = 8.2;
    aggregate["total_ask_quantity_raw"] = 900.0;

    auto result = tdx::Json::object();
    result["schema"] = "tdx-level2-sdk-host-quote-state-v1";
    result["base_quote"] = std::move(base);
    result["ask_levels"] = levels(ask1, 20.0, 300.0);
    result["bid_levels"] = levels(bid1, 30.0, 400.0);
    result["aggregate"] = std::move(aggregate);
    return result;
}

tdx::Json projection_context(int security_class = 0,
                             int price_guard = 0,
                             bool small_last_guard = false,
                             double multiplier = 2.0,
                             std::uint32_t auxiliary = 12,
                             std::uint16_t host_word = 0) {
    auto result = tdx::Json::object();
    result["security_class_raw"] = security_class;
    result["price_transition_guard_raw"] = price_guard;
    result["small_last_price_fallback_predicate_raw"] = small_last_guard;
    result["special_volume_multiplier_raw"] = multiplier;
    result["security_auxiliary_dword_73_raw"] =
        static_cast<std::uint64_t>(auxiliary);
    result["host_word_280_raw"] = static_cast<std::uint64_t>(host_word);
    return result;
}

tdx::Json project(tdx::Bytes body, tdx::Json previous,
                  tdx::Json context,
                  tdx::Level2SdkCallbackDataType type =
                      tdx::Level2SdkCallbackDataType::quote_update) {
    tdx::Level2SdkQuoteTransitionRequest request;
    request.data_type = type;
    request.body = std::move(body);
    request.previous = std::move(previous);
    request.context = std::move(context);
    return tdx::project_level2_sdk_quote_transition(request);
}

const tdx::Json& base(const tdx::Json& result,
                      const char* state = "projected_state") {
    return result.at(state).at("base_quote");
}

const tdx::Json& level(const tdx::Json& result, const char* side,
                       std::size_t index,
                       const char* state = "projected_state") {
    return result.at(state).at(side).as_array().at(index);
}

void require_throws(const std::function<void()>& action,
                    const char* expected_fragment) {
    try {
        action();
    } catch (const tdx::Error& error) {
        require(std::string(error.what()).find(expected_fragment) !=
                    std::string::npos,
                "error includes the expected contract detail");
        return;
    }
    throw tdx::Error("expected contract validation failure");
}

void test_normal_transition_and_offline_boundary() {
    const auto result = project(quote_body(), previous_state(),
                                projection_context());
    require(result.at("schema").as_string() ==
                "tdx-level2-sdk-quote-transition-v1",
            "transition schema is stable");
    require_number(result.at("data_type"), 1807,
                   "1807 data type is retained");
    require_number(base(result).at("time_hhmmss_raw"), 93102,
                   "epoch milliseconds become local HHMMSS");
    require_number(base(result).at("last_price"), 10.5,
                   "normal security updates last price");
    require_number(base(result).at("cumulative_volume_raw"), 105,
                   "normal security updates low volume dword");
    require_number(base(result).at("host_auxiliary_142_raw"), 88,
                   "normal security projects source auxiliary +72");
    require_number(result.at("transition").at("quote_kind_class_raw"), 1,
                   "quote kind 11 maps to class 1");
    require_number(result.at("transition").at("price_transition_case_raw"), 9,
                   "last above the old midpoint selects raw case 9");
    require_number(result.at("transition").at("price_transition_class_raw"), 0,
                   "raw case 9 folds to class 0");
    require_number(base(result).at("first_volume_bucket_raw"), 10,
                   "class 0 leaves first bucket unchanged");
    require_number(base(result).at("second_volume_bucket_raw"), 25,
                   "class 0 adds positive delta to second bucket");
    require_number(base(result).at("last_positive_volume_delta_raw"), 5,
                   "positive delta is retained in its host field");
    require_number(base(result).at("host_status_flags_raw"), 0xC4,
                   "quote kind and transition classes update only recovered bits");
    require_number(level(result, "ask_levels", 0).at("quantity_raw"), 101,
                   "top ask depth is projected");
    require_number(level(result, "bid_levels", 9).at("quantity_raw"), 210,
                   "tenth bid depth is projected");
    require_number(result.at("projected_state").at("aggregate")
                       .at("total_ask_quantity_raw"),
                   1300, "aggregate fields are projected under recovered guard");
    require(result.at("source_raw").at("special_volume_host_projection_semantics")
                .is_null() &&
                result.at("source_raw").at("auxiliary_68_host_projection_semantics")
                    .is_null(),
            "unconsumed source fields keep null semantics");
    require(result.at("offline").as_bool() &&
                !result.at("sdk_called").as_bool() &&
                !result.at("callback_executed").as_bool() &&
                !result.at("host_message_dispatch_attempted").as_bool() &&
                !result.at("wire_bytes_built").as_bool() &&
                !result.at("request_sent").as_bool(),
            "projection has no SDK, callback, host-message, wire, or send side effect");
}

void test_first_second_bucket_classes() {
    auto neutral = project(quote_body(9, 31, 2, 105, 10.0, 10.0, 10.0),
                           previous_state(93000, 100, 0.0, 0.0),
                           projection_context());
    require_number(neutral.at("transition").at("price_transition_case_raw"), 1,
                   "empty prior best prices select raw case 1");
    require_number(neutral.at("transition").at("price_transition_class_raw"), 2,
                   "raw case 1 folds to class 2");
    require_number(base(neutral).at("first_volume_bucket_raw"), 13,
                   "odd neutral delta gives its remainder to first bucket");
    require_number(base(neutral).at("second_volume_bucket_raw"), 22,
                   "neutral delta half is added to second bucket");

    auto first = project(quote_body(9, 31, 2, 105, 9.5, 10.0, 10.2),
                         previous_state(93000, 100, 10.0, 10.2),
                         projection_context());
    require_number(first.at("transition").at("price_transition_case_raw"), 10,
                   "last below prior midpoint selects raw case 10");
    require_number(first.at("transition").at("price_transition_class_raw"), 1,
                   "raw case 10 folds to class 1");
    require_number(base(first).at("first_volume_bucket_raw"), 15,
                   "class 1 adds all positive delta to first bucket");
    require_number(base(first).at("second_volume_bucket_raw"), 20,
                   "class 1 leaves second bucket unchanged");
}

void test_quote_kind_raw_mapping() {
    struct Case {
        std::uint32_t raw;
        int expected_class;
    };
    constexpr Case cases[] = {
        {11, 1}, {12, 2}, {13, 3}, {14, 13}, {15, 5}, {16, 12},
        {17, 8}, {18, 4}, {19, 9}, {20, 10}, {22, 7}, {23, 5},
        {21, 0},
    };
    for (const auto& item : cases) {
        auto body = quote_body(9, 31, 2, 100);
        put_u32(body, 64, item.raw);
        const auto result = project(std::move(body), previous_state(),
                                    projection_context());
        require_number(result.at("transition").at("quote_kind_class_raw"),
                       item.expected_class,
                       "quote kind follows the recovered raw mapping table");
    }
}

void test_signed_epoch_projection() {
    auto body = quote_body();
    put_u64(body, 0, std::numeric_limits<std::uint64_t>::max());
    const auto result = project(std::move(body), previous_state(),
                                projection_context());
    require(result.at("source_raw").at("time_epoch_ms_raw").as_number() == -1.0,
            "callback epoch milliseconds retain native signed __time64_t semantics");
    require(result.at("transition").at("time_conversion_valid").as_bool(),
            "signed -1 ms truncates to the local epoch instead of overflowing");
}

void test_nonpositive_delta_does_not_reclassify() {
    const auto result = project(quote_body(9, 31, 2, 99),
                                previous_state(), projection_context());
    require(!result.at("transition").at("price_transition_evaluated").as_bool(),
            "negative delta does not evaluate the price transition helper");
    require_number(result.at("transition").at("observed_volume_delta_raw"), -1,
                   "volume subtraction is observed as signed int32");
    require(result.at("transition").at("price_transition_case_raw").is_null() &&
                result.at("transition").at("price_transition_class_raw").is_null(),
            "unevaluated transition fields remain null");
    require_number(base(result).at("first_volume_bucket_raw"), 10,
                   "zero delta preserves first bucket");
    require_number(base(result).at("second_volume_bucket_raw"), 20,
                   "zero delta preserves second bucket");
    require_number(base(result).at("last_positive_volume_delta_raw"), 3,
                   "negative delta preserves prior positive-delta field");

    const auto wrapped = project(
        quote_body(9, 31, 2, 3),
        previous_state(93000, 0xFFFFFFFEU), projection_context());
    require_number(wrapped.at("transition").at("observed_volume_delta_raw"), 5,
                   "unsigned volume subtraction wraps before signed int32 view");
    require(wrapped.at("transition").at("positive_volume_delta_applied")
                .as_bool(),
            "wrapped positive delta is applied");
}

void test_stale_base_rejection_keeps_extended_depth() {
    const auto result = project(
        quote_body(9, 29, 59, 99), previous_state(), projection_context());
    require(!result.at("transition").at("base_quote_update_applied").as_bool(),
            "strictly older time and lower volume reject the 150-byte base");
    require_number(base(result).at("last_price"), 10.1,
                   "rejected base keeps previous last price");
    require_number(level(result, "ask_levels", 0).at("price"), 10.2,
                   "rejected base keeps prior levels one through five");
    require_number(level(result, "ask_levels", 5).at("quantity_raw"), 106,
                   "96-byte extension still replaces levels six through ten");
    require_number(result.at("projected_state").at("aggregate")
                       .at("average_bid_price"),
                   10.2, "96-byte extension still replaces aggregate data");
    require_number(level(result, "ask_levels", 0, "candidate_state").at("price"),
                   10.6, "candidate state exposes the rejected new top depth");
}

void test_special_security_and_extended_type() {
    auto body = quote_body(9, 31, 2, 105, 88.0, 87.0, 89.0);
    const auto result = project(
        std::move(body), previous_state(),
        projection_context(22, 0, false, 2.0, 12),
        tdx::Level2SdkCallbackDataType::extended_quote);
    require_number(result.at("data_type"), 18071,
                   "18071 uses the same exact transition contract");
    require(result.at("transition").at("special_security_branch").as_bool(),
            "security class 22 selects the recovered special branch");
    require_number(base(result).at("last_price"), 10.1,
                   "special branch preserves the normal last price");
    require_number(base(result).at("cumulative_volume_raw"), 100,
                   "special branch preserves cumulative volume");
    require_number(base(result).at("special_volume_projected_raw"), 210,
                   "special branch projects signed source volume times multiplier");
    require_number(base(result).at("host_auxiliary_142_raw"), 100,
                   "special branch wraps host dword 73 plus source +72");
    require_number(result.at("transition").at("observed_volume_delta_raw"), 0,
                   "preserved cumulative volume yields zero observed delta");
}

void test_small_last_fallback_and_aggregate_guard() {
    auto body = quote_body(9, 31, 2, 101, 0.005, 0.004, 0.006);
    const auto result = project(std::move(body), previous_state(),
                                projection_context(0, 1, true, 2.0, 12, 2));
    require(result.at("transition").at("small_last_price_fallback_applied")
                .as_bool(),
            "explicit unresolved predicate enables small-last correction");
    require_number(base(result).at("last_price"), 9.9,
                   "small last is replaced by the stored candidate preclose");
    require_number(result.at("projected_state").at("aggregate")
                       .at("average_bid_price"),
                   8.1, "host word above one blocks aggregate replacement");
    require_number(result.at("transition").at("price_transition_case_raw"), 0,
                   "nonzero raw price guard forces case zero");

    const auto promoted_boundary = project(
        quote_body(9, 31, 3, 102, 0.009, 0.004, 0.006),
        previous_state(), projection_context(0, 1, true, 2.0, 12, 2));
    require(promoted_boundary.at("transition")
                .at("small_last_price_fallback_applied").as_bool(),
            "float 0.009 promoted to double remains below the native double boundary");
    require_number(base(promoted_boundary).at("last_price"), 9.9,
                   "native mixed-precision 0.009 boundary applies fallback");
}

void test_contract_validation() {
    require_throws([] {
        project(tdx::Bytes(379), previous_state(), projection_context());
    }, "exactly 380");
    require_throws([] {
        project(quote_body(), previous_state(), projection_context(),
                tdx::Level2SdkCallbackDataType::transaction);
    }, "1807 or 18071");
    require_throws([] {
        auto previous = previous_state();
        previous["ask_levels"] = tdx::Json::array();
        project(quote_body(), std::move(previous), projection_context());
    }, "exactly 10");
    require_throws([] {
        auto previous = previous_state();
        previous["schema"] = "tdx-level2-sdk-host-quote-state-v2";
        project(quote_body(), std::move(previous), projection_context());
    }, "previous.schema");
    require_throws([] {
        auto previous = previous_state();
        previous.as_object().erase("schema");
        project(quote_body(), std::move(previous), projection_context());
    }, "schema");
    require_throws([] {
        auto context = projection_context();
        context.as_object().erase("security_class_raw");
        project(quote_body(), previous_state(), std::move(context));
    }, "security_class_raw");
}

std::string body_hex(const tdx::Bytes& body) {
    constexpr char digits[] = "0123456789abcdef";
    std::string result;
    result.reserve(body.size() * 2);
    for (const auto byte : body) {
        result.push_back(digits[byte >> 4]);
        result.push_back(digits[byte & 0x0f]);
    }
    return result;
}

void test_cli_surface() {
    namespace fs = std::filesystem;
    const auto suffix = std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count());
    const auto directory = fs::temp_directory_path() /
        fs::path("tdx-level2-quote-transition-cli-" + suffix);
    fs::create_directories(directory);
    struct Cleanup {
        fs::path path;
        ~Cleanup() {
            std::error_code ignored;
            fs::remove_all(path, ignored);
        }
    } cleanup{directory};

    const auto raw_body = quote_body();
    const auto raw_path = directory / "quote.bin";
    const auto hex_path = directory / "quote.hex";
    const auto state_path = directory / "state.json";
    const auto context_path = directory / "context.json";
    const auto output_path = directory / "result.json";
    tdx::atomic_write_bytes(raw_path, raw_body);
    tdx::atomic_write_text(hex_path, body_hex(raw_body));
    tdx::atomic_write_text(state_path, previous_state().dump(-1));
    tdx::atomic_write_text(context_path, projection_context().dump(-1));

    auto common = std::vector<std::string>{
        "--format", "sdk-quote-transition",
        "--state-file", tdx::path_utf8(state_path),
        "--context-file", tdx::path_utf8(context_path),
        "--data-type", "1807", "--output", tdx::path_utf8(output_path),
        "--compact"};
    auto raw_arguments = common;
    raw_arguments.insert(raw_arguments.end(), {
        "--input", tdx::path_utf8(raw_path), "--encoding", "raw"});
    require(tdx::command_level2_project(raw_arguments) == 0,
            "raw CLI quote projection succeeds");
    auto result = tdx::Json::parse(tdx::read_text_utf8(output_path));
    require(result.at("schema").as_string() ==
                "tdx-level2-sdk-quote-transition-v1" &&
                result.at("data_type").as_number() == 1807.0 &&
                !result.at("sdk_called").as_bool() &&
                !result.at("host_message_dispatch_attempted").as_bool() &&
                !result.at("request_sent").as_bool(),
            "raw CLI preserves the offline transition contract");

    auto hex_arguments = common;
    hex_arguments[hex_arguments.size() - 4] = "18071";
    hex_arguments.insert(hex_arguments.end(), {
        "--input", tdx::path_utf8(hex_path), "--encoding", "hex"});
    require(tdx::command_level2_project(hex_arguments) == 0,
            "hex CLI quote projection succeeds");
    result = tdx::Json::parse(tdx::read_text_utf8(output_path));
    require(result.at("data_type").as_number() == 18071.0,
            "hex CLI retains extended quote data type");

    const auto short_path = directory / "short.bin";
    tdx::atomic_write_bytes(short_path, tdx::Bytes(379, 0));
    auto short_arguments = common;
    short_arguments.insert(short_arguments.end(), {
        "--input", tdx::path_utf8(short_path), "--encoding", "raw"});
    require_throws([&] {
        (void)tdx::command_level2_project(short_arguments);
    }, "exactly 380");

    auto missing_encoding = common;
    missing_encoding.insert(missing_encoding.end(), {
        "--input", tdx::path_utf8(raw_path)});
    require_throws([&] {
        (void)tdx::command_level2_project(missing_encoding);
    }, "requires --encoding");

    auto unexpected_arguments = raw_arguments;
    unexpected_arguments.insert(unexpected_arguments.end(), {
        "--url", "https://example.invalid"});
    require_throws([&] {
        (void)tdx::command_level2_project(unexpected_arguments);
    }, "unknown argument");
}

}  // namespace

int main() {
    try {
        test_normal_transition_and_offline_boundary();
        test_first_second_bucket_classes();
        test_quote_kind_raw_mapping();
        test_signed_epoch_projection();
        test_nonpositive_delta_does_not_reclassify();
        test_stale_base_rejection_keeps_extended_depth();
        test_special_security_and_extended_type();
        test_small_last_fallback_and_aggregate_guard();
        test_contract_validation();
        test_cli_surface();
        std::cout << "level2 SDK quote transition tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "level2 SDK quote transition tests failed: "
                  << error.what() << '\n';
        return 1;
    }
}
