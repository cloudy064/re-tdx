#include "tdx/formulas.hpp"

#include "tdx/common.hpp"

#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace tdx {
namespace {

constexpr std::uintmax_t maximum_formula_bundle_json_bytes = 2U * 1024U * 1024U;
constexpr std::uintmax_t maximum_formula_bundle_binary_bytes = 2U * 1024U * 1024U;

const Json* optional_member(const Json& object, const std::string& key) {
    if (!object.is_object()) return nullptr;
    const auto found = object.as_object().find(key);
    return found == object.as_object().end() ? nullptr : &found->second;
}

std::string required_string(const Json& object, const std::string& key) {
    const auto* value = optional_member(object, key);
    if (!value || !value->is_string())
        throw Error("formula bundle manifest requires string " + key);
    return value->as_string();
}

std::uint64_t required_count(const Json& object, const std::string& key) {
    const auto* value = optional_member(object, key);
    if (!value || !value->is_number() || value->as_number() < 0.0)
        throw Error("formula bundle manifest requires non-negative " + key);
    return static_cast<std::uint64_t>(value->as_number());
}

fs::path safe_asset_path(const fs::path& directory, const std::string& name) {
    const auto relative = fs::u8path(name);
    if (relative.empty() || relative.is_absolute() || relative.has_parent_path())
        throw Error("formula bundle asset name must be a plain file name");
    return directory / relative;
}

void require_regular_file(const fs::path& path, std::uintmax_t maximum) {
    std::error_code error;
    if (!fs::is_regular_file(path, error) || error)
        throw Error("formula bundle asset does not exist: " + path_utf8(path));
    const auto size = fs::file_size(path, error);
    if (error || size > maximum)
        throw Error("formula bundle asset exceeds its safe size: " + path_utf8(path));
}

void require_sha256(const fs::path& path, const std::string& expected) {
    const auto actual = lower_ascii(sha256_file(path));
    if (actual != lower_ascii(expected))
        throw Error("formula bundle asset SHA-256 mismatch: " + path_utf8(path));
}

Json load_bundle_manifest(const fs::path& directory) {
    const auto path = directory / "bundle-manifest.json";
    require_regular_file(path, maximum_formula_bundle_json_bytes);
    auto manifest = Json::parse(read_text_utf8(path));
    if (!manifest.is_object() ||
        required_string(manifest, "schema") != "tdx-formula-bundle-v1")
        throw Error("formula bundle manifest schema is unsupported");
    return manifest;
}

void select_formula_kind(Json& library, const std::string& requested_kind) {
    const auto kind = lower_ascii(trim(requested_kind));
    if (kind == "all") return;
    if (kind != "technical" && kind != "selection" && kind != "expert" &&
        kind != "color-k")
        throw Error("unknown formula kind: " + requested_kind);

    Json selected = Json::array();
    std::uint64_t source_count = 0;
    std::uint64_t embedded_count = 0;
    std::uint64_t recovered_count = 0;
    for (const auto& formula : library.at("formulas").as_array()) {
        if (!formula.is_object() || !formula.as_object().count("kind_key") ||
            formula.at("kind_key").as_string() != kind)
            continue;
        if (formula.as_object().count("source_text_available") &&
            formula.at("source_text_available").as_bool()) {
            ++source_count;
            if (formula.as_object().count("source_text_origin")) {
                const auto& origin = formula.at("source_text_origin").as_string();
                if (origin == "embedded") ++embedded_count;
                else if (origin == "native-recovered") ++recovered_count;
            }
        }
        selected.push_back(formula);
    }
    library["formulas"] = std::move(selected);
    library["counts"] = Json::object();
    library["counts"][kind] =
        static_cast<std::uint64_t>(library.at("formulas").size());
    library["source_text_count"] = source_count;
    library["source_text_missing_count"] =
        static_cast<std::uint64_t>(library.at("formulas").size()) - source_count;
    library["embedded_source_text_count"] = embedded_count;
    library["recovered_source_text_count"] = recovered_count;
    if (library.as_object().count("categories") &&
        library.at("categories").is_object()) {
        Json categories = Json::object();
        const auto found = library.at("categories").as_object().find(kind);
        if (found != library.at("categories").as_object().end())
            categories[kind] = found->second;
        library["categories"] = std::move(categories);
    }
}

}  // namespace

fs::path locate_formula_bundle(const fs::path& explicit_directory) {
    if (!explicit_directory.empty()) {
        std::error_code error;
        if (fs::is_regular_file(
                explicit_directory / "bundle-manifest.json", error) && !error)
            return fs::weakly_canonical(explicit_directory);
        throw Error("formula bundle directory is invalid: " +
                    path_utf8(explicit_directory));
    }
    std::vector<fs::path> candidates;
    if (const char* environment = std::getenv("TDX_FORMULA_ASSETS");
        environment && *environment)
        candidates.push_back(fs::u8path(environment));
    const auto executable = running_executable_path();
    if (!executable.empty()) {
        candidates.push_back(executable.parent_path() / "formula-assets");
        candidates.push_back(executable.parent_path().parent_path() /
                             "share" / "tdx-tool" / "formula-assets");
    }
    candidates.push_back(fs::path("formula-assets"));
    candidates.push_back(fs::path("native") / "resources" / "formula");
#ifdef TDX_FORMULA_BUNDLE_SOURCE_DIR
    candidates.emplace_back(TDX_FORMULA_BUNDLE_SOURCE_DIR);
#endif
    for (const auto& candidate : candidates) {
        std::error_code error;
        if (fs::is_regular_file(candidate / "bundle-manifest.json", error) &&
            !error)
            return fs::weakly_canonical(candidate);
    }
    throw Error("cannot locate formula-assets/bundle-manifest.json; set "
                "TDX_FORMULA_ASSETS or install the bundled formula assets");
}

