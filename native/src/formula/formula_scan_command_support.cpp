#include "formula_scan_internal.hpp"
#include "tdx/time.hpp"

namespace tdx::formula_scan_detail {
namespace fs = std::filesystem;

void print_scan_help() {
    std::cout <<
        "Usage: tdx-tool formulas scan --formula CODE [universe] [options]\n\n"
        "Universe (choose one):\n"
        "  --input PATH             K-line array/bundle or market-securities JSON\n"
        "  --securities LIST        Comma-separated sz000001,sh600000,...\n"
        "  --all                    Download the server A-share directory\n\n"
        "Options:\n"
        "  --library PATH           Formula JSON override; bundled snapshot is the default\n"
        "  --root TDX               Used for market/context data, not system formula loading\n"
        "  --include-user          Include root/T0002/PriGS.dat (CLI only)\n"
        "  --source-file PATH       Scan caller-supplied formula source instead of a library formula\n"
        "  --period day --pages 1 --page-size 800 --lookback 1\n"
        "  --workers 4 --cache-dir PATH --refresh --param NAME=NUMBER\n"
        "  --adjust none|qfq|hfq|fixed_qfq|fixed_hfq --anchor-date DATE\n"
        "  --adjust-cache-ttl-seconds 900 --refresh-adjustment\n"
        "  --point-in-time-finance  Use archived actual disclosure dates; no current fallback\n"
        "  --output PATH --compact  Write JSON instead of stdout / use compact JSON\n";
}

void print_formula_watch_help() {
    std::cout <<
        "Usage: tdx-tool formulas watch --formula CODE [universe] [scan options] [watch options]\n\n"
        "Continuously rerun a condition formula, persist active membership and emit JSONL diffs.\n"
        "The formula and universe options are identical to `formulas scan`.\n\n"
        "Watch options:\n"
        "  --interval-seconds N   Delay after each evaluation, 1..86400 (default 60)\n"
        "  --iterations N         Stop after N evaluations; 0 runs until interrupted (default 0)\n"
        "  --heartbeat            Emit unchanged evaluations after the initial snapshot/resume\n"
        "  --state-file PATH      Atomically persist active membership for restart diffing\n"
        "  --block-output PATH    Atomically write current matches as an explicit TDX .blk file\n"
        "  --output PATH          Write JSONL events to a newly truncated file instead of stdout\n";
}

std::string formula_watch_time_text() {
    const auto now = std::time(nullptr);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    std::ostringstream output;
    output << local_timestamp_text(local);
    return output.str();
}

std::string formula_watch_configuration_id(const std::vector<std::string>& scan_args,
                                           const Json& scan) {
    std::string material;
    for (const auto& argument : scan_args) {
        material += std::to_string(argument.size()) + ":" + argument + "\n";
    }
    if (const auto* digest = optional(scan, "formula_source_md5");
        digest && digest->is_string())
        material += "source=" + digest->as_string() + "\n";
    Bytes bytes(material.begin(), material.end());
    return md5_bytes(bytes);
}

std::string normalized_file_path(const std::string& value) {
    if (value.empty()) return {};
    return lower_ascii(path_utf8(fs::absolute(from_utf8(value)).lexically_normal()));
}

void require_distinct_formula_watch_paths(const std::vector<std::string>& scan_args,
                                          const std::string& output,
                                          const std::string& state_file,
                                          const std::string& block_output) {
    std::map<std::string, std::string> targets;
    for (const auto& [label, value] : std::vector<std::pair<std::string, std::string>>{
             {"--output", output}, {"--state-file", state_file},
             {"--block-output", block_output}}) {
        if (value.empty()) continue;
        const auto normalized = normalized_file_path(value);
        const auto inserted = targets.emplace(normalized, label);
        if (!inserted.second)
            throw Error(label + " must be different from " + inserted.first->second);
    }
    Args protected_args(scan_args);
    for (const auto* option : {"--input", "--source-file", "--library"}) {
        const auto value = protected_args.take_option(option);
        if (value.empty()) continue;
        const auto normalized = normalized_file_path(value);
        const auto found = targets.find(normalized);
        if (found != targets.end())
            throw Error(found->second + " must not overwrite " + std::string(option));
    }
}

}  // namespace tdx::formula_scan_detail
