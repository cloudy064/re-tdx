#include "tdx/ttplugin_redirect.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <limits>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace tdx {
namespace {

fs::path from_utf8(const std::string& value) {
#ifdef _WIN32
    return fs::path(utf8_to_wide(value));
#else
    return fs::path(value);
#endif
}

const Json* find_value(const Json& object, std::string_view key) {
    if (!object.is_object()) throw Error("TTPlugin redirect request must be a JSON object");
    const auto found = object.as_object().find(std::string(key));
    return found == object.as_object().end() ? nullptr : &found->second;
}

std::int64_t integer_value(const Json& object, std::string_view key,
                           std::int64_t fallback = 0, bool required = false) {
    const auto* value = find_value(object, key);
    if (!value) {
        if (required) throw Error("missing TTPlugin redirect field: " + std::string(key));
        return fallback;
    }
    if (!value->is_number())
        throw Error("TTPlugin redirect field must be numeric: " + std::string(key));
    const double number = value->as_number();
    if (!std::isfinite(number) || std::floor(number) != number ||
        number < static_cast<double>(std::numeric_limits<std::int64_t>::min()) ||
        number > static_cast<double>(std::numeric_limits<std::int64_t>::max()))
        throw Error("TTPlugin redirect field must be an integer: " + std::string(key));
    return static_cast<std::int64_t>(number);
}

std::string string_value(const Json& object, std::string_view key) {
    const auto* value = find_value(object, key);
    if (!value) return {};
    if (!value->is_string())
        throw Error("TTPlugin redirect field must be text: " + std::string(key));
    return value->as_string();
}

std::uint8_t checked_u8(const Json& object, std::string_view key) {
    const auto value = integer_value(object, key);
    if (value < 0 || value > 0xff)
        throw Error("TTPlugin redirect field is outside uint8 range: " + std::string(key));
    return static_cast<std::uint8_t>(value);
}

std::uint16_t checked_u16(const Json& object, std::string_view key) {
    const auto value = integer_value(object, key);
    if (value < 0 || value > 0xffff)
        throw Error("TTPlugin redirect field is outside uint16 range: " + std::string(key));
    return static_cast<std::uint16_t>(value);
}

std::int32_t checked_i32(const Json& object, std::string_view key) {
    const auto value = integer_value(object, key);
    if (value < std::numeric_limits<std::int32_t>::min() ||
        value > std::numeric_limits<std::int32_t>::max())
        throw Error("TTPlugin redirect field is outside int32 range: " + std::string(key));
    return static_cast<std::int32_t>(value);
}

void write_u16(Bytes& output, std::size_t offset, std::uint16_t value) {
    output.at(offset) = static_cast<std::uint8_t>(value & 0xff);
    output.at(offset + 1) = static_cast<std::uint8_t>(value >> 8);
}

void write_i32(Bytes& output, std::size_t offset, std::int32_t value) {
    const auto bits = static_cast<std::uint32_t>(value);
    for (int index = 0; index < 4; ++index)
        output.at(offset + index) = static_cast<std::uint8_t>(bits >> (8 * index));
}

Bytes encode_gbk(std::string_view value) {
#ifdef _WIN32
    const auto wide = utf8_to_wide(value);
    if (wide.empty()) return {};
    const int size = WideCharToMultiByte(936, WC_NO_BEST_FIT_CHARS, wide.data(),
                                         static_cast<int>(wide.size()), nullptr, 0,
                                         nullptr, nullptr);
    if (size <= 0) throw Error("failed to encode TTPlugin text as GBK");
    Bytes result(static_cast<std::size_t>(size));
    if (WideCharToMultiByte(936, WC_NO_BEST_FIT_CHARS, wide.data(),
                            static_cast<int>(wide.size()),
                            reinterpret_cast<char*>(result.data()), size,
                            nullptr, nullptr) != size)
        throw Error("failed to encode TTPlugin text as GBK");
    return result;
#else
    Bytes result;
    for (const auto ch : value) {
        if (static_cast<unsigned char>(ch) >= 0x80)
            throw Error("non-ASCII TTPlugin text requires Windows GBK conversion");
        result.push_back(static_cast<std::uint8_t>(ch));
    }
    return result;
#endif
}

void write_text(Bytes& output, std::size_t offset, std::size_t width,
                const Json& request, std::string_view key) {
    const auto encoded = encode_gbk(string_value(request, key));
    if (encoded.size() >= width)
        throw Error("TTPlugin redirect text does not fit fixed field: " + std::string(key));
    std::copy(encoded.begin(), encoded.end(), output.begin() + offset);
}

Json field(const char* name, const char* type, int offset, int size) {
    Json result = Json::object();
    result["name"] = name;
    result["type"] = type;
    result["offset"] = offset;
    result["size"] = size;
    result["missing_value_behavior"] = type[0] == 'c' ? "empty string" : "zero";
    return result;
}

Json names(std::initializer_list<const char*> values) {
    Json result = Json::array();
    for (const auto* value : values) result.push_back(value);
    return result;
}

Json protocol(int req, const char* symbol, const char* meaning, int wire_size,
              Json request_fields, Json response_fields) {
    Json result = Json::object();
    result["req"] = req;
    result["request_struct"] = symbol;
    result["meaning"] = meaning;
    result["wire_size"] = wire_size;
    result["request_fields"] = std::move(request_fields);
    result["response_object_fields"] = std::move(response_fields);
    return result;
}

Json request_fields_4611() {
    Json result = Json::array();
    result.push_back(field("req", "uint16_le", 0, 2));
    result.push_back(field("setcode", "uint16_le", 2, 2));
    result.push_back(field("code", "char_gbk_z", 4, 8));
    result.push_back(field("reserved", "uint8", 12, 1));
    return result;
}

Json request_fields_4612() {
    Json result = Json::array();
    result.push_back(field("req", "uint16_le", 0, 2));
    result.push_back(field("setcode", "uint16_le", 2, 2));
    result.push_back(field("code", "char_gbk_z", 4, 8));
    result.push_back(field("sFilePath", "char_gbk_z", 12, 80));
    result.push_back(field("nOffset", "int32_le", 92, 4));
    result.push_back(field("nLength", "int32_le", 96, 4));
    result.push_back(field("whichjbm", "uint16_le", 100, 2));
    result.push_back(field("reserved", "uint8", 102, 1));
    return result;
}

Json request_fields_4618() {
    Json result = Json::array();
    result.push_back(field("req", "uint16_le", 0, 2));
    result.push_back(field("search_type", "uint8", 2, 1));
    result.push_back(field("from_order", "int32_le", 3, 4));
    result.push_back(field("wantnum", "uint16_le", 7, 2));
    result.push_back(field("setcode", "uint16_le", 9, 2));
    result.push_back(field("code", "char_gbk_z", 11, 22));
    result.push_back(field("fl_str", "char_gbk_z", 33, 11));
    result.push_back(field("type_id", "uint8", 44, 1));
    return result;
}

Json request_fields_4631() {
    Json result = Json::array();
    result.push_back(field("req", "uint16_le", 0, 2));
    result.push_back(field("flag", "int32_le", 2, 4));
    result.push_back(field("pos", "int32_le", 6, 4));
    result.push_back(field("wantlen", "int32_le", 10, 4));
    result.push_back(field("filename", "char_gbk_z", 14, 100));
    return result;
}

Json request_fields_4632() {
    Json result = Json::array();
    result.push_back(field("req", "uint16_le", 0, 2));
    result.push_back(field("setcode", "uint16_le", 2, 2));
    result.push_back(field("code", "char_gbk_z", 4, 22));
    result.push_back(field("blocktype", "uint8", 26, 1));
    result.push_back(field("blockstyle", "uint8", 27, 1));
    result.push_back(field("blockid", "char_gbk_z", 28, 21));
    return result;
}

std::string hex(const Bytes& value) {
    static constexpr char digits[] = "0123456789abcdef";
    std::string result;
    result.reserve(value.size() * 2);
    for (const auto byte : value) {
        result.push_back(digits[byte >> 4]);
        result.push_back(digits[byte & 0x0f]);
    }
    return result;
}

}  // namespace

