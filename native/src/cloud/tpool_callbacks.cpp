#include "tpool_internal.hpp"

#include <array>
#include <cstdint>
#include <string_view>

namespace tdx::tpool_detail {

namespace {

template <std::size_t Size>
Json integer_array(const std::array<int, Size>& values) {
    Json result = Json::array();
    for (const int value : values) result.push_back(value);
    return result;
}

Json operation(int id, std::string_view semantics) {
    Json result = Json::object();
    result["id"] = id;
    result["semantics"] = std::string(semantics);
    return result;
}

Json data_callback() {
    constexpr std::array<int, 10> kOperations{
        4, 32, 102, 104, 105, 111, 122, 125, 126, 134};
    Json value = Json::object();
    value["slot"] = 1;
    value["registration_argument"] = 1;
    value["tpool_global"] = "dword_10067494";
    value["host_function"] = "sub_61B630";
    value["host_function_ea"] = "0x61B630";
    value["host_signature"] =
        "int __stdcall(int,int,int,int,int16,int,int,int,int,int,int)";
    value["logical_argument_count"] = 11;
    value["operation_selector_argument"] = 3;
    value["role"] = "market-data-query";
    value["observed_operation_ids"] = integer_array(kOperations);
    value["observed_operation_count"] =
        static_cast<std::uint64_t>(kOperations.size());
    value["runtime_worker_use"] = true;
    value["editor_ui_use"] = true;
    value["operations"] = Json::array();
    value["operations"].push_back(operation(4, "historical-record query"));
    value["operations"].push_back(operation(32, "TPool UI data record"));
    value["operations"].push_back(operation(102, "security-state gate"));
    value["operations"].push_back(operation(104, "103-byte quote/security state"));
    value["operations"].push_back(operation(105, "201-byte finance/quote expansion"));
    value["operations"].push_back(operation(111, "two-pass security list"));
    value["operations"].push_back(operation(122, "96-byte auxiliary quote record"));
    value["operations"].push_back(operation(125, "lookup handle"));
    value["operations"].push_back(operation(126, "lookup result"));
    value["operations"].push_back(operation(134, "selector-dependent list query"));
    return value;
}

Json action_callback() {
    constexpr std::array<int, 7> kOperations{9, 12, 15, 31, 32, 57, 88};
    Json value = Json::object();
    value["slot"] = 2;
    value["registration_argument"] = 2;
    value["tpool_global"] = "dword_10067498";
    value["host_function"] = "sub_62B8D0";
    value["host_function_ea"] = "0x62B8D0";
    value["host_signature"] =
        "int __stdcall(char*,uint16,char*,int,uint16,uint16,uint32)";
    value["logical_argument_count"] = 7;
    value["operation_selector_argument"] = 5;
    value["role"] = "host-ui-action-router";
    value["observed_operation_ids"] = integer_array(kOperations);
    value["observed_operation_count"] =
        static_cast<std::uint64_t>(kOperations.size());
    value["runtime_worker_use"] = true;
    value["editor_ui_use"] = true;
    value["operations"] = Json::array();
    value["operations"].push_back(operation(9, "seven-byte security batch action"));
    value["operations"].push_back(operation(12, "security view action"));
    value["operations"].push_back(operation(15, "host URL route"));
    value["operations"].push_back(operation(31, "security action route"));
    value["operations"].push_back(operation(32, "alternate security view action"));
    value["operations"].push_back(operation(57, "25-byte security-record batch route"));
    value["operations"].push_back(operation(88, "save configured block"));
    return value;
}

Json auxiliary_callback() {
    constexpr std::array<int, 2> kOperations{10, 31};
    Json value = Json::object();
    value["slot"] = 3;
    value["registration_argument"] = 3;
    value["tpool_global"] = "dword_1006749C";
    value["host_function"] = "sub_630950";
    value["host_function_ea"] = "0x630950";
    value["host_signature"] =
        "int __stdcall(int,int64,uint64,int)";
    value["logical_argument_count"] = 4;
    value["x86_stack_dword_count"] = 6;
    value["operation_selector_argument"] = 1;
    value["role"] = "auxiliary-operation-router";
    value["observed_operation_ids"] = integer_array(kOperations);
    value["observed_operation_count"] =
        static_cast<std::uint64_t>(kOperations.size());
    value["runtime_worker_use"] = true;
    value["editor_ui_use"] = true;
    value["operations"] = Json::array();
    auto selection = operation(10, "editor security-selection dialog");
    selection["output_record_size_bytes"] = 25;
    value["operations"].push_back(std::move(selection));
    auto history = operation(31, "runtime historical auxiliary calculation");
    history["source_size_bytes"] = 65;
    history["destination_capacity_bytes"] = 24;
    value["operations"].push_back(std::move(history));
    return value;
}

}  // namespace

Json tpool_host_callback_contracts_document() {
    Json result = Json::object();
    result["schema"] = "tdx-tpool-host-callback-contracts-v1";
    result["native_sources"] = "TPool.dll + TdxW.exe";
    result["registration_export"] = "TPool_RegisterCallBack";
    result["registration_function"] = "TdxW.exe sub_731570";
    result["callback_count"] = 3;
    result["execution_mode"] = "inspection-only";
    result["callbacks_invoked"] = false;
    result["callbacks"] = Json::array();
    result["callbacks"].push_back(data_callback());
    result["callbacks"].push_back(action_callback());
    result["callbacks"].push_back(auxiliary_callback());
    result["evidence"] =
        "TdxW sub_731570 registers sub_61B630/sub_62B8D0/sub_630950; TPool direct xrefs delimit the observed operation sets";
    return result;
}

}  // namespace tdx::tpool_detail
