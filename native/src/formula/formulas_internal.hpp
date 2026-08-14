#pragma once

#include "tdx/formulas.hpp"
#include "tdx/formulas_extractor.hpp"

#include <array>
#include <cstdint>
#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace tdx::formulas_detail {

inline constexpr std::size_t record_size = 5072;
inline constexpr std::size_t source_pointer_offset = 5052;
inline constexpr std::size_t maximum_source_text_size = 64 * 1024;

struct FormulaKindDefinition {
    int id{};
    std::string_view key;
    std::string_view name;
};

const std::array<FormulaKindDefinition, 5>& formula_kind_definitions();
std::string formula_kind_key(int kind);
std::string formula_kind_name(int kind);
std::set<int> selected_kinds(const std::string& value);

struct Section {
    std::string name;
    std::uint32_t virtual_address{};
    std::uint32_t virtual_size{};
    std::uint32_t raw_offset{};
    std::uint32_t raw_size{};
};

struct TableSpec {
    int kind{};
    std::uint32_t records_rva{};
    std::uint32_t count_rva{};
    std::uint32_t expected_count{};
};

struct TreeSpec {
    int kind{};
    std::uint32_t records_rva{};
    std::uint32_t count_rva{};
    std::uint16_t expected_count{};
};

struct Profile {
    std::string name;
    std::string sha256;
    std::string description;
    std::vector<TableSpec> tables;
    std::vector<TreeSpec> trees;
};

struct Category {
    std::uint32_t id{};
    std::uint32_t parent{};
    std::string name;
    std::uint32_t flags{};
};

struct FormulaParameter {
    std::string name;
    double minimum{};
    double maximum{};
    double step{};
    double default_value{};
    double current_value{};
};

struct Formula {
    int kind{};
    std::string kind_key;
    std::string kind_name;
    std::uint32_t index{};
    std::string code;
    std::string name;
    std::uint8_t category_id{};
    std::string category_name;
    std::uint16_t display_flags{};
    std::uint32_t attribute_flags{};
    std::string source;
    bool is_custom{};
    std::uint32_t source_rva{};
    std::string source_text;
    bool source_text_recovered{};
    std::vector<FormulaParameter> parameters;
    std::vector<std::string> outputs;
    std::string source_text_origin;
};

const Profile& supported_profile();
std::string recovered_native_source(const std::string& code);

class PEImage {
public:
    explicit PEImage(Bytes bytes);

    Bytes read_rva(std::uint32_t rva, std::size_t size) const;
    std::uint16_t u16(std::uint32_t rva) const;
    std::uint32_t u32(std::uint32_t rva) const;
    std::string decode_va_cstring(
        std::uint32_t va,
        std::size_t maximum = maximum_source_text_size) const;
    std::size_t rva_to_offset(std::uint32_t rva, std::size_t size) const;
    Bytes read_resource(std::uint32_t type, std::uint32_t id) const;
    std::uint64_t image_base() const { return image_base_; }

private:
    struct ResourceEntry {
        bool directory{};
        std::uint32_t offset{};
    };

    ResourceEntry resource_entry_at(std::uint32_t directory_offset,
                                    std::size_t index) const;
    ResourceEntry numeric_resource_entry(std::uint32_t directory_offset,
                                         std::uint32_t wanted) const;
    ResourceEntry first_resource_entry(std::uint32_t directory_offset) const;
    Bytes slice(std::size_t offset, std::size_t size) const;
    std::uint16_t u16_at(std::size_t offset) const;
    std::uint32_t u32_at(std::size_t offset) const;
    std::uint64_t u64_at(std::size_t offset) const;

    Bytes data_;
    std::uint64_t image_base_{};
    std::uint32_t size_of_headers_{};
    std::uint32_t resource_rva_{};
    std::uint32_t resource_size_{};
    std::vector<Section> sections_;
};

std::string validate_formula_dll(const std::filesystem::path& dll);
Bytes bitmap_dib_to_transparent_png(const Bytes& dib);
Bytes bitmap_file_from_dib(const Bytes& dib);

using ExtractedFormulaData =
    std::pair<std::map<int, std::vector<Category>>, std::vector<Formula>>;

ExtractedFormulaData extract_formula_data(
    const PEImage& image, const Profile& profile, const std::set<int>& kinds);
std::vector<Formula> extract_user_formula_data(
    const std::filesystem::path& pri_gs, const std::set<int>& kinds,
    Json* native_metadata = nullptr);
Json category_json(const Category& item);
Json formula_json(const Formula& item);
std::string render_formula_csv(const std::vector<Formula>& formulas);
}  // namespace tdx::formulas_detail
