#include "tdx/level2.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

namespace tdx {
namespace {

constexpr std::size_t batch_limit = 100;
constexpr std::size_t callback_key_pair_size = 8;
constexpr std::size_t correlation_record_size = 25;
constexpr std::uint32_t registry_mode_raw = 9;
constexpr std::size_t abi_slot_count = 6;

struct SubscribeTemplate {
    int data_type;
    const char* name;
};

SubscribeTemplate subscribe_template(Level2SdkFnSubscribeDataType data_type) {
    switch (data_type) {
    case Level2SdkFnSubscribeDataType::transaction:
        return {1801, "transaction"};
    case Level2SdkFnSubscribeDataType::order:
        return {1802, "order"};
    case Level2SdkFnSubscribeDataType::multi_level_quote:
        return {1803, "multi-level-quote"};
    }
    if (static_cast<int>(data_type) == 1807)
        throw Error("fnSubscribeData type 1807 must use the existing sdk-1807-plan");
    throw Error("fnSubscribeData batch plan data type must be 1801, 1802, or 1803");
}

bool six_digit_suffix(const std::string& symbol) {
    return symbol.size() == 8 &&
           std::all_of(symbol.begin() + 2, symbol.end(),
                       [](unsigned char ch) {
                           return ch >= '0' && ch <= '9';
                       });
}

std::uint16_t symbol_market(const std::string& symbol) {
    if (!six_digit_suffix(symbol))
        throw Error("fnSubscribeData symbols must be SZ, SH, or BJ plus exactly six ASCII digits");
    if (symbol.compare(0, 2, "SZ") == 0) return 0;
    if (symbol.compare(0, 2, "SH") == 0) return 1;
    if (symbol.compare(0, 2, "BJ") == 0) return 2;
    throw Error("fnSubscribeData symbols must use an uppercase SZ, SH, or BJ prefix");
}

std::string join_symbols(const std::vector<std::string>& symbols) {
    std::string result;
    result.reserve(symbols.size() * 9 - 1);
    for (std::size_t index = 0; index < symbols.size(); ++index) {
        if (index) result.push_back(',');
        result += symbols[index];
    }
    return result;
}

Json field_layout(const char* name, std::size_t offset, std::size_t size,
                  const char* type) {
    Json field = Json::object();
    field["name"] = name;
    field["offset"] = static_cast<std::uint64_t>(offset);
    field["size"] = static_cast<std::uint64_t>(size);
    field["type"] = type;
    return field;
}

Json correlation_layout() {
    Json layout = Json::array();
    layout.push_back(field_layout("callback_key_1_raw", 0, 4, "u32-le"));
    layout.push_back(field_layout("callback_key_2_raw", 4, 4, "u32-le"));
    layout.push_back(field_layout("market_raw", 8, 2, "u16-le"));
    layout.push_back(field_layout("code_raw", 10, 6, "char[6]"));
    layout.push_back(field_layout("reserved_zero", 16, 1, "u8"));
    layout.push_back(field_layout("data_type", 17, 4, "u32-le"));
    layout.push_back(field_layout("registry_mode_raw", 21, 4, "u32-le"));
    return layout;
}

Json scalar_slot(std::size_t index, const char* name, const char* abi_type,
                 std::uint32_t raw_value) {
    Json slot = Json::object();
    slot["index"] = static_cast<std::uint64_t>(index);
    slot["name"] = name;
    slot["abi_type"] = abi_type;
    slot["raw_value"] = static_cast<std::uint64_t>(raw_value);
    slot["resolved"] = true;
    return slot;
}

Json pointer_slot(std::size_t index, const char* name, const char* abi_type,
                  const char* blocked_by) {
    Json slot = Json::object();
    slot["index"] = static_cast<std::uint64_t>(index);
    slot["name"] = name;
    slot["abi_type"] = abi_type;
    slot["raw_value"] = Json(nullptr);
    slot["resolved"] = false;
    slot["blocked_by"] = blocked_by;
    return slot;
}

}  // namespace

Json build_level2_sdk_fnsubscribe_batch_call_plan(
    const Level2SdkFnSubscribeBatchCallPlanRequest& request) {
    const auto plan_template = subscribe_template(request.data_type);
    if (request.symbols.empty())
        throw Error("fnSubscribeData batch plan requires at least one resolved symbol");
    if (request.symbols.size() > batch_limit)
        throw Error("fnSubscribeData batch plan accepts at most 100 resolved symbols");

    Json symbols = Json::array();
    Json correlation_templates = Json::array();
    for (std::size_t index = 0; index < request.symbols.size(); ++index) {
        const auto& symbol = request.symbols[index];
        const auto market = symbol_market(symbol);

        Json item = Json::object();
        item["index"] = static_cast<std::uint64_t>(index);
        item["symbol"] = symbol;
        item["market_raw"] = static_cast<std::uint64_t>(market);
        item["code_ascii"] = symbol.substr(2);
        item["pre_resolved"] = true;
        symbols.push_back(std::move(item));

        Json correlation = Json::object();
        correlation["index"] = static_cast<std::uint64_t>(index);
        correlation["symbol"] = symbol;
        correlation["callback_key_1_raw"] = Json(nullptr);
        correlation["callback_key_2_raw"] = Json(nullptr);
        correlation["market_raw"] = static_cast<std::uint64_t>(market);
        correlation["code_ascii"] = symbol.substr(2);
        correlation["reserved_zero"] = 0;
        correlation["data_type"] = plan_template.data_type;
        correlation["registry_mode_raw"] =
            static_cast<std::uint64_t>(registry_mode_raw);
        correlation["callback_policy"] =
            "retained-after-matching-callback";
        correlation["ready"] = false;
        correlation["record_built"] = false;
        correlation_templates.push_back(std::move(correlation));
    }

    const auto symbol_list = join_symbols(request.symbols);
    Json logical_arguments = Json::object();
    logical_arguments["symbol_list"] = symbol_list;
    logical_arguments["symbol_delimiter"] = ",";
    logical_arguments["symbol_count"] =
        static_cast<std::uint64_t>(request.symbols.size());
    logical_arguments["data_type"] = plan_template.data_type;
    logical_arguments["reserved_zero"] = 0;

    Json slots = Json::array();
    slots.push_back(pointer_slot(
        0, "owner_window_raw", "host-hwnd-u32", "live-owner-window"));
    auto callback_output = pointer_slot(
        1, "callback_key_outputs_raw", "out-pointer-u32",
        "runtime-callback-key-array-storage");
    callback_output["pointee_item_size"] =
        static_cast<std::uint64_t>(callback_key_pair_size);
    callback_output["pointee_item_count"] =
        static_cast<std::uint64_t>(request.symbols.size());
    callback_output["native_capacity"] =
        static_cast<std::uint64_t>(batch_limit);
    slots.push_back(std::move(callback_output));
    auto list_pointer = pointer_slot(
        2, "symbol_list_raw", "pointer-u32", "runtime-storage");
    list_pointer["logical_value"] = symbol_list;
    list_pointer["materialization"] = "caller-owned comma-delimited ASCII storage";
    slots.push_back(std::move(list_pointer));
    slots.push_back(scalar_slot(
        3, "symbol_count", "i32",
        static_cast<std::uint32_t>(request.symbols.size())));
    slots.push_back(scalar_slot(
        4, "data_type", "i32",
        static_cast<std::uint32_t>(plan_template.data_type)));
    slots.push_back(scalar_slot(5, "reserved_zero", "u32", 0));

    Json blocked_by = Json::array();
    blocked_by.push_back("live-owner-window");
    blocked_by.push_back("loaded-fnSubscribeData-export");
    blocked_by.push_back("runtime-callback-key-array-storage");

    Json sdk_call = Json::object();
    sdk_call["api"] = "fnSubscribeData";
    sdk_call["logical_arguments"] = logical_arguments;
    sdk_call["raw_abi_word_count"] =
        static_cast<std::uint64_t>(abi_slot_count);
    sdk_call["raw_abi_word_size"] = 4;
    sdk_call["raw_abi_slots"] = std::move(slots);
    sdk_call["ready"] = false;
    sdk_call["invoked"] = false;
    sdk_call["blocked_by"] = std::move(blocked_by);

    Json callback_correlation = Json::object();
    callback_correlation["kind"] = "25-byte-local-registry-templates";
    callback_correlation["record_size"] =
        static_cast<std::uint64_t>(correlation_record_size);
    callback_correlation["template_count"] =
        static_cast<std::uint64_t>(request.symbols.size());
    callback_correlation["layout"] = correlation_layout();
    callback_correlation["registry_mode_raw"] =
        static_cast<std::uint64_t>(registry_mode_raw);
    callback_correlation["callback_policy"] =
        "retained-after-matching-callback";
    callback_correlation["templates"] = std::move(correlation_templates);
    callback_correlation["ready"] = false;
    callback_correlation["records_built"] = false;
    callback_correlation["requires_sdk_output"] = true;
    callback_correlation["key_semantics"] =
        "opaque callback correlation keys; not sessions or tokens";

    Json result = Json::object();
    result["schema"] = "tdx-level2-sdk-fnsubscribe-batch-call-plan-v1";
    result["format"] = "sdk-fnsubscribe-batch-plan";
    result["plan_kind"] = "sdk-export-logical-call";
    result["template"] = plan_template.name;
    result["data_type"] = plan_template.data_type;
    result["symbols_pre_resolved"] = true;
    result["symbol_count"] =
        static_cast<std::uint64_t>(request.symbols.size());
    result["batch_limit"] = static_cast<std::uint64_t>(batch_limit);
    result["symbols"] = std::move(symbols);
    result["logical_arguments"] = std::move(logical_arguments);
    result["sdk_call"] = std::move(sdk_call);
    result["callback_correlation"] = std::move(callback_correlation);
    result["host_view_replayed"] = false;
    result["security_resolver_invoked"] = false;
    result["callback_records_built"] = false;
    result["abi_stack_bytes_built"] = false;
    result["wire_bytes_built"] = false;
    result["standalone_network_request"] = false;
    result["network_request_bytes_built"] = false;
    result["network_requests"] = 0;
    result["operation_executed"] = false;
    result["request_sent"] = false;
    result["subscription_sent"] = false;
    result["offline"] = true;
    result["entitlement_bypass"] = false;
    result["evidence"] =
        "TdxW sub_68AB20 constructs comma-delimited SZ/SH/BJ symbols (100-entry host view bound); sub_68C320 calls fnSubscribeData(owner, callback-key-array, list, count, data-type, 0) and prepares mode-9 correlation records";
    return result;
}

}  // namespace tdx
