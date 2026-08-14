#include "jsn_variants_internal.hpp"

#include "tdx/common.hpp"
#include "tdx/jsn.hpp"
#include "tdx/jsn_data.hpp"
#include "tdx/tqlex.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iterator>
#include <iostream>
#include <map>
#include <optional>
#include <regex>
#include <set>
#include <sstream>
#include <string_view>

namespace fs = std::filesystem;

namespace tdx {
namespace {

int parse_top(const std::string& text) {
    try {
        std::size_t used = 0;
        const int result = std::stoi(text, &used);
        if (used != text.size() || result < 0 || result > 1000)
            throw std::invalid_argument("range");
        return result;
    } catch (...) {
        throw Error("--top must be in 0..1000");
    }
}

}  // namespace

using namespace jsn_variant_detail;

int command_recon_jsn_variants(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout <<
            "Usage: tdx-tool recon jsn-variants [options]\n\n"
            "Audit XML/CFG JSN resource templates against typed C++ business commands.\n"
            "This command is read-only and never downloads resources.\n\n"
            "  --root PATH           TDX installation root\n"
            "  --jsn-root PATH       Downloaded JSN directory (default output/tdx-jsn)\n"
            "  --gaps-only           Only emit resources without a typed command\n"
            "  --top N               Ranked high-value gaps (default 50; 0 disables)\n"
            "  --allow-gaps          Return success even when generic-only gaps remain\n"
            "  --availability-report FILE  Reuse a jsn download --report manifest in ranking\n"
            "  --output FILE         Default output/tdx-jsn-variants.json\n"
            "  --compact\n";
        return 0;
    }
    const auto root_text = args.take_option("--root");
    const auto jsn_root_text = args.take_option("--jsn-root", "output/tdx-jsn");
    const bool gaps_only = args.take_flag("--gaps-only");
    const int top = parse_top(args.take_option("--top", "50"));
    const bool allow_gaps = args.take_flag("--allow-gaps");
    const auto availability_text = args.take_option("--availability-report");
    const auto output_text = args.take_option("--output", "output/tdx-jsn-variants.json");
    const bool compact = args.take_flag("--compact");
    args.require_empty();
    const auto root = find_tdx_root(root_text.empty() ? fs::path{} : from_utf8(root_text));
    auto report = jsn_variant_coverage_document(
        root, from_utf8(jsn_root_text), gaps_only, top);
    if (!availability_text.empty()) {
        const auto manifest = Json::parse(
            read_text_utf8(from_utf8(availability_text)));
        if (!manifest.is_object() ||
            optional_string(manifest, "schema") !=
                "tdx-jsn-remote-manifest-native-v1")
            throw Error("invalid JSN availability report schema");
        const auto resources = manifest.as_object().find("resources");
        if (resources == manifest.as_object().end() || !resources->second.is_array())
            throw Error("JSN availability report lacks resources");
        std::map<std::string, Json> remote;
        for (const auto& item : resources->second.as_array()) {
            const auto name = optional_string(item, "resource");
            if (!name.empty()) remote[lower_ascii(name)] = item;
        }
        struct Ranked { int class_rank{}; int score{}; Json row; };
        std::vector<Ranked> ranked;
        std::uint64_t matched = 0, available = 0, missing = 0;
        for (auto& row : report["resources"].as_array()) {
            const auto name = lower_ascii(row.at("resource").as_string());
            const auto found = remote.find(name);
            Json probe = Json::object();
            int rank_class = 1;
            int score = row.at("gap_score").is_number()
                ? static_cast<int>(row.at("gap_score").as_number()) : 0;
            if (found == remote.end()) {
                probe["matched"] = false;
                probe["status"] = "unknown";
                probe["size"] = nullptr;
            } else {
                ++matched;
                probe["matched"] = true;
                const auto explicit_status = optional_string(found->second, "status");
                const auto size = optional_uint(found->second, "size");
                const bool absent = explicit_status == "missing" || size == 0;
                probe["status"] = absent ? "missing" : "available";
                probe["size"] = absent ? Json(nullptr) : Json(size);
                const auto md5 = optional_string(found->second, "md5");
                probe["md5"] = md5.empty() ? Json(nullptr) : Json(md5);
                if (absent) {
                    ++missing;
                    rank_class = 0;
                    score -= 500;
                } else {
                    ++available;
                    rank_class = 2;
                    score += 60;
                    if (size >= 100000) score += 20;
                    else if (size >= 10000) score += 10;
                }
            }
            row["remote_probe"] = std::move(probe);
            if (row.at("coverage").as_string() == "generic-only" && rank_class > 0) {
                row["remote_rank_score"] = score;
                ranked.push_back({rank_class, score, row});
            }
        }
        std::sort(ranked.begin(), ranked.end(), [](const Ranked& left,
                                                   const Ranked& right) {
            if (left.class_rank != right.class_rank)
                return left.class_rank > right.class_rank;
            if (left.score != right.score) return left.score > right.score;
            return lower_ascii(left.row.at("resource").as_string()) <
                   lower_ascii(right.row.at("resource").as_string());
        });
        Json high_value = Json::array();
        for (std::size_t index = 0;
             index < ranked.size() && index < static_cast<std::size_t>(top); ++index)
            high_value.push_back(ranked[index].row);
        report["high_value_gaps"] = std::move(high_value);
        report["availability_report"] = availability_text;
        report["summary"]["remote_manifest_resource_count"] =
            static_cast<std::uint64_t>(remote.size());
        report["summary"]["remote_probe_matched_template_count"] = matched;
        report["summary"]["remote_available_template_count"] = available;
        report["summary"]["remote_missing_template_count"] = missing;
        report["semantics"] = report.at("semantics").as_string() +
            " When an availability report is supplied, confirmed non-empty resources rank "
            "before unprobed gaps and confirmed zero-length resources are excluded from "
            "high_value_gaps; remote status never changes typed coverage by itself.";
    }
    const auto output = from_utf8(output_text);
    atomic_write_text(output, report.dump(compact ? -1 : 2) + "\n");
    const auto& summary = report.at("summary");
    std::cout << "JSN variants: " << summary.at("typed_template_count").as_number()
              << '/' << summary.at("resource_template_count").as_number()
              << " typed; " << summary.at("downloaded_file_count").as_number()
              << " downloaded -> " << path_utf8(output) << '\n';
    return allow_gaps || summary.at("generic_only_template_count").as_number() == 0 ? 0 : 1;
}

