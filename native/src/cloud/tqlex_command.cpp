#include "tqlex_internal.hpp"

#include "tdx/cloud_resilience.hpp"

#include <filesystem>
#include <iostream>
#include <map>

namespace fs = std::filesystem;

namespace tdx {
namespace {

struct TqlexCommandDefaults {
    static constexpr const char* base_url =
        cloud_endpoints::tqlex;
    static constexpr const char* output = "output/tdx-tqlex-native.json";
    static constexpr const char* max_pages = "100";
    static constexpr const char* timeout_ms = "15000";
    static constexpr const char* attempts = "3";
    static constexpr const char* attempt_delay_ms = "250";
};

std::map<std::string, std::string> assignments(const std::vector<std::string>& values,
                                                std::string_view option) {
    std::map<std::string, std::string> result;
    for (const auto& value : values) {
        const auto equals = value.find('=');
        if (equals == std::string::npos || equals == 0)
            throw Error(std::string(option) + " expects NAME=VALUE");
        result[value.substr(0, equals)] = value.substr(equals + 1);
    }
    return result;
}

int parse_integer_option(const std::string& text, std::string_view name,
                         int minimum, int maximum) {
    try {
        std::size_t used = 0;
        const int value = std::stoi(text, &used);
        if (used != text.size() || value < minimum || value > maximum)
            throw std::invalid_argument("range");
        return value;
    } catch (...) {
        throw Error(std::string(name) + " must be in " + std::to_string(minimum) +
                    ".." + std::to_string(maximum));
    }
}

}  // namespace

int command_cloud_tqlex(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout <<
            "Usage: tdx-tool cloud tqlex [options]\n\n"
            "Native reqformat=2 TQLEX config discovery and HTTP JSON query.\n\n"
            "Options:\n"
            "  --list                 List active reqformat=2 configs\n"
            "  --req-id ID            Select a cloud_cfg request template\n"
            "  --entry NAME           Limit selection to an Entry\n"
            "  --source-file NAME     Limit selection to one XML file\n"
            "  --body-contains TEXT   Repeatable request-body selector\n"
            "  --set NAME=VALUE       Repeatable template replacement\n"
            "  --param NAME=VALUE     Repeatable request field override\n"
            "  --request-json JSON    Explicit JSON; requires --entry\n"
            "  --page N               Page number (default 0)\n"
            "  --page-size N          Page size (default 20)\n"
            "  --all-pages            Fetch and merge all pages\n"
            "  --max-pages N          Safety limit (default 100)\n"
            "  --base-url URL         Default static.tdx.com.cn:7615/TQLEX\n"
            "  --timeout-ms N         Default 15000\n"
            "  --attempts N           Whole-query attempts, 1..10 (default 3)\n"
            "  --attempt-delay-ms N   Linear retry delay base (default 250)\n"
            "  --root PATH            TDX installation root\n"
            "  --output PATH          Default output/tdx-tqlex-native.json\n"
            "  --compact              Compact JSON\n";
        return 0;
    }
    const bool list = args.take_flag("--list");
    const auto request_id = args.take_option("--req-id");
    const auto entry = args.take_option("--entry");
    const auto source_file = args.take_option("--source-file");
    const auto body_contains = args.take_options("--body-contains");
    const auto replacements = assignments(args.take_options("--set"), "--set");
    const auto overrides = assignments(args.take_options("--param"), "--param");
    const auto request_json = args.take_option("--request-json");
    const bool page_supplied = args.has("--page");
    const bool page_size_supplied = args.has("--page-size");
    const int page = page_supplied
        ? parse_integer_option(args.take_option("--page"), "--page", 0, 1000000) : -1;
    const int page_size = page_size_supplied
        ? parse_integer_option(args.take_option("--page-size"),
                               "--page-size", 1, 1000000) : 0;
    const bool all_pages = args.take_flag("--all-pages");
    const int max_pages = parse_integer_option(args.take_option("--max-pages", TqlexCommandDefaults::max_pages),
                                               "--max-pages", 1, 1000);
    const auto base_url = args.take_option("--base-url", TqlexCommandDefaults::base_url);
    const int timeout_ms = parse_integer_option(args.take_option("--timeout-ms", TqlexCommandDefaults::timeout_ms),
                                                "--timeout-ms", 100, 600000);
    const int max_attempts = parse_integer_option(args.take_option("--attempts", TqlexCommandDefaults::attempts),
                                                  "--attempts", 1, 10);
    const int attempt_delay_ms = parse_integer_option(
        args.take_option("--attempt-delay-ms", TqlexCommandDefaults::attempt_delay_ms),
        "--attempt-delay-ms", 0, 60000);
    const auto root_text = args.take_option("--root");
    const auto output_text = args.take_option("--output", TqlexCommandDefaults::output);
    const bool compact = args.take_flag("--compact");
    args.require_empty();
    const auto root = find_tdx_root(root_text.empty() ? fs::path{} : fs::u8path(root_text));
    Json document;
    if (list) document = tqlex_configs_document(root);
    else {
        int attempts = 0;
        document = detail::retry_cloud_json([&]() -> Json {
            if (!request_json.empty()) {
                if (entry.empty()) throw Error("--request-json requires --entry");
                auto request = Json::parse(request_json);
                for (const auto& [name, value] : overrides)
                    set_tqlex_request_value(request, name, value);
                const int effective_page = page >= 0
                    ? page : detail::tqlex_request_integer(request, "Page", 0);
                const int effective_page_size = page_size > 0
                    ? page_size : detail::tqlex_request_integer(request, "PageSize", 20);
                if (page >= 0)
                    set_tqlex_request_value(request, "Page", std::to_string(page));
                if (page_size > 0)
                    set_tqlex_request_value(request, "PageSize", std::to_string(page_size));
                auto response = all_pages
                    ? query_tqlex_all_pages(entry, request, effective_page,
                                            effective_page_size, max_pages,
                                            base_url, timeout_ms)
                    : query_tqlex(entry, request, base_url, timeout_ms);
                Json result = Json::object();
                result["schema"] = "tdx-tqlex-native-v1";
                result["request_id"] = request_id;
                result["entry"] = entry;
                result["source_file"] = "";
                result["all_pages"] = all_pages;
                result["request"] = std::move(request);
                result["response"] = std::move(response);
                return result;
            }
            if (request_id.empty())
                throw Error("provide --req-id, --request-json, or --list");
            return execute_tqlex_config(root, request_id, replacements, overrides,
                entry, source_file, body_contains, all_pages, page, page_size,
                max_pages, base_url, timeout_ms);
        }, detail::is_transient_tqlex_error, attempts,
           max_attempts, attempt_delay_ms);
        document["attempts"] = attempts;
        document["max_attempts"] = max_attempts;
    }
    const auto output = fs::u8path(output_text);
    atomic_write_text(output, document.dump(compact ? -1 : 2) + "\n");
    std::cout << (list ? "listed TQLEX configs" : "completed TQLEX request")
              << " -> " << path_utf8(output) << '\n';
    return 0;
}

}  // namespace tdx