Json load_bundled_formula_library_document(
    const fs::path& bundle_directory, const std::string& kind) {
    const auto directory = locate_formula_bundle(bundle_directory);
    const auto manifest = load_bundle_manifest(directory);
    const auto& library_spec = manifest.at("library");
    if (!library_spec.is_object())
        throw Error("formula bundle library entry must be an object");
    const auto path = safe_asset_path(
        directory, required_string(library_spec, "file"));
    require_regular_file(path, maximum_formula_bundle_json_bytes);
    require_sha256(path, required_string(library_spec, "sha256"));
    auto library = Json::parse(read_text_utf8(path));
    if (!library.is_object()) throw Error("bundled formula library must be an object");
    const auto* formulas = optional_member(library, "formulas");
    if (!formulas || !formulas->is_array() ||
        formulas->size() != required_count(library_spec, "formula_count"))
        throw Error("bundled formula count does not match its manifest");
    const auto* source_count = optional_member(library, "source_text_count");
    if (!source_count || !source_count->is_number() ||
        static_cast<std::uint64_t>(source_count->as_number()) !=
            required_count(library_spec, "source_text_count"))
        throw Error("bundled source-text count does not match its manifest");
    select_formula_kind(library, kind);
    library["runtime_library_origin"] = "bundled-snapshot";
    library["runtime_library_file"] = path.filename().string();
    library["runtime_bundle_version"] = required_string(manifest, "version");
    library["runtime_bundle_sha256"] = lower_ascii(
        required_string(library_spec, "sha256"));
    library["runtime_dll_accessed"] = false;
    library["runtime_dll_loaded"] = false;
    library["runtime_network_requests"] = 0;
    return library;
}

Json load_bundled_installed_formula_library_document(
    const fs::path& root, bool include_user, const fs::path& bundle_directory) {
    auto library = load_bundled_formula_library_document(bundle_directory);
    if (!include_user) return library;
    if (root.empty())
        throw Error("including user formulas requires a TDX root for PriGS.dat");
    return merge_user_formula_library_document(
        std::move(library), root / "T0002" / "PriGS.dat", "all");
}

FormulaIconSprite load_bundled_formula_icon_sprite(
    const fs::path& bundle_directory) {
    const auto directory = locate_formula_bundle(bundle_directory);
    const auto bundle = load_bundle_manifest(directory);
    const auto& icons = bundle.at("icons");
    if (!icons.is_object())
        throw Error("formula bundle icons entry must be an object");
    const auto manifest_path = safe_asset_path(
        directory, required_string(icons, "manifest_file"));
    const auto png_path = safe_asset_path(
        directory, required_string(icons, "png_file"));
    const auto bitmap_path = safe_asset_path(
        directory, required_string(icons, "bitmap_file"));
    require_regular_file(manifest_path, maximum_formula_bundle_json_bytes);
    require_regular_file(png_path, maximum_formula_bundle_binary_bytes);
    require_regular_file(bitmap_path, maximum_formula_bundle_binary_bytes);
    require_sha256(manifest_path, required_string(icons, "manifest_sha256"));
    require_sha256(png_path, required_string(icons, "png_sha256"));
    require_sha256(bitmap_path, required_string(icons, "bitmap_sha256"));
    auto manifest = Json::parse(read_text_utf8(manifest_path));
    if (!manifest.is_object() ||
        required_string(manifest, "schema") != "tdx-formula-icon-sprite-v1")
        throw Error("bundled formula icon manifest schema is unsupported");
    auto png = read_bytes(png_path);
    auto bitmap = read_bytes(bitmap_path);
    if (static_cast<std::uint64_t>(png.size()) !=
            required_count(manifest, "png_bytes") ||
        static_cast<std::uint64_t>(bitmap.size()) !=
            required_count(manifest, "bitmap_bytes"))
        throw Error("bundled formula icon byte counts do not match the manifest");
    manifest["png_output"] = Json(nullptr);
    manifest["bitmap_output"] = Json(nullptr);
    manifest["runtime_asset_origin"] = "bundled-snapshot";
    manifest["runtime_bundle_version"] = required_string(bundle, "version");
    manifest["runtime_dll_accessed"] = false;
    manifest["runtime_dll_loaded"] = false;
    manifest["runtime_network_requests"] = 0;
    return FormulaIconSprite{
        std::move(bitmap), std::move(png), std::move(manifest)};
}

}  // namespace tdx
