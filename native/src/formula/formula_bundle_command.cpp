#include "tdx/formulas.hpp"

#include "tdx/common.hpp"

#include <filesystem>
#include <iostream>
#include <sstream>

namespace fs = std::filesystem;

namespace tdx {
namespace {

fs::path path_from_utf8(const std::string& value) { return fs::u8path(value); }

std::string csv_quote(const std::string& value) {
    if (value.find_first_of(",\"\r\n") == std::string::npos) return value;
    std::string result = "\"";
    for (const char ch : value) result += ch == '\"' ? "\"\"" : std::string(1, ch);
    return result + '\"';
}

std::string bundled_formula_csv(const Json& library) {
    std::ostringstream output;
    output << "kind,kind_key,kind_name,index,code,name,category_id,category_name,"
              "display_flags,attribute_flags,source,is_custom,source_text_available,"
              "source_rva,source_text\n";
    for (const auto& formula : library.at("formulas").as_array()) {
        output << static_cast<int>(formula.at("kind").as_number()) << ','
               << formula.at("kind_key").as_string() << ','
               << csv_quote(formula.at("kind_name").as_string()) << ','
               << static_cast<std::uint64_t>(formula.at("index").as_number()) << ','
               << csv_quote(formula.at("code").as_string()) << ','
               << csv_quote(formula.at("name").as_string()) << ','
               << static_cast<int>(formula.at("category_id").as_number()) << ','
               << csv_quote(formula.at("category_name").as_string()) << ','
               << static_cast<std::uint64_t>(formula.at("display_flags").as_number()) << ','
               << static_cast<std::uint64_t>(formula.at("attribute_flags").as_number()) << ','
               << formula.at("source").as_string() << ','
               << (formula.at("is_custom").as_bool() ? "true" : "false") << ','
               << (formula.at("source_text_available").as_bool() ? "true" : "false")
               << ',';
        if (!formula.at("source_rva").is_null())
            output << static_cast<std::uint64_t>(formula.at("source_rva").as_number());
        output << ',' << csv_quote(formula.at("source_text").as_string()) << '\n';
    }
    return output.str();
}

}  // namespace

int command_formulas_extract(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout <<
            "Usage: tdx-tool formulas extract [--bundle DIR] [options]\n\n"
            "Export the verified standalone formula snapshot. No DLL is read or loaded.\n\n"
            "Options:\n"
            "  --kind technical|selection|expert|color-k|all (default technical)\n"
            "  --format json|csv       Output format (default json)\n"
            "  --output PATH           Write to file instead of stdout\n"
            "  --compact               Compact JSON\n";
        return 0;
    }
    const auto bundle_text = args.take_option("--bundle");
    const auto kind = lower_ascii(trim(args.take_option("--kind", "technical")));
    const auto format = lower_ascii(trim(args.take_option("--format", "json")));
    const auto output_text = args.take_option("--output");
    const bool compact = args.take_flag("--compact");
    args.require_empty();
    if (format != "json" && format != "csv")
        throw Error("--format must be json or csv");
    const auto library = load_bundled_formula_library_document(
        bundle_text.empty() ? fs::path{} : path_from_utf8(bundle_text), kind);
    auto report = format == "csv"
        ? bundled_formula_csv(library)
        : library.dump(compact ? -1 : 2) + "\n";
    if (format == "csv" && !output_text.empty())
        report = std::string("\xEF\xBB\xBF") + report;
    if (output_text.empty()) std::cout << report;
    else {
        const auto output = path_from_utf8(output_text);
        atomic_write_text(output, report);
        std::cout << "exported " << library.at("formulas").size()
                  << " bundled formulas -> " << path_utf8(output) << '\n';
    }
    return 0;
}

int command_formulas_icons(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout <<
            "Usage: tdx-tool formulas icons [--bundle DIR] [options]\n\n"
            "Export the verified bundled DRAWICON sprite. No DLL is read or loaded.\n\n"
            "Options:\n"
            "  --output PATH           Write the transparent PNG sprite\n"
            "  --bmp-output PATH       Also write the byte-exact BMP resource\n"
            "  --manifest PATH         Write metadata JSON instead of stdout\n"
            "  --compact               Compact JSON\n";
        return 0;
    }
    const auto bundle_text = args.take_option("--bundle");
    const auto output_text = args.take_option("--output");
    const auto bitmap_output_text = args.take_option("--bmp-output");
    const auto manifest_text = args.take_option("--manifest");
    const bool compact = args.take_flag("--compact");
    args.require_empty();
    auto sprite = load_bundled_formula_icon_sprite(
        bundle_text.empty() ? fs::path{} : path_from_utf8(bundle_text));
    if (!output_text.empty()) {
        const auto output = path_from_utf8(output_text);
        atomic_write_bytes(output, sprite.png);
        sprite.manifest["png_output"] = path_utf8(output);
    }
    if (!bitmap_output_text.empty()) {
        const auto output = path_from_utf8(bitmap_output_text);
        atomic_write_bytes(output, sprite.bitmap);
        sprite.manifest["bitmap_output"] = path_utf8(output);
    }
    const auto report = sprite.manifest.dump(compact ? -1 : 2) + "\n";
    if (manifest_text.empty()) std::cout << report;
    else {
        const auto output = path_from_utf8(manifest_text);
        atomic_write_text(output, report);
        std::cout << "exported bundled DRAWICON manifest -> "
                  << path_utf8(output) << '\n';
    }
    return 0;
}

}  // namespace tdx