Json ttplugin_redirect_contract_document() {
    Json protocols = Json::array();
    protocols.push_back(protocol(
        4611, "mp_f10cfg_req", "F10 configuration/catalog", 14,
        request_fields_4611(), names({"titlenum", "list", "sTitle", "sFilePath",
                                     "nOffset", "nLength"})));
    protocols.push_back(protocol(
        4612, "mp_f10txt_req", "F10 text", 104,
        request_fields_4612(), names({"setcode", "code", "num", "buf"})));
    protocols.push_back(protocol(
        4618, "mp_infotitle_req", "information-title search", 74,
        request_fields_4618(), names({"totalnum", "from_order", "titlenum", "list",
                                     "rec_id", "time_ymd", "time_hms", "title",
                                     "delflag", "unused", "show_id", "proc_id",
                                     "info_src", "info_format", "info_url"})));
    protocols.push_back(protocol(
        4630, "mp_infotitle_req", "information-title search alternate", 74,
        request_fields_4618(), names({"totalnum", "from_order", "titlenum", "list",
                                     "rec_id", "time_ymd", "time_hms", "title",
                                     "delflag", "unused", "show_id", "proc_id",
                                     "info_src", "info_format", "info_url"})));
    protocols.push_back(protocol(
        4631, "MP_FILE_REQ", "information file chunk", 114,
        request_fields_4631(), names({"flag", "num", "buf"})));
    protocols.push_back(protocol(
        4632, "MP_XMLBLOCK_REQ", "XML block", 49,
        request_fields_4632(), names({"blocktype", "blockstyle", "blockid", "num", "buf"})));

    Json flow = Json::array();
    flow.push_back("HQDataService.SetOption(RedirectData, req, JSON)");
    flow.push_back("JSON key req is used when the numeric option argument is zero");
    flow.push_back("ProtocolTrans encodes a fixed legacy request body");
    flow.push_back("CTAJob_Redirect carries Target, ReqNo, and Body");
    flow.push_back("the response becomes CTAJob_InetTQL Name=Local:HQDataService with JSON Body");

    Json evidence = Json::object();
    evidence["module"] = "TTPlugin.dll";
    evidence["redirect_option_handler"] = "0x10286350";
    evidence["request_translator"] = "0x102979D0";
    evidence["response_translator"] = "0x102983A0";
    evidence["redirect_job_builder"] = "0x1028A6D0";
    evidence["redirect_response_bridge"] = "0x10287B30";
    evidence["source_path"] = "TDXTradePlugin/SessionManager/ProtocolTrans.cpp";

    Json boundary = Json::object();
    boundary["offline_only"] = true;
    boundary["network_sent"] = false;
    boundary["dll_loaded"] = false;
    boundary["login_or_session_required"] = false;
    boundary["redirect_data_is_endpoint_configuration"] = false;
    boundary["unknown_trailing_bytes_are_zero_filled"] = true;
    boundary["response_offsets_recovered"] = false;
    boundary["interpretation"] =
        "This is a request/response protocol translation contract, not a server discovery result.";

    Json result = Json::object();
    result["schema"] = "tdx-ttplugin-redirect-contract-native-v1";
    result["protocols"] = std::move(protocols);
    result["flow"] = std::move(flow);
    result["evidence"] = std::move(evidence);
    result["boundary"] = std::move(boundary);
    return result;
}

