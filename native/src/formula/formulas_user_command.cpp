#include "tdx/common.hpp"
#include "tdx/formulas.hpp"

#include <filesystem>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

namespace tdx {

int command_formulas_user_library(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout <<
            "Usage: tdx-tool formulas user-library [--root TDX | --pri-gs FILE] [options]\n\n"
            "Read PriGS.dat without loading TCalc.dll or modifying the user directory.\n\n"
            "Options:\n"
            "  --scope user|combined   User formulas only or merge with the bundled system library\n"
            "  --bundle DIR            Standalone system-formula asset override\n"
            "  --kind technical|selection|expert|color-k|all (default all)\n"
            "  --output PATH           Write compatible formula-library JSON\n"
            "  --compact               Compact JSON\n";
        return 0;
    }
    const auto root_text = args.take_option("--root");
    const auto input_text = args.take_option("--pri-gs");
    const auto bundle_text = args.take_option("--bundle");
    const auto scope = lower_ascii(trim(args.take_option("--scope", "user")));
    const auto kind = lower_ascii(trim(args.take_option("--kind", "all")));
    const auto output_text = args.take_option("--output");
    const bool compact = args.take_flag("--compact");
    args.require_empty();
    if (scope != "user" && scope != "combined")
        throw Error("--scope must be user or combined");

    fs::path root;
    if (!root_text.empty() || input_text.empty())
        root = find_tdx_root(root_text.empty() ? fs::path{} : fs::u8path(root_text));
    const auto pri_gs = input_text.empty()
        ? root / "T0002" / "PriGS.dat"
        : fs::u8path(input_text);
    Json document;
    if (scope == "combined") {
        auto system = load_bundled_formula_library_document(
            bundle_text.empty() ? fs::path{} : fs::u8path(bundle_text), kind);
        document = merge_user_formula_library_document(
            std::move(system), pri_gs, kind);
    } else {
        document = extract_user_formulas_document(pri_gs, kind);
    }
    const auto report = document.dump(compact ? -1 : 2) + "\n";
    if (output_text.empty()) std::cout << report;
    else {
        const auto output = fs::u8path(output_text);
        atomic_write_text(output, report);
        std::cout << "extracted " << document.at("formulas").size()
                  << " formulas -> " << path_utf8(output) << '\n';
    }
    return 0;
}

}  // namespace tdx
