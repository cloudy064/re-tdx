#include "formula_scan_internal.hpp"

namespace tdx {
namespace fs = std::filesystem;
using namespace formula_scan_detail;

int command_formulas_watch(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        print_formula_watch_help();
        return 0;
    }
    const int interval = integer_option(args, "--interval-seconds", 60, 1, 86400);
    const int iterations = integer_option(args, "--iterations", 0, 0, 1000000);
    const bool heartbeat = args.take_flag("--heartbeat");
    (void)args.take_flag("--compact");
    const auto output = args.take_option("--output");
    const auto state_file = args.take_option("--state-file");
    const auto block_output = args.take_option("--block-output");
    const std::vector<std::string> scan_args = args.remaining();
    require_distinct_formula_watch_paths(scan_args, output, state_file, block_output);

    Json persisted = Json::object();
    bool loaded_state = false;
    if (!state_file.empty() && fs::is_regular_file(from_utf8(state_file))) {
        persisted = Json::parse(read_text_utf8(from_utf8(state_file)));
        if (!persisted.is_object() ||
            !optional(persisted, "schema") ||
            persisted.at("schema").as_string() != "tdx-formula-watch-state-v1" ||
            !optional(persisted, "active_matches") ||
            !persisted.at("active_matches").is_array())
            throw Error("formula watch state file has an incompatible schema");
        loaded_state = true;
    }

    std::ofstream output_file;
    if (!output.empty()) {
        const auto output_path = from_utf8(output);
        if (!output_path.parent_path().empty())
            fs::create_directories(output_path.parent_path());
        output_file.open(output_path, std::ios::binary | std::ios::trunc);
        if (!output_file) throw Error("cannot open formula watch output: " + path_utf8(output_path));
    }
    auto emit = [&](const Json& event) {
        auto& stream = output.empty() ? std::cout : output_file;
        stream << event.dump(-1) << '\n';
        stream.flush();
        if (!stream) throw Error("cannot write formula watch event");
    };

    int run_iteration = 0;
    while (iterations == 0 || run_iteration < iterations) {
        ++run_iteration;
        try {
            auto invocation = execute_formula_scan_once(scan_args, true);
            auto& scan = invocation.result;
            const auto requested = static_cast<std::uint64_t>(scan.at("requested_count").as_number());
            const auto evaluated = static_cast<std::uint64_t>(scan.at("evaluated").as_number());
            const auto formula_errors = static_cast<std::uint64_t>(scan.at("error_count").as_number());
            const auto fetch_errors = static_cast<std::uint64_t>(scan.at("fetch_error_count").as_number());
            const bool complete = requested > 0 && evaluated == requested &&
                                  formula_errors == 0 && fetch_errors == 0;
            if (!complete) {
                Json event = Json::object();
                event["schema_version"] = 1;
                event["schema"] = "tdx-formula-watch-event-v1";
                event["generated_at"] = formula_watch_time_text();
                event["run_iteration"] = run_iteration;
                event["type"] = "degraded";
                event["message"] = "incomplete scan did not change persisted membership";
                event["requested_count"] = requested;
                event["evaluated"] = evaluated;
                event["formula_error_count"] = formula_errors;
                event["fetch_error_count"] = fetch_errors;
                event["errors"] = scan.at("errors");
                event["fetch_errors"] = scan.at("fetch_errors");
                event["state_preserved"] = loaded_state || optional(persisted, "active_matches");
                emit(event);
            } else {
                const auto configuration_id = formula_watch_configuration_id(scan_args, scan);
                const auto* previous_configuration = optional(persisted, "configuration_id");
                const bool resumable = previous_configuration &&
                    previous_configuration->is_string() &&
                    previous_configuration->as_string() == configuration_id;
                const bool state_reset = loaded_state && !resumable;
                Json previous = resumable ? persisted : Json::object();
                auto diff = diff_formula_scan_results(previous, scan);
                const bool changed = diff.at("changed").as_bool();
                std::uint64_t state_iteration = 1;
                if (resumable) {
                    const auto* value = optional(persisted, "iteration");
                    if (value && value->is_number())
                        state_iteration = static_cast<std::uint64_t>(value->as_number()) + 1;
                }
                auto next_state = make_formula_scan_watch_state(
                    scan, configuration_id, state_iteration);
                next_state["observed_at"] = formula_watch_time_text();
                if (!state_file.empty())
                    atomic_write_text(from_utf8(state_file), next_state.dump(2) + "\n");
                if (!block_output.empty())
                    atomic_write_text(from_utf8(block_output), formula_scan_block_text(scan));

                if (run_iteration == 1 || changed || heartbeat) {
                    Json event = Json::object();
                    event["schema_version"] = 1;
                    event["schema"] = "tdx-formula-watch-event-v1";
                    event["generated_at"] = formula_watch_time_text();
                    event["run_iteration"] = run_iteration;
                    event["state_iteration"] = state_iteration;
                    event["type"] = run_iteration == 1
                        ? (resumable ? (changed ? "change" : "resume") : "snapshot")
                        : (changed ? "change" : "heartbeat");
                    event["formula"] = scan.at("formula");
                    event["period"] = scan.at("period");
                    event["lookback"] = scan.at("lookback");
                    if (const auto* value = optional(scan, "adjustment_mode"))
                        event["adjustment_mode"] = *value;
                    if (const auto* value = optional(scan, "adjustment_summary"))
                        event["adjustment_summary"] = *value;
                    event["requested_count"] = requested;
                    event["evaluated"] = evaluated;
                    event["match_count"] = scan.at("match_count");
                    event["configuration_id"] = configuration_id;
                    event["resumed"] = resumable;
                    event["state_reset"] = state_reset;
                    event["state_persisted"] = !state_file.empty();
                    event["state_file"] = state_file;
                    event["block_exported"] = !block_output.empty();
                    event["block_output"] = block_output;
                    event["diff"] = std::move(diff);
                    emit(event);
                }
                persisted = std::move(next_state);
                loaded_state = true;
            }
        } catch (const std::exception& error) {
            Json event = Json::object();
            event["schema_version"] = 1;
            event["schema"] = "tdx-formula-watch-event-v1";
            event["generated_at"] = formula_watch_time_text();
            event["run_iteration"] = run_iteration;
            event["type"] = "error";
            event["message"] = error.what();
            event["state_preserved"] = loaded_state;
            emit(event);
        }
        if (iterations != 0 && run_iteration >= iterations) break;
        std::this_thread::sleep_for(std::chrono::seconds(interval));
    }
    return 0;
}

}  // namespace tdx
