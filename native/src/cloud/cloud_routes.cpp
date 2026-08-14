#include "tdx/cloud_routes.hpp"

#include "tdx/common.hpp"
#include "tdx/tqlex.hpp"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <map>
#include <set>
#include <string>

namespace fs = std::filesystem;

namespace tdx {
namespace {

fs::path from_utf8(const std::string& value) {
#ifdef _WIN32
    return fs::path(utf8_to_wide(value));
#else
    return fs::path(value);
#endif
}

std::string entry_family(const std::string& value) {
    const auto folded = lower_ascii(trim(value));
    const auto dot = folded.find('.');
    if (dot != std::string::npos) return folded.substr(0, dot);
    for (const auto& prefix : {"pcwebcall_", "tdxyj_", "cfg_", "tdx_"})
        if (folded.rfind(prefix, 0) == 0) return std::string(prefix).substr(0, std::string(prefix).size() - 1);
    const auto underscore = folded.find('_');
    return underscore == std::string::npos ? folded : folded.substr(0, underscore);
}

std::string pe_architecture(const fs::path& path) {
    if (!fs::is_regular_file(path)) return "missing";
    try {
        const auto bytes = read_bytes(path);
        if (bytes.size() < 0x40 || bytes[0] != 'M' || bytes[1] != 'Z') return "not-pe";
        const auto pe = static_cast<std::size_t>(read_u32_le(bytes.data() + 0x3C));
        if (pe + 6 > bytes.size() || bytes[pe] != 'P' || bytes[pe + 1] != 'E' ||
            bytes[pe + 2] != 0 || bytes[pe + 3] != 0) return "invalid-pe";
        const auto machine = read_u16_le(bytes.data() + pe + 4);
        if (machine == 0x014C) return "x86";
        if (machine == 0x8664) return "x64";
        if (machine == 0xAA64) return "arm64";
        return "machine-" + std::to_string(machine);
    } catch (...) {
        return "unreadable";
    }
}

Json config_row(const TqlexConfig& config,
                const std::set<std::string, std::less<>>& sibling_formats) {
    Json row = Json::object();
    row["source_file"] = config.source_file;
    row["entry"] = config.entry;
    row["entry_family"] = entry_family(config.entry);
    row["request_format"] = config.request_format;
    row["request_id"] = config.request_id;
    row["body"] = config.body;
    Json placeholders = Json::array();
    for (const auto& value : config.placeholders) placeholders.push_back(value);
    row["placeholders"] = std::move(placeholders);
    Json siblings = Json::array();
    for (const auto& value : sibling_formats) if (value != "1") siblings.push_back(value);
    row["same_entry_other_formats"] = std::move(siblings);
    if (config.entry.empty()) {
        row["route_assessment"] = "missing-entry";
        row["reason"] = "datasource has no service name";
    } else if (sibling_formats.size() > 1) {
        row["route_assessment"] = "cross-format-candidate";
        row["reason"] = "the same entry name occurs in another reqformat and deserves targeted comparison";
    } else if (lower_ascii(config.entry).rfind("cwserv.", 0) == 0) {
        row["route_assessment"] = "tpdata-domain-service";
        row["reason"] = "CWServ domain family; route availability depends on TPData session/server registration";
    } else {
        row["route_assessment"] = "legacy-named-service";
        row["reason"] = "no exact modern-format entry was found in the installed cloud_cfg set";
    }
    row["network_probed"] = false;
    row["safe_next_action"] = "compare a captured client request or test through a bitness-compatible TPData message-loop host";
    return row;
}

}  // namespace

Json cloud_routes_document(const fs::path& root, const std::string& entry_filter,
                           const std::string& source_filter) {
    const std::vector<std::string> formats{"1", "2", "11", "20", "22"};
    std::map<std::string, std::vector<TqlexConfig>, std::less<>> by_format;
    std::map<std::string, std::set<std::string, std::less<>>, std::less<>> entry_formats;
    for (const auto& format : formats) {
        auto configs = inventory_cloud_configs(root, format);
        for (const auto& config : configs)
            if (!config.entry.empty()) entry_formats[lower_ascii(config.entry)].insert(format);
        by_format.emplace(format, std::move(configs));
    }
    Json counts = Json::object();
    for (const auto& format : formats)
        counts[format] = static_cast<std::uint64_t>(by_format.at(format).size());

    const auto entry_needle = lower_ascii(trim(entry_filter));
    const auto source_needle = lower_ascii(trim(source_filter));
    Json rows = Json::array();
    std::set<std::string, std::less<>> unique_entries;
    std::map<std::string, std::uint64_t, std::less<>> families;
    std::map<std::string, std::uint64_t, std::less<>> assessments;
    for (const auto& config : by_format.at("1")) {
        if (!entry_needle.empty() && lower_ascii(config.entry).find(entry_needle) == std::string::npos)
            continue;
        if (!source_needle.empty() && lower_ascii(config.source_file).find(source_needle) == std::string::npos)
            continue;
        unique_entries.insert(lower_ascii(config.entry));
        const auto family = entry_family(config.entry);
        ++families[family];
        auto row = config_row(config, entry_formats[lower_ascii(config.entry)]);
        ++assessments[row.at("route_assessment").as_string()];
        rows.push_back(std::move(row));
    }

    const auto tpdata = root / "ZDPlugins" / "TPData100.dll";
    const auto module_arch = pe_architecture(tpdata);
    const std::string process_arch = sizeof(void*) == 8 ? "x64" : "x86";
    Json compatibility = Json::object();
    compatibility["tpdata_path"] = path_utf8(tpdata);
    compatibility["tpdata_exists"] = fs::is_regular_file(tpdata);
    compatibility["tpdata_architecture"] = module_arch;
    compatibility["tool_process_architecture"] = process_arch;
    compatibility["load_in_process_possible"] = module_arch == process_arch;
    compatibility["message_loop_required"] = true;
    compatibility["network_sent"] = false;
    compatibility["diagnostic_boundary"] =
        "this command inventories and correlates routes only; it never loads TPData100.dll or sends requests";

    Json family_rows = Json::object();
    for (const auto& [name, count] : families) family_rows[name] = count;
    Json assessment_rows = Json::object();
    for (const auto& [name, count] : assessments) assessment_rows[name] = count;
    Json observed = Json::array();
    Json legacy = Json::object();
    legacy["entry"] = "pcwebcall_jjzt_etfjj_etfzt";
    legacy["request_format"] = "1";
    legacy["anonymous_result"] = "S999(-7415): route not registered";
    legacy["verified_at"] = "2026-07-31";
    observed.push_back(std::move(legacy));
    Json domain = Json::object();
    domain["entry"] = "CWServ.SecuInfo";
    domain["request_format"] = "0";
    domain["anonymous_result"] = "ErrorCode -1005: database execution failed";
    domain["verified_at"] = "2026-07-31";
    observed.push_back(std::move(domain));

    Json result = Json::object();
    result["schema_version"] = 1;
    result["schema"] = "tdx-cloud-route-diagnostics-native-v1";
    result["root"] = path_utf8(root);
    result["read_only"] = true;
    result["request_format_counts"] = std::move(counts);
    result["reqformat1_record_count"] = static_cast<std::uint64_t>(rows.size());
    result["reqformat1_unique_entry_count"] = static_cast<std::uint64_t>(unique_entries.size());
    result["entry_families"] = std::move(family_rows);
    result["route_assessments"] = std::move(assessment_rows);
    result["compatibility"] = std::move(compatibility);
    result["observed_probes"] = std::move(observed);
    result["records"] = std::move(rows);
    return result;
}

int command_cloud_routes(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout <<
            "Usage: tdx-tool cloud routes [options]\n\n"
            "Inventory reqformat=1 and correlate exact entry names with formats 2/11/20/22.\n"
            "This is read-only and does not load TPData100.dll or send network requests.\n\n"
            "Options:\n"
            "  --root PATH       TDX installation root\n"
            "  --entry TEXT      Filter reqformat=1 entry names\n"
            "  --source TEXT     Filter cloud_cfg filenames\n"
            "  --output PATH     Default output/tdx-cloud-route-diagnostics.json\n"
            "  --compact         Compact JSON\n";
        return 0;
    }
    const auto root_text = args.take_option("--root");
    const auto root = find_tdx_root(root_text.empty() ? fs::path{} : from_utf8(root_text));
    const auto entry = args.take_option("--entry");
    const auto source = args.take_option("--source");
    const auto output = from_utf8(args.take_option(
        "--output", "output/tdx-cloud-route-diagnostics.json"));
    const bool compact = args.take_flag("--compact");
    args.require_empty();
    const auto document = cloud_routes_document(root, entry, source);
    atomic_write_text(output, document.dump(compact ? -1 : 2) + "\n");
    std::cout << "diagnosed " << static_cast<std::uint64_t>(
        document.at("reqformat1_record_count").as_number())
              << " reqformat=1 rows -> " << path_utf8(output) << '\n';
    return 0;
}

}  // namespace tdx
