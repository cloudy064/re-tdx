#include "tdx/pbrpc.hpp"

#include "tdx/cloud_resilience.hpp"
#include "tdx/http.hpp"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <filesystem>
#include <iostream>
#include <set>
#include <sstream>
#include <thread>
#include <tuple>
#include <variant>

namespace fs = std::filesystem;

namespace tdx {
namespace {

struct WireField {
    std::uint32_t number{};
    std::uint32_t wire_type{};
    std::variant<std::uint64_t, Bytes> value;
};

std::uint64_t decode_varint(const Bytes& data, std::size_t& offset) {
    std::uint64_t value = 0;
    for (unsigned shift = 0; shift < 70; shift += 7) {
        if (offset >= data.size()) throw Error("truncated protobuf varint");
        const auto byte = data[offset++];
        if (shift == 63 && (byte & 0xFEu)) throw Error("protobuf varint overflow");
        value |= static_cast<std::uint64_t>(byte & 0x7Fu) << shift;
        if (!(byte & 0x80u)) return value;
    }
    throw Error("invalid protobuf varint");
}

std::vector<WireField> decode_fields(const Bytes& data) {
    std::vector<WireField> result;
    std::size_t offset = 0;
    while (offset < data.size()) {
        const auto tag = decode_varint(data, offset);
        const auto number = static_cast<std::uint32_t>(tag >> 3);
        const auto wire_type = static_cast<std::uint32_t>(tag & 7u);
        if (!number) throw Error("invalid protobuf field number 0");
        if (wire_type == 0) {
            result.push_back(WireField{number, wire_type, decode_varint(data, offset)});
        } else if (wire_type == 2) {
            const auto length = decode_varint(data, offset);
            if (length > data.size() - offset)
                throw Error("truncated protobuf length-delimited field");
            const auto end = offset + static_cast<std::size_t>(length);
            result.push_back(WireField{number, wire_type,
                Bytes(data.begin() + static_cast<std::ptrdiff_t>(offset),
                      data.begin() + static_cast<std::ptrdiff_t>(end))});
            offset = end;
        } else {
            throw Error("unsupported protobuf wire type " + std::to_string(wire_type));
        }
    }
    return result;
}

const WireField* last_field(const std::vector<WireField>& fields, std::uint32_t number) {
    const auto found = std::find_if(fields.rbegin(), fields.rend(),
        [&](const auto& field) { return field.number == number; });
    return found == fields.rend() ? nullptr : &*found;
}

std::uint64_t integer_field(const std::vector<WireField>& fields, std::uint32_t number) {
    const auto* field = last_field(fields, number);
    if (!field) return 0;
    if (!std::holds_alternative<std::uint64_t>(field->value))
        throw Error("protobuf field " + std::to_string(number) + " must be a varint");
    return std::get<std::uint64_t>(field->value);
}

Bytes bytes_field(const std::vector<WireField>& fields, std::uint32_t number) {
    const auto* field = last_field(fields, number);
    if (!field) return {};
    if (!std::holds_alternative<Bytes>(field->value))
        throw Error("protobuf field " + std::to_string(number) + " must contain bytes");
    return std::get<Bytes>(field->value);
}

std::int32_t int32_value(std::uint64_t value) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(value & 0xFFFFFFFFu));
}

std::string bytes_text(const Bytes& value) {
    return std::string(reinterpret_cast<const char*>(value.data()), value.size());
}

void append_bytes(Bytes& output, const Bytes& value) {
    output.insert(output.end(), value.begin(), value.end());
}

void replace_all(std::string& text, const std::string& needle,
                 const std::string& replacement) {
    if (needle.empty()) return;
    std::size_t offset = 0;
    while ((offset = text.find(needle, offset)) != std::string::npos) {
        text.replace(offset, needle.size(), replacement);
        offset += replacement.size();
    }
}

std::vector<std::string> unresolved_placeholders(std::string_view body) {
    std::vector<std::string> result;
    std::size_t offset = 0;
    while (offset + 4 <= body.size()) {
        const auto start = body.find("$$", offset);
        if (start == std::string_view::npos) break;
        const auto end = body.find("$$", start + 2);
        if (end == std::string_view::npos) break;
        if (end > start + 2) {
            const auto name = std::string(body.substr(start + 2, end - start - 2));
            if (std::find(result.begin(), result.end(), name) == result.end())
                result.push_back(name);
        }
        offset = end + 2;
    }
    return result;
}

