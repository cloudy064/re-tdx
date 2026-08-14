#include "tdx/local_signals.hpp"

#include "tdx/common.hpp"

#include <filesystem>
#include <iostream>
#include <limits>

namespace fs = std::filesystem;

namespace tdx {
namespace {

int integer_option(const std::string& value, std::string_view name) {
    try {
        std::size_t used = 0;
        const auto parsed = std::stoll(value, &used);
        if (used != value.size() ||
            parsed < std::numeric_limits<int>::min() ||
            parsed > std::numeric_limits<int>::max())
            throw std::invalid_argument("range");
        return static_cast<int>(parsed);
    } catch (...) {
        throw Error(std::string(name) + " must be a 32-bit integer");
    }
}

std::size_t limit_option(const std::string& value) {
    const int parsed = integer_option(value, "limit");
    if (parsed < 0 || parsed > 30000)
        throw Error("limit must be in 0..30000");
    return static_cast<std::size_t>(parsed);
}

int market_option(std::string value) {
    value = lower_ascii(trim(std::move(value)));
    if (value == "sz") return 0;
    if (value == "sh") return 1;
    if (value == "bj") return 2;
    const int parsed = integer_option(value, "market");
    if (parsed < 0 || parsed > 65535)
        throw Error("market must be sz/sh/bj or an integer in 0..65535");
    return parsed;
}

std::optional<LocalSignalNamespace> namespace_option(std::string value) {
    value = lower_ascii(trim(std::move(value)));
    if (value.empty() || value == "all") return std::nullopt;
    if (value == "system" || value == "sys")
        return LocalSignalNamespace::system;
    if (value == "user") return LocalSignalNamespace::user;
    throw Error("namespace must be system, user or all");
}

}  // namespace

int command_formulas_local_signals(const std::vector<std::string>& values) {
    Args arguments(values);
    if (arguments.take_flag("--help") || arguments.take_flag("-h")) {
        std::cout <<
            "Usage: tdx-tool formulas local-signals [options]\n\n"
            "  --root PATH              TDX installation root\n"
            "  --namespace system|user|all  Catalog namespace (default all)\n"
            "  --id N                   Select one signal series\n"
            "  --market sz|sh|bj|ID     Exact security market for --id\n"
            "  --code CODE              Exact security code for --id\n"
            "  --limit N                Return at most N points (0..30000)\n"
            "  --start YYYYMMDD         Inclusive date floor\n"
            "  --end YYYYMMDD           Inclusive date ceiling\n"
            "  --output FILE            Write JSON instead of stdout\n"
            "  --compact                Compact JSON\n";
        return 0;
    }
    const auto root_text = arguments.take_option("--root");
    const auto namespace_text = arguments.take_option("--namespace", "all");
    const auto id_text = arguments.take_option("--id");
    const auto market_text = arguments.take_option("--market");
    const auto code = trim(arguments.take_option("--code"));
    const auto limit_text = arguments.take_option("--limit", "30000");
    const auto start_text = arguments.take_option("--start", "0");
    const auto end_text = arguments.take_option("--end", "99991231");
    const auto output_text = arguments.take_option("--output");
    const bool compact = arguments.take_flag("--compact");
    arguments.require_empty();

    auto signal_namespace = namespace_option(namespace_text);
    std::optional<int> signal_id, market_id;
    if (!id_text.empty()) signal_id = integer_option(id_text, "id");
    if (!market_text.empty()) market_id = market_option(market_text);
    if (signal_id && !signal_namespace)
        throw Error("--id requires --namespace system or user");
    const auto root = find_tdx_root(
        root_text.empty() ? fs::path{} : fs::u8path(root_text));
    const auto document = local_signal_document(
        root, signal_namespace, signal_id, market_id, code,
        limit_option(limit_text), integer_option(start_text, "start"),
        integer_option(end_text, "end"));
    const auto rendered = document.dump(compact ? -1 : 2) + "\n";
    if (output_text.empty()) {
        std::cout << rendered;
    } else {
        const auto output = fs::u8path(output_text);
        atomic_write_text(output, rendered);
        std::cout << "exported local signal diagnostics -> "
                  << path_utf8(output) << '\n';
    }
    return 0;
}

}  // namespace tdx
