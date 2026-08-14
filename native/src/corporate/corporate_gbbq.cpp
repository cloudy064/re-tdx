#include "corporate_internal.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace tdx::corporate_detail {
namespace {

constexpr std::size_t gbbq_header_size = 4;
constexpr std::size_t gbbq_record_size = 29;
constexpr std::size_t gbbq_encrypted_size = 24;
constexpr std::size_t gbbq_cipher_state_size = 4168;

// Expanded Blowfish state for the literal key loaded by TdxW sub_5175B0.
// Keeping the expanded state here avoids an OpenSSL/runtime DLL dependency.
constexpr const char gbbq_cipher_state_base64[] =
    "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";

int base64_digit(char value) {
    if (value >= 'A' && value <= 'Z') return value - 'A';
    if (value >= 'a' && value <= 'z') return value - 'a' + 26;
    if (value >= '0' && value <= '9') return value - '0' + 52;
    if (value == '+') return 62;
    if (value == '/') return 63;
    return -1;
}

const std::array<std::uint8_t, gbbq_cipher_state_size>& gbbq_cipher_state() {
    static const auto state = [] {
        std::array<std::uint8_t, gbbq_cipher_state_size> decoded{};
        std::size_t output = 0;
        std::uint32_t buffer = 0;
        int bits = -8;
        for (const char value : gbbq_cipher_state_base64) {
            if (value == '\0' || value == '=') break;
            const int digit = base64_digit(value);
            if (digit < 0) throw Error("embedded GBBQ cipher state is invalid");
            buffer = (buffer << 6) | static_cast<std::uint32_t>(digit);
            bits += 6;
            if (bits >= 0) {
                if (output >= decoded.size())
                    throw Error("embedded GBBQ cipher state is too long");
                decoded[output++] =
                    static_cast<std::uint8_t>((buffer >> bits) & 0xffU);
                bits -= 8;
            }
        }
        if (output != decoded.size())
            throw Error("embedded GBBQ cipher state has an invalid length");
        return decoded;
    }();
    return state;
}

void write_u32_le(std::uint8_t* output, std::uint32_t value) {
    for (int shift = 0; shift < 32; shift += 8)
        *output++ = static_cast<std::uint8_t>(value >> shift);
}

void decrypt_gbbq_block(const std::uint8_t* encrypted,
                        std::uint8_t* clear) {
    const auto& state = gbbq_cipher_state();
    auto state_word = [&](std::size_t offset) {
        return read_u32_le(state.data() + offset);
    };

    std::uint32_t current = state_word(0x44) ^ read_u32_le(encrypted);
    std::uint32_t previous = read_u32_le(encrypted + 4);
    for (int offset = 0x40; offset >= 4; offset -= 4) {
        std::uint32_t value =
            state_word(0x448 + ((current >> 16) & 0xffU) * 4);
        value += state_word(0x48 + (current >> 24) * 4);
        value ^= state_word(0x848 + ((current >> 8) & 0xffU) * 4);
        value += state_word(0xc48 + (current & 0xffU) * 4);
        value ^= state_word(static_cast<std::size_t>(offset));
        const auto next_previous = current;
        current = previous ^ value;
        previous = next_previous;
    }
    previous ^= state_word(0);
    write_u32_le(clear, previous);
    write_u32_le(clear + 4, current);
}

std::array<std::uint8_t, gbbq_record_size> decrypt_gbbq_record(
    const std::uint8_t* encrypted) {
    std::array<std::uint8_t, gbbq_record_size> clear{};
    for (std::size_t offset = 0; offset < gbbq_encrypted_size; offset += 8)
        decrypt_gbbq_block(encrypted + offset, clear.data() + offset);
    std::copy_n(encrypted + gbbq_encrypted_size,
                gbbq_record_size - gbbq_encrypted_size,
                clear.data() + gbbq_encrypted_size);
    return clear;
}

bool valid_code_bytes(const std::uint8_t* record) {
    return std::all_of(record + 1, record + 7, [](std::uint8_t value) {
        return value >= '0' && value <= '9';
    });
}

void validate_gbbq_record(const std::uint8_t* record, std::size_t index) {
    if (record[0] > 2 || !valid_code_bytes(record) || record[7] != 0)
        throw Error("GBBQ record " + std::to_string(index) +
                    " has an invalid security identity");
    const auto raw_date = read_u32_le(record + 8);
    if (date_json(raw_date).is_null())
        throw Error("GBBQ record " + std::to_string(index) +
                    " has an invalid event date");
    for (std::size_t field = 0; field < 4; ++field) {
        if (!std::isfinite(read_f32_le(record + 13 + field * 4)))
            throw Error("GBBQ record " + std::to_string(index) +
                        " has a non-finite value");
    }
}

Json local_transport(const fs::path& source) {
    Json value = Json::object();
    value["protocol"] = "local-file";
    value["format"] = "TdxW hq_cache/gbbq";
    value["record_size"] = static_cast<std::uint64_t>(gbbq_record_size);
    value["encrypted_prefix_size"] =
        static_cast<std::uint64_t>(gbbq_encrypted_size);
    value["source_path"] = path_utf8(source);
    return value;
}

}  // namespace