std::pair<std::string, std::string> split_descriptor(const std::string& body) {
    constexpr std::string_view prefix = "pb_rpc_req:";
    if (body.size() < prefix.size() ||
        lower_ascii(body.substr(0, prefix.size())) != prefix)
        throw Error("datasource body is not a pb_rpc_req descriptor");
    const auto lowered = lower_ascii(body);
    const auto marker = lowered.find(";reqbyte=", prefix.size());
    if (marker == std::string::npos)
        throw Error("pb_rpc_req descriptor has no ReqByte");
    const auto head = body.substr(prefix.size(), marker - prefix.size());
    std::string module;
    std::size_t offset = 0;
    while (offset <= head.size()) {
        const auto end = head.find(';', offset);
        const auto assignment = head.substr(offset,
            end == std::string::npos ? std::string::npos : end - offset);
        const auto equal = assignment.find('=');
        if (equal != std::string::npos &&
            lower_ascii(trim(assignment.substr(0, equal))) == "moduledll") {
            module = trim(assignment.substr(equal + 1));
            break;
        }
        if (end == std::string::npos) break;
        offset = end + 1;
    }
    if (module.empty()) throw Error("pb_rpc_req descriptor has no Moduledll");
    return {module, trim(body.substr(marker + 9))};
}

std::map<std::string, std::string> assignments(const std::vector<std::string>& values,
                                               std::string_view option) {
    std::map<std::string, std::string> result;
    for (const auto& value : values) {
        const auto separator = value.find('=');
        if (separator == std::string::npos || !separator)
            throw Error(std::string(option) + " expects NAME=VALUE");
        result[value.substr(0, separator)] = value.substr(separator + 1);
    }
    return result;
}

int integer_option(const std::string& text, std::string_view name,
                   int minimum, int maximum) {
    try {
        std::size_t used = 0;
        const int value = std::stoi(text, &used);
        if (used != text.size() || value < minimum || value > maximum)
            throw std::invalid_argument("range");
        return value;
    } catch (...) {
        throw Error(std::string(name) + " must be in " + std::to_string(minimum) +
                    ".." + std::to_string(maximum));
    }
}

int json_int(const Json& value, std::string_view label) {
    try {
        if (value.is_number()) return static_cast<int>(value.as_number());
        if (value.is_string()) {
            std::size_t used = 0;
            const auto result = std::stoi(value.as_string(), &used);
            if (used == value.as_string().size()) return result;
        }
    } catch (...) {}
    throw Error(std::string(label) + " is not an integer");
}

void validate_business_result(const Json& value) {
    if (!value.is_object()) return;
    const auto& object = value.as_object();
    const auto found = object.find("ErrorCode");
    if (found == object.end()) return;
    const int code = json_int(found->second, "PBRPC ErrorCode");
    if (!code) return;
    const auto info = object.find("ErrorInfo");
    throw Error("PBRPC business ErrorCode " + std::to_string(code) +
                (info == object.end() ? "" : ": " + info->second.dump(-1)));
}

std::string hex_bytes(const Bytes& data) {
    static constexpr char digits[] = "0123456789abcdef";
    std::string result;
    result.resize(data.size() * 2);
    for (std::size_t index = 0; index < data.size(); ++index) {
        result[index * 2] = digits[data[index] >> 4];
        result[index * 2 + 1] = digits[data[index] & 0x0F];
    }
    return result;
}

}  // namespace

Bytes encode_pbrpc_varint(std::uint64_t value) {
    Bytes result;
    while (value >= 0x80) {
        result.push_back(static_cast<std::uint8_t>((value & 0x7F) | 0x80));
        value >>= 7;
    }
    result.push_back(static_cast<std::uint8_t>(value));
    return result;
}

Bytes encode_pbrpc_varint_field(std::uint32_t field, std::uint64_t value) {
    if (!field) throw Error("protobuf field number must be positive");
    auto result = encode_pbrpc_varint(static_cast<std::uint64_t>(field) << 3);
    append_bytes(result, encode_pbrpc_varint(value));
    return result;
}

Bytes encode_pbrpc_bytes_field(std::uint32_t field, const Bytes& value) {
    if (!field) throw Error("protobuf field number must be positive");
    auto result = encode_pbrpc_varint((static_cast<std::uint64_t>(field) << 3) | 2);
    append_bytes(result, encode_pbrpc_varint(value.size()));
    append_bytes(result, value);
    return result;
}

Bytes encode_pbrpc_bytes_field(std::uint32_t field, const std::string& value) {
    return encode_pbrpc_bytes_field(field, Bytes(value.begin(), value.end()));
}

