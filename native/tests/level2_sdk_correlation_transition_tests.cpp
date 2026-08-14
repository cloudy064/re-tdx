#include "tdx/level2.hpp"

#include "tdx/common.hpp"

#include <chrono>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <functional>
#include <iostream>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw tdx::Error(message);
}

void require_number(const tdx::Json& value, std::uint64_t expected,
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

tdx::Level2SdkCorrelationRecord record(
    std::uint32_t key_1, std::uint32_t key_2, std::uint16_t market,
    std::string code, tdx::Level2SdkCallbackDataType data_type,
    std::uint32_t mode) {
    tdx::Level2SdkCorrelationRecord result;
    result.callback_key_1_raw = key_1;
    result.callback_key_2_raw = key_2;
    result.market_raw = market;
    result.code_ascii = std::move(code);
    result.data_type = data_type;
    result.registry_mode_raw = mode;
    return result;
}

tdx::Level2SdkCorrelationTransitionRequest callback_request(
    std::vector<tdx::Level2SdkCorrelationRecord> registry,
    tdx::Level2SdkCallbackDataType data_type, std::uint32_t key_1,
    std::uint32_t key_2) {
    tdx::Level2SdkCorrelationCallbackMatch callback;
    callback.data_type = data_type;
    callback.callback_key_1_raw = key_1;
    callback.callback_key_2_raw = key_2;
    tdx::Level2SdkCorrelationTransitionRequest request;
    request.kind =
        tdx::Level2SdkCorrelationTransitionKind::callback_match;
    request.previous_registry = std::move(registry);
    request.callback_match = callback;
    return request;
}

tdx::Level2SdkCorrelationTransitionRequest upsert_request(
    std::vector<tdx::Level2SdkCorrelationRecord> registry,
    tdx::Level2SdkCorrelationRecord value) {
    tdx::Level2SdkCorrelationTransitionRequest request;
    request.kind =
        tdx::Level2SdkCorrelationTransitionKind::sdk_key_upsert;
    request.previous_registry = std::move(registry);
    request.sdk_key_upsert = std::move(value);
    return request;
}

const tdx::Json& projected_records(const tdx::Json& result) {
    return result.at("projected_registry").at("records");
}

void test_callback_first_match_retain_and_no_match() {
    using Type = tdx::Level2SdkCallbackDataType;
    const std::vector registry{
        record(11, 12, 0, "000001", Type::transaction, 9),
        record(11, 12, 1, "600000", Type::transaction, 3),
        record(21, 22, 0, "000002", Type::order, 9)};
    auto result = tdx::project_level2_sdk_correlation_transition(
        callback_request(registry, Type::transaction, 11, 12));
    require(result.at("schema").as_string() ==
                "tdx-level2-sdk-correlation-transition-v1" &&
                result.at("operation").as_string() == "callback-match",
            "callback transition schema and operation are stable");
    require(result.at("matched").as_bool(), "callback finds a match");
    require_number(result.at("matched_index"), 0,
                   "callback uses the first matching record");
    require(result.at("retained_after_matching_callback").as_bool(),
            "mode 9 record is retained");
    require(!result.at("removed_before_host_security_resolution").as_bool(),
            "mode 9 record is not consumed");
    require_number(result.at("projected_registry_record_count"), 3,
                   "retained transition preserves record count");
    require_number(result.at("identity").at("market_raw"), 0,
                   "first match determines the security identity");
    require_number(projected_records(result).as_array()[1]
                       .at("registry_mode_raw"),
                   3, "later duplicate is untouched");
    require(!result.at("sdk_called").as_bool() &&
                !result.at("host_security_lookup_attempted").as_bool() &&
                !result.at("host_message_dispatch_attempted").as_bool() &&
                !result.at("wire_bytes_built").as_bool() &&
                !result.at("credentials_accessed").as_bool() &&
                result.at("offline").as_bool(),
            "callback replay remains purely offline");

    result = tdx::project_level2_sdk_correlation_transition(
        callback_request(registry, Type::transaction, 99, 100));
    require(!result.at("matched").as_bool() &&
                result.at("matched_index").is_null() &&
                result.at("identity").is_null() &&
                result.at("state_change").as_string() == "no-match",
            "missing callback key leaves the registry unchanged");
    require_number(result.at("projected_registry_record_count"), 3,
                   "no-match preserves all records");
}

void test_callback_non_mode_9_consumes_before_resolution() {
    using Type = tdx::Level2SdkCallbackDataType;
    const std::vector registry{
        record(1, 2, 0, "000001", Type::transaction, 9),
        record(3, 4, 1, "600000", Type::order, 1),
        record(5, 6, 2, "430001", Type::quote_update, 0)};
    const auto result = tdx::project_level2_sdk_correlation_transition(
        callback_request(registry, Type::order, 3, 4));
    require(result.at("matched").as_bool(),
            "non-mode-9 callback finds its record");
    require_number(result.at("matched_index"), 1,
                   "non-mode-9 matched index is reported");
    require(!result.at("retained_after_matching_callback").as_bool() &&
                result.at("removed_before_host_security_resolution").as_bool(),
            "non-mode-9 record is consumed before host resolution");
    require(result.at("identity").at("code_ascii").as_string() == "600000",
            "identity survives consumption of the copied record");
    require_number(result.at("projected_registry_record_count"), 2,
                   "consumption removes exactly one record");
    require(projected_records(result).as_array()[1]
                    .at("code_ascii").as_string() == "430001",
            "records after the consumed slot shift left in order");
}

void test_sdk_key_upsert_replaces_or_appends() {
    using Type = tdx::Level2SdkCallbackDataType;
    const std::vector registry{
        record(10, 20, 0, "000001", Type::transaction, 9),
        record(30, 40, 0, "000001", Type::transaction, 9),
        record(50, 60, 1, "600000", Type::order, 1)};
    auto result = tdx::project_level2_sdk_correlation_transition(
        upsert_request(registry,
                       record(101, 202, 0, "000001", Type::transaction, 9)));
    require(result.at("matched").as_bool(),
            "upsert finds an existing identity");
    require_number(result.at("matched_index"), 0,
                   "upsert updates the first matching identity");
    require(result.at("state_change").as_string() ==
                "existing-record-keys-replaced",
            "existing upsert reports key replacement");
    require_number(projected_records(result).as_array()[0]
                       .at("callback_key_1_raw"),
                   101, "existing upsert replaces key 1");
    require_number(projected_records(result).as_array()[0]
                       .at("callback_key_2_raw"),
                   202, "existing upsert replaces key 2");
    require_number(projected_records(result).as_array()[1]
                       .at("callback_key_1_raw"),
                   30, "later duplicate identity is untouched");
    require_number(result.at("projected_registry_record_count"), 3,
                   "key replacement does not append");

    result = tdx::project_level2_sdk_correlation_transition(
        upsert_request(registry,
                       record(70, 80, 2, "430001", Type::extended_quote, 9)));
    require(!result.at("matched").as_bool() &&
                result.at("matched_index").is_null() &&
                result.at("state_change").as_string() ==
                    "new-record-appended",
            "new identity appends a correlation record");
    require_number(result.at("projected_registry_record_count"), 4,
                   "append grows the registry once");
    require(projected_records(result).as_array()[3]
                    .at("code_ascii").as_string() == "430001",
            "appended record retains its identity");
}

void test_replay_boundary_and_typed_contract() {
    using Type = tdx::Level2SdkCallbackDataType;
    std::vector<tdx::Level2SdkCorrelationRecord> full(
        10000, record(1, 2, 0, "000001", Type::transaction, 9));
    auto result = tdx::project_level2_sdk_correlation_transition(
        upsert_request(full,
                       record(3, 4, 0, "000001", Type::transaction, 9)));
    require_number(result.at("projected_registry_record_count"), 10000,
                   "identity update is allowed at the 10000-record bound");
    require_throws([&] {
        (void)tdx::project_level2_sdk_correlation_transition(
            upsert_request(full,
                           record(3, 4, 1, "600000", Type::order, 9)));
    }, "record 10001");

    full.push_back(record(5, 6, 2, "430001", Type::quote_update, 0));
    require_throws([&] {
        (void)tdx::project_level2_sdk_correlation_transition(
            callback_request(full, Type::transaction, 1, 2));
    }, "at most 10000");

    auto bad_action = callback_request({}, Type::transaction, 1, 2);
    bad_action.sdk_key_upsert =
        record(1, 2, 0, "000001", Type::transaction, 9);
    require_throws([&] {
        (void)tdx::project_level2_sdk_correlation_transition(bad_action);
    }, "exactly the action");
    require_throws([&] {
        (void)tdx::project_level2_sdk_correlation_transition(
            upsert_request({}, record(1, 2, 3, "000001",
                                      Type::transaction, 9)));
    }, "market_raw must be 0..2");
    require_throws([&] {
        (void)tdx::project_level2_sdk_correlation_transition(
            upsert_request({}, record(1, 2, 0, "ABC001",
                                      Type::transaction, 9)));
    }, "six ASCII digits");
    require_throws([&] {
        (void)tdx::project_level2_sdk_correlation_transition(
            callback_request({}, Type::order_queue_at_price, 1, 2));
    }, "local registry");
}

tdx::Json record_json(const tdx::Level2SdkCorrelationRecord& value) {
    tdx::Json result = tdx::Json::object();
    result["callback_key_1_raw"] =
        static_cast<std::uint64_t>(value.callback_key_1_raw);
    result["callback_key_2_raw"] =
        static_cast<std::uint64_t>(value.callback_key_2_raw);
    result["market_raw"] = static_cast<std::uint64_t>(value.market_raw);
    result["code_ascii"] = value.code_ascii;
    result["reserved_zero"] = 0;
    result["data_type"] = static_cast<int>(value.data_type);
    result["registry_mode_raw"] =
        static_cast<std::uint64_t>(value.registry_mode_raw);
    return result;
}

tdx::Json registry_json(
    const std::vector<tdx::Level2SdkCorrelationRecord>& values) {
    auto records = tdx::Json::array();
    for (const auto& value : values) records.push_back(record_json(value));
    auto result = tdx::Json::object();
    result["schema"] = "tdx-level2-sdk-correlation-registry-state-v1";
    result["record_size"] = 25;
    result["record_count"] = static_cast<std::uint64_t>(values.size());
    result["records"] = std::move(records);
    return result;
}

tdx::Json callback_input(tdx::Json registry, bool unexpected = false) {
    auto callback = tdx::Json::object();
    callback["data_type"] = 1801;
    callback["callback_key_1_raw"] = 11;
    callback["callback_key_2_raw"] = 12;
    auto result = tdx::Json::object();
    result["schema"] =
        "tdx-level2-sdk-correlation-transition-input-v1";
    result["operation"] = "callback-match";
    result["previous_registry"] = std::move(registry);
    result["callback"] = std::move(callback);
    if (unexpected) result["url"] = "https://example.invalid";
    return result;
}

tdx::Json upsert_input(tdx::Json registry,
                       tdx::Level2SdkCorrelationRecord value) {
    auto result = tdx::Json::object();
    result["schema"] =
        "tdx-level2-sdk-correlation-transition-input-v1";
    result["operation"] = "sdk-key-upsert";
    result["previous_registry"] = std::move(registry);
    result["sdk_key_upsert"] = record_json(value);
    return result;
}

void test_cli_strict_json_utf8_and_size() {
    namespace fs = std::filesystem;
    using Type = tdx::Level2SdkCallbackDataType;
    const auto suffix = std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count());
    const auto directory = fs::temp_directory_path() /
        fs::path("tdx-level2-correlation-transition-cli-" + suffix);
    fs::create_directories(directory);
    struct Cleanup {
        fs::path path;
        ~Cleanup() {
            std::error_code ignored;
            fs::remove_all(path, ignored);
        }
    } cleanup{directory};

    const auto input_path = directory / "input.json";
    const auto output_path = directory / "output.json";
    const auto state = registry_json(
        {record(11, 12, 0, "000001", Type::transaction, 1),
         record(21, 22, 1, "600000", Type::order, 9)});
    tdx::atomic_write_text(input_path, callback_input(state).dump(-1));
    require(tdx::command_level2_project({
                "--format", "sdk-correlation-transition", "--input",
                tdx::path_utf8(input_path), "--output",
                tdx::path_utf8(output_path), "--compact"}) == 0,
            "strict JSON CLI correlation transition succeeds");
    auto result = tdx::Json::parse(tdx::read_text_utf8(output_path));
    require(result.at("operation").as_string() == "callback-match" &&
                result.at("matched").as_bool() &&
                result.at("removed_before_host_security_resolution").as_bool(),
            "CLI dispatches the callback transition");
    require_number(result.at("projected_registry_record_count"), 1,
                   "CLI emits the replayable projected registry");

    tdx::atomic_write_text(
        input_path,
        upsert_input(
            result.at("projected_registry"),
            record(31, 32, 2, "430001", Type::extended_quote, 9))
            .dump(-1));
    require(tdx::command_level2_project({
                "--format", "sdk-correlation-transition", "--input",
                tdx::path_utf8(input_path), "--output",
                tdx::path_utf8(output_path), "--compact"}) == 0,
            "CLI accepts projected_registry unchanged for the next upsert");
    result = tdx::Json::parse(tdx::read_text_utf8(output_path));
    require(result.at("operation").as_string() == "sdk-key-upsert" &&
                !result.at("matched").as_bool() &&
                result.at("state_change").as_string() ==
                    "new-record-appended",
            "CLI dispatches SDK-key upsert and appends a new identity");
    require_number(result.at("projected_registry_record_count"), 2,
                   "CLI projected registry remains replayable after upsert");

    auto maximum_u32_input = callback_input(registry_json({record(
        0xffffffffU, 0xffffffffU, 0, "000001", Type::transaction, 9)}));
    maximum_u32_input["callback"]["callback_key_1_raw"] =
        static_cast<std::uint64_t>(0xffffffffU);
    maximum_u32_input["callback"]["callback_key_2_raw"] =
        static_cast<std::uint64_t>(0xffffffffU);
    tdx::atomic_write_text(input_path, maximum_u32_input.dump(-1));
    require(tdx::command_level2_project({
                "--format", "sdk-correlation-transition", "--input",
                tdx::path_utf8(input_path), "--output",
                tdx::path_utf8(output_path), "--compact"}) == 0,
            "CLI accepts the full opaque u32 callback-key range");
    result = tdx::Json::parse(tdx::read_text_utf8(output_path));
    require(result.at("matched").as_bool(),
            "maximum u32 callback keys remain exact through JSON");

    maximum_u32_input["callback"]["callback_key_1_raw"] = 4294967296.0;
    tdx::atomic_write_text(input_path, maximum_u32_input.dump(-1));
    require_throws([&] {
        (void)tdx::command_level2_project({
            "--format", "sdk-correlation-transition", "--input",
            tdx::path_utf8(input_path)});
    }, "unsigned 32-bit integer");

    maximum_u32_input["callback"]["callback_key_1_raw"] = -1;
    tdx::atomic_write_text(input_path, maximum_u32_input.dump(-1));
    require_throws([&] {
        (void)tdx::command_level2_project({
            "--format", "sdk-correlation-transition", "--input",
            tdx::path_utf8(input_path)});
    }, "unsigned 32-bit integer");

    tdx::atomic_write_text(input_path,
                           callback_input(state, true).dump(-1));
    require_throws([&] {
        (void)tdx::command_level2_project({
            "--format", "sdk-correlation-transition", "--input",
            tdx::path_utf8(input_path)});
    }, "unexpected field url");

    auto wrong_registry = state;
    wrong_registry["record_count"] = 99;
    tdx::atomic_write_text(
        input_path, callback_input(std::move(wrong_registry)).dump(-1));
    require_throws([&] {
        (void)tdx::command_level2_project({
            "--format", "sdk-correlation-transition", "--input",
            tdx::path_utf8(input_path)});
    }, "record_count must equal");

    tdx::Bytes invalid_utf8{'{', '"', 'x', '"', ':', '"', 0xc0, 0xaf,
                            '"', '}'};
    tdx::atomic_write_bytes(input_path, invalid_utf8);
    require_throws([&] {
        (void)tdx::command_level2_project({
            "--format", "sdk-correlation-transition", "--input",
            tdx::path_utf8(input_path)});
    }, "valid UTF-8");

    tdx::atomic_write_text(input_path, "{\"x\":\"\\ud800\"}");
    require_throws([&] {
        (void)tdx::command_level2_project({
            "--format", "sdk-correlation-transition", "--input",
            tdx::path_utf8(input_path)});
    }, "invalid Unicode scalar");

    tdx::atomic_write_bytes(input_path,
                            tdx::Bytes(4U * 1024U * 1024U + 1U, '{'));
    require_throws([&] {
        (void)tdx::command_level2_project({
            "--format", "sdk-correlation-transition", "--input",
            tdx::path_utf8(input_path)});
    }, "no larger than 4 MiB");

    require_throws([&] {
        (void)tdx::command_level2_project({
            "--format", "sdk-correlation-transition", "--input",
            tdx::path_utf8(output_path), "--encoding", "json"});
    }, "unknown argument");
}

}  // namespace

int main() {
    try {
        test_callback_first_match_retain_and_no_match();
        test_callback_non_mode_9_consumes_before_resolution();
        test_sdk_key_upsert_replaces_or_appends();
        test_replay_boundary_and_typed_contract();
        test_cli_strict_json_utf8_and_size();
        std::cout << "level2 SDK correlation transition tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "level2 SDK correlation transition tests failed: "
                  << error.what() << '\n';
        return 1;
    }
}
