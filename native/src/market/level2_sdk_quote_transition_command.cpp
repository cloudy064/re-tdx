#include "tdx/level2.hpp"

#include "tdx/common.hpp"
#include "level2_sdk_180x_host_projection_command.hpp"
#include "level2_sdk_1803_record_projection_command.hpp"
#include "level2_sdk_1804_host_projection_command.hpp"
#include "level2_sdk_4655_companion_raw_transition_command.hpp"
#include "level2_sdk_correlation_transition_command.hpp"
#include "level2_sdk_dual_snapshot_transition_command.hpp"
#include "level2_sdk_fixed_snapshot_projection_command.hpp"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <filesystem>
#include <iostream>
#include <string>
#include <system_error>
#include <utility>

namespace fs = std::filesystem;

namespace tdx {
namespace {

constexpr std::uintmax_t quote_body_size = 380;
constexpr std::uintmax_t maximum_hex_file_size = 16U * 1024U;
constexpr std::uintmax_t maximum_json_file_size = level2_offline_payload_limit;

fs::path project_path_from_utf8(const std::string& value) {
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
        throw Error(std::string("level2 project cannot inspect ") +
                    option + ": " + error.message());
    return size;
}

int signed_integer(const std::string& value, const char* option) {
    int result{};
    const auto parsed = std::from_chars(
        value.data(), value.data() + value.size(), result);
    if (value.empty() || parsed.ec != std::errc{} ||
        parsed.ptr != value.data() + value.size())
        throw Error(std::string(option) + " must be a signed integer");
    return result;
}

int hex_digit(unsigned char value) {
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    return -1;
}

Bytes parse_project_hex(std::string text) {
    text.erase(std::remove_if(text.begin(), text.end(),
                              [](unsigned char value) {
                                  return std::isspace(value) != 0;
                              }),
               text.end());
    if (text.empty()) throw Error("level2 project hex body is empty");
    if (text.size() % 2 != 0)
        throw Error("level2 project hex body must contain an even number of digits");
    Bytes result;
    result.reserve(text.size() / 2);
    for (std::size_t index = 0; index < text.size(); index += 2) {
        const int high = hex_digit(static_cast<unsigned char>(text[index]));
        const int low = hex_digit(static_cast<unsigned char>(text[index + 1]));
        if (high < 0 || low < 0)
            throw Error("level2 project hex body contains a non-hexadecimal character");
        result.push_back(static_cast<std::uint8_t>((high << 4) | low));
    }
    return result;
}

Json read_project_object(const fs::path& path, const char* option) {
    const auto size = checked_file_size(path, option);
    if (size > maximum_json_file_size)
        throw Error(std::string(option) + " exceeds the 384 KiB JSON limit");
    auto value = Json::parse(read_text_utf8(path));
    if (!value.is_object())
        throw Error(std::string(option) + " must contain a JSON object");
    return value;
}

void project_help() {
    std::cout <<
        "Usage: tdx-tool level2 project --format sdk-quote-transition\n"
        "       --input BODY --encoding raw|hex --state-file JSON\n"
        "       --context-file JSON --data-type 1807|18071\n"
        "       [--output FILE] [--compact]\n"
        "   or: tdx-tool level2 project --format sdk-correlation-transition\n"
        "       --input JSON_FILE [--output FILE] [--compact]\n\n"
        "   or: tdx-tool level2 project --format sdk-1804-host-projection\n"
        "       --input BODY --encoding raw|hex [--output FILE] [--compact]\n\n"
        "   or: tdx-tool level2 project --format sdk-1803-host-projection\n"
        "       --input BODY --encoding raw|hex [--output FILE] [--compact]\n"
        "   or: tdx-tool level2 project --format sdk-18031-host-projection\n"
        "       --input BODY --encoding raw|hex [--output FILE] [--compact]\n\n"
        "   or: tdx-tool level2 project --format sdk-1803-depth-record-projection\n"
        "       --input BODY --encoding raw|hex [--output FILE] [--compact]\n"
        "   or: tdx-tool level2 project --format sdk-18031-queue-record-projection\n"
        "       --input BODY --encoding raw|hex --market 0..2 --code 6DIGITS\n"
        "       [--output FILE] [--compact]\n\n"
        "   or: tdx-tool level2 project --format sdk-1801-host-projection\n"
        "       --input BODY --encoding raw|hex --market 0..2 --code 6DIGITS\n"
        "       [--output FILE] [--compact]\n"
        "   or: tdx-tool level2 project --format sdk-1802-host-projection\n"
        "       --input BODY --encoding raw|hex --market 0..2 --code 6DIGITS\n"
        "       [--output FILE] [--compact]\n\n"
        "   or: tdx-tool level2 project --format sdk-4654-dual-snapshot-transition\n"
        "       --decoded-input FILE --raw-input FILE [--output FILE] [--compact]\n"
        "   or: tdx-tool level2 project --format sdk-4651-dual-snapshot-transition\n"
        "       --decoded-input FILE --raw-input FILE [--state-file JSON]\n"
        "       [--output FILE] [--compact]\n\n"
        "   or: tdx-tool level2 project --format sdk-4655-companion-raw-transition\n"
        "       --companion-input FILE --raw-input FILE\n"
        "       [--output FILE] [--compact]\n\n"
        "Project one exact 380-byte authorized callback snapshot onto an\n"
        "explicit previous host quote state. The command does not call the SDK,\n"
        "dispatch a callback or host message, access a network, or bypass authorization.\n"
        "The correlation format replays only the proven local 25-byte registry\n"
        "match/consume/retain or SDK-key upsert transition from strict UTF-8 JSON.\n";
}

}  // namespace

int command_level2_project(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        project_help();
        return 0;
    }

