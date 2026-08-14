#include "tdx/ttplugin_servers.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>

namespace fs = std::filesystem;

namespace tdx {
namespace {

using IniSection = std::map<std::string, std::string, std::less<>>;

struct IniSectionRecord {
    std::string name;
    IniSection values;
};

struct ParsedAuthority {
    std::string host;
    int port{};
    std::string host_kind;
    bool credentials_present{};
    int credential_components{};
};

struct ParsedUrl {
    std::string scheme;
    bool proxied{};
    ParsedAuthority primary;
    std::optional<ParsedAuthority> target;
};

fs::path from_utf8(const std::string& value) {
#ifdef _WIN32
    return fs::path(utf8_to_wide(value));
#else
    return fs::path(value);
#endif
}

std::string ascii_lower(std::string value) {
    for (auto& ch : value)
        if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
    return value;
}

std::vector<IniSectionRecord> parse_ini(std::string_view text) {
    std::vector<IniSectionRecord> result;
    IniSectionRecord* current = nullptr;
    std::size_t start = 0;
    while (start <= text.size()) {
        const auto end = text.find('\n', start);
        std::string line(text.substr(start, end == std::string_view::npos
                                           ? text.size() - start : end - start));
        if (!line.empty() && line.back() == '\r') line.pop_back();
        line = trim(line);
        if (!line.empty() && line.front() != ';' && line.front() != '#') {
            if (line.size() >= 2 && line.front() == '[' && line.back() == ']') {
                result.push_back({trim(line.substr(1, line.size() - 2)), {}});
                current = &result.back();
            } else if (current) {
                const auto equals = line.find('=');
                if (equals != std::string::npos) {
                    const auto key = ascii_lower(trim(line.substr(0, equals)));
                    if (!key.empty()) current->values[key] = trim(line.substr(equals + 1));
                }
            }
        }
        if (end == std::string_view::npos) break;
        start = end + 1;
    }
    return result;
}

std::optional<int> parse_count(const IniSection& section, std::string_view key) {
    const auto found = section.find(ascii_lower(std::string(key)));
    if (found == section.end()) return std::nullopt;
    try {
        std::size_t used = 0;
        const auto value = std::stoi(found->second, &used);
        if (used == found->second.size() && value >= 0 && value <= 10000) return value;
    } catch (...) {}
    return std::nullopt;
}

bool valid_hostname(std::string_view host) {
    if (host.empty() || !std::isalnum(static_cast<unsigned char>(host.front()))) return false;
    for (std::size_t index = 1; index < host.size(); ++index) {
        const auto ch = static_cast<unsigned char>(host[index]);
        if (std::isalnum(ch) || ch == '_' || ch == '-') continue;
        if (ch != '.' || host[index - 1] == '.') return false;
    }
    return host.back() != '-' && host.back() != '.';
}

ParsedAuthority parse_authority(std::string_view text) {
    if (text.empty()) throw Error("empty authority");
    ParsedAuthority result;
    const auto at = text.find('@');
    if (at != std::string_view::npos) {
        const auto credentials = text.substr(0, at);
        result.credentials_present = true;
        result.credential_components = credentials.find(':') == std::string_view::npos ? 1 : 2;
        text.remove_prefix(at + 1);
    }
    const auto colon = text.rfind(':');
    if (colon == std::string_view::npos) throw Error("authority has no port separator");
    auto host = std::string(text.substr(0, colon));
    const auto port_text = std::string(text.substr(colon + 1));
    if (host.size() > 2 && host.front() == '[' && host.back() == ']')
        host = host.substr(1, host.size() - 2);
    if (host.empty()) throw Error("authority has an empty host");
    if (!valid_hostname(host) && host.find(':') == std::string::npos &&
        host.find('.') == std::string::npos)
        throw Error("authority host is invalid");
    if (port_text.empty() || !std::all_of(port_text.begin(), port_text.end(),
                                          [](unsigned char ch) { return std::isdigit(ch); }))
        throw Error("authority port is not decimal");
    int port = 0;
    try { port = std::stoi(port_text); } catch (...) { throw Error("authority port is invalid"); }
    if (port < 1 || port > 65534) throw Error("authority port is outside 1..65534");
    result.host = std::move(host);
    result.port = port;
    result.host_kind = result.host.find(':') != std::string::npos
                           ? "ipv6"
                           : result.host.find('.') != std::string::npos ? "ipv4-or-dns" : "hostname";
    return result;
}

ParsedUrl parse_server_url(std::string_view value) {
    const auto marker = value.find("://");
    if (marker == std::string_view::npos) throw Error("URL has no :// delimiter");
    ParsedUrl result;
    result.scheme = ascii_lower(std::string(value.substr(0, marker)));
    const auto body = value.substr(marker + 3);
    static const std::set<std::string, std::less<>> direct{"tcp", "tcp6", "ssl"};
    static const std::set<std::string, std::less<>> proxy{"http", "socks4", "socks5"};
    if (direct.count(result.scheme)) {
        result.primary = parse_authority(body);
        return result;
    }
    if (!proxy.count(result.scheme)) throw Error("unsupported TTPlugin URL scheme");
    result.proxied = true;
    const auto slash = body.find('/');
    if (slash == std::string_view::npos) throw Error("proxy URL has no target separator");
    result.primary = parse_authority(body.substr(0, slash));
    result.target = parse_authority(body.substr(slash + 1));
    return result;
}

Json authority_json(const ParsedAuthority& value) {
    Json result = Json::object();
    result["host"] = value.host;
    result["port"] = value.port;
    result["host_kind"] = value.host_kind;
    result["credentials_present"] = value.credentials_present;
    result["credential_components"] = value.credential_components;
    return result;
}

Json url_json(std::string_view value) {
    Json result = Json::object();
    try {
        const auto parsed = parse_server_url(value);
        result["valid"] = true;
        result["scheme"] = parsed.scheme;
        result["proxied"] = parsed.proxied;
        result[parsed.proxied ? "proxy" : "endpoint"] = authority_json(parsed.primary);
        if (parsed.target) result["target"] = authority_json(*parsed.target);
        result["credentials_redacted"] = true;
    } catch (const std::exception& error) {
        result["valid"] = false;
        result["error"] = error.what();
        result["credentials_redacted"] = true;
    }
    return result;
}

bool url_json_has_credentials(const Json& entry) {
    const auto& object = entry.as_object();
    for (const auto* key : {"endpoint", "proxy", "target"}) {
        const auto found = object.find(key);
        if (found != object.end() &&
            found->second.at("credentials_present").as_bool()) return true;
    }
    return false;
}

bool selected_section(const std::vector<std::string>& filters, std::string_view section) {
    if (filters.empty()) return true;
    const auto lowered = ascii_lower(std::string(section));
    return std::any_of(filters.begin(), filters.end(), [&](const auto& item) {
        return ascii_lower(item) == lowered;
    });
}

Json family_json(const IniSection& section, const char* prefix, const char* kind,
                 std::uint64_t& declared, std::uint64_t& parsed,
                 std::uint64_t& invalid, std::uint64_t& credentials) {
    Json result = Json::object();
    result["prefix"] = prefix;
    result["kind"] = kind;
    const auto count = parse_count(section, std::string(prefix) + "_Num").value_or(0);
    result["declared_count"] = count;
    declared += static_cast<std::uint64_t>(count);
    Json entries = Json::array();
    for (int index = 1; index <= count; ++index) {
        const auto key = ascii_lower(std::string(prefix) + "_" + std::to_string(index));
        const auto found = section.find(key);
        if (found == section.end() || found->second.empty()) continue;
        auto entry = url_json(found->second);
        entry["index"] = index;
        entry["source_key"] = std::string(prefix) + "_" + std::to_string(index);
        if (entry.at("valid").as_bool()) {
            ++parsed;
            if (url_json_has_credentials(entry)) ++credentials;
        } else {
            ++invalid;
        }
        entries.push_back(std::move(entry));
    }
    result["entries"] = std::move(entries);
    result["parsed_entry_count"] = static_cast<std::uint64_t>(result.at("entries").size());
    return result;
}

bool has_server_keys(const IniSection& section) {
    for (const auto& [key, value] : section) {
        (void)value;
        if (key.rfind("mduserserver", 0) == 0 ||
            key.rfind("traderserver", 0) == 0 || key == "tdxproxy") return true;
    }
    return false;
}

std::string safe_text(const fs::path& path) {
    const auto bytes = read_bytes(path);
    try { return decode_gbk(bytes); } catch (...) {
        return std::string(bytes.begin(), bytes.end());
    }
}

std::vector<fs::path> discover_inputs(const fs::path& root) {
    std::vector<fs::path> result;
    std::set<std::string, std::less<>> seen;
    const auto consider = [&](const fs::path& path) {
        std::error_code error;
        if (!fs::is_regular_file(path, error) || error) return;
        const auto extension = ascii_lower(path.extension().string());
        if (extension != ".ini" && extension != ".cfg" && extension != ".conf" &&
            extension != ".txt") return;
        const auto size = fs::file_size(path, error);
        if (error || size > 1024 * 1024) return;
        const auto key = path_utf8(path.lexically_normal());
        if (seen.insert(key).second) result.push_back(path);
    };
    std::error_code error;
    for (const auto& item : fs::directory_iterator(root, error)) consider(item.path());
    const auto plugins = root / "QHPlugins";
    error.clear();
    if (fs::is_directory(plugins, error)) {
        for (const auto& item : fs::recursive_directory_iterator(plugins, error)) {
            if (error) break;
            consider(item.path());
        }
    }
    return result;
}

Json static_contract() {
    Json schemes = Json::array();
    for (const auto* value : {"tcp", "tcp6", "ssl", "http", "socks4", "socks5"})
        schemes.push_back(value);
    Json families = Json::array();
    for (const auto* value : {"MDUserServerNormal", "MDUserServerExtend",
                              "MDUserServer", "TraderServer"})
        families.push_back(value);
    Json modes = Json::object();
    modes["1"] = "option";
    modes["4"] = "trade";
    modes["5"] = "login";
    modes["6"] = "futures";
    modes["7"] = "rpc";
    Json current = Json::object();
    current["CurrentConnectInfo"] = "connected address and port";
    current["CurrentConnectInfoEx"] = "connected address, port, HostID and auxiliary route text";
    Json sensitive = Json::array();
    for (const auto* value : {"LoginID", "LoginPass", "UniqueName", "MachineInfo", "MAC"})
        sensitive.push_back(value);
    Json result = Json::object();
    result["schemes"] = std::move(schemes);
    result["server_families"] = std::move(families);
    result["user_mode_labels"] = std::move(modes);
    result["proxy_override_key"] = "TDXProxy";
    result["target_semantics"] =
        "HQDataService.Target is a host-supplied uint8 route value copied into CTAJob_Redirect; it is not a URL.";
    result["current_connection_getters"] = std::move(current);
    result["sensitive_runtime_getters_not_queried"] = std::move(sensitive);
    return result;
}

}  // namespace

Json ttplugin_server_config_document(const std::vector<fs::path>& inputs,
                                     const std::vector<std::string>& urls,
                                     const std::vector<std::string>& sections) {
    std::uint64_t declared = 0;
    std::uint64_t parsed = 0;
    std::uint64_t invalid = 0;
    std::uint64_t credentials = 0;
    std::uint64_t matching_sections = 0;
    Json files = Json::array();
    Json configurations = Json::array();
    for (const auto& path : inputs) {
        Json file = Json::object();
        file["path"] = path_utf8(path);
        file["exists"] = fs::is_regular_file(path);
        std::uint64_t matches = 0;
        if (fs::is_regular_file(path)) {
            for (const auto& section : parse_ini(safe_text(path))) {
                if (!selected_section(sections, section.name) || !has_server_keys(section.values))
                    continue;
                ++matches;
                ++matching_sections;
                Json configuration = Json::object();
                configuration["path"] = path_utf8(path);
                configuration["section"] = section.name;
                const auto proxy = section.values.find("tdxproxy");
                Json proxy_meta = Json::object();
                proxy_meta["present"] = proxy != section.values.end() && !proxy->second.empty();
                proxy_meta["value_redacted"] = true;
                proxy_meta["encoded_length"] = proxy == section.values.end()
                    ? 0 : static_cast<std::uint64_t>(proxy->second.size());
                proxy_meta["comma_field_count"] = proxy == section.values.end() || proxy->second.empty()
                    ? 0 : static_cast<std::uint64_t>(1 + std::count(proxy->second.begin(), proxy->second.end(), ','));
                configuration["tdx_proxy"] = std::move(proxy_meta);
                Json families = Json::array();
                families.push_back(family_json(section.values, "MDUserServerNormal",
                                               "market-data-normal", declared, parsed,
                                               invalid, credentials));
                families.push_back(family_json(section.values, "MDUserServerExtend",
                                               "market-data-extended", declared, parsed,
                                               invalid, credentials));
                families.push_back(family_json(section.values, "MDUserServer",
                                               "market-data", declared, parsed,
                                               invalid, credentials));
                families.push_back(family_json(section.values, "TraderServer",
                                               "trading", declared, parsed,
                                               invalid, credentials));
                configuration["families"] = std::move(families);
                configurations.push_back(std::move(configuration));
            }
        }
        file["matching_sections"] = matches;
        files.push_back(std::move(file));
    }

    Json ad_hoc = Json::array();
    for (std::size_t index = 0; index < urls.size(); ++index) {
        auto item = url_json(urls[index]);
        item["index"] = static_cast<std::uint64_t>(index + 1);
        if (item.at("valid").as_bool()) {
            ++parsed;
            if (url_json_has_credentials(item)) ++credentials;
        } else {
            ++invalid;
        }
        ad_hoc.push_back(std::move(item));
    }

    Json counts = Json::object();
    counts["input_files"] = static_cast<std::uint64_t>(inputs.size());
    counts["matching_sections"] = matching_sections;
    counts["declared_servers"] = declared;
    counts["valid_server_urls"] = parsed;
    counts["invalid_server_urls"] = invalid;
    counts["server_urls_with_credentials"] = credentials;

    Json evidence = Json::object();
    evidence["url_parser"] = "TTPlugin.dll:0x100BCA10";
    evidence["server_config_reader"] = "TTPlugin.dll:0x101E0650";
    evidence["hq_target_setter"] = "TTPlugin.dll:0x10286350";
    evidence["redirect_job_builder"] = "TTPlugin.dll:0x1028A6D0";
    evidence["current_connection_getter"] = "TTPlugin.dll:0x10324DE0";

    Json boundary = Json::object();
    boundary["read_only"] = true;
    boundary["offline_only"] = true;
    boundary["network_sent"] = false;
    boundary["dll_loaded"] = false;
    boundary["credentials_emitted"] = false;
    boundary["runtime_getters_queried"] = false;
    boundary["interpretation"] =
        "Parses caller-owned TTPlugin-style INI/URL configuration; it does not discover a live authenticated route.";

    Json result = Json::object();
    result["schema"] = "tdx-ttplugin-server-config-native-v1";
    result["counts"] = std::move(counts);
    result["files"] = std::move(files);
    result["configurations"] = std::move(configurations);
    result["ad_hoc_urls"] = std::move(ad_hoc);
    result["static_contract"] = static_contract();
    result["evidence"] = std::move(evidence);
    result["boundary"] = std::move(boundary);
    return result;
}

int command_recon_ttplugin_servers(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout <<
            "Usage: tdx-tool recon ttplugin-servers [options]\n\n"
            "Parse TTPlugin-style market/trading server URLs and INI families offline.\n"
            "Credentials and TDXProxy contents are never emitted. No DLL is loaded and no\n"
            "connection or runtime getter is used.\n\n"
            "Options:\n"
            "  --root PATH       Discover small INI/CFG files under root and QHPlugins\n"
            "  --input PATH      Repeat for explicit INI/CFG input; disables discovery\n"
            "  --section NAME    Repeat to select INI sections\n"
            "  --url URL         Repeat to validate a caller-supplied server URL\n"
            "  --output PATH     Default output/tdx-ttplugin-server-config.json\n"
            "  --compact         Compact JSON\n";
        return 0;
    }
    const auto input_texts = args.take_options("--input");
    const auto section_filters = args.take_options("--section");
    const auto urls = args.take_options("--url");
    const auto root_text = args.take_option("--root");
    const auto output = from_utf8(args.take_option(
        "--output", "output/tdx-ttplugin-server-config.json"));
    const bool compact = args.take_flag("--compact");
    args.require_empty();

    std::vector<fs::path> inputs;
    for (const auto& value : input_texts) inputs.push_back(from_utf8(value));
    if (inputs.empty()) {
        const auto root = find_tdx_root(root_text.empty() ? fs::path{} : from_utf8(root_text));
        inputs = discover_inputs(root);
    }
    const auto document = ttplugin_server_config_document(inputs, urls, section_filters);
    atomic_write_text(output, document.dump(compact ? -1 : 2) + "\n");
    std::cout << "audited "
              << static_cast<std::uint64_t>(document.at("counts").at("input_files").as_number())
              << " files and "
              << static_cast<std::uint64_t>(document.at("counts").at("valid_server_urls").as_number())
              << " valid TTPlugin server URLs with credentials redacted -> "
              << path_utf8(output) << '\n';
    return 0;
}

}  // namespace tdx
