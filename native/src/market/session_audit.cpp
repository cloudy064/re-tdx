#include "tdx/session_audit.hpp"

#include "session_endpoint_roles_internal.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string>

namespace fs = std::filesystem;

namespace tdx {
namespace {

using IniSection = std::map<std::string, std::string, std::less<>>;
using IniDocument = std::map<std::string, IniSection, std::less<>>;

fs::path from_utf8(const std::string& value) {
#ifdef _WIN32
    return fs::path(utf8_to_wide(value));
#else
    return fs::path(value);
#endif
}

std::string bytes_text(const fs::path& path) {
    const auto bytes = read_bytes(path);
    return std::string(bytes.begin(), bytes.end());
}

IniDocument parse_ini(std::string_view text) {
    IniDocument result;
    std::string section;
    std::size_t start = 0;
    while (start <= text.size()) {
        const auto end = text.find('\n', start);
        std::string line(text.substr(start, end == std::string_view::npos
                                           ? text.size() - start : end - start));
        if (!line.empty() && line.back() == '\r') line.pop_back();
        line = trim(line);
        if (!line.empty() && line.front() != ';' && line.front() != '#') {
            if (line.size() >= 2 && line.front() == '[' && line.back() == ']') {
                section = lower_ascii(trim(line.substr(1, line.size() - 2)));
            } else {
                const auto equals = line.find('=');
                if (equals != std::string::npos) {
                    const auto key = lower_ascii(trim(line.substr(0, equals)));
                    if (!key.empty()) result[section][key] = trim(line.substr(equals + 1));
                }
            }
        }
        if (end == std::string_view::npos) break;
        start = end + 1;
    }
    return result;
}

std::optional<int> integer_value(const IniSection& section, const std::string& key) {
    const auto found = section.find(lower_ascii(key));
    if (found == section.end()) return std::nullopt;
    try {
        std::size_t used = 0;
        const int value = std::stoi(found->second, &used);
        if (used == found->second.size()) return value;
    } catch (...) {}
    return std::nullopt;
}

std::string indexed_key(std::string_view prefix, int index) {
    std::ostringstream output;
    output << prefix << std::setw(2) << std::setfill('0') << index;
    return lower_ascii(output.str());
}

std::string decode_label(const std::string& value) {
    if (value.empty()) return {};
    try {
        return decode_gbk(Bytes(value.begin(), value.end()));
    } catch (...) {
        return value;
    }
}

Json endpoint_groups(const IniDocument& config) {
    Json groups = Json::array();
    for (const auto& [name, values] : config) {
        const auto count = integer_value(values, "hostnum");
        if (!count || *count < 0 || *count > 10000) continue;
        const auto configured_primary = integer_value(values, "primaryhost");
        const bool section_primary =
            session_audit_detail::endpoint_group_uses_section_primary(name);
        const int primary = section_primary && *count > 0
            ? std::clamp(configured_primary.value_or(0), 0, *count - 1) : 0;
        const int default_port =
            session_audit_detail::endpoint_group_loader_default_port(name);
        Json endpoints = Json::array();
        Json selected = Json(nullptr);
        for (int index = 1; index <= *count; ++index) {
            const auto address_it = values.find(indexed_key("ipaddress", index));
            if (address_it == values.end() || address_it->second.empty()) continue;
            const int port = integer_value(values, indexed_key("port", index))
                .value_or(default_port);
            if (port < 1 || port > 65535) continue;
            Json endpoint = Json::object();
            endpoint["index"] = index;
            endpoint["address"] = address_it->second;
            endpoint["port"] = port;
            endpoint["endpoint"] = address_it->second + ":" + std::to_string(port);
            const auto name_it = values.find(indexed_key("hostname", index));
            endpoint["name"] = name_it == values.end() ? "" : decode_label(name_it->second);
            endpoint["primary"] = section_primary && index - 1 == primary;
            if (section_primary && index - 1 == primary) selected = endpoint;
            endpoints.push_back(std::move(endpoint));
        }
        Json group = Json::object();
        group["section"] = name;
        group["declared_host_count"] = *count;
        group["parsed_host_count"] = static_cast<std::uint64_t>(endpoints.size());
        group["configured_primary_index"] = configured_primary
            ? Json(*configured_primary) : Json(nullptr);
        group["primary_index"] = section_primary && *count > 0
            ? Json(primary) : Json(nullptr);
        group["primary_index_base"] = 0;
        group["primary_configured"] = section_primary && configured_primary.has_value();
        group["selected_primary"] = std::move(selected);
        group["role"] = session_audit_detail::endpoint_group_role_document(name);
        group["endpoints"] = std::move(endpoints);
        groups.push_back(std::move(group));
    }
    return groups;
}

Json static_defaults() {
    Json timeout = Json::object();
    timeout["core_job_ms"] = 150000;
    timeout["create_ms"] = 3000;
    timeout["transaction_ms"] = 3000;
    Json heartbeat = Json::object();
    heartbeat["timespan_seconds"] = 30;
    heartbeat["inet_debug"] = false;
    heartbeat["on_idle"] = true;
    heartbeat["just_no_queue"] = false;
    Json packet = Json::object();
    packet["request_buffer_bytes"] = 8192;
    packet["answer_buffer_bytes"] = 65535;
    packet["request_segment_bytes"] = 8192;
    packet["ack_segment_bytes"] = -1;
    Json clusters = Json::array();
    for (const auto& item : std::initializer_list<std::pair<int, const char*>>{
             {100, "TPSYS"}, {200, "TPSUBSYS"}, {300, "TPL2SYS"},
             {1000, "TPSYS"}, {2000, "TPSUBSYS"}, {3000, "TPL2SYS"}}) {
        Json row = Json::object();
        row["id"] = item.first;
        row["name"] = item.second;
        row["default_host_id"] = 1001;
        row["balance"] = true;
        clusters.push_back(std::move(row));
    }
    Json options = Json::array();
    for (const auto* name : {"DisConnect", "LazyTimeOut", "MaxReConTimes",
                             "JobTimeOut", "MaxTimeOutTimes",
                             "LoginStateNoChangeHost", "ReConnectTimeOut",
                             "EnableTimeoutProc"}) options.push_back(name);
    Json result = Json::object();
    result["source"] = "tpbus.dll embedded TAEngine XML and CTDXSession::SetOpt";
    result["source_build"] = "2025-11-14 x86 installation module";
    result["timeouts"] = std::move(timeout);
    result["heartbeat"] = std::move(heartbeat);
    result["packet"] = std::move(packet);
    result["channel_check_connect"] = 3;
    result["clusters"] = std::move(clusters);
    result["session_option_keys"] = std::move(options);
    result["runtime_override_status"] = "not observed in installation text configuration";
    return result;
}

bool sensitive_key(std::string_view value) {
    static const std::set<std::string, std::less<>> exact{
        "tpsession", "tdxtoken", "oid", "reguid", "regphone"
    };
    return exact.count(lower_ascii(std::string(value))) != 0;
}

Json private_state_presence(const fs::path& path) {
    Json result = Json::object();
    result["path"] = path_utf8(path);
    result["exists"] = fs::is_regular_file(path);
    result["values_redacted"] = true;
    result["hashes_emitted"] = false;
    Json fields = Json::array();
    if (fs::is_regular_file(path)) {
        const auto document = parse_ini(bytes_text(path));
        for (const auto& [section, values] : document) {
            for (const auto& [key, value] : values) {
                if (!sensitive_key(key)) continue;
                Json field = Json::object();
                field["section"] = section;
                field["key"] = key;
                field["present"] = !value.empty();
                field["encoded_length"] = static_cast<std::uint64_t>(value.size());
                field["encrypted_envelope"] = value.rfind("@ec", 0) == 0;
                fields.push_back(std::move(field));
            }
        }
    }
    result["field_count"] = static_cast<std::uint64_t>(fields.size());
    result["fields"] = std::move(fields);
    return result;
}

}  // namespace

QuoteEndpointSelection load_public_quote_endpoints(
    const fs::path& root, std::size_t max_endpoints) {
    if (max_endpoints < 1 || max_endpoints > 64)
        throw Error("quote endpoint selection limit must be in 1..64");
    const auto fallback = [&] {
        QuoteEndpointSelection result;
        result.endpoints.push_back(parse_endpoint("110.41.147.114:7709"));
        result.endpoints.front().name = "compiled public fallback";
        result.source = "compiled-default";
        result.available_endpoint_count = 1;
        result.primary_configured = false;
        return result;
    };
    const auto path = root / "connect.cfg";
    if (!fs::is_regular_file(path)) return fallback();
    try {
        const auto document = parse_ini(bytes_text(path));
        const auto section = document.find("hqhost");
        if (section == document.end()) return fallback();
        const auto count = integer_value(section->second, "hostnum");
        if (!count || *count < 1 || *count > 10000) return fallback();
        const auto configured_primary = integer_value(
            section->second, "primaryhost");
        const int primary = std::clamp(
            configured_primary.value_or(0), 0, *count - 1);
        std::vector<std::pair<int, Endpoint>> parsed;
        for (int index = 1; index <= *count; ++index) {
            const auto address = section->second.find(indexed_key("ipaddress", index));
            if (address == section->second.end() || address->second.empty()) continue;
            const int port = integer_value(
                section->second, indexed_key("port", index)).value_or(7709);
            if (port < 1 || port > 65535) continue;
            auto endpoint = parse_endpoint(
                address->second + ":" + std::to_string(port));
            const auto name = section->second.find(indexed_key("hostname", index));
            endpoint.name = name == section->second.end()
                ? "connect.cfg hqhost" : decode_label(name->second);
            parsed.emplace_back(index, std::move(endpoint));
        }
        if (parsed.empty()) return fallback();
        const bool primary_present = std::any_of(
            parsed.begin(), parsed.end(), [&](const auto& value) {
                return value.first - 1 == primary;
            });
        if (primary_present) {
            std::stable_sort(parsed.begin(), parsed.end(), [&](const auto& left,
                                                               const auto& right) {
                const auto rank = [&](int index) {
                    const int ordinal = index - 1;
                    return ordinal >= primary ? ordinal - primary
                                              : *count - primary + ordinal;
                };
                return rank(left.first) < rank(right.first);
            });
        }
        QuoteEndpointSelection result;
        result.source = "connect.cfg:hqhost-primary-first";
        result.available_endpoint_count = parsed.size();
        result.primary_configured = configured_primary.has_value() && primary_present;
        for (auto& [index, endpoint] : parsed) {
            (void)index;
            if (result.endpoints.size() >= max_endpoints) break;
            result.endpoints.push_back(std::move(endpoint));
        }
        return result;
    } catch (const std::exception&) {
        return fallback();
    }
}

QuoteEndpointSelection select_public_quote_endpoints(
    const fs::path& root, const std::vector<std::string>& explicit_hosts,
    std::size_t max_endpoints) {
    if (max_endpoints < 1 || max_endpoints > 64)
        throw Error("quote endpoint selection limit must be in 1..64");
    if (explicit_hosts.empty()) {
        if (!root.empty())
            return load_public_quote_endpoints(root, max_endpoints);
        try {
            return load_public_quote_endpoints(find_tdx_root({}), max_endpoints);
        } catch (const std::exception&) {
            return load_public_quote_endpoints(root, max_endpoints);
        }
    }
    if (explicit_hosts.size() > 64)
        throw Error("at most 64 explicit quote endpoints are allowed");
    QuoteEndpointSelection result;
    result.source = "explicit-host";
    result.available_endpoint_count = explicit_hosts.size();
    for (const auto& host : explicit_hosts) {
        if (result.endpoints.size() >= max_endpoints) break;
        result.endpoints.push_back(parse_endpoint(host));
    }
    return result;
}

Json public_quote_transport_document(
    const QuoteEndpointSelection& selection, int connection_attempts,
    int transient_retries, int endpoints_attempted,
    int max_attempts_per_endpoint) {
    Json transport = Json::object();
    transport["connection_attempts"] = connection_attempts;
    transport["transient_retries"] = transient_retries;
    transport["endpoints_attempted"] = endpoints_attempted;
    transport["max_attempts_per_endpoint"] = max_attempts_per_endpoint;
    transport["recovered_after_retry"] = transient_retries > 0;
    transport["endpoint_source"] = selection.source;
    transport["available_endpoint_count"] =
        static_cast<std::uint64_t>(selection.available_endpoint_count);
    transport["primary_configured"] = selection.primary_configured;
    transport["endpoint_failover"] = endpoints_attempted > 1;
    return transport;
}

Json session_config_document(const fs::path& root) {
    const auto connect_path = root / "connect.cfg";
    Json groups = Json::array();
    if (fs::is_regular_file(connect_path))
        groups = endpoint_groups(parse_ini(bytes_text(connect_path)));

    std::uint64_t endpoint_count = 0;
    std::uint64_t hq_endpoint_count = 0;
    std::uint64_t resolved_group_count = 0;
    std::uint64_t protocol_verified_group_count = 0;
    std::uint64_t unresolved_group_count = 0;
    for (const auto& group : groups.as_array()) {
        const auto count = static_cast<std::uint64_t>(
            group.at("parsed_host_count").as_number());
        endpoint_count += count;
        if (group.at("section").as_string() == "hqhost") hq_endpoint_count = count;
        const auto status = group.at("role").at("status").as_string();
        if (status == "unresolved") ++unresolved_group_count;
        else ++resolved_group_count;
        if (status == "protocol-verified") ++protocol_verified_group_count;
    }

    Json sources = Json::array();
    Json connect_source = Json::object();
    connect_source["path"] = path_utf8(connect_path);
    connect_source["exists"] = fs::is_regular_file(connect_path);
    connect_source["classification"] = "public endpoint configuration";
    sources.push_back(std::move(connect_source));
    Json user_source = Json::object();
    user_source["path"] = path_utf8(root / "T0002" / "user.ini");
    user_source["exists"] = fs::is_regular_file(root / "T0002" / "user.ini");
    user_source["classification"] = "private state; presence metadata only";
    sources.push_back(std::move(user_source));

    Json counts = Json::object();
    counts["endpoint_groups"] = static_cast<std::uint64_t>(groups.size());
    counts["endpoints"] = endpoint_count;
    counts["hq_endpoints"] = hq_endpoint_count;
    counts["resolved_endpoint_groups"] = resolved_group_count;
    counts["protocol_verified_endpoint_groups"] = protocol_verified_group_count;
    counts["unresolved_endpoint_groups"] = unresolved_group_count;

    Json boundary = Json::object();
    boundary["read_only"] = true;
    boundary["network_sent"] = false;
    boundary["dll_loaded"] = false;
    boundary["credentials_emitted"] = false;
    boundary["session_values_emitted"] = false;
    boundary["interpretation"] =
        "endpoint roles are evidence-graded; configured hosts and tpbus/TaApi application sessions are separate layers";

    Json result = Json::object();
    result["schema"] = "tdx-session-config-audit-native-v1";
    result["root"] = path_utf8(root);
    result["counts"] = std::move(counts);
    result["endpoint_groups"] = std::move(groups);
    result["tpbus_static_defaults"] = static_defaults();
    result["private_state"] = private_state_presence(root / "T0002" / "user.ini");
    result["sources"] = std::move(sources);
    result["boundary"] = std::move(boundary);
    return result;
}

int command_recon_session_config(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout <<
            "Usage: tdx-tool recon session-config [options]\n\n"
            "Audit public endpoint groups, recovered tpbus session defaults, and only the\n"
            "presence/length of private local session fields. No values or hashes are emitted.\n\n"
            "Options:\n"
            "  --root PATH       TDX installation root\n"
            "  --output PATH     Default output/tdx-session-config-audit.json\n"
            "  --compact         Compact JSON\n";
        return 0;
    }
    const auto root_text = args.take_option("--root");
    const auto root = find_tdx_root(root_text.empty() ? fs::path{} : from_utf8(root_text));
    const auto output = from_utf8(args.take_option(
        "--output", "output/tdx-session-config-audit.json"));
    const bool compact = args.take_flag("--compact");
    args.require_empty();
    const auto document = session_config_document(root);
    atomic_write_text(output, document.dump(compact ? -1 : 2) + "\n");
    std::cout << "audited "
              << static_cast<std::uint64_t>(document.at("counts").at("endpoints").as_number())
              << " endpoints with private fields redacted -> " << path_utf8(output) << '\n';
    return 0;
}

}  // namespace tdx
