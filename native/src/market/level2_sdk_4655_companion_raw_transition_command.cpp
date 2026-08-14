#include "level2_sdk_4655_companion_raw_transition_command.hpp"

#include "tdx/level2.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>
#include <system_error>
#include <utility>

namespace fs = std::filesystem;

namespace tdx::level2_detail {
namespace {

constexpr std::uintmax_t companion_snapshot_size = 46;
constexpr std::uintmax_t dispatcher_minimum_raw_size = 57;
constexpr std::uintmax_t maximum_snapshot_size = level2_offline_payload_limit;

fs::path command_path_from_utf8(const std::string& value) {
#ifdef _WIN32
    return fs::path(utf8_to_wide(value));
#else
    return fs::path(value);
#endif
}

std::uintmax_t checked_file_size(const fs::path& path,
                                 const char* option) {
    std::error_code error;
    const auto size = fs::file_size(path, error);
    if (error)
        throw Error(
            std::string("level2 SDK 4655 companion/raw transition cannot ") +
            "inspect " + option + ": " + error.message());
    return size;
}

Bytes read_companion(const fs::path& path) {
    const auto size = checked_file_size(path, "--companion-input");
    if (size != companion_snapshot_size)
        throw Error(
            "--companion-input must be an exact 46-byte binary file");
    auto result = read_bytes(path);
    if (result.size() != companion_snapshot_size)
        throw Error("--companion-input changed while it was being read");
    return result;
}

Bytes read_raw(const fs::path& path) {
    const auto size = checked_file_size(path, "--raw-input");
    if (size < dispatcher_minimum_raw_size)
        throw Error(
            "--raw-input must contain at least 57 dispatcher-qualified bytes");
    if (size > maximum_snapshot_size)
        throw Error("--raw-input exceeds the 384 KiB offline safety limit");
    auto result = read_bytes(path);
    if (result.size() < dispatcher_minimum_raw_size ||
        result.size() > maximum_snapshot_size)
        throw Error("--raw-input changed while it was being read");
    return result;
}

}  // namespace

int command_level2_sdk_4655_companion_raw_transition(Args& args) {
    const auto companion_name = args.take_option("--companion-input");
    const auto raw_name = args.take_option("--raw-input");
    if (companion_name.empty() || raw_name.empty())
        throw Error(
            "SDK 4655 companion/raw transition requires --companion-input "
            "FILE and --raw-input FILE");
    const auto output_name = args.take_option("--output");
    const bool compact = args.take_flag("--compact");
    args.require_empty();

    Level2Sdk4655CompanionRawTransitionRequest request;
    request.companion_snapshot = read_companion(
        command_path_from_utf8(companion_name));
    request.raw = read_raw(command_path_from_utf8(raw_name));
    const auto rendered =
        project_level2_sdk_4655_companion_raw_transition(request)
            .dump(compact ? -1 : 2) + "\n";
    if (output_name.empty()) std::cout << rendered;
    else atomic_write_text(command_path_from_utf8(output_name), rendered);
    return 0;
}

}  // namespace tdx::level2_detail
