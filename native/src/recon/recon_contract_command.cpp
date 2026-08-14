#include "tdx/recon.hpp"

#include "recon_contract_internal.hpp"
#include "tdx/common.hpp"

#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

namespace tdx {
namespace {

int bounded_integer(const std::string& raw, std::string_view name,
                    int minimum, int maximum) {
    try {
        std::size_t used = 0;
        const int value = std::stoi(raw, &used);
        if (used != raw.size() || value < minimum || value > maximum)
            throw std::invalid_argument("range");
        return value;
    } catch (...) {
        throw Error(std::string(name) + " must be in " + std::to_string(minimum) +
                    ".." + std::to_string(maximum));
    }
}

void print_api_contract_help() {
    std::cout <<
        "Usage: tdx-tool recon api-contracts [options]\n\n"
        "  --base-url URL          Default http://127.0.0.1:8765\n"
        "  --profile quick|full    Default full\n"
        "  --case ID               Repeat to select individual contracts\n"
        "  --timeout-ms N          Per-request timeout, default 15000\n"
        "  --output FILE           Write JSON report instead of stdout\n"
        "  --compact               Compact JSON output\n\n"
        "Cases (generated from the contract catalog):\n";
    for (const auto& spec : recon_contract_detail::api_contract_specs())
        std::cout << "  " << spec.id << '\n';
}

}  // namespace

int command_recon_api_contracts(const std::vector<std::string>& values) {
    Args args(values);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        print_api_contract_help();
        return 0;
    }
    const auto base_url = args.take_option("--base-url", "http://127.0.0.1:8765");
    const auto profile = args.take_option("--profile", "full");
    const auto cases = args.take_options("--case");
    const int timeout_ms = bounded_integer(args.take_option("--timeout-ms", "15000"),
                                           "--timeout-ms", 100, 60000);
    const auto output_name = args.take_option("--output");
    const bool compact = args.take_flag("--compact");
    args.require_empty();
    const auto report = run_api_contract_audit(base_url, profile, cases, timeout_ms);
    const auto rendered = report.dump(compact ? -1 : 2) + '\n';
    if (output_name.empty()) {
        std::cout << rendered;
    } else {
        const auto output = fs::u8path(output_name);
        atomic_write_text(output, rendered);
        std::cout << "API contracts: " << report.at("summary").at("passed").as_number()
                  << '/' << report.at("summary").at("total").as_number()
                  << " passed -> " << path_utf8(output) << '\n';
    }
    return report.at("ok").as_bool() ? 0 : 1;
}

}  // namespace tdx
