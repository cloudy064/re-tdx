#include "tpool_internal.hpp"

#include "tdx/blocks.hpp"
#include "tdx/common.hpp"

#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace fs = std::filesystem;

namespace tdx {

namespace {

int bounded_history_integer(Args& args, std::string_view name, int fallback,
                            int minimum, int maximum) {
    const auto value = args.take_option(name, std::to_string(fallback));
    const int parsed = tpool_detail::integer_text(value, minimum - 1);
    if (parsed < minimum || parsed > maximum)
        throw Error(std::string(name) + " is outside the safe range");
    return parsed;
}

fs::path comparable_history_path(const fs::path& path) {
    std::error_code error;
    auto resolved = fs::weakly_canonical(path, error);
    if (!error) return resolved;
    error.clear();
    resolved = fs::absolute(path, error);
    return (error ? path : resolved).lexically_normal();
}

bool same_history_path(const fs::path& left, const fs::path& right) {
    return !left.empty() && !right.empty() &&
        comparable_history_path(left) == comparable_history_path(right);
}

void protect_history_sources(const Json& result, const fs::path& json_output,
                             const fs::path& text_output) {
    if (same_history_path(json_output, text_output))
        throw Error("pool history JSON and native text outputs must be different files");
    const auto* files = tpool_detail::optional(result, "files");
    if (!files || !files->is_array()) return;
    for (const auto& file : files->as_array()) {
        const auto* source = tpool_detail::optional(file, "source");
        if (!source || !source->is_string()) continue;
        const auto source_path = tpool_detail::from_utf8(source->as_string());
        if (same_history_path(source_path, json_output) ||
            same_history_path(source_path, text_output))
            throw Error("pool history output must not overwrite an input history file");
    }
}

void print_history_help() {
    std::cout <<
        "Usage: tdx-tool pool history [--input FILE | --root TDX] [options]\n\n"
        "Read original TPool daily .dat snapshots and .log entry histories without writeback.\n\n"
        "Options:\n"
        "  --input FILE           Inspect one .dat/.log/native *_his.txt file (repeatable)\n"
        "  --root PATH            Scan tpool/<pool>/<cell>/<YYYYMMDD>.dat|.log\n"
        "  --pool NAME            Restrict root scan to one pool directory\n"
        "  --cell NAME            Restrict root scan to one cell directory\n"
        "  --kind VALUE           all, snapshot or entry (default all)\n"
        "  --from YYYYMMDD        Inclusive history start date\n"
        "  --to YYYYMMDD          Inclusive history end date\n"
        "  --limit N              Maximum returned files, 1..10000 (default 1000)\n"
        "  --native-text-output P Write original TPool pipe-delimited history text\n"
        "  --native-text-encoding GBK or UTF-8 (default GBK)\n"
        "  --name-root PATH       TDX root used to resolve names for --input files\n"
        "  --output PATH          Write JSON to a file\n"
        "  --compact              Compact JSON output\n";
}

}  // namespace

int command_pool_history(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        print_history_help();
        return 0;
    }
    const auto inputs = args.take_options("--input");
    const auto root_text = args.take_option("--root");
    const auto pool = args.take_option("--pool");
    const auto cell = args.take_option("--cell");
    const auto kind = args.take_option("--kind", "all");
    const int from_date = bounded_history_integer(
        args, "--from", 0, 0, 99999999);
    const int to_date = bounded_history_integer(
        args, "--to", 0, 0, 99999999);
    const int limit = bounded_history_integer(
        args, "--limit", 1000, 1, 10000);
    const auto native_text_output = args.take_option("--native-text-output");
    const auto native_text_encoding = lower_ascii(
        args.take_option("--native-text-encoding", "gbk"));
    const auto name_root_text = args.take_option("--name-root");
    const auto output = args.take_option("--output");
    const bool compact = args.take_flag("--compact");
    args.require_empty();
    if (!inputs.empty() && !root_text.empty())
        throw Error("pool history accepts --input or --root, not both");
    if (!inputs.empty() && (!pool.empty() || !cell.empty() || kind != "all" ||
                           from_date || to_date || limit != 1000))
        throw Error("pool history filters apply only to --root scans");
    if (native_text_encoding != "gbk" && native_text_encoding != "utf-8" &&
        native_text_encoding != "utf8")
        throw Error("pool history native text encoding must be gbk or utf-8");
    if (native_text_output.empty() && !name_root_text.empty())
        throw Error("pool history --name-root requires --native-text-output");

    const auto output_path = output.empty()
        ? fs::path{} : tpool_detail::from_utf8(output);
    const auto native_text_path = native_text_output.empty()
        ? fs::path{} : tpool_detail::from_utf8(native_text_output);

    Json result;
    std::optional<fs::path> scanned_root;
    if (!inputs.empty()) {
        std::vector<fs::path> paths;
        paths.reserve(inputs.size());
        for (const auto& input : inputs)
            paths.push_back(tpool_detail::from_utf8(input));
        result = inspect_tpool_history_files_document(paths);
    } else {
        const auto root = find_tdx_root(
            root_text.empty() ? fs::path{} : tpool_detail::from_utf8(root_text));
        scanned_root = root;
        result = inspect_tpool_history_root_document(
            root, pool, cell, kind, from_date, to_date,
            static_cast<std::size_t>(limit));
    }
    protect_history_sources(result, output_path, native_text_path);
    if (!native_text_output.empty()) {
        std::optional<fs::path> name_root = scanned_root;
        if (!name_root_text.empty())
            name_root = find_tdx_root(tpool_detail::from_utf8(name_root_text));
        else if (!name_root) {
            try {
                name_root = find_tdx_root();
            } catch (const Error&) {
                // Name resolution is useful but not required for a structurally
                // faithful export. Unresolved names remain empty and are counted.
            }
        }
        tpool_detail::TpoolSecurityNames names;
        if (name_root) {
            for (const auto& [key, security] : load_blocks(*name_root, {}).securities)
                names.emplace(key, security.name);
        }
        const auto rendered =
            tpool_detail::render_tpool_history_native_text(result, names);
        Bytes bytes;
        const bool gbk = native_text_encoding == "gbk";
        if (gbk) bytes = encode_gbk(rendered.text_utf8);
        else bytes.assign(rendered.text_utf8.begin(), rendered.text_utf8.end());
        atomic_write_bytes(native_text_path, bytes);
        Json export_info = Json::object();
        export_info["path"] = path_utf8(native_text_path);
        export_info["kind"] = rendered.kind;
        export_info["encoding"] = gbk ? "GBK" : "UTF-8";
        export_info["original_encoding"] = "GBK";
        export_info["native_column_layout"] = true;
        export_info["row_count"] = static_cast<std::uint64_t>(rendered.row_count);
        export_info["resolved_name_count"] =
            static_cast<std::uint64_t>(rendered.resolved_name_count);
        export_info["unresolved_name_count"] = static_cast<std::uint64_t>(
            rendered.row_count - rendered.resolved_name_count);
        export_info["byte_size"] = static_cast<std::uint64_t>(bytes.size());
        export_info["sha256"] = lower_ascii(sha256_file(native_text_path));
        export_info["name_root"] = name_root
            ? Json(path_utf8(*name_root)) : Json(nullptr);
        export_info["evidence"] = rendered.kind == "daily-snapshot"
            ? "TPool.dll sub_1003AC40"
            : "TPool.dll sub_1003A720";
        result["native_text_export"] = std::move(export_info);
    }
    const auto report = result.dump(compact ? -1 : 2) + "\n";
    if (output.empty()) std::cout << report;
    else atomic_write_text(output_path, report);
    return 0;
}

}  // namespace tdx
