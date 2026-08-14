#include "tdx/announcement_signals.hpp"
#include "tdx/announcement_signals_internal.hpp"
#include "tdx/blocks.hpp"
#include "tdx/common.hpp"

#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

namespace tdx {

using detail::announcement_signals::bounded;

namespace {

fs::path native_path(const std::string& value) {
#ifdef _WIN32
    return fs::path(utf8_to_wide(value));
#else
    return fs::path(value);
#endif
}

}  // namespace

int command_market_announcement_signals(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout <<
            "Usage: tdx-tool market announcement-signals [options]\n\n"
            "Typed TDX announcement selection, risk list and per-security history.\n\n"
            "  --root PATH             TDX installation root\n"
            "  --view NAME             selected|risks|security|history|catalog\n"
            "  --market sz|sh|bj       Required by security/history\n"
            "  --code CODE             Required by security/history\n"
            "  --query TEXT            Search title/type/security content\n"
            "  --direction NAME        all|bullish|bearish|unknown\n"
            "  --type TEXT             Announcement-type substring\n"
            "  --from DATE --to DATE   Announcement date filter\n"
            "  --sort NAME             date|recent-3d|recent-10d|pre-3d|post-3d|source-rank|code\n"
            "  --order asc|desc        Default desc\n"
            "  --no-history            Security view: omit ggjx history\n"
            "  --offset N --limit N    Default 0 / 500\n"
            "  --refresh --cache-ttl N --timeout-ms N\n"
            "  --output FILE --compact\n";
        return 0;
    }
    const auto root_text = args.take_option("--root");
    AnnouncementSignalsQuery query;
    query.view = args.take_option("--view", "selected");
    query.market = args.take_option("--market");
    query.code = trim(args.take_option("--code"));
    query.query = trim(args.take_option("--query"));
    query.direction = args.take_option("--direction", "all");
    query.announcement_type = trim(args.take_option("--type"));
    query.from = args.take_option("--from");
    query.to = args.take_option("--to");
    query.sort = args.take_option("--sort", "date");
    query.order = args.take_option("--order", "desc");
    query.include_history = !args.take_flag("--no-history");
    query.offset = bounded(args.take_option("--offset", "0"), "offset", 0, 1000000);
    query.limit = bounded(args.take_option("--limit", "500"), "limit", 1, 10000);
    query.refresh = args.take_flag("--refresh");
    query.cache_ttl_seconds = bounded(args.take_option("--cache-ttl", "300"),
                                      "cache-ttl", 0, 86400);
    query.timeout_ms = bounded(args.take_option("--timeout-ms", "15000"),
                               "timeout-ms", 100, 60000);
    const auto output_name = args.take_option("--output");
    const bool compact = args.take_flag("--compact");
    args.require_empty();

    std::map<std::pair<int, std::string>, Security> securities;
    if (lower_ascii(trim(query.view)) != "catalog") {
        const auto root = find_tdx_root(root_text.empty() ? fs::path{} : native_path(root_text));
        securities = load_blocks(root, {}).securities;
    }
    AnnouncementSignalsService service(std::move(securities));
    const auto rendered = service.query(query).dump(compact ? -1 : 2) + '\n';
    if (output_name.empty()) std::cout << rendered;
    else {
        const auto output = native_path(output_name);
        atomic_write_text(output, rendered);
        std::cout << "completed announcement-signals query -> " << path_utf8(output) << '\n';
    }
    return 0;
}

}  // namespace tdx
