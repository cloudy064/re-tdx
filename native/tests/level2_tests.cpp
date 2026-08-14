#include "tdx/level2.hpp"

#include "tdx/common.hpp"

#include <cmath>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <utility>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) throw tdx::Error(message);
}

void append_u16(tdx::Bytes& data, std::uint16_t value) {
    data.push_back(static_cast<std::uint8_t>(value));
    data.push_back(static_cast<std::uint8_t>(value >> 8));
}

void append_u32(tdx::Bytes& data, std::uint32_t value) {
    for (int shift = 0; shift < 32; shift += 8) data.push_back(static_cast<std::uint8_t>(value >> shift));
}

void put_u32(tdx::Bytes& data, std::size_t offset, std::uint32_t value) {
    std::memcpy(data.data() + offset, &value, sizeof(value));
}

void put_u64(tdx::Bytes& data, std::size_t offset, std::uint64_t value) {
    std::memcpy(data.data() + offset, &value, sizeof(value));
}

void put_f64(tdx::Bytes& data, std::size_t offset, double value) {
    std::memcpy(data.data() + offset, &value, sizeof(value));
}

void put_f32(tdx::Bytes& data, std::size_t offset, float value) {
    std::memcpy(data.data() + offset, &value, sizeof(value));
}

void append_varint(tdx::Bytes& data, std::int64_t value) {
    auto magnitude = static_cast<std::uint64_t>(value < 0 ? -value : value);
    auto first = static_cast<std::uint8_t>(magnitude & 0x3F);
    magnitude >>= 6;
    if (value < 0) first |= 0x40;
    if (magnitude) first |= 0x80;
    data.push_back(first);
    while (magnitude) {
        auto byte = static_cast<std::uint8_t>(magnitude & 0x7F);
        magnitude >>= 7;
        if (magnitude) byte |= 0x80;
        data.push_back(byte);
    }
}

std::string hex(const tdx::Bytes& data) {
    static const char* digits = "0123456789abcdef";
    std::string result;
    for (auto byte : data) { result.push_back(digits[byte >> 4]); result.push_back(digits[byte & 15]); }
    return result;
}

}  // namespace

