#include "tpool_internal.hpp"

#include "tdx/common.hpp"
#include "tdx/formula_calc.hpp"
#include "tdx/formula_context.hpp"
#include "tdx/formula_engine.hpp"
#include "tdx/formulas.hpp"
#include "tdx/minute.hpp"
#include "tdx/security_directory.hpp"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <cmath>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace fs = std::filesystem;

namespace tdx {

using namespace tpool_detail;

namespace {

int bounded_option(Args& args, std::string_view name, int fallback, int minimum, int maximum) {
    const auto value = args.take_option(name, std::to_string(fallback));
    const int parsed = integer_text(value, minimum - 1);
    if (parsed < minimum || parsed > maximum) throw Error(std::string(name) + " is outside the safe range");
    return parsed;
}

void print_help() {
    std::cout <<
        "Usage: tdx-tool pool inspect [--input FILE | --root TDX] [options]\n\n"
        "Read TPool XML without loading TPool.dll, starting workers, or changing pool state.\n\n"
        "Options:\n"
        "  --input FILE           Inspect one XML file (repeatable)\n"
        "  --root PATH            Scan tpool directories under a TDX installation\n"
        "  --output PATH          Write JSON to a file\n"
        "  --compact              Compact JSON\n";
}

void print_evaluate_help() {
    std::cout <<
        "Usage: tdx-tool pool evaluate --input FILE [options]\n\n"
        "Read a TPool XML and evaluate supported formula rules without mutating TDX state.\n\n"
        "Options:\n"
        "  --root PATH            TDX root used for market/context data\n"
        "  --library PATH         Formula JSON override; bundled snapshot is the default\n"
        "  --pages N              K-line history pages, 1..20 (default 1)\n"
        "  --page-size N          Bars per page, 1..800 (default 800)\n"
        "  --limit N              Maximum securities, 1..200 (default 20)\n"
        "  --timeout-ms N         Per request timeout (default 10000)\n"
        "  --output PATH          Write JSON to a file\n"
        "  --compact              Compact JSON\n";
}

void print_watch_help() {
    std::cout <<
        "Usage: tdx-tool pool watch --input FILE [options]\n\n"
        "Continuously evaluate a TPool and emit JSONL enter/exit alerts without pool writeback.\n\n"
        "Options:\n"
        "  --root PATH            TDX root used for market/context data\n"
        "  --library PATH         Formula JSON override; bundled snapshot is the default\n"
        "  --interval-seconds N   Delay between evaluations, 1..86400 (default 60)\n"
        "  --iterations N         Stop after N evaluations; 0 runs until interrupted (default 0)\n"
        "  --pages N              K-line history pages, 1..20 (default 1)\n"
        "  --page-size N          Bars per page, 1..800 (default 800)\n"
        "  --limit N              Maximum securities, 1..200 (default 20)\n"
        "  --timeout-ms N         Per request timeout (default 10000)\n"
        "  --heartbeat            Emit unchanged successful evaluations too\n"
        "  --output PATH          Write JSONL to a newly truncated file instead of stdout\n"
        "  --state-file PATH      Atomically persist/resume native flow state as JSON\n";
}

}  // namespace

int command_pool_inspect(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) { print_help(); return 0; }
    const auto input_values = args.take_options("--input");
    const auto root_value = args.take_option("--root");
    const auto output_value = args.take_option("--output");
    const bool compact = args.take_flag("--compact");
    args.require_empty();
    if (!input_values.empty() && !root_value.empty())
        throw Error("pool inspect accepts --input or --root, not both");
    Json result;
    if (!input_values.empty()) {
        Json pools = Json::array();
        for (const auto& value : input_values) pools.push_back(inspect_tpool_file_document(from_utf8(value)));
        result = Json::object();
        result["schema_version"] = 1;
        result["schema"] = "tdx-tpool-catalog-v1";
        result["read_only"] = true;
        result["pool_count"] = static_cast<std::uint64_t>(pools.size());
        result["pools"] = std::move(pools);
    } else {
        const auto root = find_tdx_root(root_value.empty() ? fs::path{} : from_utf8(root_value));
        result = inspect_tpool_root_document(root);
    }
    const auto report = result.dump(compact ? -1 : 2) + "\n";
    if (output_value.empty()) std::cout << report;
    else atomic_write_text(from_utf8(output_value), report);
    return 0;
}

int command_pool_evaluate(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) { print_evaluate_help(); return 0; }
    const auto input = args.take_option("--input");
    if (input.empty()) throw Error("pool evaluate requires --input");
    const int pages = bounded_option(args, "--pages", 1, 1, 20);
    const int page_size = bounded_option(args, "--page-size", 800, 1, 800);
    const int limit = bounded_option(args, "--limit", 20, 1, 200);
    const int timeout = bounded_option(args, "--timeout-ms", 10000, 1, 600000);
    const auto root_text = args.take_option("--root"), library_text = args.take_option("--library");
    const auto output = args.take_option("--output");
    const bool compact = args.take_flag("--compact");
    args.require_empty();
    fs::path root;
    Json library;
    if (!library_text.empty()) {
        library = Json::parse(read_text_utf8(from_utf8(library_text)));
        if (!root_text.empty()) root = find_tdx_root(from_utf8(root_text));
    } else {
        root = find_tdx_root(root_text.empty() ? fs::path{} : from_utf8(root_text));
        library = load_bundled_formula_library_document();
    }
    const auto report = evaluate_tpool_file_document(from_utf8(input), pages, page_size,
        timeout, limit, &library, root).dump(compact ? -1 : 2) + "\n";
    if (output.empty()) std::cout << report;
    else atomic_write_text(from_utf8(output), report);
    return 0;
}