Bytes build_pbrpc_request(const std::string& module, const std::string& request_json,
                          std::int32_t rpc_id, std::int32_t start_pos,
                          const std::string& charset, std::int32_t target,
                          const std::string& sso_token) {
    if (module.empty()) throw Error("PBRPC module is required");
    if (rpc_id < 0 || start_pos < 0 || target < 0)
        throw Error("PBRPC request integers must be non-negative");
    auto head = encode_pbrpc_bytes_field(1, charset);
    if (!sso_token.empty()) append_bytes(head, encode_pbrpc_bytes_field(4, sso_token));
    if (target) append_bytes(head, encode_pbrpc_varint_field(5, target));

    auto result = encode_pbrpc_bytes_field(1, head);
    if (rpc_id) append_bytes(result, encode_pbrpc_varint_field(2, rpc_id));
    if (start_pos) append_bytes(result, encode_pbrpc_varint_field(3, start_pos));
    append_bytes(result, encode_pbrpc_bytes_field(4, module));
    append_bytes(result, encode_pbrpc_bytes_field(5, request_json));
    return result;
}

PbrpcResponse parse_pbrpc_response(const Bytes& data) {
    const auto fields = decode_fields(data);
    const auto head_bytes = bytes_field(fields, 1);
    const auto head = head_bytes.empty() ? std::vector<WireField>{} : decode_fields(head_bytes);
    const auto message = bytes_field(head, 2);
    PbrpcResponse result;
    result.code = int32_value(integer_field(head, 1));
    result.message = bytes_text(message);
    result.rpc_id = int32_value(integer_field(fields, 2));
    result.start_pos = int32_value(integer_field(fields, 3));
    result.total_len = int32_value(integer_field(fields, 4));
    result.ret_byte_num = int32_value(integer_field(fields, 5));
    result.ret_byte = bytes_field(fields, 6);
    return result;
}

std::vector<PbrpcConfig> inventory_pbrpc_configs(const fs::path& root) {
    std::vector<PbrpcConfig> result;
    for (const auto& config : inventory_cloud_configs(root, "22")) {
        const auto [module, request_json] = split_descriptor(config.body);
        (void)request_json;
        result.push_back(PbrpcConfig{config.source_file, config.entry,
            config.request_id, module, config.placeholders, config.body});
    }
    return result;
}

Json pbrpc_configs_document(const fs::path& root) {
    const auto configs = inventory_pbrpc_configs(root);
    std::set<std::string> entries, modules, request_ids;
    Json records = Json::array();
    for (const auto& config : configs) {
        entries.insert(config.entry);
        modules.insert(config.module);
        if (!config.request_id.empty()) request_ids.insert(config.request_id);
        Json item = Json::object();
        item["source_file"] = config.source_file;
        item["entry"] = config.entry;
        item["request_format"] = "22";
        item["request_id"] = config.request_id;
        item["module"] = config.module;
        Json placeholders = Json::array();
        for (const auto& name : config.placeholders) placeholders.push_back(name);
        item["placeholders"] = std::move(placeholders);
        item["body"] = config.body;
        records.push_back(std::move(item));
    }
    Json document = Json::object();
    document["schema"] = "tdx-pbrpc-configs-native-v1";
    document["config_count"] = static_cast<std::uint64_t>(configs.size());
    document["entry_count"] = static_cast<std::uint64_t>(entries.size());
    document["module_count"] = static_cast<std::uint64_t>(modules.size());
    document["request_id_count"] = static_cast<std::uint64_t>(request_ids.size());
    document["records"] = std::move(records);
    return document;
}

std::pair<std::string, Json> parse_pbrpc_descriptor(
    std::string body, const std::map<std::string, std::string>& replacements) {
    for (const auto& [name, value] : replacements)
        replace_all(body, "$$" + name + "$$", value);
    const auto unresolved = unresolved_placeholders(body);
    if (!unresolved.empty()) {
        std::string detail = "unresolved PBRPC placeholders:";
        for (const auto& name : unresolved) detail += " " + name;
        throw Error(detail);
    }
    const auto [module, request_json] = split_descriptor(body);
    const auto request = Json::parse(request_json);
    if (!request.is_object()) throw Error("PBRPC ReqByte JSON must be an object");
    return {module, request};
}