int main() {
    try {
        const auto request = tdx::build_level2_direct_request(
            "transaction", 1, "600000", 0x11223344, 1500);
        require(request.size() == 26, "direct request size");
        require(hex(request) == "000000000000100010005405010036303030303044332211dc05",
                "direct request bytes");
        const auto initial_transaction_request =
            tdx::build_level2_direct_request(
                "transaction", 1, "600000", 0x11223344, 1500,
                tdx::Level2DirectRequestVariant::initial);
        require(hex(initial_transaction_request) ==
                    "000000000000100010005305010036303030303044332211dc05",
                "direct initial transaction request command");
        const auto initial_order_request = tdx::build_level2_direct_request(
            "order", 1, "600000", 0x11223344, 1500,
            tdx::Level2DirectRequestVariant::initial);
        require(hex(initial_order_request) ==
                    "000000000000100010005d05010036303030303044332211dc05",
                "direct initial order request command");

        const auto sdk_4653_request = tdx::build_level2_sdk_redirect_request(
            4653, 1, "600000", 80, 10, true, true);
        require(sdk_4653_request.size() == 40 &&
                    sdk_4653_request[0] == 0x2d &&
                    sdk_4653_request[1] == 0x12 &&
                    sdk_4653_request[30] == 1 &&
                    sdk_4653_request[31] == 1,
                "SDK 4653 redirect request layout");
        const auto sdk_4655_request = tdx::build_level2_sdk_redirect_request(
            4655, 1, "600000", 501, 10, true, false);
        require(sdk_4655_request.size() == 46 &&
                    sdk_4655_request[0] == 0x2f &&
                    sdk_4655_request[1] == 0x12 &&
                    sdk_4655_request[34] == 80 &&
                    sdk_4655_request[35] == 0 &&
                    sdk_4655_request[36] == 1,
                "SDK 4655 redirect request layout and native want-number fallback");
        const auto sdk_4680_request = tdx::build_level2_sdk_redirect_request(
            4680, 1, "600000", 80, 5);
        require(sdk_4680_request.size() == 37 &&
                    sdk_4680_request[0] == 0x48 &&
                    sdk_4680_request[1] == 0x12 &&
                    sdk_4680_request[26] == 5,
                "SDK 4680 redirect request layout");

        const auto sdk_json_4655 = tdx::normalize_level2_sdk_json(
            4655, tdx::Json::parse(R"json({"Data":[
              {"transactionPrice":10.01,"transactionTime":9300100,
               "singleVolume":100,"transactionStatus":"B"},
              {"transactionPrice":10.02,"transactionTime":9300200,
               "singleVolume":200,"transactionStatus":"S"}]})json"), 10);
        require(sdk_json_4655.at("schema").as_string() ==
                    "tdx-level2-sdk-json-4655-v1" &&
                    sdk_json_4655.at("native_record_size").as_number() == 18.0 &&
                    sdk_json_4655.at("records").as_array()[0]
                            .at("source_index").as_number() == 1.0 &&
                    sdk_json_4655.at("records").as_array()[0]
                            .at("time").as_string() == "09:30:02" &&
                    sdk_json_4655.at("records").as_array()[0]
                            .at("side").as_string() == "sell" &&
                    sdk_json_4655.at("records").as_array()[1]
                            .at("side").as_string() == "buy",
                "SDK 4655 JSON adapter order, time, and status mapping");
        const auto sdk_json_4655_limited = tdx::normalize_level2_sdk_json(
            4655, tdx::Json::parse(R"json({"Data":[
              {"transactionPrice":10.01,"transactionTime":9300100,
               "singleVolume":100,"transactionStatus":"B"},
              {"transactionPrice":10.02,"transactionTime":9300200,
               "singleVolume":200,"transactionStatus":"S"}]})json"), 1);
        require(sdk_json_4655_limited.at("returned").as_number() == 1.0 &&
                    sdk_json_4655_limited.at("truncated").as_bool(),
                "SDK 4655 JSON adapter bounded output");

        const auto sdk_json_4671 = tdx::normalize_level2_sdk_json(
            4671, tdx::Json::parse(R"json({"Data":{
              "buyList":[{"QUANTITY_":[100,250]}],
              "sellList":[{"QUANTITY_":[999]},
                          {"QUANTITY_":[300,400]}]}})json"), 10);
        require(sdk_json_4671.at("native_body_size").as_number() == 242.0 &&
                    sdk_json_4671.at("native_quantity_capacity").as_number() == 101.0 &&
                    sdk_json_4671.at("buy_quantities_hands").as_array()[1]
                            .as_number() == 2.0 &&
                    sdk_json_4671.at("sell_quantities_hands").as_array()[0]
                            .as_number() == 3.0 &&
                    sdk_json_4671.at("sell_quantities_hands").as_array()[1]
                            .as_number() == 4.0,
                "SDK 4671 JSON adapter selects first buy and last sell arrays");

        const auto sdk_json_4680 = tdx::normalize_level2_sdk_json(
            4680, tdx::Json::parse(R"json({"Data":"{\"datetime\":\"20260801153000\",\"preClosePrice\":9.9,\"openPrice\":10,\"highPrice\":10.5,\"lowPrice\":9.8,\"lastPrice\":10.2,\"volume\":1234,\"amount\":5678.5,\"openInterest\":9,\"buyPrices\":[10.1,10.0,9.9],\"buyVolumes\":[1,2,3],\"sellPrices\":[10.2,10.3,10.4],\"sellVolumes\":[4,5,6]}"})json"),
            20, 5);
        require(sdk_json_4680.at("time").as_string() == "15:30:00" &&
                    sdk_json_4680.at("depth_count").as_number() == 3.0 &&
                    sdk_json_4680.at("buy_levels").as_array()[0]
                            .at("price").as_number() == 9.9 &&
                    sdk_json_4680.at("buy_levels").as_array()[0]
                            .at("volume_raw").as_number() == 3000.0 &&
                    sdk_json_4680.at("sell_levels").as_array()[0]
                            .at("price").as_number() == 10.2 &&
                    sdk_json_4680.at("sell_levels").as_array()[0]
                            .at("volume_raw").as_number() == 4000.0,
                "SDK 4680 JSON adapter orientation and scaling");

        bool sdk_json_bad_time_rejected = false;
        try {
            (void)tdx::normalize_level2_sdk_json(
                4655, tdx::Json::parse(R"json({"Data":[{
                  "transactionPrice":1,"transactionTime":25610000,
                  "singleVolume":1,"transactionStatus":"B"}]})json"));
        } catch (const tdx::Error&) {
            sdk_json_bad_time_rejected = true;
        }
        require(sdk_json_bad_time_rejected,
                "SDK JSON adapter rejects invalid HHMMSS values");
        bool sdk_json_queue_capacity_rejected = false;
        try {
            tdx::Json quantities = tdx::Json::array();
            for (int index = 0; index < 102; ++index) quantities.push_back(100);
            tdx::Json item = tdx::Json::object();
            item["QUANTITY_"] = std::move(quantities);
            tdx::Json side = tdx::Json::array();
            side.push_back(std::move(item));
            tdx::Json data = tdx::Json::object();
            data["buyList"] = std::move(side);
            data["sellList"] = tdx::Json::array();
            tdx::Json envelope = tdx::Json::object();
            envelope["Data"] = std::move(data);
            (void)tdx::normalize_level2_sdk_json(4671, envelope);
        } catch (const tdx::Error&) {
            sdk_json_queue_capacity_rejected = true;
        }
        require(sdk_json_queue_capacity_rejected,
                "SDK 4671 JSON adapter enforces recovered fixed-body capacity");
        bool sdk_json_unpaired_depth_rejected = false;
        try {
            (void)tdx::normalize_level2_sdk_json(
                4680, tdx::Json::parse(R"json({"Data":{
                  "datetime":"20260801153000","buyPrices":[1,2],
                  "buyVolumes":[1],"sellPrices":[],"sellVolumes":[]}})json"));
        } catch (const tdx::Error&) {
            sdk_json_unpaired_depth_rejected = true;
        }
        require(sdk_json_unpaired_depth_rejected,
                "SDK 4680 JSON adapter rejects unpaired price-volume arrays");

        tdx::Level2SdkFnReqDataCallPlanRequest transaction_plan_request;
        transaction_plan_request.market_id = 1;
        transaction_plan_request.code = "600000";
        transaction_plan_request.data_type =
            tdx::Level2SdkFnReqDataType::transaction;
        transaction_plan_request.cursor_raw =
            std::numeric_limits<std::uint32_t>::max();
        transaction_plan_request.request_count = 1500;
        const auto fnreqdata_1801 =
            tdx::build_level2_sdk_fnreqdata_call_plan(
                transaction_plan_request);
        const auto& fnreqdata_1801_slots =
            fnreqdata_1801.at("sdk_call").at("raw_abi_slots").as_array();
        require(fnreqdata_1801.at("schema").as_string() ==
                    "tdx-level2-sdk-fnreqdata-call-plan-v1" &&
                fnreqdata_1801.at("plan_kind").as_string() ==
                    "sdk-export-logical-call" &&
                fnreqdata_1801.at("security").at("market_prefix").as_string() ==
                    "SH" &&
                fnreqdata_1801_slots.size() == 12 &&
                fnreqdata_1801_slots[0].at("raw_value").is_null() &&
                !fnreqdata_1801_slots[0].at("resolved").as_bool() &&
                fnreqdata_1801_slots[2].at("logical_value").as_string() ==
                    "SH" &&
                fnreqdata_1801_slots[3].at("logical_value").as_string() ==
                    "600000" &&
                fnreqdata_1801_slots[4].at("raw_value").as_number() == 0.0 &&
                fnreqdata_1801_slots[5].at("raw_value").as_number() == 0.0 &&
                fnreqdata_1801_slots[6].at("raw_value").as_number() == 0.0 &&
                fnreqdata_1801_slots[7].at("raw_value").as_number() == 1801.0 &&
                fnreqdata_1801_slots[8].at("raw_value").as_number() == 0.0 &&
                fnreqdata_1801_slots[9].at("raw_value").as_number() ==
                    static_cast<double>(std::numeric_limits<std::uint32_t>::max()) &&
                fnreqdata_1801_slots[10].at("raw_value").as_number() == 1500.0 &&
                fnreqdata_1801_slots[11].at("raw_value").as_number() == 0.0 &&
                fnreqdata_1801.at("callback_correlation")
                        .at("registry_mode_raw").as_number() == 1.0 &&
                fnreqdata_1801.at("callback_correlation")
                        .at("callback_key_1_raw").is_null() &&
                !fnreqdata_1801.at("callback_correlation").at("ready").as_bool() &&
                !fnreqdata_1801.at("sdk_call").at("ready").as_bool() &&
                !fnreqdata_1801.at("sdk_call").at("invoked").as_bool() &&
                !fnreqdata_1801.at("abi_stack_bytes_built").as_bool() &&
                !fnreqdata_1801.at("wire_bytes_built").as_bool() &&
                !fnreqdata_1801.at("network_request_bytes_built").as_bool() &&
                !fnreqdata_1801.at("request_sent").as_bool(),
                "fnReqData 1801 exact ABI slots and unresolved offline boundary");

        tdx::Level2SdkFnReqDataCallPlanRequest order_plan_request;
        order_plan_request.market_id = 0;
        order_plan_request.code = "000001";
        order_plan_request.data_type = tdx::Level2SdkFnReqDataType::order;
        order_plan_request.cursor_raw = 0;
        order_plan_request.request_count = 1;
        const auto fnreqdata_1802 =
            tdx::build_level2_sdk_fnreqdata_call_plan(order_plan_request);
        require(fnreqdata_1802.at("data_type").as_number() == 1802.0 &&
                fnreqdata_1802.at("security").at("market_prefix").as_string() ==
                    "SZ" &&
                fnreqdata_1802.at("logical_arguments")
                        .at("cursor_raw").as_number() == 0.0 &&
                fnreqdata_1802.at("logical_arguments")
                        .at("request_count").as_number() == 1.0 &&
                fnreqdata_1802.at("callback_correlation")
                        .at("registry_mode_raw").as_number() == 0.0,
                "fnReqData 1802 zero cursor keeps registry mode raw zero");

        for (const auto data_type : {
                 tdx::Level2SdkFnReqDataType::multi_level_quote,
                 tdx::Level2SdkFnReqDataType::price_queue,
                 tdx::Level2SdkFnReqDataType::extended_quote}) {
            tdx::Level2SdkFnReqDataCallPlanRequest fixed_request;
            fixed_request.market_id = 2;
            fixed_request.code = "430001";
            fixed_request.data_type = data_type;
            const auto fixed_plan =
                tdx::build_level2_sdk_fnreqdata_call_plan(fixed_request);
            const auto expected_type = static_cast<int>(data_type);
            const auto& fixed_slots =
                fixed_plan.at("sdk_call").at("raw_abi_slots").as_array();
            require(fixed_plan.at("data_type").as_number() == expected_type &&
                    fixed_plan.at("security").at("market_prefix").as_string() ==
                        "BJ" &&
                    fixed_plan.at("window_arguments").as_string() ==
                        "fixed-0-and-1" &&
                    fixed_slots[7].at("raw_value").as_number() == expected_type &&
                    fixed_slots[9].at("raw_value").as_number() == 0.0 &&
                    fixed_slots[10].at("raw_value").as_number() == 1.0 &&
                    fixed_plan.at("callback_correlation")
                            .at("registry_mode_raw").as_number() == 0.0,
                    "fixed fnReqData caller exact type/cursor/count slots");
        }

        bool fnreqdata_missing_window_rejected = false;
        try {
            tdx::Level2SdkFnReqDataCallPlanRequest missing;
            missing.market_id = 0;
            missing.code = "000001";
            missing.data_type = tdx::Level2SdkFnReqDataType::transaction;
            missing.cursor_raw = 0;
            (void)tdx::build_level2_sdk_fnreqdata_call_plan(missing);
        } catch (const tdx::Error&) {
            fnreqdata_missing_window_rejected = true;
        }
        require(fnreqdata_missing_window_rejected,
                "fnReqData 1801/1802 require both explicit window fields");
        bool fnreqdata_count_rejected = false;
        try {
            auto invalid = order_plan_request;
            invalid.request_count = 1501;
            (void)tdx::build_level2_sdk_fnreqdata_call_plan(invalid);
        } catch (const tdx::Error&) {
            fnreqdata_count_rejected = true;
        }
        require(fnreqdata_count_rejected,
                "fnReqData 1801/1802 enforce the recovered 1500 count limit");
        bool fnreqdata_fixed_override_rejected = false;
        try {
            tdx::Level2SdkFnReqDataCallPlanRequest invalid;
            invalid.market_id = 0;
            invalid.code = "000001";
            invalid.data_type = tdx::Level2SdkFnReqDataType::price_queue;
            invalid.cursor_raw = 0;
            (void)tdx::build_level2_sdk_fnreqdata_call_plan(invalid);
        } catch (const tdx::Error&) {
            fnreqdata_fixed_override_rejected = true;
        }
        require(fnreqdata_fixed_override_rejected,
                "fixed fnReqData callers reject cursor/count overrides");
        for (const auto invalid_request : {
                 tdx::Level2SdkFnReqDataCallPlanRequest{
                     3, "000001", tdx::Level2SdkFnReqDataType::price_queue},
                 tdx::Level2SdkFnReqDataCallPlanRequest{
                     0, "00000A", tdx::Level2SdkFnReqDataType::price_queue},
                 tdx::Level2SdkFnReqDataCallPlanRequest{
                     0, "000001",
                     static_cast<tdx::Level2SdkFnReqDataType>(1807)}}) {
            bool rejected = false;
            try {
                (void)tdx::build_level2_sdk_fnreqdata_call_plan(invalid_request);
            } catch (const tdx::Error&) {
                rejected = true;
            }
            require(rejected,
                    "fnReqData plan rejects invalid market/code/data-type");
        }

        std::ostringstream fnreqdata_cli_output;
        auto* fnreqdata_previous_output =
            std::cout.rdbuf(fnreqdata_cli_output.rdbuf());
        int fnreqdata_cli_status = -1;
        try {
            fnreqdata_cli_status = tdx::command_level2_build({
                "--format", "sdk-fnreqdata-plan", "--data-type", "1801",
                "--market", "1", "--code", "600000", "--cursor-raw",
                "0xffffffff", "--count", "1500", "--compact"});
        } catch (...) {
            std::cout.rdbuf(fnreqdata_previous_output);
            throw;
        }
        std::cout.rdbuf(fnreqdata_previous_output);
        const auto fnreqdata_cli =
            tdx::Json::parse(fnreqdata_cli_output.str());
        require(fnreqdata_cli_status == 0 &&
                fnreqdata_cli.at("logical_arguments")
                        .at("cursor_raw").as_number() ==
                    static_cast<double>(std::numeric_limits<std::uint32_t>::max()) &&
                fnreqdata_cli.at("logical_arguments")
                        .at("request_count").as_number() == 1500.0,
                "fnReqData CLI preserves the complete unsigned cursor range");
        bool fnreqdata_cli_fixed_override_rejected = false;
        try {
            (void)tdx::command_level2_build({
                "--format", "sdk-fnreqdata-plan", "--data-type", "1803",
                "--market", "0", "--code", "000001", "--count", "1"});
        } catch (const tdx::Error&) {
            fnreqdata_cli_fixed_override_rejected = true;
        }
        require(fnreqdata_cli_fixed_override_rejected,
                "fnReqData CLI rejects overrides for fixed callers");

        const auto fnreqdata_18031_side_1 =
            tdx::build_level2_sdk_fnreqdata_18031_call_plan(
                {1, "600000", 1, 10.25F});
        const auto& fnreqdata_18031_arguments =
            fnreqdata_18031_side_1.at("logical_arguments");
        const auto& fnreqdata_18031_slots =
            fnreqdata_18031_side_1.at("sdk_call")
                .at("raw_abi_slots").as_array();
        const auto& fnreqdata_18031_callback =
            fnreqdata_18031_side_1.at("callback_correlation");
        require(fnreqdata_18031_side_1.at("schema").as_string() ==
                    "tdx-level2-sdk-fnreqdata-18031-call-plan-v1" &&
                fnreqdata_18031_side_1.at("format").as_string() ==
                    "sdk-fnreqdata-18031-plan" &&
                fnreqdata_18031_side_1.at("data_type").as_number() ==
                    18031.0 &&
                fnreqdata_18031_side_1.at("side_mode_raw").as_number() ==
                    1.0 &&
                fnreqdata_18031_side_1.at("sdk_selector_raw").as_number() ==
                    0.0 &&
                fnreqdata_18031_side_1.at("security")
                        .at("market_prefix").as_string() == "SH" &&
                fnreqdata_18031_arguments.at("selected_price_f32_bits_hex")
                        .as_string() == "0x41240000" &&
                fnreqdata_18031_arguments.at("selected_price_f64_bits_hex")
                        .as_string() == "0x4024800000000000" &&
                fnreqdata_18031_arguments.at("cursor_raw").as_number() == 0.0 &&
                fnreqdata_18031_arguments.at("request_count").as_number() ==
                    1.0 &&
                fnreqdata_18031_arguments.at("registry_mode_raw").as_number() ==
                    2.0 &&
                !fnreqdata_18031_arguments
                        .at("registry_mode_is_sdk_abi_slot").as_bool() &&
                fnreqdata_18031_slots.size() == 12 &&
                fnreqdata_18031_slots[0].at("raw_value").is_null() &&
                fnreqdata_18031_slots[1].at("raw_value").is_null() &&
                fnreqdata_18031_slots[1].at("pointee_size").as_number() == 8.0 &&
                fnreqdata_18031_slots[2].at("logical_value").as_string() ==
                    "SH" &&
                fnreqdata_18031_slots[3].at("logical_value").as_string() ==
                    "600000" &&
                fnreqdata_18031_slots[4].at("raw_value").as_number() == 0.0 &&
                fnreqdata_18031_slots[5].at("raw_hex").as_string() ==
                    "0x00000000" &&
                fnreqdata_18031_slots[6].at("raw_hex").as_string() ==
                    "0x40248000" &&
                fnreqdata_18031_slots[7].at("raw_value").as_number() ==
                    18031.0 &&
                fnreqdata_18031_slots[8].at("raw_value").as_number() == 0.0 &&
                fnreqdata_18031_slots[9].at("raw_value").as_number() == 0.0 &&
                fnreqdata_18031_slots[10].at("raw_value").as_number() == 1.0 &&
                fnreqdata_18031_slots[11].at("raw_value").as_number() == 0.0 &&
                fnreqdata_18031_callback.at("kind").as_string() ==
                    "host-global-correlation" &&
                fnreqdata_18031_callback.at("status").as_string() ==
                    "needs-live-host-context" &&
                fnreqdata_18031_callback.at("key").is_null() &&
                fnreqdata_18031_callback.at("registry_mode_raw").as_number() ==
                    2.0 &&
                !fnreqdata_18031_callback
                        .at("uses_25_byte_local_registry").as_bool() &&
                !fnreqdata_18031_callback
                        .at("local_registry_record_built").as_bool() &&
                fnreqdata_18031_callback.as_object().find("record_size") ==
                    fnreqdata_18031_callback.as_object().end() &&
                !fnreqdata_18031_side_1.at("sdk_call").at("ready").as_bool() &&
                !fnreqdata_18031_side_1.at("sdk_call").at("invoked").as_bool() &&
                !fnreqdata_18031_side_1.at("abi_stack_bytes_built").as_bool() &&
                !fnreqdata_18031_side_1.at("wire_bytes_built").as_bool() &&
                !fnreqdata_18031_side_1
                        .at("network_request_bytes_built").as_bool() &&
                !fnreqdata_18031_side_1.at("request_sent").as_bool(),
                "fnReqData 18031 exact promoted-price ABI and global callback boundary");

        for (const auto side_mode_raw : {0, 255}) {
            const auto plan =
                tdx::build_level2_sdk_fnreqdata_18031_call_plan(
                    {0, "000001", static_cast<std::uint8_t>(side_mode_raw),
                     -0.5F});
            require(plan.at("side_mode_raw").as_number() == side_mode_raw &&
                    plan.at("sdk_selector_raw").as_number() == 1.0 &&
                    plan.at("sdk_call").at("raw_abi_slots").as_array()[4]
                            .at("raw_value").as_number() == 1.0,
                    "fnReqData 18031 preserves raw side mode and applies exact selector collapse");
        }

        for (const auto selected_price : {
                 std::numeric_limits<float>::lowest(),
                 std::numeric_limits<float>::denorm_min(),
                 std::numeric_limits<float>::max()}) {
            const auto plan =
                tdx::build_level2_sdk_fnreqdata_18031_call_plan(
                    {2, "430001", 1, selected_price});
            require(plan.at("security").at("market_prefix").as_string() ==
                        "BJ" &&
                    plan.at("logical_arguments")
                        .at("selected_price_f32_bits_hex").is_string(),
                    "fnReqData 18031 accepts finite representable float boundaries");
        }
        for (const auto selected_price : {
                 std::numeric_limits<float>::infinity(),
                 -std::numeric_limits<float>::infinity(),
                 std::numeric_limits<float>::quiet_NaN()}) {
            bool rejected = false;
            try {
                (void)tdx::build_level2_sdk_fnreqdata_18031_call_plan(
                    {0, "000001", 1, selected_price});
            } catch (const tdx::Error&) {
                rejected = true;
            }
            require(rejected,
                    "fnReqData 18031 rejects non-finite float prices");
        }
        for (const auto invalid_request : {
                 tdx::Level2SdkFnReqData18031CallPlanRequest{
                     3, "000001", 1, 1.0F},
                 tdx::Level2SdkFnReqData18031CallPlanRequest{
                     0, "00000A", 1, 1.0F}}) {
            bool rejected = false;
            try {
                (void)tdx::build_level2_sdk_fnreqdata_18031_call_plan(
                    invalid_request);
            } catch (const tdx::Error&) {
                rejected = true;
            }
            require(rejected,
                    "fnReqData 18031 rejects invalid market/code identity");
        }

        std::ostringstream fnreqdata_18031_cli_output;
        auto* fnreqdata_18031_previous_output =
            std::cout.rdbuf(fnreqdata_18031_cli_output.rdbuf());
        int fnreqdata_18031_cli_status = -1;
        try {
            fnreqdata_18031_cli_status = tdx::command_level2_build({
                "--format", "sdk-fnreqdata-18031-plan", "--market", "1",
                "--code", "600000", "--side-mode-raw", "255",
                "--selected-price", "10.25", "--compact"});
        } catch (...) {
            std::cout.rdbuf(fnreqdata_18031_previous_output);
            throw;
        }
        std::cout.rdbuf(fnreqdata_18031_previous_output);
        const auto fnreqdata_18031_cli =
            tdx::Json::parse(fnreqdata_18031_cli_output.str());
        require(fnreqdata_18031_cli_status == 0 &&
                fnreqdata_18031_cli.at("side_mode_raw").as_number() == 255.0 &&
                fnreqdata_18031_cli.at("sdk_selector_raw").as_number() == 1.0 &&
                fnreqdata_18031_cli.at("logical_arguments")
                        .at("selected_price_f64_bits_hex").as_string() ==
                    "0x4024800000000000" &&
                !fnreqdata_18031_cli.at("request_sent").as_bool(),
                "fnReqData 18031 CLI emits exact offline logical call plan");
        std::ostringstream fnreqdata_18031_side_1_cli_output;
        fnreqdata_18031_previous_output =
            std::cout.rdbuf(fnreqdata_18031_side_1_cli_output.rdbuf());
        int fnreqdata_18031_side_1_cli_status = -1;
        try {
            fnreqdata_18031_side_1_cli_status = tdx::command_level2_build({
                "--format", "sdk-fnreqdata-18031-plan", "--market", "0",
                "--code", "000001", "--side-mode-raw", "1",
                "--selected-price", "0.1", "--compact"});
        } catch (...) {
            std::cout.rdbuf(fnreqdata_18031_previous_output);
            throw;
        }
        std::cout.rdbuf(fnreqdata_18031_previous_output);
        const auto fnreqdata_18031_side_1_cli =
            tdx::Json::parse(fnreqdata_18031_side_1_cli_output.str());
        require(fnreqdata_18031_side_1_cli_status == 0 &&
                fnreqdata_18031_side_1_cli.at("side_mode_raw").as_number() ==
                    1.0 &&
                fnreqdata_18031_side_1_cli.at("sdk_selector_raw").as_number() ==
                    0.0 &&
                fnreqdata_18031_side_1_cli.at("callback_correlation")
                        .at("status").as_string() ==
                    "needs-live-host-context" &&
                !fnreqdata_18031_side_1_cli.at("sdk_call")
                        .at("invoked").as_bool(),
                "fnReqData 18031 CLI side-mode 1 smoke remains offline");
        for (const auto& override_args :
             std::vector<std::vector<std::string>>{
                 {"--cursor", "0"}, {"--cursor-raw", "0"},
                 {"--count", "1"}}) {
            auto cli_args = std::vector<std::string>{
                "--format", "sdk-fnreqdata-18031-plan", "--market", "0",
                "--code", "000001", "--side-mode-raw", "1",
                "--selected-price", "1"};
            cli_args.insert(cli_args.end(), override_args.begin(),
                            override_args.end());
            bool rejected = false;
            try {
                (void)tdx::command_level2_build(cli_args);
            } catch (const tdx::Error&) {
                rejected = true;
            }
            require(rejected,
                    "fnReqData 18031 CLI rejects cursor/count overrides");
        }
        bool fnreqdata_18031_cli_price_overflow_rejected = false;
        try {
            (void)tdx::command_level2_build({
                "--format", "sdk-fnreqdata-18031-plan", "--market", "0",
                "--code", "000001", "--side-mode-raw", "1",
                "--selected-price", "3.5e38"});
        } catch (const tdx::Error&) {
            fnreqdata_18031_cli_price_overflow_rejected = true;
        }
        require(fnreqdata_18031_cli_price_overflow_rejected,
                "fnReqData 18031 CLI rejects prices outside finite float range");

        const auto callback_route_1801 =
            tdx::build_level2_sdk_callback_route_plan({
                tdx::Level2SdkCallbackDataType::transaction, 9,
                std::nullopt});
        const auto& callback_route_1801_first =
            callback_route_1801.at("routes").as_array().front();
        require(callback_route_1801.at("schema").as_string() ==
                    "tdx-level2-sdk-callback-route-plan-v1" &&
                callback_route_1801.at("format").as_string() ==
                    "sdk-callback-route-plan" &&
                callback_route_1801.at("data_type").as_number() == 1801.0 &&
                callback_route_1801.at("callback_correlation")
                        .at("record_retention").as_string() ==
                    "retained-after-matching-callback" &&
                callback_route_1801_first.at("message_id_raw").as_number() ==
                    0x54e &&
                callback_route_1801_first.at("api").as_string() ==
                    "SendMessageA" &&
                callback_route_1801_first.at("delivery").as_string() ==
                    "synchronous" &&
                callback_route_1801_first.at("lparam")
                        .at("raw_value").as_number() == 9.0 &&
                !callback_route_1801_first.at("wparam")
                        .at("resolved").as_bool() &&
                !callback_route_1801.at("host_message_dispatch_attempted")
                        .as_bool() &&
                !callback_route_1801.at("sdk_callback_executed").as_bool() &&
                callback_route_1801.at("host_messages_sent").as_number() ==
                    0.0 &&
                !callback_route_1801.at("wire_bytes_built").as_bool() &&
                !callback_route_1801.at("network_request_bytes_built")
                        .as_bool() &&
                !callback_route_1801.at("request_sent").as_bool() &&
                !callback_route_1801.at("subscription_sent").as_bool(),
                "SDK callback 1801 mode 9 route and offline boundary");

        const auto callback_route_1802 =
            tdx::build_level2_sdk_callback_route_plan({
                tdx::Level2SdkCallbackDataType::order, 0,
                std::nullopt});
        require(callback_route_1802.at("routes").as_array().front()
                        .at("message_id_raw").as_number() == 0x54f &&
                callback_route_1802.at("routes").as_array().front()
                        .at("api").as_string() == "PostMessageA" &&
                callback_route_1802.at("callback_correlation")
                        .at("record_retention").as_string() ==
                    "removed-before-body-consumption",
                "SDK callback 1802 non-nine mode is asynchronous while consumed");
        const auto callback_route_1802_mode_9 =
            tdx::build_level2_sdk_callback_route_plan({
                tdx::Level2SdkCallbackDataType::order, 9,
                std::nullopt});
        const auto callback_route_1801_non_9 =
            tdx::build_level2_sdk_callback_route_plan({
                tdx::Level2SdkCallbackDataType::transaction, 8,
                std::nullopt});
        require(callback_route_1802_mode_9.at("routes").as_array().front()
                        .at("api").as_string() == "SendMessageA" &&
                callback_route_1802_mode_9.at("routes").as_array().front()
                        .at("delivery").as_string() == "synchronous" &&
                callback_route_1801_non_9.at("routes").as_array().front()
                        .at("api").as_string() == "PostMessageA" &&
                callback_route_1801_non_9.at("routes").as_array().front()
                        .at("delivery").as_string() == "asynchronous",
                "SDK callback 1801/1802 alone switch delivery on exact mode 9");

        const auto callback_route_1803 =
            tdx::build_level2_sdk_callback_route_plan({
                tdx::Level2SdkCallbackDataType::multi_level_quote, 9,
                std::nullopt});
        require(callback_route_1803.at("routes").as_array().front()
                        .at("message_id_raw").as_number() == 0x551 &&
                callback_route_1803.at("routes").as_array().front()
                        .at("api").as_string() == "PostMessageA" &&
                callback_route_1803.at("callback_correlation")
                        .at("retained_after_matching_callback").as_bool(),
                "SDK callback 1803 mode 9 retains registry but remains asynchronous");

        const auto callback_route_18031 =
            tdx::build_level2_sdk_callback_route_plan({
                tdx::Level2SdkCallbackDataType::order_queue_at_price,
                std::nullopt, std::nullopt});
        require(callback_route_18031.at("routes").as_array().front()
                        .at("message_id_raw").as_number() == 0x551 &&
                callback_route_18031.at("routes").as_array().front()
                        .at("lparam").at("raw_value").as_number() == 2.0 &&
                callback_route_18031.at("callback_correlation")
                        .at("kind").as_string() ==
                    "host-global-correlation" &&
                !callback_route_18031.at("callback_correlation")
                        .at("uses_25_byte_local_registry").as_bool(),
                "SDK callback 18031 fixed lParam and global correlation");

        for (const auto& [type, message_id] :
             std::vector<std::pair<tdx::Level2SdkCallbackDataType, int>>{
                 {tdx::Level2SdkCallbackDataType::price_queue, 0x54d},
                 {tdx::Level2SdkCallbackDataType::extended_quote, 0x54d}}) {
            const auto plan = tdx::build_level2_sdk_callback_route_plan(
                {type, std::numeric_limits<std::uint32_t>::max(),
                 std::nullopt});
            require(plan.at("routes").as_array().front()
                            .at("message_id_raw").as_number() == message_id &&
                    plan.at("routes").as_array().front()
                            .at("lparam").at("raw_value").as_number() ==
                        static_cast<int>(type) &&
                    plan.at("input").at("registry_mode_raw").as_number() ==
                        static_cast<double>(
                            std::numeric_limits<std::uint32_t>::max()),
                    "SDK callback default route preserves full u32 registry mode");
        }

        const auto callback_route_1807_unknown =
            tdx::build_level2_sdk_callback_route_plan({
                tdx::Level2SdkCallbackDataType::quote_update, 9,
                std::nullopt});
        require(callback_route_1807_unknown.at("route_count").as_number() ==
                    3.0 &&
                callback_route_1807_unknown.at("host_time_advanced_branch")
                        .at("conditional_alternatives_preserved").as_bool() &&
                callback_route_1807_unknown.at("routes").as_array()[0]
                        .at("message_id_raw").as_number() == 0x54d &&
                callback_route_1807_unknown.at("routes").as_array()[1]
                        .at("message_id_raw").as_number() == 0x8b9 &&
                callback_route_1807_unknown.at("routes").as_array()[2]
                        .at("message_id_raw").as_number() == 0x91e &&
                callback_route_1807_unknown.at("routes").as_array()[2]
                        .at("condition").as_array().back().as_string() ==
                    "sub_7E44F0-recipient-predicate",
                "SDK callback 1807 unknown host time preserves exact conditional routes");
        const auto callback_route_1807_old =
            tdx::build_level2_sdk_callback_route_plan({
                tdx::Level2SdkCallbackDataType::quote_update, 0, false});
        const auto callback_route_1807_advanced =
            tdx::build_level2_sdk_callback_route_plan({
                tdx::Level2SdkCallbackDataType::quote_update, 0, true});
        require(callback_route_1807_old.at("route_count").as_number() == 1.0 &&
                callback_route_1807_old.at("routes").as_array().front()
                        .at("message_id_raw").as_number() == 0x54d &&
                callback_route_1807_advanced.at("route_count").as_number() ==
                    2.0 &&
                callback_route_1807_advanced.at("routes").as_array().front()
                        .at("message_id_raw").as_number() == 0x8b9,
                "SDK callback 1807 explicit time state selects one host branch");

        for (const auto type : {
                 tdx::Level2SdkCallbackDataType::transaction,
                 tdx::Level2SdkCallbackDataType::order,
                 tdx::Level2SdkCallbackDataType::multi_level_quote,
                 tdx::Level2SdkCallbackDataType::price_queue,
                 tdx::Level2SdkCallbackDataType::quote_update,
                 tdx::Level2SdkCallbackDataType::order_queue_at_price,
                 tdx::Level2SdkCallbackDataType::extended_quote}) {
            const bool is_18031 =
                type == tdx::Level2SdkCallbackDataType::order_queue_at_price;
            const auto plan = tdx::build_level2_sdk_callback_route_plan({
                type,
                is_18031 ? std::optional<std::uint32_t>{}
                         : std::optional<std::uint32_t>{9},
                std::nullopt});
            require(!plan.at("sdk_callback_invoked").as_bool() &&
                    !plan.at("sdk_callback_executed").as_bool() &&
                    !plan.at("host_message_dispatch_attempted").as_bool() &&
                    plan.at("host_messages_sent").as_number() == 0.0 &&
                    !plan.at("wire_bytes_built").as_bool() &&
                    !plan.at("network_request_bytes_built").as_bool() &&
                    plan.at("network_requests").as_number() == 0.0 &&
                    !plan.at("request_sent").as_bool() &&
                    !plan.at("subscription_sent").as_bool(),
                    "every SDK callback route remains offline metadata only");
            for (const auto& route : plan.at("routes").as_array()) {
                require(!route.at("dispatch_attempted").as_bool() &&
                            route.at("messages_sent").as_number() == 0.0,
                        "every SDK callback host route remains unexecuted");
            }
        }

        for (const auto invalid_request : {
                 tdx::Level2SdkCallbackRoutePlanRequest{
                     tdx::Level2SdkCallbackDataType::transaction,
                     std::nullopt, std::nullopt},
                 tdx::Level2SdkCallbackRoutePlanRequest{
                     tdx::Level2SdkCallbackDataType::order_queue_at_price, 9,
                     std::nullopt},
                 tdx::Level2SdkCallbackRoutePlanRequest{
                     tdx::Level2SdkCallbackDataType::order, 9, true},
                 tdx::Level2SdkCallbackRoutePlanRequest{
                     static_cast<tdx::Level2SdkCallbackDataType>(9999), 0,
                     std::nullopt}}) {
            bool rejected = false;
            try {
                (void)tdx::build_level2_sdk_callback_route_plan(
                    invalid_request);
            } catch (const tdx::Error&) {
                rejected = true;
            }
            require(rejected,
                    "SDK callback route plan rejects missing, conflicting, or unknown inputs");
        }

        std::ostringstream callback_route_cli_output;
        auto* callback_route_previous_output =
            std::cout.rdbuf(callback_route_cli_output.rdbuf());
        int callback_route_cli_status = -1;
        try {
            callback_route_cli_status = tdx::command_level2_build({
                "--format", "sdk-callback-route-plan", "--data-type",
                "1807", "--registry-mode-raw", "4294967295",
                "--host-time-advanced", "true", "--compact"});
        } catch (...) {
            std::cout.rdbuf(callback_route_previous_output);
            throw;
        }
        std::cout.rdbuf(callback_route_previous_output);
        const auto callback_route_cli =
            tdx::Json::parse(callback_route_cli_output.str());
        require(callback_route_cli_status == 0 &&
                callback_route_cli.at("input").at("registry_mode_raw")
                        .as_number() == 4294967295.0 &&
                callback_route_cli.at("input").at("host_time_advanced")
                        .as_bool() &&
                callback_route_cli.at("route_count").as_number() == 2.0 &&
                !callback_route_cli.at("host_message_dispatch_attempted")
                        .as_bool(),
                "SDK callback route CLI preserves full u32 and explicit 1807 branch");
        for (const auto invalid_cli : {
                 std::vector<std::string>{
                     "--format", "sdk-callback-route-plan", "--data-type",
                     "1801"},
                 std::vector<std::string>{
                     "--format", "sdk-callback-route-plan", "--data-type",
                     "18031", "--registry-mode-raw", "9"},
                 std::vector<std::string>{
                     "--format", "sdk-callback-route-plan", "--data-type",
                     "1807", "--registry-mode-raw", "0",
                     "--host-time-advanced", "unknown"},
                 std::vector<std::string>{
                     "--format", "sdk-callback-route-plan", "--data-type",
                     "1804", "--registry-mode-raw", "4294967296"},
                 std::vector<std::string>{
                     "--format", "sdk-callback-route-plan", "--data-type",
                     "1804", "--registry-mode-raw", "0", "--market", "1"}}) {
            bool rejected = false;
            try {
                (void)tdx::command_level2_build(invalid_cli);
            } catch (const tdx::Error&) {
                rejected = true;
            }
            require(rejected,
                    "SDK callback route CLI enforces typed fields and u32 bounds");
        }

        const auto callback_invocation = [](
            tdx::Level2SdkCallbackDataType type, std::size_t body_size,
            std::int32_t arg5_raw, std::uint32_t arg6_raw,
            std::optional<std::uint32_t> registry_mode_raw,
            int limit = 20) {
            tdx::Level2SdkCallbackInvocationRequest request;
            request.data_type = type;
            request.callback_arg5_raw = arg5_raw;
            request.callback_arg6_raw = arg6_raw;
            request.registry_mode_raw = registry_mode_raw;
            request.body = tdx::Bytes(body_size, 0);
            return tdx::decode_level2_sdk_callback_invocation(
                request, limit);
        };

        const auto invocation_1801 = callback_invocation(
            tdx::Level2SdkCallbackDataType::transaction, 104, 2,
            std::numeric_limits<std::uint32_t>::max(),
            std::numeric_limits<std::uint32_t>::max(), 1);
        require(invocation_1801.at("schema").as_string() ==
                    "tdx-level2-sdk-callback-invocation-v1" &&
                invocation_1801.at("format").as_string() ==
                    "sdk-callback-invocation" &&
                invocation_1801.at("record_count").as_number() == 2.0 &&
                invocation_1801.at("body_size").as_number() == 104.0 &&
                invocation_1801.at("callback_arguments")
                        .at("callback_arg5_consumed_as_record_count")
                        .as_bool() &&
                invocation_1801.at("callback_arguments")
                        .at("callback_arg6_raw").as_number() ==
                    4294967295.0 &&
                invocation_1801.at("callback_arguments")
                        .at("callback_arg6_semantics").is_null() &&
                !invocation_1801.at("callback_arguments")
                        .at("callback_arg6_consumed_by_recovered_dispatcher")
                        .as_bool() &&
                invocation_1801.at("body_contract")
                        .at("kind").as_string() ==
                    "counted-record-array" &&
                invocation_1801.at("decoded_document")
                        .at("schema").as_string() ==
                    "tdx-level2-sdk-1801-v1" &&
                invocation_1801.at("decoded_document")
                        .at("count").as_number() == 2.0 &&
                invocation_1801.at("decoded_document")
                        .at("records").size() == 1 &&
                invocation_1801.at("decoded_document")
                        .at("truncated").as_bool() &&
                invocation_1801.at("callback_correlation")
                        .at("registry_mode_raw").as_number() ==
                    4294967295.0 &&
                invocation_1801.at("route_plan").at("routes")
                        .as_array().front().at("message_id_raw")
                        .as_number() == 0x54e &&
                !invocation_1801.at("input_body_retained").as_bool(),
                "SDK callback invocation validates arg5 count and reuses 1801 decoder/route plan");

        const auto invocation_1802 = callback_invocation(
            tdx::Level2SdkCallbackDataType::order, 40, 1, 0, 9);
        require(invocation_1802.at("record_count").as_number() == 1.0 &&
                invocation_1802.at("decoded_document")
                        .at("schema").as_string() ==
                    "tdx-level2-sdk-1802-v1" &&
                invocation_1802.at("route_plan").at("routes")
                        .as_array().front().at("api").as_string() ==
                    "SendMessageA" &&
                invocation_1802.at("decoder_reference")
                        .at("existing_decoder_reused").as_bool() &&
                !invocation_1802.at("decoder_reference")
                        .at("parser_logic_duplicated").as_bool(),
                "SDK callback invocation reuses 1802 decoder and exact route metadata");

        struct FixedInvocationFixture {
            tdx::Level2SdkCallbackDataType type;
            std::size_t size;
            const char* schema;
        };
        const std::vector<FixedInvocationFixture> fixed_invocations{
            {tdx::Level2SdkCallbackDataType::multi_level_quote, 32016,
             "tdx-level2-sdk-1803-v1"},
            {tdx::Level2SdkCallbackDataType::order_queue_at_price, 20012,
             "tdx-level2-sdk-18031-v1"},
            {tdx::Level2SdkCallbackDataType::price_queue, 432,
             "tdx-level2-sdk-1804-v1"},
            {tdx::Level2SdkCallbackDataType::quote_update, 380,
             "tdx-level2-sdk-1807-v1"},
            {tdx::Level2SdkCallbackDataType::extended_quote, 380,
             "tdx-level2-sdk-18071-v1"}};
        for (const auto& fixture : fixed_invocations) {
            const bool global_correlation =
                fixture.type ==
                tdx::Level2SdkCallbackDataType::order_queue_at_price;
            const auto normalized = callback_invocation(
                fixture.type, fixture.size,
                std::numeric_limits<std::int32_t>::min(),
                std::numeric_limits<std::uint32_t>::max(),
                global_correlation
                    ? std::optional<std::uint32_t>{}
                    : std::optional<std::uint32_t>{0});
            require(normalized.at("record_count").as_number() == 1.0 &&
                    normalized.at("body_size").as_number() ==
                        static_cast<double>(fixture.size) &&
                    normalized.at("body_contract").at("kind")
                            .as_string() == "single-fixed-body" &&
                    !normalized.at("body_contract")
                            .at("multiple_bodies_allowed").as_bool() &&
                    !normalized.at("callback_arguments")
                            .at("callback_arg5_consumed_as_record_count")
                            .as_bool() &&
                    normalized.at("callback_arguments")
                            .at("callback_arg5_semantics").is_null() &&
                    normalized.at("callback_arguments")
                            .at("callback_arg5_raw").as_number() ==
                        static_cast<double>(
                            std::numeric_limits<std::int32_t>::min()) &&
                    normalized.at("decoded_document").at("schema")
                            .as_string() == fixture.schema &&
                    normalized.at("callback_correlation").at("kind")
                            .as_string() ==
                        (global_correlation ? "host-global-correlation"
                                            : "25-byte-local-registry") &&
                    !normalized.at("callback_executed").as_bool() &&
                    !normalized.at("sdk_callback_invoked").as_bool() &&
                    !normalized.at("sdk_callback_executed").as_bool() &&
                    !normalized.at("host_message_dispatch_attempted")
                            .as_bool() &&
                    normalized.at("host_messages_sent").as_number() == 0.0 &&
                    !normalized.at("sdk_called").as_bool() &&
                    !normalized.at("wire_bytes_built").as_bool() &&
                    !normalized.at("network_request_bytes_built").as_bool() &&
                    normalized.at("network_requests").as_number() == 0.0 &&
                    !normalized.at("request_sent").as_bool() &&
                    !normalized.at("subscription_sent").as_bool() &&
                    normalized.at("offline").as_bool() &&
                    !normalized.at("entitlement_bypass").as_bool(),
                    "fixed SDK callback invocation exact body/decode/no-execution contract");
        }

        for (const auto& fixture : fixed_invocations) {
            for (const auto bad_size : {
                     std::size_t{0}, fixture.size - 1,
                     fixture.size + 1, fixture.size * 2}) {
                bool rejected = false;
                try {
                    const bool global_correlation =
                        fixture.type ==
                        tdx::Level2SdkCallbackDataType::order_queue_at_price;
                    (void)callback_invocation(
                        fixture.type, bad_size, 1, 0,
                        global_correlation
                            ? std::optional<std::uint32_t>{}
                            : std::optional<std::uint32_t>{9});
                } catch (const tdx::Error&) {
                    rejected = true;
                }
                require(rejected,
                        "fixed SDK callback invocation rejects empty, size-1, size+1, and double bodies");
            }
        }

        struct CountedInvocationInvalidFixture {
            tdx::Level2SdkCallbackDataType type;
            std::size_t size;
            std::int32_t arg5;
        };
        for (const auto& fixture :
             std::vector<CountedInvocationInvalidFixture>{
                 {tdx::Level2SdkCallbackDataType::transaction, 0, 0},
                 {tdx::Level2SdkCallbackDataType::transaction, 51, 1},
                 {tdx::Level2SdkCallbackDataType::transaction, 53, 1},
                 {tdx::Level2SdkCallbackDataType::transaction, 104, 1},
                 {tdx::Level2SdkCallbackDataType::transaction, 52, -1},
                 {tdx::Level2SdkCallbackDataType::order, 0, 0},
                 {tdx::Level2SdkCallbackDataType::order, 39, 1},
                 {tdx::Level2SdkCallbackDataType::order, 41, 1},
                 {tdx::Level2SdkCallbackDataType::order, 80, 1},
                 {tdx::Level2SdkCallbackDataType::order, 40, -1}}) {
            bool rejected = false;
            try {
                (void)callback_invocation(
                    fixture.type, fixture.size, fixture.arg5, 0, 9);
            } catch (const tdx::Error&) {
                rejected = true;
            }
            require(rejected,
                    "counted SDK callback invocation rejects malformed size/count combinations");
        }

        constexpr std::size_t callback_invocation_body_limit = 384 * 1024;
        constexpr std::size_t maximum_1801_records =
            callback_invocation_body_limit / 52;
        const auto invocation_1801_at_limit = callback_invocation(
            tdx::Level2SdkCallbackDataType::transaction,
            maximum_1801_records * 52,
            static_cast<std::int32_t>(maximum_1801_records), 0, 9, 0);
        require(invocation_1801_at_limit.at("record_count").as_number() ==
                    static_cast<double>(maximum_1801_records) &&
                invocation_1801_at_limit.at("body_contract")
                        .at("maximum_input_body_size").as_number() ==
                    static_cast<double>(callback_invocation_body_limit),
                "counted SDK callback invocation accepts the largest aligned body within 384 KiB");
        bool invocation_count_over_limit_rejected = false;
        try {
            (void)callback_invocation(
                tdx::Level2SdkCallbackDataType::transaction, 52,
                static_cast<std::int32_t>(maximum_1801_records + 1),
                0, 9);
        } catch (const tdx::Error&) {
            invocation_count_over_limit_rejected = true;
        }
        require(invocation_count_over_limit_rejected,
                "counted SDK callback invocation caps arg5 by the 384 KiB body budget");

        for (const auto invalid_request : {
                 tdx::Level2SdkCallbackInvocationRequest{
                     tdx::Level2SdkCallbackDataType::transaction, 1, 0,
                     std::nullopt, tdx::Bytes(52, 0)},
                 tdx::Level2SdkCallbackInvocationRequest{
                     tdx::Level2SdkCallbackDataType::order_queue_at_price,
                     1, 0, 9, tdx::Bytes(20012, 0)},
                 tdx::Level2SdkCallbackInvocationRequest{
                     static_cast<tdx::Level2SdkCallbackDataType>(9999), 1,
                     0, 0, tdx::Bytes(1, 0)}}) {
            bool rejected = false;
            try {
                (void)tdx::decode_level2_sdk_callback_invocation(
                    invalid_request);
            } catch (const tdx::Error&) {
                rejected = true;
            }
            require(rejected,
                    "SDK callback invocation rejects missing/conflicting correlation and unknown type");
        }

        const auto callback_invocation_fixture =
            std::filesystem::temp_directory_path() /
            "tdx-level2-sdk-callback-invocation-test.bin";
        std::error_code callback_invocation_remove_error;
        std::filesystem::remove(callback_invocation_fixture,
                                callback_invocation_remove_error);
        tdx::atomic_write_bytes(callback_invocation_fixture,
                                tdx::Bytes(432, 0));
        std::ostringstream callback_invocation_cli_output;
        auto* callback_invocation_previous_output =
            std::cout.rdbuf(callback_invocation_cli_output.rdbuf());
        int callback_invocation_cli_status = -1;
        try {
            callback_invocation_cli_status = tdx::command_level2_decode({
                "--format", "sdk-callback-invocation", "--input",
                callback_invocation_fixture.string(), "--data-type", "1804",
                "--arg5-raw", "-2147483648", "--arg6-raw",
                "4294967295", "--registry-mode-raw", "4294967295",
                "--compact"});
        } catch (...) {
            std::cout.rdbuf(callback_invocation_previous_output);
            std::filesystem::remove(callback_invocation_fixture,
                                    callback_invocation_remove_error);
            throw;
        }
        std::cout.rdbuf(callback_invocation_previous_output);
        const auto callback_invocation_cli =
            tdx::Json::parse(callback_invocation_cli_output.str());
        require(callback_invocation_cli_status == 0 &&
                callback_invocation_cli.at("data_type").as_number() ==
                    1804.0 &&
                callback_invocation_cli.at("callback_arguments")
                        .at("callback_arg5_raw").as_number() ==
                    -2147483648.0 &&
                callback_invocation_cli.at("callback_arguments")
                        .at("callback_arg6_raw").as_number() ==
                    4294967295.0 &&
                callback_invocation_cli.at("input")
                        .at("registry_mode_raw").as_number() ==
                    4294967295.0 &&
                !callback_invocation_cli.at("callback_executed").as_bool(),
                "SDK callback invocation CLI preserves signed arg5 and full u32 raw fields");

        for (const auto invalid_cli : {
                 std::vector<std::string>{
                     "--format", "sdk-callback-invocation", "--input",
                     callback_invocation_fixture.string(), "--data-type",
                     "1804", "--arg6-raw", "0", "--registry-mode-raw", "0"},
                 std::vector<std::string>{
                     "--format", "sdk-callback-invocation", "--input",
                     callback_invocation_fixture.string(), "--data-type",
                     "1804", "--arg5-raw", "1", "--registry-mode-raw", "0"},
                 std::vector<std::string>{
                     "--format", "sdk-callback-invocation", "--input",
                     callback_invocation_fixture.string(), "--arg5-raw", "1",
                     "--arg6-raw", "0", "--registry-mode-raw", "0"},
                 std::vector<std::string>{
                     "--format", "sdk-callback-invocation", "--input",
                     callback_invocation_fixture.string(), "--data-type",
                     "1804", "--arg5-raw", "1", "--arg6-raw",
                     "4294967296", "--registry-mode-raw", "0"},
                 std::vector<std::string>{
                     "--format", "sdk-callback-invocation", "--input",
                     callback_invocation_fixture.string(), "--data-type",
                     "1804", "--arg5-raw", "1", "--arg6-raw", "0",
                     "--registry-mode-raw", "4294967296"},
                 std::vector<std::string>{
                     "--format", "sdk-callback-invocation", "--input",
                     callback_invocation_fixture.string(), "--data-type",
                     "1804", "--arg5-raw", "1", "--arg6-raw", "0",
                     "--registry-mode-raw", "0", "--xor-key", "0"}}) {
            bool rejected = false;
            try {
                (void)tdx::command_level2_decode(invalid_cli);
            } catch (const tdx::Error&) {
                rejected = true;
            }
            require(rejected,
                    "SDK callback invocation CLI enforces required fields, u32 bounds, and strict options");
        }
        tdx::atomic_write_bytes(
            callback_invocation_fixture,
            tdx::Bytes(callback_invocation_body_limit + 1, 0));
        bool callback_invocation_cli_oversize_rejected = false;
        try {
            (void)tdx::command_level2_decode({
                "--format", "sdk-callback-invocation", "--input",
                callback_invocation_fixture.string(), "--data-type", "1801",
                "--arg5-raw", "1", "--arg6-raw", "0",
                "--registry-mode-raw", "0"});
        } catch (const tdx::Error&) {
            callback_invocation_cli_oversize_rejected = true;
        }
        require(callback_invocation_cli_oversize_rejected,
                "SDK callback invocation CLI rejects oversized raw files before decoding");
        std::filesystem::remove(callback_invocation_fixture,
                                callback_invocation_remove_error);

        const auto fnsubscribe_batch =
            tdx::build_level2_sdk_fnsubscribe_batch_call_plan({
                tdx::Level2SdkFnSubscribeDataType::transaction,
                {"SZ000001", "SH600000", "BJ430001"}});
        const auto& fnsubscribe_arguments =
            fnsubscribe_batch.at("logical_arguments");
        const auto& fnsubscribe_slots =
            fnsubscribe_batch.at("sdk_call").at("raw_abi_slots").as_array();
        const auto& fnsubscribe_correlations =
            fnsubscribe_batch.at("callback_correlation");
        const auto& fnsubscribe_templates =
            fnsubscribe_correlations.at("templates").as_array();
        require(fnsubscribe_batch.at("schema").as_string() ==
                    "tdx-level2-sdk-fnsubscribe-batch-call-plan-v1" &&
                fnsubscribe_batch.at("format").as_string() ==
                    "sdk-fnsubscribe-batch-plan" &&
                fnsubscribe_batch.at("data_type").as_number() == 1801.0 &&
                fnsubscribe_batch.at("symbol_count").as_number() == 3.0 &&
                fnsubscribe_batch.at("batch_limit").as_number() == 100.0 &&
                fnsubscribe_arguments.at("symbol_list").as_string() ==
                    "SZ000001,SH600000,BJ430001" &&
                fnsubscribe_arguments.at("symbol_delimiter").as_string() ==
                    "," &&
                fnsubscribe_arguments.at("symbol_count").as_number() == 3.0 &&
                fnsubscribe_arguments.at("data_type").as_number() == 1801.0 &&
                fnsubscribe_arguments.at("reserved_zero").as_number() == 0.0 &&
                fnsubscribe_slots.size() == 6 &&
                fnsubscribe_slots[0].at("raw_value").is_null() &&
                fnsubscribe_slots[1].at("raw_value").is_null() &&
                fnsubscribe_slots[1].at("pointee_item_size").as_number() ==
                    8.0 &&
                fnsubscribe_slots[1].at("pointee_item_count").as_number() ==
                    3.0 &&
                fnsubscribe_slots[1].at("native_capacity").as_number() ==
                    100.0 &&
                fnsubscribe_slots[2].at("logical_value").as_string() ==
                    "SZ000001,SH600000,BJ430001" &&
                fnsubscribe_slots[3].at("raw_value").as_number() == 3.0 &&
                fnsubscribe_slots[4].at("raw_value").as_number() == 1801.0 &&
                fnsubscribe_slots[5].at("raw_value").as_number() == 0.0 &&
                fnsubscribe_correlations.at("record_size").as_number() ==
                    25.0 &&
                fnsubscribe_correlations.at("template_count").as_number() ==
                    3.0 &&
                fnsubscribe_correlations.at("registry_mode_raw").as_number() ==
                    9.0 &&
                fnsubscribe_correlations.at("callback_policy").as_string() ==
                    "retained-after-matching-callback" &&
                fnsubscribe_templates.size() == 3 &&
                fnsubscribe_templates[0].at("market_raw").as_number() == 0.0 &&
                fnsubscribe_templates[1].at("market_raw").as_number() == 1.0 &&
                fnsubscribe_templates[2].at("market_raw").as_number() == 2.0 &&
                fnsubscribe_templates[2].at("code_ascii").as_string() ==
                    "430001" &&
                fnsubscribe_templates[0].at("callback_key_1_raw").is_null() &&
                fnsubscribe_templates[0].at("callback_key_2_raw").is_null() &&
                fnsubscribe_templates[0].at("registry_mode_raw").as_number() ==
                    9.0 &&
                !fnsubscribe_templates[0].at("ready").as_bool() &&
                !fnsubscribe_batch.at("sdk_call").at("ready").as_bool() &&
                !fnsubscribe_batch.at("sdk_call").at("invoked").as_bool() &&
                !fnsubscribe_batch.at("host_view_replayed").as_bool() &&
                !fnsubscribe_batch.at("security_resolver_invoked").as_bool() &&
                !fnsubscribe_batch.at("callback_records_built").as_bool() &&
                !fnsubscribe_batch.at("wire_bytes_built").as_bool() &&
                !fnsubscribe_batch.at("network_request_bytes_built").as_bool() &&
                !fnsubscribe_batch.at("request_sent").as_bool() &&
                !fnsubscribe_batch.at("subscription_sent").as_bool(),
                "fnSubscribeData exact batch call, correlation templates, and offline boundary");

        for (const auto type : {tdx::Level2SdkFnSubscribeDataType::order,
                                tdx::Level2SdkFnSubscribeDataType::multi_level_quote}) {
            const auto plan =
                tdx::build_level2_sdk_fnsubscribe_batch_call_plan(
                    {type, {"SZ000001"}});
            require(plan.at("data_type").as_number() ==
                        static_cast<int>(type) &&
                    plan.at("sdk_call").at("raw_abi_slots").as_array()[4]
                            .at("raw_value").as_number() ==
                        static_cast<int>(type),
                    "fnSubscribeData accepts each statically proven data type");
        }

        const auto fnsubscribe_max_batch =
            tdx::build_level2_sdk_fnsubscribe_batch_call_plan({
                tdx::Level2SdkFnSubscribeDataType::transaction,
                std::vector<std::string>(100, "SZ000001")});
        require(fnsubscribe_max_batch.at("symbol_count").as_number() == 100.0,
                "fnSubscribeData accepts the exact native 100-symbol bound");
        for (const auto symbols : {
                 std::vector<std::string>{},
                 std::vector<std::string>(101, "SZ000001")}) {
            bool rejected = false;
            try {
                (void)tdx::build_level2_sdk_fnsubscribe_batch_call_plan({
                    tdx::Level2SdkFnSubscribeDataType::transaction, symbols});
            } catch (const tdx::Error&) {
                rejected = true;
            }
            require(rejected,
                    "fnSubscribeData enforces the 1..100 symbol batch bound");
        }
        for (const auto* invalid_symbol : {
                 "sz000001", "HK000001", "SZ00001", "SH60000A"}) {
            bool rejected = false;
            try {
                (void)tdx::build_level2_sdk_fnsubscribe_batch_call_plan({
                    tdx::Level2SdkFnSubscribeDataType::transaction,
                    {invalid_symbol}});
            } catch (const tdx::Error&) {
                rejected = true;
            }
            require(rejected,
                    "fnSubscribeData accepts only resolved uppercase mainland symbols");
        }
        std::string fnsubscribe_1807_error;
        try {
            (void)tdx::build_level2_sdk_fnsubscribe_batch_call_plan({
                static_cast<tdx::Level2SdkFnSubscribeDataType>(1807),
                {"SZ000001"}});
        } catch (const tdx::Error& error) {
            fnsubscribe_1807_error = error.what();
        }
        require(fnsubscribe_1807_error.find("sdk-1807-plan") !=
                    std::string::npos,
                "fnSubscribeData type 1807 points to its existing specialized plan");

        const auto fnsubscribe_fixture =
            std::filesystem::temp_directory_path() /
            "tdx-level2-fnsubscribe-batch-plan-test.json";
        std::error_code fnsubscribe_cleanup_error;
        std::filesystem::remove(fnsubscribe_fixture,
                                fnsubscribe_cleanup_error);
        tdx::atomic_write_text(
            fnsubscribe_fixture,
            R"json({"data_type":1802,"symbols":["SZ000001","BJ430001"]})json");
        std::ostringstream fnsubscribe_cli_output;
        auto* fnsubscribe_previous_output =
            std::cout.rdbuf(fnsubscribe_cli_output.rdbuf());
        int fnsubscribe_cli_status = -1;
        try {
            fnsubscribe_cli_status = tdx::command_level2_build({
                "--format", "sdk-fnsubscribe-batch-plan", "--input",
                tdx::path_utf8(fnsubscribe_fixture), "--compact"});
        } catch (...) {
            std::cout.rdbuf(fnsubscribe_previous_output);
            throw;
        }
        std::cout.rdbuf(fnsubscribe_previous_output);
        const auto fnsubscribe_cli =
            tdx::Json::parse(fnsubscribe_cli_output.str());
        require(fnsubscribe_cli_status == 0 &&
                fnsubscribe_cli.at("data_type").as_number() == 1802.0 &&
                fnsubscribe_cli.at("logical_arguments")
                        .at("symbol_list").as_string() ==
                    "SZ000001,BJ430001" &&
                !fnsubscribe_cli.at("sdk_call").at("invoked").as_bool() &&
                !fnsubscribe_cli.at("subscription_sent").as_bool(),
                "fnSubscribeData CLI consumes the strict JSON batch input");

        const auto fnsubscribe_cli_rejects =
            [&](const std::string& document) {
                tdx::atomic_write_text(fnsubscribe_fixture, document);
                try {
                    (void)tdx::command_level2_build({
                        "--format", "sdk-fnsubscribe-batch-plan", "--input",
                        tdx::path_utf8(fnsubscribe_fixture), "--compact"});
                } catch (const tdx::Error&) {
                    return true;
                }
                return false;
            };
        require(fnsubscribe_cli_rejects("[]") &&
                    fnsubscribe_cli_rejects(
                        R"json({"data_type":1801,"symbols":["SZ000001"],"extra":0})json") &&
                    fnsubscribe_cli_rejects(
                        R"json({"data_type":1801.5,"symbols":["SZ000001"]})json") &&
                    fnsubscribe_cli_rejects(
                        R"json({"data_type":1801,"symbols":"SZ000001"})json"),
                "fnSubscribeData CLI enforces its exact JSON object schema");
        tdx::atomic_write_bytes(
            fnsubscribe_fixture,
            tdx::Bytes{'{', '"', 'x', '"', ':', '"', 0xc0, 0xaf, '"', '}'});
        bool fnsubscribe_invalid_utf8_rejected = false;
        try {
            (void)tdx::command_level2_build({
                "--format", "sdk-fnsubscribe-batch-plan", "--input",
                tdx::path_utf8(fnsubscribe_fixture)});
        } catch (const tdx::Error&) {
            fnsubscribe_invalid_utf8_rejected = true;
        }
        require(fnsubscribe_invalid_utf8_rejected,
                "fnSubscribeData CLI rejects malformed UTF-8 before JSON parsing");
        std::filesystem::remove(fnsubscribe_fixture,
                                fnsubscribe_cleanup_error);

        const auto fasthq_subscribe =
            tdx::build_level2_fasthq_subscribe_job_plan(
                {0, "000001", 2, tdx::Level2FastHqOperation::subscribe});
        const auto& fasthq_subscribe_job =
            fasthq_subscribe.at("logical_job");
        require(fasthq_subscribe.at("schema").as_string() ==
                    "tdx-level2-fasthq-subscribe-job-plan-v1" &&
                fasthq_subscribe_job.as_object().size() == 8 &&
                fasthq_subscribe_job.at("Name").as_string() ==
                    "FastHQ.Subscribe" &&
                fasthq_subscribe_job.at("CODE").as_string() == "000001" &&
                fasthq_subscribe_job.at("SC").as_number() == 0.0 &&
                fasthq_subscribe_job.at("LX").as_number() == 2.0 &&
                fasthq_subscribe_job.at("PkgType").as_number() == 0.0 &&
                fasthq_subscribe_job.at("OperType").as_number() == 1.0 &&
                fasthq_subscribe_job.at("PushType").as_number() == 3.0 &&
                fasthq_subscribe_job.at("BatchPush").as_number() == 1.0 &&
                fasthq_subscribe.at("request_field_count").as_number() == 7.0 &&
                fasthq_subscribe.at("projected_field_count").as_number() == 8.0 &&
                fasthq_subscribe.at("logical_field_count").as_number() == 8.0 &&
                fasthq_subscribe.at("field_origins").at("CODE").as_string() ==
                    "IXReq body item" &&
                fasthq_subscribe.at("job_envelope").at("Name").as_string() ==
                    "FastHQ.Subscribe" &&
                fasthq_subscribe.at("job_envelope").at("Body").is_null() &&
                !fasthq_subscribe.at("job_envelope")
                        .at("materialized").as_bool() &&
                fasthq_subscribe.at("lx_raw").as_number() == 2.0 &&
                fasthq_subscribe.at("lx_mapping").is_null() &&
                !fasthq_subscribe.at("wire_bytes_built").as_bool() &&
                !fasthq_subscribe.at("network_request_bytes_built").as_bool() &&
                !fasthq_subscribe.at("job_enqueued").as_bool() &&
                !fasthq_subscribe.at("subscription_sent").as_bool(),
                "FastHQ subscribe exact logical fields and offline boundary");

        const auto fasthq_unsubscribe =
            tdx::build_level2_fasthq_subscribe_job_plan(
                {65535, "999999", std::numeric_limits<std::int32_t>::min(),
                 tdx::Level2FastHqOperation::unsubscribe});
        require(fasthq_unsubscribe.at("operation").as_string() ==
                    "unsubscribe" &&
                fasthq_unsubscribe.at("logical_job").at("SC").as_number() ==
                    65535.0 &&
                fasthq_unsubscribe.at("logical_job").at("LX").as_number() ==
                    static_cast<double>(std::numeric_limits<std::int32_t>::min()) &&
                fasthq_unsubscribe.at("logical_job").at("OperType").as_number() ==
                    0.0 &&
                !fasthq_unsubscribe.at("unsubscribe_sent").as_bool(),
                "FastHQ unsubscribe preserves signed raw LX and remains offline");
        const auto fasthq_lx_max =
            tdx::build_level2_fasthq_subscribe_job_plan(
                {1, "600000", std::numeric_limits<std::int32_t>::max(),
                 tdx::Level2FastHqOperation::subscribe});
        require(fasthq_lx_max.at("logical_job").at("LX").as_number() ==
                    static_cast<double>(std::numeric_limits<std::int32_t>::max()),
                "FastHQ plan accepts the complete signed LX raw range");

        for (const auto* invalid_code : {"00001", "00000A"}) {
            bool rejected = false;
            try {
                (void)tdx::build_level2_fasthq_subscribe_job_plan(
                    {0, invalid_code, 0,
                     tdx::Level2FastHqOperation::subscribe});
            } catch (const tdx::Error&) {
                rejected = true;
            }
            require(rejected, "FastHQ plan rejects invalid CODE");
        }
        bool fasthq_invalid_operation_rejected = false;
        try {
            (void)tdx::build_level2_fasthq_subscribe_job_plan(
                {0, "000001", 0,
                 static_cast<tdx::Level2FastHqOperation>(99)});
        } catch (const tdx::Error&) {
            fasthq_invalid_operation_rejected = true;
        }
        require(fasthq_invalid_operation_rejected,
                "FastHQ plan rejects invalid typed operation");

        std::ostringstream fasthq_cli_output;
        auto* previous_output = std::cout.rdbuf(fasthq_cli_output.rdbuf());
        int fasthq_cli_status = -1;
        try {
            fasthq_cli_status = tdx::command_level2_build({
                "--format", "fasthq-subscribe-plan", "--market", "1",
                "--code", "600000", "--lx-raw", "-7", "--unsubscribe",
                "--compact"});
        } catch (...) {
            std::cout.rdbuf(previous_output);
            throw;
        }
        std::cout.rdbuf(previous_output);
        const auto fasthq_cli = tdx::Json::parse(fasthq_cli_output.str());
        require(fasthq_cli_status == 0 &&
                    fasthq_cli.at("logical_job").at("SC").as_number() == 1.0 &&
                    fasthq_cli.at("logical_job").at("LX").as_number() == -7.0 &&
                    fasthq_cli.at("logical_job").at("OperType").as_number() == 0.0,
                "FastHQ CLI emits the typed logical job plan");
        bool fasthq_cli_binary_rejected = false;
        try {
            (void)tdx::command_level2_build({
                "--format", "fasthq-subscribe-plan", "--market", "0",
                "--code", "000001", "--lx-raw", "2", "--binary-output",
                "must-not-be-created.bin"});
        } catch (const tdx::Error&) {
            fasthq_cli_binary_rejected = true;
        }
        require(fasthq_cli_binary_rejected,
                "FastHQ CLI rejects binary output because no bytes are built");

        const auto tdxw_1369_request = tdx::build_level2_tdxw_1369_request(
            {1, "600000", 1000});
        require(tdxw_1369_request.size() == 40 &&
                    hex(tdxw_1369_request) ==
                        "5905010036303030303000000000000000000000000000000000000000000000000000000001e803",
                "TdxW 1369 exact fixed request body");
        const auto tdxw_1371_request = tdx::build_level2_tdxw_1371_request(
            {0, "000001", 11, 10.25F, -1, 5000});
        require(tdxw_1371_request.size() == 48 &&
                    hex(tdxw_1371_request) ==
                        "5b0500003030303030310000000000000000000000000000000000000000000000000000000b00002441ffffffff8813",
                "TdxW 1371 exact fixed request body");
        bool tdxw_1369_count_rejected = false;
        try {
            (void)tdx::build_level2_tdxw_1369_request({0, "000001", 1001});
        } catch (const tdx::Error&) {
            tdxw_1369_count_rejected = true;
        }
        require(tdxw_1369_count_rejected,
                "TdxW 1369 must reject request-count above 1000");
        bool tdxw_market_rejected = false;
        try {
            (void)tdx::build_level2_tdxw_1369_request({3, "000001", 11});
        } catch (const tdx::Error&) {
            tdxw_market_rejected = true;
        }
        require(tdxw_market_rejected,
                "TdxW fixed requests must reject market-id above 2");
        bool tdxw_1371_price_rejected = false;
        try {
            (void)tdx::build_level2_tdxw_1371_request(
                {0, "000001", 0, 0.0F, -1, 5000});
        } catch (const tdx::Error&) {
            tdxw_1371_price_rejected = true;
        }
        require(tdxw_1371_price_rejected,
                "TdxW 1371 must reject a non-positive selected price");
        bool tdxw_1371_count_rejected = false;
        try {
            (void)tdx::build_level2_tdxw_1371_request(
                {0, "000001", 0, 10.0F, -1, 5001});
        } catch (const tdx::Error&) {
            tdxw_1371_count_rejected = true;
        }
        require(tdxw_1371_count_rejected,
                "TdxW 1371 must reject request-count above 5000");

        tdx::Bytes transaction;
        append_u16(transaction, 2); append_u32(transaction, 77);
        append_u16(transaction, 12600); append_varint(transaction, 100000);
        append_varint(transaction, 5); append_varint(transaction, 2);
        append_varint(transaction, 0); append_varint(transaction, 10);
        append_u16(transaction, 12601); append_varint(transaction, 5);
        append_varint(transaction, 7); append_varint(transaction, 1);
        append_varint(transaction, 1); append_varint(transaction, 11);
        auto direct = tdx::decode_level2_document("direct-transaction", transaction);
        require(direct.at("count").as_number() == 2, "transaction count");
        const auto& rows = direct.at("records").as_array();
        require(rows[0].at("time").as_string() == "09:30:00", "transaction time");
        require(std::abs(rows[0].at("price").as_number() - 10.0) < 1e-9, "transaction price");
        require(rows[1].at("side").as_string() == "sell", "transaction side");

        tdx::Bytes overflowing_varint;
        append_u16(overflowing_varint, 1);
        append_u32(overflowing_varint, 0);
        append_u16(overflowing_varint, 0);
        overflowing_varint.push_back(0x80);
        for (int index = 0; index < 8; ++index)
            overflowing_varint.push_back(0x80);
        overflowing_varint.push_back(0x04);
        for (int index = 0; index < 4; ++index)
            overflowing_varint.push_back(0);
        bool oversized_varint_rejected = false;
        try {
            (void)tdx::decode_level2_document(
                "direct-transaction", overflowing_varint);
        } catch (const tdx::Error& error) {
            oversized_varint_rejected =
                std::string(error.what()).find("price_delta exceeds int64") !=
                std::string::npos;
        }
        require(oversized_varint_rejected,
                "direct varint must reject high bits at shift 62 before truncation");

        tdx::Bytes cumulative_price_overflow;
        append_u16(cumulative_price_overflow, 2);
        append_u32(cumulative_price_overflow, 0);
        append_u16(cumulative_price_overflow, 0);
        append_varint(cumulative_price_overflow,
                      std::numeric_limits<std::int64_t>::max());
        for (int index = 0; index < 4; ++index)
            append_varint(cumulative_price_overflow, 0);
        append_u16(cumulative_price_overflow, 1);
        append_varint(cumulative_price_overflow, 1);
        for (int index = 0; index < 4; ++index)
            append_varint(cumulative_price_overflow, 0);
        bool cumulative_overflow_rejected = false;
        try {
            (void)tdx::decode_level2_document(
                "direct-transaction", cumulative_price_overflow);
        } catch (const tdx::Error& error) {
            cumulative_overflow_rejected =
                std::string(error.what()).find(
                    "cumulative Level2 price overflows int64") !=
                std::string::npos;
        }
        require(cumulative_overflow_rejected,
                "direct cumulative price must reject signed overflow");

        tdx::Bytes sdk_transactions(104, 0);
        put_u64(sdk_transactions, 0, 9300012);
        put_f64(sdk_transactions, 8, 10.25);
        put_u64(sdk_transactions, 16, 12300);
        put_u32(sdk_transactions, 36, 81);
        put_u32(sdk_transactions, 40, 82);
        put_u32(sdk_transactions, 48, 1);
        put_u64(sdk_transactions, 52, 9300013);
        put_f64(sdk_transactions, 60, 10.24);
        put_u64(sdk_transactions, 68, 4500);
        put_u32(sdk_transactions, 100, 2);
        const auto sdk_1801 = tdx::decode_level2_document(
            "sdk-1801", sdk_transactions, 1);
        require(sdk_1801.at("record_size").as_number() == 52.0 &&
                    sdk_1801.at("count").as_number() == 2.0 &&
                    sdk_1801.at("batch_semantics").as_string() ==
                            "counted-record-array" &&
                    sdk_1801.at("truncated").as_bool() &&
                    sdk_1801.at("records").as_array().front()
                            .at("side").as_string() == "buy" &&
                    sdk_1801.at("records").as_array().front()
                            .at("auxiliary_2_raw").as_number() == 82.0,
                "SDK 1801 fixed transaction records");

        tdx::Bytes sdk_orders(80, 0);
        put_u64(sdk_orders, 0, 9300020);
        put_f64(sdk_orders, 8, 10.26);
        put_u64(sdk_orders, 16, 8800);
        put_u32(sdk_orders, 24, 70001);
        put_u32(sdk_orders, 36, 3);
        put_u64(sdk_orders, 40, 9300021);
        put_f64(sdk_orders, 48, 10.27);
        put_u64(sdk_orders, 56, 9900);
        put_u32(sdk_orders, 64, 70002);
        put_u32(sdk_orders, 76, 4);
        const auto sdk_1802 = tdx::decode_level2_document(
            "sdk-1802", sdk_orders);
        require(sdk_1802.at("record_size").as_number() == 40.0 &&
                    sdk_1802.at("count").as_number() == 2.0 &&
                    sdk_1802.at("batch_semantics").as_string() ==
                            "counted-record-array" &&
                    sdk_1802.at("records").as_array()[0]
                            .at("record_type").as_string() == "buy-cancel" &&
                    sdk_1802.at("records").as_array()[1]
                            .at("record_type").as_string() == "sell-cancel" &&
                    sdk_1802.at("records").as_array()[1]
                            .at("order_id_raw").as_number() == 70002.0,
                "SDK 1802 fixed order and cancellation records");

        tdx::Bytes sdk_price_queues(432, 0);
        put_f64(sdk_price_queues, 8, 10.23);
        put_f32(sdk_price_queues, 16, 100.0F);
        put_f32(sdk_price_queues, 20, 200.0F);
        put_f64(sdk_price_queues, 216, 10.24);
        put_f32(sdk_price_queues, 224, 300.0F);
        put_f32(sdk_price_queues, 228, 400.0F);
        put_f32(sdk_price_queues, 232, 500.0F);
        put_u32(sdk_price_queues, 424, 2);
        put_u32(sdk_price_queues, 428, 3);
        const auto sdk_1804 = tdx::decode_level2_document(
            "sdk-1804", sdk_price_queues, 4);
        require(sdk_1804.at("record_size").as_number() == 432.0 &&
                    sdk_1804.at("body_count").as_number() == 1.0 &&
                    sdk_1804.at("batch_semantics").as_string() ==
                            "single-fixed-body" &&
                    sdk_1804.at("first_count").as_number() == 2.0 &&
                    sdk_1804.at("second_count").as_number() == 3.0 &&
                    sdk_1804.at("first_quantities_raw").size() == 2 &&
                    sdk_1804.at("second_quantities_raw").size() == 2 &&
                    sdk_1804.at("second_quantities_raw").as_array()[1]
                            .as_number() == 400.0 &&
                    sdk_1804.at("truncated").as_bool(),
                "SDK 1804 two-price quantity arrays and bounded output");

        tdx::Bytes sdk_quote_updates(380, 0);
        put_u64(sdk_quote_updates, 0, 1786498200123ULL);
        put_f64(sdk_quote_updates, 8, 10.25);
        put_f64(sdk_quote_updates, 16, 10.10);
        put_f64(sdk_quote_updates, 24, 10.30);
        put_f64(sdk_quote_updates, 32, 10.05);
        put_f64(sdk_quote_updates, 40, 123456789.0);
        put_u64(sdk_quote_updates, 48, 987654);
        put_u32(sdk_quote_updates, 64, 11);
        put_u32(sdk_quote_updates, 72, 4321);
        put_f64(sdk_quote_updates, 76, 10.00);
        put_f64(sdk_quote_updates, 108, 10.26);
        put_f32(sdk_quote_updates, 188, 1200.0F);
        put_f64(sdk_quote_updates, 228, 10.24);
        put_f32(sdk_quote_updates, 308, 1500.0F);
        put_f64(sdk_quote_updates, 348, 10.20);
        put_f32(sdk_quote_updates, 356, 8500.0F);
        put_f64(sdk_quote_updates, 364, 10.28);
        put_f32(sdk_quote_updates, 372, 7600.0F);
        const auto sdk_1807 = tdx::decode_level2_document(
            "sdk-1807", sdk_quote_updates, 1);
        const auto &sdk_quote = sdk_1807.at("records").as_array().front();
        require(sdk_1807.at("record_size").as_number() == 380.0 &&
                    sdk_1807.at("count").as_number() == 1.0 &&
                    sdk_1807.at("batch_semantics").as_string() ==
                            "single-fixed-body" &&
                    !sdk_1807.at("truncated").as_bool() &&
                    std::abs(sdk_quote.at("last_price").as_number() - 10.25) < 1e-9 &&
                    std::abs(sdk_quote.at("ask_levels").as_array().front()
                                     .at("price").as_number() - 10.26) < 1e-9 &&
                    sdk_quote.at("bid_levels").as_array().front()
                            .at("quantity_raw").as_number() == 1500.0 &&
                    sdk_quote.at("aggregate").at("total_ask_quantity_raw")
                            .as_number() == 7600.0,
                "SDK 1807 quote update and ten-level depth layout");
        const auto sdk_18071 = tdx::decode_level2_document(
            "sdk-18071", tdx::Bytes(sdk_quote_updates.begin(),
                                      sdk_quote_updates.begin() + 380));
        require(sdk_18071.at("data_type").as_number() == 18071.0 &&
                    sdk_18071.at("schema").as_string() ==
                            "tdx-level2-sdk-18071-v1" &&
                    sdk_18071.at("batch_semantics").as_string() ==
                            "single-fixed-body",
                "SDK 18071 shares the recovered quote-update body");
        for (const auto& [format, invalid_size] :
             std::vector<std::pair<std::string, std::size_t>>{
                 {"sdk-1803", 32017}, {"sdk-18031", 20013},
                 {"sdk-1807", 760}, {"sdk-18071", 759}}) {
            bool rejected = false;
            try {
                (void)tdx::decode_level2_document(
                    format, tdx::Bytes(invalid_size, 0), 1);
            } catch (const tdx::Error&) {
                rejected = true;
            }
            require(rejected,
                    "fixed SDK callbacks must reject trailing or batched bytes");
        }
        for (const auto& [format, exact_size] :
             std::vector<std::pair<std::string, std::size_t>>{
                 {"sdk-1803", 32016}, {"sdk-18031", 20012},
                 {"sdk-1804", 432}, {"sdk-1807", 380},
                 {"sdk-18071", 380}}) {
            for (const auto invalid_size :
                 {exact_size - 1, exact_size + 1, exact_size * 2}) {
                bool rejected = false;
                try {
                    (void)tdx::decode_level2_document(
                        format, tdx::Bytes(invalid_size, 0), 1);
                } catch (const tdx::Error&) {
                    rejected = true;
                }
                require(rejected,
                        "single fixed SDK body must reject size-1, size+1 and double");
            }
        }
        const auto sdk_1803_empty = tdx::decode_level2_document(
            "sdk-1803", tdx::Bytes(32016, 0), 1);
        require(sdk_1803_empty.at("record_size").as_number() == 32016.0 &&
                    sdk_1803_empty.at("body_count").as_number() == 1.0 &&
                    sdk_1803_empty.at("batch_semantics").as_string() ==
                            "single-fixed-body",
                "SDK 1803 single fixed-body metadata");
        const auto sdk_18031_empty = tdx::decode_level2_document(
            "sdk-18031", tdx::Bytes(20012, 0), 1);
        require(sdk_18031_empty.at("record_size").as_number() == 20012.0 &&
                    sdk_18031_empty.at("body_count").as_number() == 1.0 &&
                    sdk_18031_empty.at("batch_semantics").as_string() ==
                            "single-fixed-body",
                "SDK 18031 single fixed-body metadata");

        tdx::Bytes sdk_correlation;
        const auto append_correlation_record =
            [&](std::uint32_t key_1, std::uint32_t key_2,
                std::uint16_t market, const std::string& code,
                std::uint32_t data_type, std::uint32_t mode) {
                append_u32(sdk_correlation, key_1);
                append_u32(sdk_correlation, key_2);
                append_u16(sdk_correlation, market);
                sdk_correlation.insert(
                    sdk_correlation.end(), code.begin(), code.end());
                sdk_correlation.push_back(0);
                append_u32(sdk_correlation, data_type);
                append_u32(sdk_correlation, mode);
            };
        append_correlation_record(
            0x11223344, 0x55667788, 1, "600000", 1807, 9);
        append_correlation_record(
            0xaabbccdd, 0x01020304, 0, "000001", 1803, 0);
        require(sdk_correlation.size() == 50,
                "SDK correlation exact two-record fixture size");
        const auto correlation = tdx::decode_level2_document(
            "sdk-correlation", sdk_correlation, 1);
        const auto& correlation_row =
            correlation.at("records").as_array().front();
        require(correlation.at("schema").as_string() ==
                        "tdx-level2-sdk-correlation-registry-v1" &&
                    correlation.at("record_size").as_number() == 25.0 &&
                    correlation.at("record_count").as_number() == 2.0 &&
                    correlation.at("batch_semantics").as_string() ==
                        "exact-record-array" &&
                    correlation.at("truncated").as_bool() &&
                    !correlation.at("network_payload").as_bool() &&
                    !correlation.at("session_record").as_bool() &&
                    !correlation.at("token_record").as_bool(),
                "SDK callback correlation registry document boundary");
        require(correlation_row.at("callback_key_1_raw").as_number() ==
                            0x11223344 &&
                    correlation_row.at("callback_key_2_raw").as_number() ==
                            0x55667788 &&
                    correlation_row.at("market_raw").as_number() == 1.0 &&
                    correlation_row.at("code_ascii").as_string() == "600000" &&
                    correlation_row.at("data_type").as_number() == 1807.0 &&
                    correlation_row.at("registry_mode_raw").as_number() == 9.0 &&
                    correlation_row.at("callback_policy").as_string() ==
                        "retained-after-matching-callback",
                "SDK callback correlation exact record layout and mode-9 policy");
        const auto all_correlations =
            tdx::decode_level2_sdk_correlation_registry(sdk_correlation, 2);
        require(all_correlations.at("records").size() == 2 &&
                    all_correlations.at("records").as_array()[1]
                            .at("callback_policy").as_string() ==
                        "removed-after-matching-callback",
                "SDK correlation arrays preserve non-mode-9 consumption policy");

        for (const auto invalid_size : {0U, 24U, 26U, 49U, 51U}) {
            bool rejected = false;
            try {
                (void)tdx::decode_level2_sdk_correlation_registry(
                    tdx::Bytes(invalid_size, 0), 1);
            } catch (const tdx::Error&) {
                rejected = true;
            }
            require(rejected,
                    "SDK correlation registry must reject non-record-aligned sizes");
        }
        for (const auto malformed_offset : {8U, 10U, 16U}) {
            auto malformed = tdx::Bytes(
                sdk_correlation.begin(), sdk_correlation.begin() + 25);
            malformed[malformed_offset] = malformed_offset == 8 ? 3 : 1;
            bool rejected = false;
            try {
                (void)tdx::decode_level2_sdk_correlation_registry(
                    malformed, 1);
            } catch (const tdx::Error&) {
                rejected = true;
            }
            require(rejected,
                    "SDK correlation registry must reject malformed market/code/reserved fields");
        }
        auto malformed_padding = tdx::Bytes(
            sdk_correlation.begin(), sdk_correlation.begin() + 25);
        malformed_padding[10] = 0;
        bool malformed_padding_rejected = false;
        try {
            (void)tdx::decode_level2_sdk_correlation_registry(
                malformed_padding, 1);
        } catch (const tdx::Error&) {
            malformed_padding_rejected = true;
        }
        require(malformed_padding_rejected,
                "SDK correlation registry must reject non-zero code bytes after NUL");
        bool oversized_correlation_rejected = false;
        try {
            (void)tdx::decode_level2_sdk_correlation_registry(
                tdx::Bytes(10002 * 25, 0), 1);
        } catch (const tdx::Error&) {
            oversized_correlation_rejected = true;
        }
        require(oversized_correlation_rejected,
                "SDK correlation registry must enforce native record bound");

        const tdx::Bytes sdk_1807_identities{
            0, '0', '0', '0', '0', '0', '1',
            1, '6', '0', '0', '0', '0', '0'};
        const auto sdk_1807_plan =
            tdx::build_level2_sdk_1807_request_plan(sdk_1807_identities);
        require(sdk_1807_plan.at("schema").as_string() ==
                        "tdx-level2-sdk-1807-request-plan-v1" &&
                    sdk_1807_plan.at("status").as_string() ==
                        "needs-resolution" &&
                    sdk_1807_plan.at("input").at("record_size").as_number() == 7.0 &&
                    sdk_1807_plan.at("input").at("record_count").as_number() == 2.0 &&
                    sdk_1807_plan.at("input").at("batch_limit").as_number() == 100.0 &&
                    sdk_1807_plan.at("items").as_array()[0]
                        .at("code_ascii").as_string() == "000001" &&
                    sdk_1807_plan.at("items").as_array()[1]
                        .at("route").as_string() == "needs-resolution" &&
                    sdk_1807_plan.at("needs_resolution_count").as_number() == 2.0 &&
                    sdk_1807_plan.at("unresolved").size() == 2,
                "SDK 1807 packed identities remain explicit resolution work");
        require(!sdk_1807_plan.at("sdk_call").at("ready").as_bool() &&
                    sdk_1807_plan.at("sdk_call").at("arguments")
                        .at("symbol_list").is_null() &&
                    sdk_1807_plan.at("sdk_call").at("arguments")
                        .at("symbol_count").as_number() == 0.0 &&
                    sdk_1807_plan.at("sdk_call").at("arguments")
                        .at("data_type").as_number() == 1807.0 &&
                    sdk_1807_plan.at("callback_correlation")
                        .at("registry_mode_raw").as_number() == 9.0 &&
                    sdk_1807_plan.at("callback_correlation")
                        .at("retained_on_callback").as_bool() &&
                    !sdk_1807_plan.at("network_request_bytes_built").as_bool() &&
                    sdk_1807_plan.at("local_fallback")
                        .at("record_size").as_number() == 11.0,
                "SDK 1807 plan separates SDK call, registry mode and local fallback");
        bool empty_plan_rejected = false;
        try {
            (void)tdx::build_level2_sdk_1807_request_plan({});
        } catch (const tdx::Error&) {
            empty_plan_rejected = true;
        }
        require(empty_plan_rejected, "SDK 1807 plan must reject zero records");
        bool oversized_plan_rejected = false;
        try {
            (void)tdx::build_level2_sdk_1807_request_plan(tdx::Bytes(101 * 7, 0));
        } catch (const tdx::Error&) {
            oversized_plan_rejected = true;
        }
        require(oversized_plan_rejected,
                "SDK 1807 plan must reject more than 100 records");

        tdx::Bytes tcalc_order_flow(184, 0);
        put_u32(tcalc_order_flow, 0, 20260812);
        put_f32(tcalc_order_flow, 4, 12.49F);
        for (int index = 0; index < 16; ++index) {
            put_f32(tcalc_order_flow, 8 + index * 4,
                    static_cast<float>(index + 1));
            put_f32(tcalc_order_flow, 72 + index * 4,
                    static_cast<float>(101 + index));
        }
        for (int index = 0; index < 4; ++index)
            put_f32(tcalc_order_flow, 136 + index * 4,
                    static_cast<float>(index) + 1.49f);
        for (int index = 0; index < 8; ++index)
            put_f32(tcalc_order_flow, 152 + index * 4,
                    static_cast<float>(201 + index));
        const auto tcalc = tdx::decode_level2_document(
            "tcalc-order-flow", tcalc_order_flow);
        const auto &tcalc_row = tcalc.at("records").as_array().front();
        const auto &bindings = tcalc_row.at("formula_bindings");
        require(tcalc.at("record_size").as_number() == 184.0 &&
                    tcalc_row.at("date_raw").as_number() == 20260812.0 &&
                    bindings.at("L2_VOL#3#2").as_number() == 15.0 &&
                    bindings.at("L2_AMO#0#0").as_number() == 101.0,
                "TCalc type-31 selector matrices");
        require(bindings.at("L2_VOLNUM#0#0").as_number() == 1.0 &&
                    bindings.at("ACTINVOL").as_number() == 36.0 &&
                    bindings.at("ACTOUTVOL").as_number() == 40.0 &&
                    bindings.at("LARGEINTRDVOL").as_number() == 6.0 &&
                    bindings.at("LARGEOUTTRDVOL").as_number() == 8.0,
                "TCalc type-31 derived formula bindings");
        require(bindings.at("BIDORDERVOL").as_number() == 201.0 &&
                    bindings.at("CUR_SELLORDER").as_number() == 208.0 &&
                    bindings.at("TRADENUM").as_number() == 12.0 &&
                    bindings.at("TRADEINNUM").as_number() == 5.0 &&
                    bindings.at("TRADEOUTNUM").as_number() == 7.0 &&
                    bindings.at("LARGETRDINNUM").as_number() == 1.0 &&
                    bindings.at("LARGETRDOUTNUM").as_number() == 2.0 &&
                    tcalc.at("unavailable_binding").as_string() == "ISBUYORDER",
                "TCalc type-31 scalar bindings and type-104 boundary");
        require(tcalc.at("series").at("TRADENUM")
                            .at("2026-08-12|15:00").as_number() == 12.0 &&
                    tcalc.at("_formula_context").at("direct_context_file")
                            .as_bool() &&
                    tcalc.at("_formula_context").at("binding_count")
                            .as_number() == 53.0,
                "TCalc type-31 decoder emits a direct daily formula context");

        tdx::Bytes tcalc_missing(184, 0);
        put_u32(tcalc_missing, 0, 20260813);
        for (const auto offset : {4U, 8U, 12U, 16U, 20U, 72U, 136U, 140U, 152U})
            put_u32(tcalc_missing, offset, 0xf8f8f8f8U);
        const auto missing_tcalc = tdx::decode_level2_document(
            "tcalc-order-flow", tcalc_missing);
        const auto &missing_row = missing_tcalc.at("records").as_array().front();
        const auto &missing_bindings = missing_row.at("formula_bindings");
        require(missing_row.at("host_auxiliary_f32").is_null() &&
                    missing_row.at("l2_vol").as_array()[0].as_array()[0].is_null() &&
                    missing_row.at("l2_amo").as_array()[0].as_array()[0].is_null() &&
                    missing_row.at("l2_volnum_raw").as_array()[0].as_array()[0].is_null() &&
                    missing_bindings.at("L2_VOL#0#0").is_null() &&
                    missing_bindings.at("L2_AMO#0#0").is_null() &&
                    missing_bindings.at("L2_VOLNUM#0#0").is_null() &&
                    missing_bindings.at("BIDORDERVOL").is_null(),
                "TCalc type-31 raw 0xf8f8f8f8 sentinels decode as null");
        require(missing_bindings.at("ACTINVOL").is_null() &&
                    missing_bindings.at("ACTOUTVOL").is_null() &&
                    missing_bindings.at("LARGEINTRDVOL").is_null() &&
                    missing_bindings.at("LARGEOUTTRDVOL").is_null() &&
                    missing_bindings.at("TRADENUM").is_null() &&
                    missing_bindings.at("TRADEINNUM").is_null() &&
                    missing_bindings.at("TRADEOUTNUM").is_null() &&
                    missing_bindings.at("LARGETRDINNUM").is_null() &&
                    missing_bindings.at("LARGETRDOUTNUM").is_null() &&
                    missing_tcalc.at("series").at("TRADENUM")
                            .at("2026-08-13|15:00").is_null(),
                "TCalc type-31 derived handlers preserve their native missing anchors");
        auto oversized_tcalc_count = tcalc_order_flow;
        put_u32(oversized_tcalc_count, 4, 0x7f7fffffU);
        const auto oversized_tcalc = tdx::decode_level2_document(
            "tcalc-order-flow", oversized_tcalc_count);
        require(oversized_tcalc.at("records").as_array().front()
                    .at("formula_bindings").at("TRADENUM").is_null(),
                "TCalc count conversion rejects finite values outside int range");

        tdx::Json daily_kline = tdx::Json::object();
        daily_kline["period"] = "day";
        daily_kline["bars"] = tdx::Json::array();
        for (const auto& [date, time] : {
                 std::pair{"2026-08-11", "14:57"},
                 std::pair{"2026-08-12", "14:58"},
                 std::pair{"2026-08-13", "14:59"}}) {
            tdx::Json bar = tdx::Json::object();
            bar["date"] = date;
            bar["time"] = time;
            daily_kline["bars"].push_back(std::move(bar));
        }
        const auto materialized = tdx::materialize_tcalc_level2_formula_context(
            tcalc, daily_kline);
        const auto& materialized_series = materialized.at("series").at("TRADENUM");
        require(materialized_series.at("2026-08-11|14:57").is_null() &&
                    materialized_series.at("2026-08-12|14:58").as_number() == 12.0 &&
                    materialized_series.at("2026-08-13|14:59").as_number() == 12.0 &&
                    materialized.at("_formula_context").at("exact_match_count")
                            .as_number() == 1.0 &&
                    materialized.at("_formula_context").at("trailing_inherited_count")
                            .as_number() == 1.0,
                "TCalc type-31 context materializes against real daily K-line stamps");
        auto minute_kline = daily_kline;
        minute_kline["period"] = "1m";
        bool minute_context_rejected = false;
        try {
            (void)tdx::materialize_tcalc_level2_formula_context(tcalc, minute_kline);
        } catch (const tdx::Error&) {
            minute_context_rejected = true;
        }
        require(minute_context_rejected,
                "TCalc type-31 context must not silently bind to minute bars");
        auto truncated_context = tcalc;
        truncated_context["_formula_context"]["context_complete"] = false;
        bool truncated_context_rejected = false;
        try {
            (void)tdx::materialize_tcalc_level2_formula_context(
                std::move(truncated_context), daily_kline);
        } catch (const tdx::Error&) {
            truncated_context_rejected = true;
        }
        require(truncated_context_rejected,
                "TCalc type-31 context must reject truncated decoder output");

        tdx::Bytes tcalc_order_side(104, 0);
        const auto buy_order_side = tdx::decode_level2_document(
            "tcalc-order-side", tcalc_order_side);
        require(buy_order_side.at("record_size").as_number() == 104.0 &&
                    buy_order_side.at("is_buy_order").as_number() == 1.0 &&
                    buy_order_side.at("formula_scalar_bindings")
                            .at("ISBUYORDER").as_number() == 1.0,
                "TCalc type-104 buy-order scalar context");
        tcalc_order_side[46] = 2;
        const auto sell_order_side = tdx::decode_level2_document(
            "tcalc-order-side", tcalc_order_side);
        require(sell_order_side.at("is_buy_order").as_number() == 0.0,
                "TCalc type-104 non-buy order mapping");

        tdx::Bytes queue(0x4E2C, 0);
        const std::uint32_t headers[] = {12, 34, 3, 100, 200, 300};
        std::memcpy(queue.data(), headers, sizeof(headers));
        auto decoded_queue = tdx::decode_level2_document("sdk-18031", queue, 2);
        require(decoded_queue.at("count").as_number() == 3, "18031 count");
        require(decoded_queue.at("quantities_raw").size() == 2, "18031 limit");
        require(decoded_queue.at("truncated").as_bool(), "18031 truncation");

        tdx::Bytes protobuf{0x08, 0x96, 0x01, 0x12, 0x03, 'T', 'D', 'X'};
        auto wire = tdx::decode_level2_document("protobuf", protobuf);
        require(wire.at("field_count_reported").as_number() == 2, "protobuf field count");
        require(wire.at("fields").as_array()[1].at("text_candidate").as_string() == "TDX",
                "protobuf text candidate");
        std::cout << "Level2 offline tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
