#include "tdx/recon.hpp"

#include "tdx/common.hpp"
#include "tdx/session_audit.hpp"

#include <filesystem>
#include <chrono>
#include <iostream>
#include <limits>
#include <optional>
#include <string>
#include <thread>

namespace fs = std::filesystem;

namespace tdx {
namespace {

std::uint32_t parse_pid(const std::string& text) {
    if (text.empty()) return 0;
    std::size_t consumed = 0;
    unsigned long long value = 0;
    try {
        value = std::stoull(text, &consumed, 10);
    } catch (const std::exception&) {
        throw Error("pid must be an unsigned integer");
    }
    if (consumed != text.size() || value > std::numeric_limits<std::uint32_t>::max())
        throw Error("pid is outside the uint32 range");
    return static_cast<std::uint32_t>(value);
}

std::uint64_t parse_unsigned(const std::string& text, std::string_view name,
                             std::uint64_t fallback, std::uint64_t minimum,
                             std::uint64_t maximum) {
    if (text.empty()) return fallback;
    std::size_t consumed = 0;
    unsigned long long value = 0;
    try {
        value = std::stoull(text, &consumed, 10);
    } catch (const std::exception&) {
        throw Error(std::string(name) + " must be an unsigned integer");
    }
    if (consumed != text.size() || value < minimum || value > maximum)
        throw Error(std::string(name) + " must be " + std::to_string(minimum) + ".." +
                    std::to_string(maximum));
    return static_cast<std::uint64_t>(value);
}

bool same_path(const fs::path& left, const fs::path& right) {
    const auto normalize = [](const fs::path& value) {
        return lower_ascii(path_utf8(fs::absolute(value).lexically_normal()));
    };
    return normalize(left) == normalize(right);
}

fs::path correlation_root(const std::string& root_name) {
    if (root_name.empty()) return find_tdx_root({});
    std::error_code error;
    const auto requested = fs::u8path(root_name);
    const auto canonical = fs::weakly_canonical(requested, error);
    const auto& root = error ? requested : canonical;
    if (!fs::is_directory(root / "T0002") ||
        (!fs::is_regular_file(root / "TdxW.exe") &&
         !fs::is_regular_file(root / "tdxw.exe")))
        throw Error("explicit endpoint-correlation root is not a TDX installation: " +
                    path_utf8(requested));
    return root;
}

const Json& topology_snapshot(const Json& document) {
    const auto schema = document.at("schema").as_string();
    if (schema == "tdx-runtime-topology-v1") return document;
    if (schema == "tdx-runtime-topology-watch-v1")
        return document.at("final_snapshot");
    throw Error("runtime topology baseline must be a snapshot or watch document");
}

Json capture_snapshot(const RuntimeTopologyOptions& options,
                      const std::optional<Json>& session_config) {
    auto snapshot = runtime_topology_document(options);
    if (session_config)
        snapshot = annotate_runtime_topology_endpoints(std::move(snapshot), *session_config);
    return snapshot;
}

Json capture_watch(const RuntimeTopologyOptions& options,
                   const std::optional<Json>& session_config,
                   std::uint64_t duration_ms, std::uint64_t interval_ms) {
    const auto sample_intervals = (duration_ms + interval_ms - 1) / interval_ms;
    if (sample_intervals > 20000)
        throw Error("runtime topology watch would exceed 20001 samples");
    std::vector<Json> samples;
    samples.reserve(static_cast<std::size_t>(sample_intervals + 1));
    const auto started = std::chrono::steady_clock::now();
    samples.push_back(capture_snapshot(options, session_config));
    for (std::uint64_t index = 1; index <= sample_intervals; ++index) {
        const auto offset = std::min(duration_ms, index * interval_ms);
        std::this_thread::sleep_until(started + std::chrono::milliseconds(offset));
        samples.push_back(capture_snapshot(options, session_config));
    }
    auto result = runtime_topology_watch_document(samples, duration_ms, interval_ms);
    result["actual_elapsed_ms"] = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - started).count());
    return result;
}

}  // namespace

