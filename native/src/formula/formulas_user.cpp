#include "formulas_internal.hpp"

#include "../common/blowfish_internal.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <map>
#include <numeric>

namespace tdx::formulas_detail {
namespace {

constexpr std::size_t kHeaderSize = 53;
constexpr std::size_t kIndexSize = 10;
constexpr std::size_t kMaximumFormulaCount = 100000;
constexpr std::size_t kMaximumPayloadSize = 50U * 1024U * 1024U;

std::size_t checked_add(std::size_t left, std::size_t right,
                        std::string_view label) {
    if (right > std::numeric_limits<std::size_t>::max() - left)
        throw Error(std::string(label) + " overflows the host size");
    return left + right;
}

std::size_t checked_multiply(std::size_t left, std::size_t right,
                             std::string_view label) {
    if (left && right > std::numeric_limits<std::size_t>::max() / left)
        throw Error(std::string(label) + " overflows the host size");
    return left * right;
}

void require_range(const Bytes& bytes, std::size_t offset, std::size_t size,
                   std::string_view label) {
    if (offset > bytes.size() || size > bytes.size() - offset)
        throw Error(std::string(label) + " is outside PriGS.dat");
}

std::string decode_fixed(const Bytes& bytes, std::size_t offset,
                         std::size_t size) {
    require_range(bytes, offset, size, "formula text field");
    const auto begin = bytes.begin() + static_cast<std::ptrdiff_t>(offset);
    const auto end_limit = begin + static_cast<std::ptrdiff_t>(size);
    const auto end = std::find(begin, end_limit, 0);
    return decode_gbk(Bytes(begin, end));
}

std::string decode_dynamic(const Bytes& bytes, std::size_t offset,
                           std::size_t size) {
    if (!size) return {};
    require_range(bytes, offset, size, "formula dynamic text");
    const auto begin = bytes.begin() + static_cast<std::ptrdiff_t>(offset);
    const auto end_limit = begin + static_cast<std::ptrdiff_t>(size);
    const auto end = std::find(begin, end_limit, 0);
    return decode_gbk(Bytes(begin, end));
}

double read_float(const Bytes& bytes, std::size_t offset) {
    require_range(bytes, offset, 4, "formula float field");
    const auto bits = read_u32_le(bytes.data() + offset);
    float value = 0.0F;
    static_assert(sizeof(value) == sizeof(bits));
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

std::string source_name(std::uint32_t attributes) {
    if (attributes & 0x01U) return "system";
    if (attributes & 0x10U) return "temporary";
    if (attributes & 0x800U) return "default";
    return "user";
}

std::array<std::uint16_t, 5> kind_counts(const Bytes& bytes) {
    std::array<std::uint16_t, 5> counts{};
    for (std::size_t index = 0; index < counts.size(); ++index)
        counts[index] = read_u16_le(bytes.data() + 17 + index * 2);
    return counts;
}

int kind_for_record(std::size_t record_index,
                    const std::array<std::uint16_t, 5>& counts) {
    std::size_t boundary = 0;
    for (std::size_t kind = 0; kind < counts.size(); ++kind) {
        boundary += counts[kind];
        if (record_index < boundary) return static_cast<int>(kind);
    }
    throw Error("PriGS.dat record is outside its kind counts");
}

Json counts_json(const std::array<std::uint16_t, 5>& counts) {
    Json result = Json::object();
    const auto& definitions = formula_kind_definitions();
    for (std::size_t index = 0; index < counts.size(); ++index)
        result[std::string(definitions[index].key)] =
            static_cast<std::uint64_t>(counts[index]);
    return result;
}

const Json* json_member(const Json& object, const std::string& key) {
    if (!object.is_object()) return nullptr;
    const auto found = object.as_object().find(key);
    return found == object.as_object().end() ? nullptr : &found->second;
}

void apply_category_names(Json& user, const Json& system) {
    std::map<std::pair<std::string, int>, std::string> names;
    for (const auto& [kind, categories] : system.at("categories").as_object())
        for (const auto& category : categories.as_array())
            names[{kind, static_cast<int>(category.at("id").as_number())}] =
                category.at("name").as_string();
    for (auto& formula : user["formulas"].as_array()) {
        const auto key = std::make_pair(
            formula.at("kind_key").as_string(),
            static_cast<int>(formula.at("category_id").as_number()));
        if (const auto found = names.find(key); found != names.end())
            formula["category_name"] = found->second;
    }
}

void add_numeric_member(Json& target, const std::string& key,
                        const Json& source) {
    const auto* current = json_member(target, key);
    const auto* increment = json_member(source, key);
    const auto left = current && current->is_number()
        ? static_cast<std::uint64_t>(current->as_number()) : 0;
    const auto right = increment && increment->is_number()
        ? static_cast<std::uint64_t>(increment->as_number()) : 0;
    target[key] = left + right;
}

Json merge_installed_library(Json system, Json user) {
    apply_category_names(user, system);
    for (auto& formula : user["formulas"].as_array())
        system["formulas"].push_back(std::move(formula));
    for (const auto& [key, value] : user.at("counts").as_object()) {
        const auto* existing = json_member(system["counts"], key);
        const auto prior = existing && existing->is_number()
            ? static_cast<std::uint64_t>(existing->as_number()) : 0;
        system["counts"][key] =
            prior + static_cast<std::uint64_t>(value.as_number());
    }
    add_numeric_member(system, "source_text_count", user);
    add_numeric_member(system, "source_text_missing_count", user);
    system["user_file_source_text_count"] =
        user.at("user_file_source_text_count");
    system["schema"] = "tdx-formula-installed-library-v1";
    system["private_user_data"] = true;
    system["user_library"] = user.at("native");
    system["user_library"]["source_file"] = user.at("source_file");
    system["user_library"]["sha256"] = user.at("sha256");
    return system;
}

}  // namespace

std::vector<Formula> extract_user_formula_data(
    const std::filesystem::path& pri_gs, const std::set<int>& kinds,
    Json* native_metadata) {
    const auto bytes = read_bytes(pri_gs);
    if (bytes.size() < kHeaderSize)
        throw Error("PriGS.dat is shorter than its 53-byte header");
    const auto version = bytes[0];
    if (version < 4 || version > 5)
        throw Error("unsupported PriGS.dat version: " +
                    std::to_string(version));
    const auto count = static_cast<std::size_t>(
        read_u32_le(bytes.data() + 1));
    if (count > kMaximumFormulaCount)
        throw Error("PriGS.dat formula count exceeds the native limit");
    const auto index_bytes = checked_multiply(count, kIndexSize,
                                              "PriGS index size");
    const auto expected_records_offset = checked_add(
        kHeaderSize, index_bytes, "PriGS records offset");
    const auto records_offset = static_cast<std::size_t>(
        read_u32_le(bytes.data() + 5));
    if (records_offset != expected_records_offset)
        throw Error("PriGS.dat records offset does not match its index");
    const auto records_bytes = checked_multiply(count, record_size,
                                                "PriGS records size");
    const auto expected_payload_offset = checked_add(
        records_offset, records_bytes, "PriGS payload offset");
    const auto payload_offset = static_cast<std::size_t>(
        read_u32_le(bytes.data() + 9));
    if (payload_offset != expected_payload_offset)
        throw Error("PriGS.dat payload offset does not match its records");
    const auto payload_size = static_cast<std::size_t>(
        read_u32_le(bytes.data() + 13));
    if (payload_size > kMaximumPayloadSize)
        throw Error("PriGS.dat payload exceeds the native 50 MiB limit");
    if (payload_size % 8 != 0)
        throw Error("PriGS.dat encrypted payload is not 8-byte aligned");
    const auto expected_file_size = checked_add(
        payload_offset, payload_size, "PriGS file size");
    if (bytes.size() != expected_file_size)
        throw Error("PriGS.dat byte size does not match its header");

    const auto counts = kind_counts(bytes);
    const auto counted = std::accumulate(
        counts.begin(), counts.end(), std::size_t{0});
    if (counted != count)
        throw Error("PriGS.dat kind counts do not sum to its formula count");

    Bytes encrypted(bytes.begin() + static_cast<std::ptrdiff_t>(payload_offset),
                    bytes.end());
    const auto payload = blowfish_detail::blowfish_initial_state_ecb_decrypt(
        std::move(encrypted), BlowfishWordOrder::little_endian);

    std::vector<Formula> formulas;
    formulas.reserve(count);
    std::size_t payload_cursor = 0;
    std::size_t selected_source_bytes = 0;
    for (std::size_t file_index = 0; file_index < count; ++file_index) {
        const auto kind = kind_for_record(file_index, counts);
        const auto index_offset = kHeaderSize + file_index * kIndexSize;
        const std::array<std::size_t, 4> lengths{
            read_u16_le(bytes.data() + index_offset),
            read_u16_le(bytes.data() + index_offset + 2),
            read_u16_le(bytes.data() + index_offset + 4),
            read_u32_le(bytes.data() + index_offset + 6),
        };
        std::string source_text;
        for (std::size_t segment = 0; segment < lengths.size(); ++segment) {
            if (lengths[segment] > payload.size() - payload_cursor)
                throw Error("PriGS.dat dynamic segment exceeds its payload");
            if (segment == 0)
                source_text = decode_dynamic(
                    payload, payload_cursor, lengths[segment]);
            payload_cursor += lengths[segment];
        }

        const auto record_offset = records_offset + file_index * record_size;
        require_range(bytes, record_offset, record_size, "formula record");
        if (bytes[record_offset + 2] != static_cast<std::uint8_t>(kind))
            throw Error("PriGS.dat record kind disagrees with its header");
        if (!kinds.count(kind)) continue;

        const auto code = decode_fixed(bytes, record_offset + 3, 14);
        if (code.empty())
            throw Error("PriGS.dat contains an empty formula code");
        const auto parameter_count = static_cast<std::size_t>(
            bytes[record_offset + 72]);
        if (parameter_count > 16)
            throw Error("PriGS.dat formula parameter count exceeds 16");
        std::vector<FormulaParameter> parameters;
        parameters.reserve(parameter_count);
        for (std::size_t slot = 0; slot < parameter_count; ++slot) {
            const auto offset = record_offset + 73 + slot * 132;
            parameters.push_back(FormulaParameter{
                decode_fixed(bytes, offset, 16),
                read_float(bytes, offset + 16),
                read_float(bytes, offset + 20),
                read_float(bytes, offset + 24),
                read_float(bytes, offset + 28),
                read_float(bytes, offset + 32),
            });
        }
        const auto output_count = static_cast<std::size_t>(
            bytes[record_offset + 2185]);
        if (output_count > 64)
            throw Error("PriGS.dat formula output count exceeds 64");
        std::vector<std::string> outputs;
        outputs.reserve(output_count);
        for (std::size_t slot = 0; slot < output_count; ++slot)
            outputs.push_back(decode_fixed(
                bytes, record_offset + 2186 + slot * 28, 16));
        const auto attributes = read_u32_le(
            bytes.data() + record_offset + 5068);
        selected_source_bytes += lengths[0];
        formulas.push_back(Formula{
            kind,
            formula_kind_key(kind),
            formula_kind_name(kind),
            read_u16_le(bytes.data() + record_offset),
            code,
            decode_fixed(bytes, record_offset + 17, 50),
            bytes[record_offset + 67],
            {},
            read_u16_le(bytes.data() + record_offset + 68),
            attributes,
            source_name(attributes),
            (attributes & 0x02U) != 0,
            0,
            std::move(source_text),
            false,
            std::move(parameters),
            std::move(outputs),
            "user-file-decrypted",
        });
    }
    if (payload_cursor > payload.size() ||
        payload.size() - payload_cursor >= 8)
        throw Error("PriGS.dat payload padding is outside the native range");

    if (native_metadata) {
        *native_metadata = Json::object();
        (*native_metadata)["file_version"] = static_cast<int>(version);
        (*native_metadata)["formula_count"] =
            static_cast<std::uint64_t>(count);
        (*native_metadata)["kind_counts"] = counts_json(counts);
        (*native_metadata)["header_size_bytes"] =
            static_cast<std::uint64_t>(kHeaderSize);
        (*native_metadata)["index_record_size_bytes"] =
            static_cast<std::uint64_t>(kIndexSize);
        (*native_metadata)["formula_record_size_bytes"] =
            static_cast<std::uint64_t>(record_size);
        (*native_metadata)["records_offset"] =
            static_cast<std::uint64_t>(records_offset);
        (*native_metadata)["payload_offset"] =
            static_cast<std::uint64_t>(payload_offset);
        (*native_metadata)["encrypted_payload_bytes"] =
            static_cast<std::uint64_t>(payload_size);
        (*native_metadata)["decoded_dynamic_bytes"] =
            static_cast<std::uint64_t>(payload_cursor);
        (*native_metadata)["payload_padding_bytes"] =
            static_cast<std::uint64_t>(payload.size() - payload_cursor);
        (*native_metadata)["selected_source_bytes"] =
            static_cast<std::uint64_t>(selected_source_bytes);
        (*native_metadata)["cipher"] =
            "Blowfish-ECB initial P/S state, little-endian words";
        (*native_metadata)["read_only"] = true;
        (*native_metadata)["network_requests"] = 0;
    }
    return formulas;
}

}  // namespace tdx::formulas_detail

namespace tdx {

Json extract_user_formulas_document(const std::filesystem::path& pri_gs,
                                    const std::string& kind) {
    using namespace formulas_detail;
    Json native = Json::object();
    auto formulas = extract_user_formula_data(
        pri_gs, selected_kinds(lower_ascii(kind)), &native);
    Json document = Json::object();
    document["schema"] = "tdx-formula-user-library-v1";
    document["schema_version"] = 4;
    document["source_file"] = path_utf8(pri_gs.filename());
    document["sha256"] = sha256_file(pri_gs);
    document["profile"] = "tdx-prigs-v4-v5";
    document["private_user_data"] = true;
    document["native"] = std::move(native);
    Json counts = Json::object();
    for (const auto& definition : formula_kind_definitions()) {
        const auto count_value = std::count_if(
            formulas.begin(), formulas.end(), [&](const Formula& item) {
                return item.kind == definition.id;
            });
        if (count_value)
            counts[std::string(definition.key)] =
                static_cast<std::uint64_t>(count_value);
    }
    document["counts"] = std::move(counts);
    const auto source_count = std::count_if(
        formulas.begin(), formulas.end(),
        [](const Formula& item) { return !item.source_text.empty(); });
    document["source_text_count"] =
        static_cast<std::uint64_t>(source_count);
    document["embedded_source_text_count"] = 0;
    document["recovered_source_text_count"] = 0;
    document["user_file_source_text_count"] =
        static_cast<std::uint64_t>(source_count);
    document["source_text_missing_count"] =
        static_cast<std::uint64_t>(formulas.size() - source_count);
    document["categories"] = Json::object();
    Json rows = Json::array();
    for (const auto& item : formulas) rows.push_back(formula_json(item));
    document["formulas"] = std::move(rows);
    return document;
}

Json merge_user_formula_library_document(
    Json system, const std::filesystem::path& pri_gs,
    const std::string& kind) {
    if (!std::filesystem::is_regular_file(pri_gs)) {
        system["schema"] = "tdx-formula-installed-library-v1";
        system["private_user_data"] = true;
        system["user_file_source_text_count"] = 0;
        system["user_library"] = Json::object();
        system["user_library"]["source_file"] = path_utf8(pri_gs);
        system["user_library"]["file_exists"] = false;
        system["user_library"]["formula_count"] = 0;
        system["user_library"]["read_only"] = true;
        system["user_library"]["network_requests"] = 0;
        return system;
    }
    return formulas_detail::merge_installed_library(
        std::move(system), extract_user_formulas_document(pri_gs, kind));
}

}  // namespace tdx