Bytes encode_ttplugin_redirect_request(const Json& request) {
    const auto req_value = integer_value(request, "req", 0, true);
    if (req_value < 0 || req_value > 0xffff)
        throw Error("TTPlugin redirect req is outside uint16 range");
    const auto req = static_cast<std::uint16_t>(req_value);
    std::size_t size = 0;
    switch (req) {
        case 4611: size = 14; break;
        case 4612: size = 104; break;
        case 4618:
        case 4630: size = 74; break;
        case 4631: size = 114; break;
        case 4632: size = 49; break;
        default: throw Error("TTPlugin RedirectData does not translate req " + std::to_string(req));
    }
    Bytes output(size, 0);
    write_u16(output, 0, req);
    switch (req) {
        case 4611:
            write_u16(output, 2, checked_u16(request, "setcode"));
            write_text(output, 4, 8, request, "code");
            output[12] = checked_u8(request, "reserved");
            break;
        case 4612:
            write_u16(output, 2, checked_u16(request, "setcode"));
            write_text(output, 4, 8, request, "code");
            write_text(output, 12, 80, request, "sFilePath");
            write_i32(output, 92, checked_i32(request, "nOffset"));
            write_i32(output, 96, checked_i32(request, "nLength"));
            write_u16(output, 100, checked_u16(request, "whichjbm"));
            output[102] = checked_u8(request, "reserved");
            break;
        case 4618:
        case 4630:
            output[2] = checked_u8(request, "search_type");
            write_i32(output, 3, checked_i32(request, "from_order"));
            write_u16(output, 7, checked_u16(request, "wantnum"));
            write_u16(output, 9, checked_u16(request, "setcode"));
            write_text(output, 11, 22, request, "code");
            write_text(output, 33, 11, request, "fl_str");
            output[44] = checked_u8(request, "type_id");
            break;
        case 4631:
            write_i32(output, 2, checked_i32(request, "flag"));
            write_i32(output, 6, checked_i32(request, "pos"));
            write_i32(output, 10, checked_i32(request, "wantlen"));
            write_text(output, 14, 100, request, "filename");
            break;
        case 4632:
            write_u16(output, 2, checked_u16(request, "setcode"));
            write_text(output, 4, 22, request, "code");
            output[26] = checked_u8(request, "blocktype");
            output[27] = checked_u8(request, "blockstyle");
            write_text(output, 28, 21, request, "blockid");
            break;
        default: break;
    }
    return output;
}

