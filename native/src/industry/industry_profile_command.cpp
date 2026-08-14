#include "industry_profile_internal.hpp"

#include "tdx/blocks.hpp"

#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

namespace tdx {
namespace {

struct IndustryProfileCommandDefaults {
    static constexpr const char* detail_limit = "1000";
    static constexpr const char* timeout_ms = "15000";
    static constexpr const char* output =
        "output/tdx-industry-profile-native.json";
};

int bounded_integer(const std::string& text, const std::string& name,
                    int minimum, int maximum) {
    try {
        std::size_t used = 0;
        const int result = std::stoi(text, &used);
        if (used != text.size() || result < minimum || result > maximum)
            throw std::invalid_argument("range");
        return result;
    } catch (...) {
        throw Error(name + " must be in " + std::to_string(minimum) + ".." +
                    std::to_string(maximum));
    }
}

}  // namespace

int command_market_industry_profile(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout <<
            "Usage: tdx-tool market industry-profile [options]\n\n"
            "Native research-industry institutional holdings and shareholder profile tree.\n\n"
            "Options:\n"
            "  --root PATH             TDX installation root\n"
            "  --industry 881xxx       Select a research-industry node\n"
            "  --market sz|sh|bj       Resolve a security's research-industry path\n"
            "  --code CODE             Six-digit security code\n"
            "  --detail-limit N        Returned security rows, default 1000\n"
            "  --timeout-ms N          Default 15000\n"
            "  --output PATH           Default output/tdx-industry-profile-native.json\n"
            "  --compact               Write compact JSON\n";
        return 0;
    }
    const auto root_text = args.take_option("--root");
    IndustryProfileQuery query;
    query.industry = trim(args.take_option("--industry"));
    query.market = trim(args.take_option("--market"));
    query.code = trim(args.take_option("--code"));
    query.detail_limit = bounded_integer(args.take_option("--detail-limit", IndustryProfileCommandDefaults::detail_limit),
                                         "--detail-limit", 1, 5000);
    query.timeout_ms = bounded_integer(args.take_option("--timeout-ms", IndustryProfileCommandDefaults::timeout_ms),
                                       "--timeout-ms", 100, 60000);
    query.refresh = true;
    const auto output = detail::industry_profile::native_utf8_path(args.take_option(
        "--output", IndustryProfileCommandDefaults::output));
    const bool compact = args.take_flag("--compact");
    args.require_empty();
    const auto root = find_tdx_root(root_text.empty() ? fs::path{} : detail::industry_profile::native_utf8_path(root_text));
    const auto blocks = load_blocks(root, {"research-industry"});
    IndustryProfileService service(blocks);
    const auto document = service.query(query);
    atomic_write_text(output, document.dump(compact ? -1 : 2) + "\n");
    std::cout << "completed " << document.at("mode").as_string()
              << " industry profile with "
              << document.at("counts").at("shareholder_securities").as_number()
              << " security rows -> " << path_utf8(output) << '\n';
    return 0;
}

}  // namespace tdx

