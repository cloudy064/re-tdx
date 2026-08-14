#include "formulas_internal.hpp"

#include "tdx/common.hpp"

#include <filesystem>
#include <iostream>
#include <sstream>

namespace fs = std::filesystem;

namespace tdx::formulas_detail {
namespace {

std::string csv_quote(const std::string& value) {
    if (value.find_first_of(",\"\r\n") == std::string::npos) return value;
    std::string result = "\"";
    for (const char ch : value)
        result += ch == '\"' ? "\"\"" : std::string(1, ch);
    return result + '\"';
}

void print_extract_help() {
    std::cout <<
        "Usage: tdx-formula-extractor extract [--dll TCalc.dll | --root TDX] [options]\n\n"
        "Offline native extraction of metadata, signatures, and recoverable source text;\n"
        "the DLL is parsed as a PE image and is never loaded.\n\n"
        "Options:\n"
        "  --kind technical|selection|expert|color-k|all (default technical)\n"
        "  --format json|csv       Output format (default json)\n"
        "  --output PATH           Write to file instead of stdout\n"
        "  --compact               Compact JSON\n";
}

void print_icons_help() {
    std::cout <<
        "Usage: tdx-formula-extractor icons [--dll TCalc.dll | --root TDX] [options]\n\n"
        "Extract TCalc's native DRAWICON sprite from PE resource RT_BITMAP/2060.\n"
        "The DLL is parsed offline and is never loaded.\n\n"
        "Options:\n"
        "  --output PATH           Write a browser-ready transparent PNG sprite\n"
        "  --bmp-output PATH       Also write the byte-exact bitmap resource as BMP\n"
        "  --manifest PATH         Write metadata JSON instead of stdout\n"
        "  --compact               Compact JSON\n";
}

}  // namespace

std::string render_formula_csv(const std::vector<Formula>& formulas) {
    std::ostringstream output;
    output << "kind,kind_key,kind_name,index,code,name,category_id,category_name,display_flags,"
              "attribute_flags,source,is_custom,source_text_available,source_rva,source_text\n";
    for (const auto& item : formulas) {
        output << item.kind << ',' << item.kind_key << ','
               << csv_quote(item.kind_name) << ',' << item.index << ','
               << csv_quote(item.code) << ',' << csv_quote(item.name) << ','
               << static_cast<int>(item.category_id) << ','
               << csv_quote(item.category_name) << ',' << item.display_flags << ','
               << item.attribute_flags << ',' << item.source << ','
               << (item.is_custom ? "true" : "false") << ','
               << (!item.source_text.empty() ? "true" : "false") << ',';
        if (item.source_rva) output << item.source_rva;
        output << ',' << csv_quote(item.source_text) << '\n';
    }
    return output.str();
}

fs::path formula_path_from_utf8(const std::string& value) {
#ifdef _WIN32
    return fs::path(utf8_to_wide(value));
#else
    return fs::path(value);
#endif
}

}  // namespace tdx::formulas_detail

namespace tdx {

int command_tcalc_formulas_extract(const std::vector<std::string>& raw_args) {
    using namespace formulas_detail;
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        print_extract_help();
        return 0;
    }
    const auto dll_text = args.take_option("--dll");
    const auto root_text = args.take_option("--root");
    const auto kind = lower_ascii(args.take_option("--kind", "technical"));
    const auto format = lower_ascii(args.take_option("--format", "json"));
    const auto output_text = args.take_option("--output");
    const bool compact = args.take_flag("--compact");
    args.require_empty();
    if (format != "json" && format != "csv")
        throw Error("--format must be json or csv");
    fs::path dll;
    if (!dll_text.empty()) {
        dll = formula_path_from_utf8(dll_text);
    } else {
        dll = find_tdx_root(root_text.empty() ? fs::path{} :
            formula_path_from_utf8(root_text)) / "TCalc.dll";
    }
    std::string report;
    std::size_t formula_count = 0;
    if (format == "csv") {
        validate_formula_dll(dll);
        PEImage image(read_bytes(dll));
        auto [categories, formulas] = extract_formula_data(
            image, supported_profile(), selected_kinds(kind));
        (void)categories;
        formula_count = formulas.size();
        report = render_formula_csv(formulas);
        if (!output_text.empty()) report = std::string("\xEF\xBB\xBF") + report;
    } else {
        auto document = extract_formulas_document(dll, kind);
        formula_count = document.at("formulas").size();
        report = document.dump(compact ? -1 : 2) + "\n";
    }
    if (output_text.empty()) {
        std::cout << report;
    } else {
        const auto output = formula_path_from_utf8(output_text);
        atomic_write_text(output, report);
        std::cout << "extracted " << formula_count << " formulas -> "
                  << path_utf8(output) << '\n';
    }
    return 0;
}

int command_tcalc_formulas_icons(const std::vector<std::string>& raw_args) {
    using namespace formulas_detail;
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        print_icons_help();
        return 0;
    }
    const auto dll_text = args.take_option("--dll");
    const auto root_text = args.take_option("--root");
    const auto output_text = args.take_option("--output");
    const auto bitmap_output_text = args.take_option("--bmp-output");
    const auto manifest_text = args.take_option("--manifest");
    const bool compact = args.take_flag("--compact");
    args.require_empty();
    fs::path dll;
    if (!dll_text.empty()) {
        dll = formula_path_from_utf8(dll_text);
    } else {
        dll = find_tdx_root(root_text.empty() ? fs::path{} :
            formula_path_from_utf8(root_text)) / "TCalc.dll";
    }
    auto sprite = extract_formula_icon_sprite(dll);
    if (!output_text.empty()) {
        const auto output = formula_path_from_utf8(output_text);
        atomic_write_bytes(output, sprite.png);
        sprite.manifest["png_output"] = path_utf8(output);
    } else {
        sprite.manifest["png_output"] = Json(nullptr);
    }
    if (!bitmap_output_text.empty()) {
        const auto output = formula_path_from_utf8(bitmap_output_text);
        atomic_write_bytes(output, sprite.bitmap);
        sprite.manifest["bitmap_output"] = path_utf8(output);
    } else {
        sprite.manifest["bitmap_output"] = Json(nullptr);
    }
    const auto report = sprite.manifest.dump(compact ? -1 : 2) + "\n";
    if (manifest_text.empty()) {
        std::cout << report;
    } else {
        const auto output = formula_path_from_utf8(manifest_text);
        atomic_write_text(output, report);
        std::cout << "extracted "
                  << sprite.manifest.at("cell_count").as_number()
                  << " DRAWICON cells -> " << path_utf8(output) << '\n';
    }
    return 0;
}

}  // namespace tdx