int command_recon_ttplugin_redirect(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout <<
            "Usage: tdx-tool recon ttplugin-redirect [options]\n\n"
            "Emit the statically recovered TTPlugin RedirectData contract. With --request,\n"
            "also encode one caller-supplied JSON request into its fixed legacy body.\n"
            "No DLL is loaded and no network request is sent.\n\n"
            "Options:\n"
            "  --request PATH         Optional UTF-8 JSON object containing req and fields\n"
            "  --binary-output PATH   Default output/ttplugin-redirect-request.bin\n"
            "  --output PATH          Default output/tdx-ttplugin-redirect-contract.json\n"
            "  --compact              Compact JSON\n";
        return 0;
    }
    const auto request_text = args.take_option("--request");
    const bool binary_requested = args.has("--binary-output");
    const auto binary_text = args.take_option(
        "--binary-output", "output/ttplugin-redirect-request.bin");
    const auto output = from_utf8(args.take_option(
        "--output", "output/tdx-ttplugin-redirect-contract.json"));
    const bool compact = args.take_flag("--compact");
    args.require_empty();

    auto document = ttplugin_redirect_contract_document();
    if (!request_text.empty()) {
        const auto request_path = from_utf8(request_text);
        const auto request = Json::parse(read_text_utf8(request_path));
        const auto body = encode_ttplugin_redirect_request(request);
        const auto binary_output = from_utf8(binary_text);
        atomic_write_bytes(binary_output, body);
        Json encoding = Json::object();
        encoding["request_path"] = path_utf8(request_path);
        encoding["binary_output"] = path_utf8(binary_output);
        encoding["req"] = integer_value(request, "req", 0, true);
        encoding["wire_size"] = static_cast<std::uint64_t>(body.size());
        encoding["wire_md5"] = md5_bytes(body);
        encoding["wire_hex"] = hex(body);
        document["encoding"] = std::move(encoding);
    } else if (binary_requested) {
        throw Error("--binary-output requires --request");
    }
    atomic_write_text(output, document.dump(compact ? -1 : 2) + "\n");
    std::cout << "recovered 6 TTPlugin RedirectData request contracts";
    if (!request_text.empty()) std::cout << " and encoded one request";
    std::cout << " -> " << path_utf8(output) << '\n';
    return 0;
}

}  // namespace tdx