PbrpcRequestSpec find_pbrpc_config_spec(
    const fs::path& root, const std::string& request_id,
    const std::string& entry, const std::string& source_file,
    const std::vector<std::string>& body_contains,
    const std::map<std::string, std::string>& replacements) {
    const auto lowered_entry = lower_ascii(entry);
    const auto lowered_source = lower_ascii(source_file);
    std::vector<PbrpcConfig> candidates;
    for (const auto& config : inventory_pbrpc_configs(root)) {
        if (config.request_id != request_id) continue;
        if (!entry.empty() && lower_ascii(config.entry) != lowered_entry) continue;
        if (!source_file.empty() && lower_ascii(config.source_file) != lowered_source) continue;
        const auto lowered_body = lower_ascii(config.body);
        bool matches = true;
        for (const auto& needle : body_contains) {
            if (lowered_body.find(lower_ascii(needle)) == std::string::npos) {
                matches = false;
                break;
            }
        }
        if (matches) candidates.push_back(config);
    }
    if (candidates.empty())
        throw Error("no reqformat=22 config found for ReqId " + request_id);
    std::stable_sort(candidates.begin(), candidates.end(), [](const auto& left, const auto& right) {
        return std::make_tuple(left.placeholders.size(), lower_ascii(left.source_file)) <
               std::make_tuple(right.placeholders.size(), lower_ascii(right.source_file));
    });
    const auto& selected = candidates.front();
    auto [module, request] = parse_pbrpc_descriptor(selected.body, replacements);
    return PbrpcRequestSpec{selected.entry, std::move(module), std::move(request),
                            selected.source_file};
}

void set_pbrpc_request_value(Json& request, const std::string& name, Json value) {
    if (!request.is_object()) throw Error("PBRPC request must be an object");
    auto& object = request.as_object();
    const auto folded = lower_ascii(name);
    auto found = std::find_if(object.begin(), object.end(), [&](const auto& item) {
        return lower_ascii(item.first) == folded;
    });
    if (found == object.end()) object[name] = std::move(value);
    else found->second = std::move(value);
}

PbrpcQueryResult query_pbrpc_raw(
    const std::string& entry, const std::string& module, const Json& request,
    const std::string& base_url, int timeout_ms, int max_rounds,
    int retry_delay_ms, const PbrpcTransport& transport,
    std::size_t max_assembled_bytes) {
    if (entry.empty()) throw Error("PBRPC Entry is required");
    if (module.empty()) throw Error("PBRPC module is required");
    if (!request.is_object()) throw Error("PBRPC request must be a JSON object");
    if (timeout_ms < 100 || max_rounds < 1 || max_rounds > 1000 ||
        retry_delay_ms < 0 || retry_delay_ms > 60000)
        throw Error("PBRPC query limits are invalid");
    if (max_assembled_bytes < 1 ||
        max_assembled_bytes > pbrpc_maximum_assembled_bytes)
        throw Error("PBRPC max_assembled_bytes must be in 1..134217728");
    const auto url = base_url + (base_url.find('?') == std::string::npos ? "?" : "&") +
                     "Entry=" + url_encode(entry);
    const auto request_json = request.dump(-1);
    std::int32_t rpc_id = 0;
    std::int32_t start_pos = 0;
    int idle_rounds = 0;
    Bytes assembled;
    PbrpcResponse response;
    for (int round = 1; round <= max_rounds; ++round) {
        const auto payload = build_pbrpc_request(module, request_json, rpc_id, start_pos);
        Bytes raw;
        if (transport) raw = transport(url, payload, timeout_ms);
        else {
            const auto http = http_post(url, payload,
                {{"Accept", "application/octet-stream"},
                 {"Content-Type", "application/octet-stream"},
                 {"User-Agent", "Mozilla/5.0"}}, timeout_ms);
            if (http.status < 200 || http.status >= 300)
                throw Error("PBRPC HTTP status " + std::to_string(http.status));
            raw = http.body;
        }
        response = parse_pbrpc_response(raw);
        if (response.code)
            throw Error("PBRPC server code " + std::to_string(response.code) +
                        (response.message.empty() ? "" : ": " + response.message));
        if (response.rpc_id <= 0)
            throw Error("PBRPC server rejected request with RpcID " +
                        std::to_string(response.rpc_id));
        if (rpc_id && response.rpc_id != rpc_id)
            throw Error("PBRPC RpcID changed during transfer");
        rpc_id = response.rpc_id;
        if (response.ret_byte_num < 0 ||
            static_cast<std::size_t>(response.ret_byte_num) != response.ret_byte.size())
            throw Error("PBRPC RetByteNum does not match RetByte length");
        if (!response.ret_byte.empty()) {
            if (response.start_pos != start_pos)
                throw Error("unexpected PBRPC StartPos " +
                            std::to_string(response.start_pos) + ", expected " +
                            std::to_string(start_pos));
            if (assembled.size() > max_assembled_bytes ||
                response.ret_byte.size() >
                    max_assembled_bytes - assembled.size())
                throw Error("PBRPC assembled response exceeds max_assembled_bytes " +
                            std::to_string(max_assembled_bytes));
            append_bytes(assembled, response.ret_byte);
            start_pos += static_cast<std::int32_t>(response.ret_byte.size());
            idle_rounds = 0;
        } else {
            ++idle_rounds;
        }
        if (response.total_len > 0 && start_pos >= response.total_len)
            return PbrpcQueryResult{std::move(assembled), rpc_id, round};
        if (!response.ret_byte.empty() && !response.total_len)
            return PbrpcQueryResult{std::move(assembled), rpc_id, round};
        if (idle_rounds >= 3) throw Error("PBRPC server returned no data for three rounds");
        if (retry_delay_ms)
            std::this_thread::sleep_for(std::chrono::milliseconds(retry_delay_ms));
    }
    if (response.total_len > 0 && assembled.size() !=
        static_cast<std::size_t>(response.total_len))
        throw Error("PBRPC response did not complete in " + std::to_string(max_rounds) +
                    " rounds");
    throw Error("PBRPC response did not complete");
}

