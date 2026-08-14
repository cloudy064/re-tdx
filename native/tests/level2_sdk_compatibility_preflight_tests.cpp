#include "tdx/level2.hpp"

#include "tdx/common.hpp"
#include "tdx/registry.hpp"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <iostream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace fs = std::filesystem;

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) throw tdx::Error(message);
}

void put_u16(tdx::Bytes& bytes, std::size_t offset, std::uint16_t value) {
    require(offset <= bytes.size() && 2 <= bytes.size() - offset,
            "fixture uint16 range");
    bytes[offset] = static_cast<std::uint8_t>(value);
    bytes[offset + 1] = static_cast<std::uint8_t>(value >> 8U);
}

void put_u32(tdx::Bytes& bytes, std::size_t offset, std::uint32_t value) {
    require(offset <= bytes.size() && 4 <= bytes.size() - offset,
            "fixture uint32 range");
    for (std::size_t index = 0; index < 4; ++index)
        bytes[offset + index] =
            static_cast<std::uint8_t>(value >> (8U * index));
}

void put_text(tdx::Bytes& bytes, std::size_t offset,
              std::string_view value) {
    require(offset <= bytes.size() && value.size() < bytes.size() - offset,
            "fixture text range");
    for (std::size_t index = 0; index < value.size(); ++index)
        bytes[offset + index] = static_cast<std::uint8_t>(value[index]);
    bytes[offset + value.size()] = 0;
}

tdx::Bytes pe_with_exports(const std::vector<std::string>& exports) {
    require(exports.size() <= 32, "fixture export count");
    tdx::Bytes bytes(0x600, 0);
    bytes[0] = 'M';
    bytes[1] = 'Z';
    put_u32(bytes, 0x3C, 0x80);
    put_text(bytes, 0x80, std::string_view("PE\0\0", 4));

    constexpr std::size_t coff = 0x84;
    put_u16(bytes, coff, 0x14C);
    put_u16(bytes, coff + 2, 1);
    put_u16(bytes, coff + 16, 0xE0);

    constexpr std::size_t optional = 0x98;
    put_u16(bytes, optional, 0x10B);
    put_u32(bytes, optional + 60, 0x200);
    put_u32(bytes, optional + 92, 16);
    put_u32(bytes, optional + 96, 0x1000);
    put_u32(bytes, optional + 100, 0x400);

    constexpr std::size_t section = 0x178;
    put_text(bytes, section, ".edata");
    put_u32(bytes, section + 8, 0x400);
    put_u32(bytes, section + 12, 0x1000);
    put_u32(bytes, section + 16, 0x400);
    put_u32(bytes, section + 20, 0x200);

    constexpr std::size_t directory = 0x200;
    const auto count = static_cast<std::uint32_t>(exports.size());
    put_u32(bytes, directory + 20, count);
    put_u32(bytes, directory + 24, count);
    put_u32(bytes, directory + 28, 0x1040);
    put_u32(bytes, directory + 32, 0x1080);
    put_u32(bytes, directory + 36, 0x10C0);

    std::uint32_t string_rva = 0x1100;
    for (std::size_t index = 0; index < exports.size(); ++index) {
        put_u32(bytes, 0x240 + index * 4,
                0x1200 + static_cast<std::uint32_t>(index));
        put_u32(bytes, 0x280 + index * 4, string_rva);
        put_u16(bytes, 0x2C0 + index * 2,
                static_cast<std::uint16_t>(index));
        const auto string_offset =
            0x300 + static_cast<std::size_t>(string_rva - 0x1100);
        put_text(bytes, string_offset, exports[index]);
        string_rva += static_cast<std::uint32_t>(exports[index].size() + 1);
    }
    require(string_rva <= 0x13F0, "fixture export strings fit section");
    return bytes;
}

const std::vector<std::string> required_exports{
    "fnSetTokenID", "fnGetInfoMsg", "fnInitTdxDataSDK",
    "fnExitTdxDataSDK", "fnSubscribeData", "fnReqData",
    "fnRegisterCallBackFunc"};

struct TemporaryRoot {
    fs::path path = fs::temp_directory_path() /
        ("tdx-level2-sdk-preflight-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));

    TemporaryRoot() { fs::create_directories(path / "SDKPlugins" / "bin"); }
    ~TemporaryRoot() {
        std::error_code ignored;
        fs::remove_all(path, ignored);
    }

    fs::path sdk(const std::string& name) const {
        return path / "SDKPlugins" / "bin" / name;
    }
};

tdx::Level2SdkCompatibilityPreflightRequest request_for(
    const TemporaryRoot& root, bool auto_raw = false,
    bool agent_raw = true, int edition_raw = 17) {
    tdx::Level2SdkCompatibilityPreflightRequest request;
    request.root = root.path;
    request.auto_use_no_sdk_l2_agent_raw = auto_raw;
    request.sdk_l2_agent_raw = agent_raw;
    request.host_edition_raw = edition_raw;
    return request;
}

bool role_present(const tdx::Json& exports, std::string_view role) {
    for (const auto& item : exports.as_array())
        if (item.at("role").as_string() == role)
            return item.at("present").as_bool();
    throw tdx::Error("export role is absent from report");
}

void require_throws(const std::function<void()>& action,
                    const std::string& message) {
    bool threw = false;
    try {
        action();
    } catch (const tdx::Error&) {
        threw = true;
    }
    require(threw, message);
}

}  // namespace