int command_recon_runtime_topology(const std::vector<std::string>& values) {
    Args args(values);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout <<
            "Usage: tdx-tool recon runtime-topology [--process NAME] [--pid N]\n"
            "       [--all-modules|--no-modules] [--no-connections] [--no-children]\n"
            "       [--watch-seconds N] [--interval-ms N]\n"
            "       [--root PATH|--no-config-match]\n"
            "       [--baseline FILE] [--output FILE] [--compact]\n\n"
            "Captures a read-only process/module/TCP snapshot. It does not attach, inject,\n"
            "read process memory, send subscriptions, or expose session credentials. Watch\n"
            "mode keeps compact sample summaries and emits full diffs only for changes.\n";
        return 0;
    }
    RuntimeTopologyOptions options;
    options.process_name = args.take_option("--process", "TdxW.exe");
    options.process_id = parse_pid(args.take_option("--pid"));
    const bool all_modules = args.take_flag("--all-modules");
    const bool no_modules = args.take_flag("--no-modules");
    if (all_modules && no_modules)
        throw Error("--all-modules and --no-modules are mutually exclusive");
    options.include_modules = !no_modules;
    options.all_modules = all_modules;
    options.include_connections = !args.take_flag("--no-connections");
    options.include_descendants = !args.take_flag("--no-children");
    const auto root_name = args.take_option("--root");
    const bool no_config_match = args.take_flag("--no-config-match");
    if (no_config_match && !root_name.empty())
        throw Error("--root and --no-config-match are mutually exclusive");
    const bool interval_supplied = args.has("--interval-ms");
    const auto watch_seconds = parse_unsigned(args.take_option("--watch-seconds"),
                                               "watch-seconds", 0, 1, 3600);
    const auto interval_ms = parse_unsigned(args.take_option("--interval-ms"),
                                             "interval-ms", 250, 50, 60000);
    if (interval_supplied && !watch_seconds)
        throw Error("--interval-ms requires --watch-seconds");
    const auto baseline_name = args.take_option("--baseline");
    const auto output_name = args.take_option("--output");
    const bool compact = args.take_flag("--compact");
    args.require_empty();

    std::optional<Json> session_config;
    std::string correlation_error;
    if (!no_config_match) {
        try {
            const auto root = correlation_root(root_name);
            session_config = session_config_document(root);
        } catch (const std::exception& error) {
            if (!root_name.empty()) throw;
            correlation_error = error.what();
        }
    }

    if (!baseline_name.empty() && !output_name.empty() &&
        same_path(fs::u8path(baseline_name), fs::u8path(output_name)))
        throw Error("runtime topology output cannot overwrite its baseline");

    auto report = watch_seconds
        ? capture_watch(options, session_config, watch_seconds * 1000, interval_ms)
        : capture_snapshot(options, session_config);
    if (!session_config) {
        Json correlation = Json::object();
        correlation["enabled"] = false;
        correlation["available"] = false;
        correlation["reason"] = no_config_match ? "disabled by --no-config-match" :
                                correlation_error.empty() ? "configuration unavailable" :
                                                            correlation_error;
        correlation["network_sent"] = false;
        correlation["private_state_included"] = false;
        correlation["credentials_emitted"] = false;
        report["endpoint_correlation"] = std::move(correlation);
    }
    if (!baseline_name.empty()) {
        const auto baseline = Json::parse(read_text_utf8(fs::u8path(baseline_name)));
        report["comparison"] = compare_runtime_topology_documents(
            topology_snapshot(baseline), topology_snapshot(report));
        report["baseline_schema"] = baseline.at("schema").as_string();
        report["baseline_path"] = path_utf8(fs::u8path(baseline_name));
    }
    const auto rendered = report.dump(compact ? -1 : 2) + "\n";
    if (output_name.empty()) std::cout << rendered;
    else atomic_write_text(fs::u8path(output_name), rendered);
    return 0;
}

}  // namespace tdx
