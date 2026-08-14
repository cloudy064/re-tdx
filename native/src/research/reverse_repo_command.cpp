#include "reverse_repo_internal.hpp"
#include "tdx/blocks.hpp"
#include <iostream>
namespace fs = std::filesystem;
namespace tdx {
int command_market_reverse_repo(const std::vector<std::string>& raw_args) {
    using namespace detail::reverse_repo; Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout << "Usage: tdx-tool market reverse-repo [options]\n\nTyped TDX government-bond reverse-repo rates and settlement calendar.\n\n"
            "  --root PATH             TDX installation root\n  --view NAME             rates|security|catalog\n"
            "  --market all|sz|sh      Exchange filter; concrete market required by security\n  --code CODE             Required by security\n"
            "  --query TEXT            Search code or name\n  --min-term-days N --max-term-days N\n"
            "  --principal-yuan N      Return calculator principal, default 100000\n  --no-quotes             Calendar only, skip public L1 snapshot\n"
            "  --sort NAME             rate|net-rate|gross-interest|net-interest|term|interest-days|available-date|withdrawable-date|turnover|code|source-rank\n"
            "  --order asc|desc        Default desc\n  --offset N --limit N    Default 0 / 100\n"
            "  --refresh --cache-ttl N --quote-cache-ttl N --timeout-ms N\n  --output FILE --compact\n"; return 0;
    }
    const auto root_text = args.take_option("--root"); ReverseRepoQuery query;
    query.view = args.take_option("--view", "rates"); query.market = args.take_option("--market", "all");
    query.code = trim(args.take_option("--code")); query.query = trim(args.take_option("--query"));
    query.min_term_days = bounded(args.take_option("--min-term-days", "0"), "min-term-days", 0, 1000);
    query.max_term_days = bounded(args.take_option("--max-term-days", "0"), "max-term-days", 0, 1000);
    query.principal_yuan = bounded(args.take_option("--principal-yuan", "100000"), "principal-yuan", 1000, 1000000000);
    query.include_quotes = !args.take_flag("--no-quotes"); query.sort = args.take_option("--sort", "rate");
    query.order = args.take_option("--order", "desc"); query.offset = bounded(args.take_option("--offset", "0"), "offset", 0, 1000000);
    query.limit = bounded(args.take_option("--limit", "100"), "limit", 1, 1000); query.refresh = args.take_flag("--refresh");
    query.cache_ttl_seconds = bounded(args.take_option("--cache-ttl", "300"), "cache-ttl", 0, 86400);
    query.quote_cache_ttl_seconds = bounded(args.take_option("--quote-cache-ttl", "5"), "quote-cache-ttl", 0, 3600);
    query.timeout_ms = bounded(args.take_option("--timeout-ms", "15000"), "timeout-ms", 100, 60000);
    const auto output_name = args.take_option("--output"); const bool compact = args.take_flag("--compact"); args.require_empty();
    const auto root = find_tdx_root(root_text.empty() ? fs::path{} : native_path(root_text));
    ReverseRepoService service(root, load_blocks(root, {}).securities); const auto rendered = service.query(query).dump(compact ? -1 : 2) + '\n';
    if (output_name.empty()) std::cout << rendered; else { const auto output = native_path(output_name); atomic_write_text(output, rendered); std::cout << "completed reverse-repo query -> " << path_utf8(output) << '\n'; }
    return 0;
}
}  // namespace tdx