int main() {
    try {
        TemporaryRoot fixture;

        const auto absent = tdx::level2_sdk_compatibility_preflight_document(
            request_for(fixture, false, false, 3));
        require(absent.at("schema").as_string() ==
                    "tdx-level2-sdk-compatibility-preflight-v1" &&
                    absent.at("status").as_string() == "absent" &&
                    absent.at("sdk").at("status").as_string() == "absent" &&
                    absent.at("selection").at("selected_name").as_string() ==
                        "TdxDataSDKPR.dll" &&
                    !absent.at("sdk_loaded").as_bool() &&
                    absent.at("sdk_calls").as_number() == 0 &&
                    !absent.at("tokens_read").as_bool() &&
                    !absent.at("proxy_settings_read").as_bool() &&
                    absent.at("authorization_ready").as_string() ==
                        "unknown" &&
                    !absent.at("live_data_ready").as_bool() &&
                    absent.at("network_requests").as_number() == 0,
                "missing selected DLL is a structured inert absent result");

        auto complete_exports = required_exports;
        complete_exports.push_back("fnSetProxy");
        tdx::atomic_write_bytes(fixture.sdk("TdxDataSDK.dll"),
                                pe_with_exports(complete_exports));
        const auto compatible =
            tdx::level2_sdk_compatibility_preflight_document(
                request_for(fixture));
        require(compatible.at("status").as_string() == "compatible" &&
                    compatible.at("compatible").as_bool() &&
                    compatible.at("sdk").at("required_exports_complete")
                        .as_bool() &&
                    role_present(compatible.at("sdk").at("optional_exports"),
                                 "SetProxy") &&
                    !role_present(
                        compatible.at("sdk").at("optional_exports"),
                        "GetCurrentConnectionStatus") &&
                    compatible.at("bootstrap_steps").size() == 3,
                "complete required exports and supported edition are compatible");

        const auto unsupported =
            tdx::level2_sdk_compatibility_preflight_document(
                request_for(fixture, false, true, 99));
        require(unsupported.at("status").as_string() ==
                    "unsupported-host-edition" &&
                    !unsupported.at("compatible").as_bool() &&
                    unsupported.at("sdk").at("status").as_string() ==
                        "compatible",
                "unsupported raw host edition remains distinct from SDK exports");

        auto incomplete_exports = required_exports;
        incomplete_exports.pop_back();
        tdx::atomic_write_bytes(fixture.sdk("TdxDataSDK.dll"),
                                pe_with_exports(incomplete_exports));
        const auto incomplete =
            tdx::level2_sdk_compatibility_preflight_document(
                request_for(fixture));
        require(incomplete.at("status").as_string() ==
                    "missing-required-exports" &&
                    !incomplete.at("compatible").as_bool() &&
                    !role_present(
                        incomplete.at("sdk").at("required_exports"),
                        "Register"),
                "one missing required export blocks compatibility");

        tdx::atomic_write_bytes(fixture.sdk("TdxDataSDK.dll"),
                                {'M', 'Z', 0, 0});
        const auto truncated =
            tdx::level2_sdk_compatibility_preflight_document(
                request_for(fixture));
        require(truncated.at("status").as_string() == "invalid-pe" &&
                    truncated.at("sdk").at("parse_error").is_string(),
                "truncated PE is rejected structurally");

        auto malicious = pe_with_exports(required_exports);
        put_u32(malicious, 0x200 + 24, 0xFFFFFFFFU);
        tdx::atomic_write_bytes(fixture.sdk("TdxDataSDK.dll"), malicious);
        const auto hostile_count =
            tdx::level2_sdk_compatibility_preflight_document(
                request_for(fixture));
        require(hostile_count.at("status").as_string() == "invalid-pe" &&
                    !hostile_count.at("sdk_loaded").as_bool() &&
                    hostile_count.at("sdk_calls").as_number() == 0,
                "malicious export count is bounded without loading the image");

        auto hostile_rva = pe_with_exports(required_exports);
        put_u32(hostile_rva, 0x280, 0xFFFFFFFCU);
        tdx::atomic_write_bytes(fixture.sdk("TdxDataSDK.dll"), hostile_rva);
        const auto unmapped =
            tdx::level2_sdk_compatibility_preflight_document(
                request_for(fixture));
        require(unmapped.at("status").as_string() == "invalid-pe",
                "unmapped export-name RVA is rejected structurally");

        const auto cli_output = fixture.path / "preflight.json";
        const auto cli_status = tdx::command_level2_preflight({
            "--format", "sdk-compatibility", "--root",
            tdx::path_utf8(fixture.path),
            "--auto-use-no-sdk-l2-agent-raw", "true",
            "--sdk-l2-agent-raw", "true", "--host-edition-raw", "53",
            "--output", tdx::path_utf8(cli_output), "--compact"});
        const auto cli_document =
            tdx::Json::parse(tdx::read_text_utf8(cli_output));
        require(cli_status == 0 &&
                    cli_document.at("format").as_string() ==
                        "sdk-compatibility" &&
                    cli_document.at("selection").at("selected_name")
                            .as_string() == "TdxDataSDKPR.dll" &&
                    tdx::find_command("level2 preflight") != nullptr,
                "dedicated CLI validates and emits the compatibility schema");
        require_throws(
            [&] {
                (void)tdx::command_level2_preflight({
                    "--format", "other", "--root",
                    tdx::path_utf8(fixture.path)});
            },
            "CLI rejects non-sdk-compatibility formats");
        require_throws(
            [&] {
                (void)tdx::command_level2_preflight({
                    "--format", "sdk-compatibility", "--root",
                    tdx::path_utf8(fixture.path),
                    "--auto-use-no-sdk-l2-agent-raw", "false",
                    "--sdk-l2-agent-raw", "true"});
            },
            "CLI requires the explicit raw host edition");

        std::cout << "Level2 SDK compatibility preflight tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Level2 SDK compatibility preflight tests failed: "
                  << error.what() << '\n';
        return 1;
    }
}