    const auto format = lower_ascii(trim(args.take_option("--format")));
    if (format == "sdk-1801-host-projection")
        return level2_detail::command_level2_sdk_180x_host_projection(
            args, 1801);
    if (format == "sdk-1802-host-projection")
        return level2_detail::command_level2_sdk_180x_host_projection(
            args, 1802);
    if (format == "sdk-1804-host-projection")
        return level2_detail::command_level2_sdk_1804_host_projection(args);
    if (format == "sdk-1803-host-projection")
        return level2_detail::command_level2_sdk_fixed_snapshot_projection(
            args, 1803);
    if (format == "sdk-18031-host-projection")
        return level2_detail::command_level2_sdk_fixed_snapshot_projection(
            args, 18031);
    if (format == "sdk-1803-depth-record-projection")
        return level2_detail::command_level2_sdk_1803_record_projection(
            args, 1803);
    if (format == "sdk-18031-queue-record-projection")
        return level2_detail::command_level2_sdk_1803_record_projection(
            args, 18031);
    if (format == "sdk-correlation-transition")
        return level2_detail::command_level2_sdk_correlation_transition(args);
    if (format == "sdk-4654-dual-snapshot-transition")
        return level2_detail::command_level2_sdk_dual_snapshot_transition(
            args, 4654);
    if (format == "sdk-4651-dual-snapshot-transition")
        return level2_detail::command_level2_sdk_dual_snapshot_transition(
            args, 4651);
    if (format == "sdk-4655-companion-raw-transition")
        return level2_detail::
            command_level2_sdk_4655_companion_raw_transition(args);
    if (format != "sdk-quote-transition")
        throw Error(
            "level2 project format must be sdk-quote-transition or "
            "sdk-correlation-transition or sdk-1801-host-projection or "
            "sdk-1802-host-projection or sdk-1803-host-projection or "
            "sdk-18031-host-projection or "
            "sdk-1803-depth-record-projection or "
            "sdk-18031-queue-record-projection or "
            "sdk-1804-host-projection or "
            "sdk-4654-dual-snapshot-transition or "
            "sdk-4651-dual-snapshot-transition or "
            "sdk-4655-companion-raw-transition");
    const auto input_name = args.take_option("--input");
    const auto state_name = args.take_option("--state-file");
    const auto context_name = args.take_option("--context-file");
    if (input_name.empty() || state_name.empty() || context_name.empty())
        throw Error("level2 project requires --input, --state-file, and --context-file");
    if (!args.has("--encoding"))
        throw Error("level2 project requires --encoding raw or hex");
    if (!args.has("--data-type"))
        throw Error("level2 project requires --data-type 1807 or 18071");

    const auto encoding = lower_ascii(trim(
        args.take_option("--encoding", "raw")));
    if (encoding != "raw" && encoding != "hex")
        throw Error("level2 project encoding must be raw or hex");
    const auto data_type = signed_integer(
        args.take_option("--data-type"), "--data-type");
    if (data_type != 1807 && data_type != 18071)
        throw Error("level2 project data-type must be 1807 or 18071");
    const auto output_name = args.take_option("--output");
    const bool compact = args.take_flag("--compact");
    args.require_empty();

    const auto input_path = project_path_from_utf8(input_name);
    const auto input_file_size = checked_file_size(input_path, "--input");
    Bytes body;
    if (encoding == "raw") {
        if (input_file_size != quote_body_size)
            throw Error("level2 project raw body must be exactly 380 bytes");
        body = read_bytes(input_path);
    } else {
        if (input_file_size > maximum_hex_file_size)
            throw Error("level2 project hex input file exceeds the 16 KiB text limit");
        body = parse_project_hex(read_text_utf8(input_path));
        if (body.size() != quote_body_size)
            throw Error("level2 project hex body must decode to exactly 380 bytes");
    }

    Level2SdkQuoteTransitionRequest request;
    request.data_type = data_type == 1807
        ? Level2SdkCallbackDataType::quote_update
        : Level2SdkCallbackDataType::extended_quote;
    request.body = std::move(body);
    request.previous = read_project_object(
        project_path_from_utf8(state_name), "--state-file");
    request.context = read_project_object(
        project_path_from_utf8(context_name), "--context-file");

    const auto rendered = project_level2_sdk_quote_transition(request)
                              .dump(compact ? -1 : 2) + "\n";
    if (output_name.empty()) std::cout << rendered;
    else atomic_write_text(project_path_from_utf8(output_name), rendered);
    return 0;
}

}  // namespace tdx
