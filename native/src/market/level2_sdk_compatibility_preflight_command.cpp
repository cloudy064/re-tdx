#include "tdx/level2.hpp"

#include "tdx/common.hpp"
#include "level2_tpbus_4650_dispatch_preflight.hpp"

#include <charconv>
#include <filesystem>
#include <iostream>
#include <string>
#include <system_error>

namespace fs = std::filesystem;

namespace tdx {
namespace {

fs::path preflight_path_from_utf8(const std::string& value) {
#ifdef _WIN32
    return fs::path(utf8_to_wide(value));
#else
    return fs::path(value);
#endif
}

bool raw_bool(const std::string& value, const char* option) {
    const auto normalized = lower_ascii(value);
    if (normalized == "true" || normalized == "1") return true;
    if (normalized == "false" || normalized == "0") return false;
    throw Error(std::string(option) + " must be true, false, 1, or 0");
}

int raw_integer(const std::string& value, const char* option) {
    int result{};
    const auto parsed = std::from_chars(value.data(),
                                        value.data() + value.size(), result);
    if (value.empty() || parsed.ec != std::errc{} ||
        parsed.ptr != value.data() + value.size())
        throw Error(std::string(option) + " must be a signed integer");
    return result;
}

void preflight_help() {
    std::cout <<
        "Usage: tdx-tool level2 preflight --format sdk-compatibility --root TDX\n"
        "       --auto-use-no-sdk-l2-agent-raw true|false\n"
        "       --sdk-l2-agent-raw true|false --host-edition-raw N\n"
        "       [--output FILE] [--compact]\n"
        "   or: tdx-tool level2 preflight --format tpbus-4650-dispatch-preflight\n"
        "       --input BODY --encoding raw|hex [--output FILE] [--compact]\n\n"
        "   or: tdx-tool level2 preflight --format tpbus-4650-price-primitives\n"
        "       --input BODY --encoding raw|hex\n"
        "       --target-market-or-mode-raw U32 [--output FILE] [--compact]\n\n"
        "Read only the selected SDKPlugins/bin/TdxDataSDK*.dll PE export table.\n"
        "The command does not read usercomm.ini, tokens, proxy settings, or endpoints;\n"
        "it never loads or calls the SDK and never performs a network request.\n";
}

}  // namespace

int command_level2_preflight(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        preflight_help();
        return 0;
    }

    const auto format = args.take_option("--format");
    if (format == "tpbus-4650-dispatch-preflight")
        return level2_detail::
            command_level2_tpbus_4650_dispatch_preflight(args);
    if (format == "tpbus-4650-price-primitives")
        return level2_detail::
            command_level2_tpbus_4650_price_primitives(args);
    if (format != "sdk-compatibility")
        throw Error(
            "level2 preflight format must be sdk-compatibility or "
            "tpbus-4650-dispatch-preflight or "
            "tpbus-4650-price-primitives");
    const auto root = args.take_option("--root");
    if (root.empty()) throw Error("level2 preflight requires --root");

    constexpr const char* auto_option =
        "--auto-use-no-sdk-l2-agent-raw";
    constexpr const char* agent_option = "--sdk-l2-agent-raw";
    constexpr const char* edition_option = "--host-edition-raw";
    if (!args.has(auto_option) || !args.has(agent_option) ||
        !args.has(edition_option))
        throw Error("level2 preflight requires both raw SDK-agent booleans and --host-edition-raw");

    Level2SdkCompatibilityPreflightRequest request;
    request.root = preflight_path_from_utf8(root);
    request.auto_use_no_sdk_l2_agent_raw =
        raw_bool(args.take_option(auto_option), auto_option);
    request.sdk_l2_agent_raw =
        raw_bool(args.take_option(agent_option), agent_option);
    request.host_edition_raw =
        raw_integer(args.take_option(edition_option), edition_option);
    const auto output = args.take_option("--output");
    const bool compact = args.take_flag("--compact");
    args.require_empty();

    const auto text = level2_sdk_compatibility_preflight_document(request)
                          .dump(compact ? -1 : 2) +
                      "\n";
    if (output.empty()) std::cout << text;
    else atomic_write_text(preflight_path_from_utf8(output), text);
    return 0;
}

}  // namespace tdx