int command_recon_jsn_discovery(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout <<
            "Usage: tdx-tool recon jsn-discovery [options]\n\n"
            "Profile downloaded JSN resources and compare them with a saved baseline.\n"
            "The scan is local-only; --capture-baseline is the only write besides --output.\n\n"
            "  --root PATH           TDX installation root\n"
            "  --jsn-root PATH       Downloaded JSN directory (default output/tdx-jsn)\n"
            "  --baseline FILE       Default output/tdx-jsn-discovery-baseline.json\n"
            "  --capture-baseline    Replace the baseline after producing this comparison\n"
            "  --top N               Ranked changes (default 100; 0 disables)\n"
            "  --output FILE         Default output/tdx-jsn-discovery.json\n"
            "  --compact\n";
        return 0;
    }
    const auto root_text = args.take_option("--root");
    const auto jsn_root_text = args.take_option("--jsn-root", "output/tdx-jsn");
    const auto baseline_text = args.take_option(
        "--baseline", "output/tdx-jsn-discovery-baseline.json");
    const bool capture = args.take_flag("--capture-baseline");
    const int top = parse_top(args.take_option("--top", "100"));
    const auto output_text = args.take_option(
        "--output", "output/tdx-jsn-discovery.json");
    const bool compact = args.take_flag("--compact");
    args.require_empty();
    const auto root = find_tdx_root(root_text.empty() ? fs::path{} : from_utf8(root_text));
    const auto output = from_utf8(output_text);
    const auto report = jsn_discovery_document(
        root, from_utf8(jsn_root_text), from_utf8(baseline_text), capture, top);
    atomic_write_text(output, report.dump(compact ? -1 : 2) + "\n");
    const auto& summary = report.at("summary");
    std::cout << "JSN discovery: " << summary.at("current_file_count").as_number()
              << " current; +" << summary.at("added_file_count").as_number()
              << " ~" << summary.at("changed_file_count").as_number()
              << " -" << summary.at("removed_file_count").as_number()
              << " -> " << path_utf8(output) << '\n';
    return 0;
}

int command_jsn_candidates(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout <<
            "Usage: tdx-tool jsn candidates [options]\n\n"
            "Generate concrete dynamic JSN paths from CFG refunit master rows.\n"
            "The command never downloads; optional probes are metadata-only and bounded.\n\n"
            "  --root PATH           TDX installation root\n"
            "  --jsn-root PATH       Downloaded JSN directory (default output/tdx-jsn)\n"
            "  --family NAME         Restrict a dynamic resource family\n"
            "  --all                 Include already downloaded candidates\n"
            "  --limit N             Returned rows, 0..100000 (default 200)\n"
            "  --probe               Probe missing candidates; requires --family\n"
            "  --max-probes N        Probe cap, 1..25 (default 10)\n"
            "  --timeout-ms N        Per-probe timeout, 100..60000 (default 10000)\n"
            "  --output FILE         Default output/tdx-jsn-candidates.json\n"
            "  --compact\n";
        return 0;
    }
    auto integer = [](const std::string& value, const char* name, int low, int high) {
        try {
            std::size_t used = 0;
            const int parsed = std::stoi(value, &used);
            if (used != value.size() || parsed < low || parsed > high)
                throw std::invalid_argument("range");
            return parsed;
        } catch (...) {
            throw Error(std::string(name) + " must be in " + std::to_string(low) +
                        ".." + std::to_string(high));
        }
    };
    const auto root_text = args.take_option("--root");
    const auto jsn_root_text = args.take_option("--jsn-root", "output/tdx-jsn");
    const auto family = args.take_option("--family");
    const bool include_present = args.take_flag("--all");
    const bool probe = args.take_flag("--probe");
    const int limit = integer(args.take_option("--limit", "200"), "--limit", 0, 100000);
    const int max_probes = integer(args.take_option("--max-probes", "10"),
                                   "--max-probes", 1, 25);
    const int timeout_ms = integer(args.take_option("--timeout-ms", "10000"),
                                   "--timeout-ms", 100, 60000);
    const auto output_text = args.take_option(
        "--output", "output/tdx-jsn-candidates.json");
    const bool compact = args.take_flag("--compact");
    args.require_empty();
    const auto root = find_tdx_root(root_text.empty() ? fs::path{} : from_utf8(root_text));
    const auto report = jsn_candidate_document(
        root, from_utf8(jsn_root_text), family, !include_present, limit,
        probe, max_probes, timeout_ms);
    const auto output = from_utf8(output_text);
    atomic_write_text(output, report.dump(compact ? -1 : 2) + "\n");
    const auto& summary = report.at("summary");
    std::cout << "JSN candidates: " << summary.at("candidate_count").as_number()
              << " candidate rows, "
              << summary.at("unique_missing_resource_count").as_number()
              << " unique local misses, " << summary.at("probed_count").as_number()
              << " probed -> " << path_utf8(output) << '\n';
    return 0;
}

}  // namespace tdx