Json decode_pbrpc_result(const Bytes& input) {
    Bytes data = input;
    while (!data.empty() && data.back() == 0) data.pop_back();
    const auto raw = bytes_text(data);
    std::string text;
    try {
        (void)utf8_to_wide(raw);
        text = raw;
    } catch (const Error&) {
        try {
            text = decode_gbk(data);
        } catch (const Error&) {
            Json binary = Json::object();
            binary["encoding"] = "hex";
            binary["data"] = hex_bytes(data);
            return binary;
        }
    }
    try {
        return Json::parse(text);
    } catch (const std::exception&) {
        return Json(text);
    }
}

Json execute_pbrpc_config(
    const fs::path& root, const std::string& request_id,
    const std::map<std::string, std::string>& replacements,
    const std::map<std::string, std::string>& overrides,
    const std::string& entry, const std::string& module,
    const std::string& source_file, const std::vector<std::string>& body_contains,
    const std::string& base_url, int timeout_ms, int max_rounds,
    int retry_delay_ms, std::size_t max_assembled_bytes) {
    auto spec = find_pbrpc_config_spec(root, request_id, entry, source_file,
                                       body_contains, replacements);
    if (!module.empty()) spec.module = module;
    for (const auto& [name, value] : overrides)
        set_pbrpc_request_value(spec.request, name, value);
    set_pbrpc_request_value(spec.request, "ReqId", request_id);
    auto query = query_pbrpc_raw(spec.entry, spec.module, spec.request, base_url,
                                  timeout_ms, max_rounds, retry_delay_ms, {},
                                  max_assembled_bytes);
    auto response = decode_pbrpc_result(query.data);
    validate_business_result(response);
    Json document = Json::object();
    document["schema"] = "tdx-pbrpc-native-v1";
    document["request_id"] = request_id;
    document["entry"] = spec.entry;
    document["module"] = spec.module;
    document["source_file"] = spec.source_file;
    document["rpc_id"] = query.rpc_id;
    document["rounds"] = query.rounds;
    document["raw_size"] = static_cast<std::uint64_t>(query.data.size());
    document["request"] = spec.request;
    document["response"] = std::move(response);
    return document;
}