int command_pool_watch(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) { print_watch_help(); return 0; }
    const auto input = args.take_option("--input");
    if (input.empty()) throw Error("pool watch requires --input");
    const int interval = bounded_option(args, "--interval-seconds", 60, 1, 86400);
    const auto iteration_text = args.take_option("--iterations", "0");
    const int iterations = integer_text(iteration_text, -1);
    if (iterations < 0 || iterations > 1000000)
        throw Error("--iterations must be in 0..1000000");
    const int pages = bounded_option(args, "--pages", 1, 1, 20);
    const int page_size = bounded_option(args, "--page-size", 800, 1, 800);
    const int limit = bounded_option(args, "--limit", 20, 1, 200);
    const int timeout = bounded_option(args, "--timeout-ms", 10000, 1, 600000);
    const auto root_text = args.take_option("--root"), library_text = args.take_option("--library");
    const bool heartbeat = args.take_flag("--heartbeat");
    const auto output = args.take_option("--output");
    const auto state_file_text = args.take_option("--state-file");
    args.require_empty();

    fs::path root;
    Json library;
    if (!library_text.empty()) {
        library = Json::parse(read_text_utf8(from_utf8(library_text)));
        if (!root_text.empty()) root = find_tdx_root(from_utf8(root_text));
    } else {
        root = find_tdx_root(root_text.empty() ? fs::path{} : from_utf8(root_text));
        library = load_bundled_formula_library_document();
    }

    const auto input_path = from_utf8(input);
    const auto state_path = state_file_text.empty() ? fs::path{} : from_utf8(state_file_text);
    const auto normalized_path = [](const fs::path& path) {
        return lower_ascii(path_utf8(fs::absolute(path).lexically_normal()));
    };
    if (!state_path.empty() && normalized_path(state_path) == normalized_path(input_path))
        throw Error("--state-file must not overwrite the TPool XML input");
    if (!state_path.empty() && !output.empty() &&
        normalized_path(state_path) == normalized_path(from_utf8(output)))
        throw Error("--state-file and --output must be different files");
    Json flow_state = Json::object();
    if (!state_path.empty() && fs::is_regular_file(state_path)) {
        flow_state = Json::parse(read_text_utf8(state_path));
        if (!flow_state.is_object()) throw Error("TPool state file must contain a JSON object");
    }

    std::ofstream file;
    if (!output.empty()) {
        const auto path = from_utf8(output);
        if (!path.parent_path().empty()) fs::create_directories(path.parent_path());
        file.open(path, std::ios::binary | std::ios::trunc);
        if (!file) throw Error("cannot open alert output: " + path_utf8(path));
    }
    auto emit = [&](const Json& event) {
        auto& stream = output.empty() ? std::cout : file;
        stream << event.dump(-1) << '\n';
        stream.flush();
        if (!stream) throw Error("cannot write TPool alert event");
    };

    Json previous = Json::object();
    int iteration = 0;
    while (iterations == 0 || iteration < iterations) {
        ++iteration;
        try {
            auto current = evaluate_tpool_file_document(input_path, pages, page_size,
                                                         timeout, limit, &library, root);
            flow_state = advance_tpool_flow_state_document(current, flow_state, local_flow_clock());
            current["flow_runtime"] = flow_state;
            if (!state_path.empty())
                atomic_write_text(state_path, flow_state.dump(2) + "\n");
            auto diff = diff_tpool_alerts_document(previous, current);
            const bool changed = diff.at("entered_count").as_number() > 0.0 ||
                                 diff.at("exited_count").as_number() > 0.0;
            if (iteration == 1 || changed || heartbeat) {
                Json event = Json::object();
                event["schema_version"] = 1;
                event["schema"] = "tdx-tpool-watch-event-v1";
                event["generated_at"] = local_time_text();
                event["iteration"] = iteration;
                event["type"] = iteration == 1 ? "snapshot" : (changed ? "change" : "heartbeat");
                event["source"] = path_utf8(from_utf8(input));
                event["diff"] = std::move(diff);
                event["evaluated_security_count"] = current.at("evaluated_security_count");
                event["evaluated_rule_count"] = current.at("evaluated_rule_count");
                event["flow_projection_evaluated"] = current.at("flow_graph_evaluated");
                event["flow_state_iteration"] = flow_state.at("iteration");
                event["flow_state_pool_tick_seconds"] = flow_state.at("pool_tick_seconds");
                event["flow_state_run_event_count"] = flow_state.at("flow_run_event_count");
                event["flow_state_events"] = flow_state.at("events");
                event["flow_state_persisted"] = !state_path.empty();
                event["flow_state_file"] = state_path.empty() ? "" : path_utf8(state_path);
                emit(event);
            }
            previous = std::move(current);
        } catch (const std::exception& error) {
            Json event = Json::object();
            event["schema_version"] = 1;
            event["schema"] = "tdx-tpool-watch-event-v1";
            event["generated_at"] = local_time_text();
            event["iteration"] = iteration;
            event["type"] = "error";
            event["source"] = path_utf8(from_utf8(input));
            event["message"] = error.what();
            emit(event);
        }
        if (iterations != 0 && iteration >= iterations) break;
        std::this_thread::sleep_for(std::chrono::seconds(interval));
    }
    return 0;
}

}  // namespace tdx
