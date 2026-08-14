#include "formula_scan_internal.hpp"

namespace tdx {
using namespace formula_scan_detail;

int command_formulas_scan(const std::vector<std::string>& raw_args) {
    Args help_args(raw_args);
    if (help_args.take_flag("--help") || help_args.take_flag("-h")) {
        print_scan_help();
        return 0;
    }
    auto invocation = execute_formula_scan_once(raw_args, false);
    const auto text = invocation.result.dump(invocation.compact ? -1 : 2) + "\n";
    if (invocation.output.empty()) std::cout << text;
    else atomic_write_text(from_utf8(invocation.output), text);
    return 0;
}

}  // namespace tdx