int command_cloud_pbrpc(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout <<
            "Usage: tdx-tool cloud pbrpc [options]\n\n"
            "Native reqformat=22 protobuf RPC config discovery and query.\n\n"
            "Options:\n"
            "  --list                 List active reqformat=22 configs\n"
            "  --req-id ID            Select a cloud_cfg request template\n"
            "  --entry NAME           Limit selection to an Entry\n"
            "  --module NAME          Override server module DLL\n"
            "  --source-file NAME     Limit selection to one XML file\n"
            "  --body-contains TEXT   Repeatable request-body selector\n"
            "  --set NAME=VALUE       Repeatable template replacement\n"
            "  --param NAME=VALUE     Repeatable ReqByte field override\n"
            "  --request-json JSON    Explicit object; requires --entry/--module\n"
            "  --max-rounds N         RpcID transfer limit (default 32)\n"
            "  --max-assembled-bytes N  Assembled response budget, 1..134217728\n"
            "  --retry-delay-ms N     Delay between rounds (default 150)\n"
            "  --base-url URL         Default static.tdx.com.cn:7615/TQLEX\n"
            "  --timeout-ms N         Default 15000\n"
            "  --attempts N           Whole-RPC attempts, 1..10 (default 3)\n"
            "  --attempt-delay-ms N   Linear retry delay base (default 250)\n"
            "  --root PATH            TDX installation root\n"
            "  --output PATH          Default output/tdx-pbrpc-native.json\n"
            "  --compact              Compact JSON\n";
        return 0;
    }
    const bool list = args.take_flag("--list");
    const auto request_id = args.take_option("--req-id");
    const auto entry = args.take_option("--entry");
    const auto module = args.take_option("--module");
    const auto source_file = args.take_option("--source-file");
    const auto body_contains = args.take_options("--body-contains");
    const auto replacements = assignments(args.take_options("--set"), "--set");
    const auto overrides = assignments(args.take_options("--param"), "--param");
    const auto request_json = args.take_option("--request-json");
    const auto base_url = args.take_option("--base-url",
        cloud_endpoints::tqlex);
    const int timeout_ms = integer_option(args.take_option("--timeout-ms", "15000"),
                                          "--timeout-ms", 100, 600000);
    const int max_rounds = integer_option(args.take_option("--max-rounds", "32"),
                                          "--max-rounds", 1, 1000);
    const auto max_assembled_bytes = static_cast<std::size_t>(integer_option(
        args.take_option("--max-assembled-bytes", "134217728"),
        "--max-assembled-bytes", 1,
        static_cast<int>(pbrpc_maximum_assembled_bytes)));
    const int retry_delay_ms = integer_option(
        args.take_option("--retry-delay-ms", "150"), "--retry-delay-ms", 0, 60000);
    const int max_attempts = integer_option(args.take_option("--attempts", "3"),
                                            "--attempts", 1, 10);
    const int attempt_delay_ms = integer_option(
        args.take_option("--attempt-delay-ms", "250"),
        "--attempt-delay-ms", 0, 60000);
    const auto root_text = args.take_option("--root");
    const auto output_text = args.take_option("--output", "output/tdx-pbrpc-native.json");
    const bool compact = args.take_flag("--compact");
    args.require_empty();

    const auto root = find_tdx_root(root_text.empty() ? fs::path{} : fs::u8path(root_text));
    Json document;
    if (list) {
        document = pbrpc_configs_document(root);
    } else {
        int attempts = 0;
        document = detail::retry_cloud_json([&]() -> Json {
            if (!request_json.empty()) {
                if (entry.empty() || module.empty())
                    throw Error("--request-json requires --entry and --module");
                auto request = Json::parse(request_json);
                if (!request.is_object())
                    throw Error("--request-json must contain an object");
                for (const auto& [name, value] : overrides)
                    set_pbrpc_request_value(request, name, value);
                if (!request_id.empty())
                    set_pbrpc_request_value(request, "ReqId", request_id);
                auto query = query_pbrpc_raw(entry, module, request, base_url,
                    timeout_ms, max_rounds, retry_delay_ms, {},
                    max_assembled_bytes);
                auto response = decode_pbrpc_result(query.data);
                validate_business_result(response);
                Json result = Json::object();
                result["schema"] = "tdx-pbrpc-native-v1";
                result["request_id"] = request_id;
                result["entry"] = entry;
                result["module"] = module;
                result["source_file"] = "";
                result["rpc_id"] = query.rpc_id;
                result["rounds"] = query.rounds;
                result["raw_size"] = static_cast<std::uint64_t>(query.data.size());
                result["request"] = std::move(request);
                result["response"] = std::move(response);
                return result;
            }
            if (request_id.empty())
                throw Error("provide --req-id, --request-json, or --list");
            return execute_pbrpc_config(root, request_id, replacements, overrides,
                entry, module, source_file, body_contains, base_url, timeout_ms,
                max_rounds, retry_delay_ms, max_assembled_bytes);
        }, detail::is_transient_pbrpc_error, attempts,
           max_attempts, attempt_delay_ms);
        document["attempts"] = attempts;
        document["max_attempts"] = max_attempts;
    }
    const auto output = fs::u8path(output_text);
    atomic_write_text(output, document.dump(compact ? -1 : 2) + "\n");
    std::cout << (list ? "listed PBRPC configs" : "completed PBRPC request")
              << " -> " << path_utf8(output) << '\n';
    return 0;
}

}  // namespace tdx
