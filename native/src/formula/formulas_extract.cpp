#include "formulas_internal.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>

namespace tdx::formulas_detail {
namespace {

std::string decode_fixed(const Bytes& data, std::size_t offset,
                         std::size_t size) {
    if (offset > data.size() || size > data.size() - offset)
        throw Error("metadata string is out of bounds");
    const auto begin = data.begin() + static_cast<std::ptrdiff_t>(offset);
    const auto end_limit = begin + static_cast<std::ptrdiff_t>(size);
    const auto end = std::find(begin, end_limit, 0);
    return decode_gbk(Bytes(begin, end));
}

double read_f32(const Bytes& data, std::size_t offset) {
    if (offset > data.size() || 4 > data.size() - offset)
        throw Error("formula float metadata is out of bounds");
    const auto bits = read_u32_le(data.data() + offset);
    float value = 0.0F;
    static_assert(sizeof(value) == sizeof(bits));
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

std::string formula_source(std::uint32_t flags) {
    if (flags & 0x01) return "system";
    if (flags & 0x10) return "temporary";
    if (flags & 0x800) return "default";
    return "user";
}

std::map<int, std::vector<Category>> extract_categories(
    const PEImage& image, const Profile& profile) {
    std::map<int, std::vector<Category>> result;
    for (const auto& spec : profile.trees) {
        const auto count = image.u16(spec.count_rva);
        if (count != spec.expected_count)
            throw Error(profile.name + ": category count mismatch for kind " +
                        std::to_string(spec.kind));
        auto& categories = result[spec.kind];
        for (std::uint16_t index = 0; index < count; ++index) {
            const auto record = image.read_rva(
                spec.records_rva + index * 44U, 44);
            categories.push_back(Category{
                read_u32_le(record.data()), read_u32_le(record.data() + 4),
                decode_fixed(record, 8, 32),
                read_u32_le(record.data() + 40),
            });
        }
    }
    return result;
}

}  // namespace

ExtractedFormulaData extract_formula_data(
    const PEImage& image, const Profile& profile, const std::set<int>& kinds) {
    auto categories = extract_categories(image, profile);
    std::map<int, std::map<std::uint32_t, std::string>> names;
    for (const auto& [kind, items] : categories)
        for (const auto& item : items) names[kind][item.id] = item.name;
    std::vector<Formula> formulas;
    for (const auto& spec : profile.tables) {
        const auto count = image.u32(spec.count_rva);
        if (count != spec.expected_count)
            throw Error(profile.name + ": formula count mismatch for kind " +
                        std::to_string(spec.kind));
        if (!kinds.count(spec.kind)) continue;
        for (std::uint32_t index = 0; index < count; ++index) {
            const auto record = image.read_rva(
                spec.records_rva +
                    static_cast<std::uint32_t>(index * record_size),
                record_size);
            const auto code = decode_fixed(record, 3, 14);
            const auto name = decode_fixed(record, 17, 50);
            if (code.empty() || name.empty())
                throw Error("empty formula metadata");
            const auto category_id = record[67];
            const auto attributes = read_u32_le(record.data() + 5068);
            const auto source_pointer = read_u32_le(
                record.data() + source_pointer_offset);
            std::uint32_t source_rva = 0;
            std::string source_text;
            bool source_text_recovered = false;
            if (source_pointer) {
                if (source_pointer < image.image_base() ||
                    static_cast<std::uint64_t>(source_pointer) -
                            image.image_base() >
                        UINT32_MAX)
                    throw Error("formula source pointer is outside the preferred PE image");
                source_rva = static_cast<std::uint32_t>(
                    static_cast<std::uint64_t>(source_pointer) -
                    image.image_base());
                source_text = image.decode_va_cstring(source_pointer);
            } else {
                source_text = recovered_native_source(code);
                source_text_recovered = !source_text.empty();
            }
            const auto parameter_count = static_cast<std::size_t>(record[72]);
            if (parameter_count > 16)
                throw Error("formula parameter count exceeds 16 slots");
            std::vector<FormulaParameter> parameters;
            parameters.reserve(parameter_count);
            for (std::size_t slot = 0; slot < parameter_count; ++slot) {
                const auto offset = 73 + slot * 132;
                parameters.push_back(FormulaParameter{
                    decode_fixed(record, offset, 16),
                    read_f32(record, offset + 16),
                    read_f32(record, offset + 20),
                    read_f32(record, offset + 24),
                    read_f32(record, offset + 28),
                    read_f32(record, offset + 32),
                });
            }
            const auto output_count = static_cast<std::size_t>(record[2185]);
            if (output_count > 64)
                throw Error("formula output count exceeds safe slot limit");
            std::vector<std::string> outputs;
            outputs.reserve(output_count);
            for (std::size_t slot = 0; slot < output_count; ++slot)
                outputs.push_back(decode_fixed(record, 2186 + slot * 28, 16));
            formulas.push_back(Formula{
                spec.kind, formula_kind_key(spec.kind),
                formula_kind_name(spec.kind), index, code, name, category_id,
                names[spec.kind][category_id],
                read_u16_le(record.data() + 68), attributes,
                formula_source(attributes), (attributes & 0x02) != 0,
                source_rva, std::move(source_text), source_text_recovered,
                std::move(parameters), std::move(outputs), {},
            });
        }
    }
    return {std::move(categories), std::move(formulas)};
}

}  // namespace tdx::formulas_detail

namespace tdx {

Json extract_formulas_document(const std::filesystem::path& dll,
                               const std::string& kind) {
    using namespace formulas_detail;
    const auto digest = validate_formula_dll(dll);
    const auto& profile = supported_profile();
    PEImage image(read_bytes(dll));
    auto [categories, formulas] = extract_formula_data(
        image, profile, selected_kinds(lower_ascii(kind)));
    Json document = Json::object();
    document["schema_version"] = 4;
    document["source_file"] = path_utf8(dll.filename());
    document["sha256"] = digest;
    document["profile"] = profile.name;
    Json counts = Json::object();
    for (const auto& definition : formula_kind_definitions()) {
        const auto count = std::count_if(
            formulas.begin(), formulas.end(), [&](const Formula& item) {
                return item.kind == definition.id;
            });
        if (count) counts[std::string(definition.key)] =
            static_cast<std::uint64_t>(count);
    }
    document["counts"] = std::move(counts);
    const auto source_text_count = std::count_if(
        formulas.begin(), formulas.end(),
        [](const Formula& item) { return !item.source_text.empty(); });
    const auto embedded_count = std::count_if(
        formulas.begin(), formulas.end(),
        [](const Formula& item) { return item.source_rva != 0; });
    const auto recovered_count = std::count_if(
        formulas.begin(), formulas.end(),
        [](const Formula& item) { return item.source_text_recovered; });
    document["source_text_count"] =
        static_cast<std::uint64_t>(source_text_count);
    document["embedded_source_text_count"] =
        static_cast<std::uint64_t>(embedded_count);
    document["recovered_source_text_count"] =
        static_cast<std::uint64_t>(recovered_count);
    document["source_text_missing_count"] =
        static_cast<std::uint64_t>(formulas.size() - source_text_count);
    Json category_groups = Json::object();
    for (const auto& [id, items] : categories) {
        if (!std::any_of(formulas.begin(), formulas.end(),
                         [&](const Formula& item) { return item.kind == id; }))
            continue;
        Json group = Json::array();
        for (const auto& item : items) group.push_back(category_json(item));
        category_groups[formula_kind_key(id)] = std::move(group);
    }
    document["categories"] = std::move(category_groups);
    Json rows = Json::array();
    for (const auto& item : formulas) rows.push_back(formula_json(item));
    document["formulas"] = std::move(rows);
    return document;
}

}  // namespace tdx
