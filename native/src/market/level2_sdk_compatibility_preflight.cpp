#include "tdx/level2.hpp"

#include "pe_export_reader.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <array>
#include <filesystem>
#include <initializer_list>
#include <set>
#include <string>
#include <utility>

namespace fs = std::filesystem;

namespace tdx {
namespace {

struct ExportContract {
    const char* role;
    const char* symbol;
    bool required;
};

constexpr std::array<ExportContract, 9> sdk_exports{{
    {"SetTokenID", "fnSetTokenID", true},
    {"GetInfoMsg", "fnGetInfoMsg", true},
    {"Init", "fnInitTdxDataSDK", true},
    {"Exit", "fnExitTdxDataSDK", true},
    {"Subscribe", "fnSubscribeData", true},
    {"ReqData", "fnReqData", true},
    {"Register", "fnRegisterCallBackFunc", true},
    {"SetProxy", "fnSetProxy", false},
    {"GetCurrentConnectionStatus", "fnGetCurrentConnectionStatus", false},
}};

constexpr std::array<int, 3> supported_editions{{3, 17, 53}};

Json string_array(std::initializer_list<const char*> values) {
    Json result = Json::array();
    for (const auto* value : values) result.push_back(value);
    return result;
}

Json edition_array() {
    Json result = Json::array();
    for (const auto edition : supported_editions) result.push_back(edition);
    return result;
}

bool edition_supported(int edition) {
    return std::find(supported_editions.begin(), supported_editions.end(),
                     edition) != supported_editions.end();
}

}  // namespace

Json level2_sdk_compatibility_preflight_document(
    const Level2SdkCompatibilityPreflightRequest& request) {
    if (request.root.empty())
        throw Error("Level2 SDK compatibility preflight requires an explicit root");

    const bool use_pr = request.auto_use_no_sdk_l2_agent_raw ||
                        !request.sdk_l2_agent_raw;
    const std::string selected_name =
        use_pr ? "TdxDataSDKPR.dll" : "TdxDataSDK.dll";
    const auto relative = fs::path("SDKPlugins") / "bin" / selected_name;
    const auto selected_path = request.root / relative;
    const bool supported = edition_supported(request.host_edition_raw);

    Json result = Json::object();
    result["schema"] = "tdx-level2-sdk-compatibility-preflight-v1";
    result["format"] = "sdk-compatibility";
    result["root"] = path_utf8(request.root);
    result["read_only"] = true;
    result["offline"] = true;

    Json config = Json::object();
    config["auto_use_no_sdk_l2_agent_raw"] =
        request.auto_use_no_sdk_l2_agent_raw;
    config["sdk_l2_agent_raw"] = request.sdk_l2_agent_raw;
    config["config_files_read"] = false;
    result["config"] = std::move(config);

    Json edition = Json::object();
    edition["host_edition_raw"] = request.host_edition_raw;
    edition["supported"] = supported;
    edition["supported_values"] = edition_array();
    result["edition"] = std::move(edition);

    Json candidates = Json::array();
    for (const auto* name : {"TdxDataSDKPR.dll", "TdxDataSDK.dll"}) {
        Json candidate = Json::object();
        candidate["name"] = name;
        candidate["relative_path"] =
            path_utf8(fs::path("SDKPlugins") / "bin" / name);
        candidate["selected"] = selected_name == name;
        candidates.push_back(std::move(candidate));
    }
    result["candidates"] = std::move(candidates);

    Json selection = Json::object();
    selection["rule"] =
        "auto_use_no_sdk_l2_agent_raw || !sdk_l2_agent_raw";
    selection["selected_name"] = selected_name;
    selection["selected_relative_path"] = path_utf8(relative);
    selection["selected_path"] = path_utf8(selected_path);
    result["selection"] = std::move(selection);

    Json sdk = Json::object();
    sdk["name"] = selected_name;
    sdk["path"] = path_utf8(selected_path);
    sdk["present"] = false;
    sdk["exports_parsed"] = false;
    sdk["required_export_count"] = 7;
    sdk["optional_export_count"] = 2;

    std::set<std::string> exported;
    std::string sdk_status = "absent";
    std::string parse_error;
    std::error_code file_error;
    const bool path_exists = fs::exists(selected_path, file_error);
    if (file_error) {
        sdk_status = "unreadable";
        parse_error = "cannot inspect selected SDK path";
    } else if (path_exists) {
        const bool regular = fs::is_regular_file(selected_path, file_error);
        if (file_error || !regular) {
            sdk_status = "invalid-file";
            parse_error = file_error
                ? "cannot inspect selected SDK file type"
                : "selected SDK path is not a regular file";
        } else {
            sdk["present"] = true;
            try {
                const auto names =
                    pe_detail::read_pe_export_names(selected_path);
                exported.insert(names.begin(), names.end());
                sdk["exports_parsed"] = true;
                sdk_status = "parsed";
            } catch (const std::exception& error) {
                sdk_status = "invalid-pe";
                parse_error = error.what();
            }
        }
    }

    Json required = Json::array();
    Json optional = Json::array();
    Json missing_required = Json::array();
    Json present_optional = Json::array();
    bool required_complete = sdk_status == "parsed";
    for (const auto& contract : sdk_exports) {
        const bool present = exported.find(contract.symbol) != exported.end();
        Json item = Json::object();
        item["role"] = contract.role;
        item["symbol"] = contract.symbol;
        item["present"] = present;
        if (contract.required) {
            required.push_back(std::move(item));
            if (!present) {
                required_complete = false;
                missing_required.push_back(contract.role);
            }
        } else {
            optional.push_back(std::move(item));
            if (present) present_optional.push_back(contract.role);
        }
    }
    if (sdk_status == "parsed")
        sdk_status = required_complete ? "compatible" :
                                         "missing-required-exports";
    sdk["status"] = sdk_status;
    if (!parse_error.empty()) sdk["parse_error"] = parse_error;
    sdk["required_exports"] = std::move(required);
    sdk["optional_exports"] = std::move(optional);
    sdk["missing_required_exports"] = std::move(missing_required);
    sdk["present_optional_exports"] = std::move(present_optional);
    sdk["required_exports_complete"] = required_complete;
    result["sdk"] = std::move(sdk);

    const bool compatible = supported && required_complete;
    result["compatible"] = compatible;
    if (!path_exists && !file_error) {
        result["status"] = "absent";
    } else if (sdk_status == "unreadable" || sdk_status == "invalid-file" ||
               sdk_status == "invalid-pe") {
        result["status"] = sdk_status;
    } else if (!required_complete) {
        result["status"] = "missing-required-exports";
    } else if (!supported) {
        result["status"] = "unsupported-host-edition";
    } else {
        result["status"] = "compatible";
    }

    // Only recovered call names are emitted. No token value, callback address,
    // proxy address, or live endpoint is materialized by this plan.
    result["bootstrap_steps"] =
        string_array({"SetTokenID", "Init", "Register"});
    result["authorization_ready"] = "unknown";
    result["live_data_ready"] = false;
    result["sdk_loaded"] = false;
    result["sdk_calls"] = 0;
    result["tokens_read"] = false;
    result["proxy_settings_read"] = false;
    result["network_requests"] = 0;
    result["entitlement_bypass"] = false;
    result["evidence"] =
        "TdxW sub_68A540 SDK selection/export resolution and sub_68D450 edition gate; output/ida-tdxw-sdk-function-table-xrefs.json; doc/99-log/2026-08-02-level2-static-protocol.md:323-345";
    return result;
}

}  // namespace tdx