Json parse_local_gbbq_capital_changes(
    const Bytes& data,
    const std::vector<std::string>& securities,
    bool include_raw,
    const fs::path& source) {
    const auto requested = parse_securities(securities);
    if (data.size() < gbbq_header_size)
        throw Error("GBBQ file is shorter than its header");
    const auto source_count = read_u32_le(data.data());
    if (source_count > 2000000U)
        throw Error("GBBQ record count exceeds the safety limit");
    const std::size_t expected =
        gbbq_header_size + static_cast<std::size_t>(source_count) *
                               gbbq_record_size;
    if (data.size() != expected)
        throw Error("GBBQ file length mismatch: expected " +
                    std::to_string(expected) + ", got " +
                    std::to_string(data.size()));

    std::map<std::pair<int, std::string>, std::size_t> requested_index;
    for (std::size_t index = 0; index < requested.size(); ++index)
        requested_index.emplace(requested[index].key(), index);
    std::vector<Json> records(requested.size(), Json::array());

    for (std::size_t index = 0; index < source_count; ++index) {
        const auto clear = decrypt_gbbq_record(
            data.data() + gbbq_header_size + index * gbbq_record_size);
        validate_gbbq_record(clear.data(), index);
        const std::string code(
            reinterpret_cast<const char*>(clear.data() + 1), 6);
        const auto found = requested_index.find({clear[0], code});
        if (found != requested_index.end())
            records[found->second].push_back(
                capital_record_json(clear.data(), include_raw));
    }

    Json blocks = Json::array();
    std::uint64_t event_count = 0;
    for (std::size_t index = 0; index < requested.size(); ++index) {
        const auto count = static_cast<std::uint64_t>(records[index].size());
        Json block = Json::object();
        block["security_id"] = requested[index].display();
        block["market"] = requested[index].market();
        block["market_id"] = requested[index].market_id;
        block["code"] = requested[index].code;
        block["block_count"] = 1;
        block["count"] = count;
        block["records"] = std::move(records[index]);
        blocks.push_back(std::move(block));
        event_count += count;
    }

    Json result = Json::object();
    result["schema"] = "tdx-market-capital-native-v1";
    result["generated_at"] = now_text();
    result["command"] = "0x000F";
    result["source_mode"] = "local";
    result["source_format"] = "gbbq";
    result["source_record_count"] =
        static_cast<std::uint64_t>(source_count);
    result["requested"] = static_cast<std::uint64_t>(requested.size());
    result["received"] = static_cast<std::uint64_t>(requested.size());
    result["event_count"] = event_count;
    result["blocks"] = std::move(blocks);
    result["transport"] = local_transport(source);
    return result;
}

Json load_local_capital_changes_document(
    const fs::path& root,
    const std::vector<std::string>& securities,
    bool include_raw,
    const fs::path& override_path) {
    const auto source = override_path.empty()
        ? root / "T0002" / "hq_cache" / "gbbq"
        : override_path;
    if (!fs::is_regular_file(source))
        throw Error("local GBBQ file is unavailable: " + path_utf8(source));
    return parse_local_gbbq_capital_changes(
        read_bytes(source), securities, include_raw, source);
}

}  // namespace tdx::corporate_detail

namespace tdx {

Json parse_local_gbbq_capital_changes(
    const Bytes& data,
    const std::vector<std::string>& securities,
    bool include_raw,
    const fs::path& source) {
    return corporate_detail::parse_local_gbbq_capital_changes(
        data, securities, include_raw, source);
}

Json load_local_capital_changes_document(
    const fs::path& root,
    const std::vector<std::string>& securities,
    bool include_raw,
    const fs::path& override_path) {
    return corporate_detail::load_local_capital_changes_document(
        root, securities, include_raw, override_path);
}

}  // namespace tdx
