#include "tdx/recon.hpp"

#include "tdx/common.hpp"

#include <iostream>

namespace fs = std::filesystem;

namespace tdx {
namespace {

std::string render_inventory_markdown(const Json& report, bool hash) {
    std::string rendered = "# TDX native install inventory\n\n- Root: `" +
        report.at("root").as_string() + "`\n- Artifacts: " +
        std::to_string(static_cast<std::uint64_t>(report.at("count").as_number())) +
        "\n\n| Path | Bytes | Modified";
    if (hash) rendered += " | SHA-256";
    rendered += " |\n| --- | ---: | ---";
    if (hash) rendered += " | ---";
    rendered += " |\n";
    for (const auto& item : report.at("artifacts").as_array()) {
        rendered += "| `" + item.at("path").as_string() + "` | " +
                    std::to_string(static_cast<std::uint64_t>(
                        item.at("size").as_number())) +
                    " | " + item.at("modified").as_string();
        if (hash) rendered += " | `" + item.at("sha256").as_string() + "`";
        rendered += " |\n";
    }
    return rendered;
}

}  // namespace

int command_recon_install(const std::vector<std::string>& values) {
    Args args(values);
    if (args.take_flag("--help")) {
        std::cout << "Usage: tdx-tool recon install [--root PATH] [--hash]"
                     " [--recursive] [--format json|markdown] [--output FILE]\n";
        return 0;
    }
    const std::string root_name = args.take_option("--root");
    const std::string output_name = args.take_option("--output");
    const std::string format = lower_ascii(args.take_option("--format", "json"));
    const bool hash = args.take_flag("--hash");
    const bool recursive = args.take_flag("--recursive");
    args.require_empty();
    if (format != "json" && format != "markdown")
        throw Error("format must be json or markdown");

    const fs::path root = find_tdx_root(
        root_name.empty() ? fs::path{} : fs::u8path(root_name));
    const auto report = inventory_document(root, recursive, hash);
    const auto count = static_cast<std::uint64_t>(report.at("count").as_number());
    const std::string rendered = format == "json"
        ? report.dump(2) + '\n'
        : render_inventory_markdown(report, hash);
    if (output_name.empty()) {
        std::cout << rendered;
    } else {
        const fs::path output = fs::u8path(output_name);
        atomic_write_text(output, rendered);
        std::cout << "wrote " << count << " artifacts to " << path_utf8(output) << '\n';
    }
    return 0;
}

}  // namespace tdx
