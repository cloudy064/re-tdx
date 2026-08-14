#include "tdx/formula_engine.hpp"

#include "formula_engine_support_internal.hpp"

#include "tdx/common.hpp"

#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>
#include <system_error>
#include <vector>

namespace fs = std::filesystem;

namespace tdx {
namespace {

constexpr std::uintmax_t maximum_context_import_input_bytes =
    32ULL * 1024ULL * 1024ULL;

Json read_bounded_json(const std::string& value, const std::string& label) {
    const auto path = formula_engine_support::formula_path_from_utf8(value);
    std::error_code error;
    if (!fs::is_regular_file(path, error) || error)
        throw Error(label + " must name a regular file");
    const auto size = fs::file_size(path, error);
    if (error) throw Error("cannot inspect " + label);
    if (size > maximum_context_import_input_bytes)
        throw Error(label + " exceeds the 32 MiB safety limit");
    return Json::parse(read_text_utf8(path));
}

fs::path normalized_path(const std::string& value) {
    if (value.empty()) return {};
    std::error_code error;
    auto path = fs::absolute(
        formula_engine_support::formula_path_from_utf8(value), error);
    if (error) throw Error("cannot resolve output path");
    return path.lexically_normal();
}

}  // namespace

int command_formulas_context_import(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout
            << "Usage: tdx-tool formulas context-import --template FILE --capture FILE [options]\n"
               "  --template FILE      Pristine tdx-formula-explicit-context-v1 template\n"
               "  --capture FILE       Caller-owned tdx-formula-caller-context-capture-v1 JSON\n"
               "  --allow-partial      Emit diagnostics for absent scalars/points instead of rejecting\n"
               "  --output FILE        Write materialized context atomically; default stdout\n"
               "  --compact            Emit compact JSON\n"
               "The command performs exact stamp alignment only. It does not read a TDX process,\n"
               "call an SDK, subscribe, access an account, send an order, or bypass authorization.\n";
        return 0;
    }
    const auto template_file = args.take_option("--template");
    const auto capture_file = args.take_option("--capture");
    const auto output_file = args.take_option("--output");
    const bool allow_partial = args.take_flag("--allow-partial");
    const bool compact = args.take_flag("--compact");
    args.require_empty();
    if (template_file.empty()) throw Error("--template is required");
    if (capture_file.empty()) throw Error("--capture is required");
    if (!output_file.empty()) {
        const auto output = normalized_path(output_file);
        if (output == normalized_path(template_file) ||
            output == normalized_path(capture_file))
            throw Error("--output must not overwrite the template or capture input");
    }
    FormulaExplicitContextCaptureImportRequest request;
    request.context_template = read_bounded_json(template_file, "--template");
    request.capture = read_bounded_json(capture_file, "--capture");
    request.allow_partial = allow_partial;
    const auto result = import_formula_explicit_context_capture(request);
    const auto text = result.dump(compact ? -1 : 2) + "\n";
    if (output_file.empty()) std::cout << text;
    else atomic_write_text(
        formula_engine_support::formula_path_from_utf8(output_file), text);
    return 0;
}

}  // namespace tdx
