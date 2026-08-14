#include "market_internal.hpp"

#include <chrono>
#include <fstream>
#include <iostream>
#include <map>
#include <thread>

namespace fs = std::filesystem;

namespace tdx {
namespace {

void watch_help() {
    std::cout
        << "Usage: tdx-tool market watch --security [MARKET:]CODE [options]\n"
           "Continuously reuse one public L1 session, poll 0x0547 quote+depth, "
           "and emit changes as JSONL.\n"
           "Options: --security/--host repeatable, --root PATH, --timeout-ms N, "
           "--batch-size N,\n"
           "  --interval-ms N (200..600000, default 1000), --iterations N "
           "(0=continuous),\n"
           "  --max-backoff-ms N (default 30000), --heartbeat, --output PATH\n"
           "The same persistent polling core feeds /api/v1/market/stream for local "
           "SSE fan-out.\n"
           "FastHQ.Subscribe is not impersonated: it requires the authenticated "
           "tpbus/TaApi session.\n";
}

Json watch_boundary() {
    Json boundary = Json::object();
    boundary["fast_hq_subscribe_used"] = false;
    boundary["reason"] =
        "FastHQ.Subscribe requires an authenticated tpbus/TaApi CTAJob_InetTQL session";
    boundary["recovered_request_fields"] =
        "CODE,SC,LX,PkgType,OperType,PushType,BatchPush";
    boundary["client_resubscribe_evidence"] =
        "TPool-independent tpbus timer retries around 10 seconds; alternate subscribed "
        "mode retries around 55 seconds";
    return boundary;
}

} // namespace

int command_market_watch(const std::vector<std::string> &raw_args) {
    using namespace market_detail;
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        watch_help();
        return 0;
    }
    const int interval_ms =
        bounded_integer(args.take_option("--interval-ms", "1000"),
                        "--interval-ms", 200, 600000);
    const int iterations =
        bounded_integer(args.take_option("--iterations", "0"), "--iterations", 0,
                        1000000);
    const int max_backoff_ms = bounded_integer(
        args.take_option("--max-backoff-ms", "30000"), "--max-backoff-ms",
        interval_ms, 600000);
    const bool heartbeat = args.take_flag("--heartbeat");
    const auto selected = parse_common_options(args, "");
    args.require_empty();

    std::ofstream file;
    std::ostream *output = &std::cout;
    if (!selected.output.empty()) {
        if (!selected.output.parent_path().empty())
            fs::create_directories(selected.output.parent_path());
        file.open(selected.output, std::ios::binary | std::ios::trunc);
        if (!file)
            throw Error("cannot open market watch output: " +
                        path_utf8(selected.output));
        output = &file;
    }
    const auto blocks = load_blocks(selected.root, {});
    MarketL1Session session(selected.root, &blocks, session_options(selected));
    const auto securities = display_codes(selected);
    std::map<std::string, std::string, std::less<>> previous;
    int iteration = 0;
    int consecutive_failures = 0;
    const auto emit = [&](const Json &event) {
        *output << event.dump(-1) << '\n';
        output->flush();
        if (!*output)
            throw Error("cannot write market watch event");
    };
    while (iterations == 0 || iteration < iterations) {
        ++iteration;
        int delay_ms = interval_ms;
        try {
            const auto document = session.poll_depth(securities);
            std::map<std::string, std::string, std::less<>> current;
            Json changed = Json::array();
            for (const auto &source : document.at("records").as_array()) {
                auto row = source;
                const auto id = row.at("security_id").as_string();
                const auto signature = row.dump(-1);
                current[id] = signature;
                const auto found = previous.find(id);
                if (iteration == 1 || found == previous.end() ||
                    found->second != signature) {
                    changed.push_back(std::move(row));
                }
            }
            Json removed = Json::array();
            for (const auto &[id, signature] : previous) {
                (void)signature;
                if (!current.count(id))
                    removed.push_back(id);
            }
            const bool has_changes = changed.size() > 0 || removed.size() > 0;
            if (iteration == 1 || has_changes || heartbeat) {
                Json event = Json::object();
                event["schema_version"] = 2;
                event["schema"] = "tdx-market-l1-watch-event-v2";
                event["generated_at"] = now_text();
                event["iteration"] = iteration;
                event["type"] = iteration == 1
                                    ? "snapshot"
                                    : has_changes ? "change" : "heartbeat";
                event["transport"] = "public-7709-L1-persistent-polling";
                event["command"] = std::string(
                    quote_surface(QuoteSurface::Depth).command_text);
                event["endpoint"] = document.at("endpoint");
                event["server_name"] = document.at("server_name");
                event["session"] = document.at("session");
                event["changed_count"] =
                    static_cast<std::uint64_t>(changed.size());
                event["removed_count"] =
                    static_cast<std::uint64_t>(removed.size());
                event["records"] = std::move(changed);
                event["removed_security_ids"] = std::move(removed);
                event["fast_hq_boundary"] = watch_boundary();
                emit(event);
            }
            previous = std::move(current);
            consecutive_failures = 0;
        } catch (const std::exception &error) {
            ++consecutive_failures;
            const auto factor =
                1ULL << std::min(consecutive_failures - 1, 20);
            delay_ms = static_cast<int>(std::min<std::uint64_t>(
                static_cast<std::uint64_t>(max_backoff_ms),
                static_cast<std::uint64_t>(interval_ms) * factor));
            Json event = Json::object();
            event["schema_version"] = 2;
            event["schema"] = "tdx-market-l1-watch-event-v2";
            event["generated_at"] = now_text();
            event["iteration"] = iteration;
            event["type"] = "reconnect";
            event["message"] = error.what();
            event["consecutive_failures"] = consecutive_failures;
            event["next_retry_ms"] = delay_ms;
            emit(event);
        }
        if (iterations != 0 && iteration >= iterations)
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
    }
    return 0;
}

} // namespace tdx
