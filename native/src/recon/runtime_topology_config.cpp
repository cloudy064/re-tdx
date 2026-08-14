#include "tdx/recon.hpp"

#include "tdx/common.hpp"

#include <map>
#include <set>
#include <string>
#include <string_view>

namespace tdx {
namespace {

const Json* member(const Json& value, std::string_view name) {
    if (!value.is_object()) return nullptr;
    const auto found = value.as_object().find(name);
    return found == value.as_object().end() ? nullptr : &found->second;
}

const Json& required_member(const Json& value, std::string_view name) {
    const auto* result = member(value, name);
    if (!result) throw Error("runtime endpoint correlation has no " + std::string(name));
    return *result;
}

std::uint64_t unsigned_number(const Json& value, std::string_view name) {
    const auto& item = required_member(value, name);
    if (!item.is_number() || item.as_number() < 0)
        throw Error("runtime endpoint correlation field is not unsigned: " +
                    std::string(name));
    return static_cast<std::uint64_t>(item.as_number());
}

std::string string_value(const Json& value, std::string_view name) {
    const auto& item = required_member(value, name);
    if (!item.is_string())
        throw Error("runtime endpoint correlation field is not text: " +
                    std::string(name));
    return item.as_string();
}

std::string normalized_address(std::string value) {
    value = lower_ascii(trim(std::move(value)));
    if (value.size() >= 2 && value.front() == '[' && value.back() == ']')
        value = value.substr(1, value.size() - 2);
    return value;
}

std::string endpoint_key(const std::string& address, std::uint64_t port) {
    return normalized_address(address) + "\n" + std::to_string(port);
}

std::string endpoint_label(const Json& connection) {
    const auto address = string_value(connection, "remote_address");
    const auto port = unsigned_number(connection, "remote_port");
    return string_value(connection, "address_family") == "ipv6"
        ? "[" + address + "]:" + std::to_string(port)
        : address + ":" + std::to_string(port);
}

using EndpointCatalog = std::multimap<std::string, Json>;

EndpointCatalog build_catalog(const Json& session_config, std::uint64_t& group_count,
                              std::uint64_t& endpoint_count) {
    if (string_value(session_config, "schema") !=
        "tdx-session-config-audit-native-v1")
        throw Error("runtime endpoint correlation requires a session config audit document");
    const auto& groups = required_member(session_config, "endpoint_groups");
    if (!groups.is_array())
        throw Error("session config endpoint_groups is not an array");
    EndpointCatalog result;
    group_count = groups.size();
    for (const auto& group : groups.as_array()) {
        const auto section = string_value(group, "section");
        const auto* role = member(group, "role");
        const auto& endpoints = required_member(group, "endpoints");
        if (!endpoints.is_array())
            throw Error("session config group endpoints is not an array");
        for (const auto& endpoint : endpoints.as_array()) {
            const auto address = string_value(endpoint, "address");
            const auto port = unsigned_number(endpoint, "port");
            Json match = Json::object();
            match["section"] = section;
            if (role && role->is_object()) {
                if (const auto* value = member(*role, "role"))
                    match["role"] = *value;
                if (const auto* value = member(*role, "status"))
                    match["role_status"] = *value;
                if (const auto* value = member(*role, "protocol_family"))
                    match["protocol_family"] = *value;
            }
            match["index"] = unsigned_number(endpoint, "index");
            match["name"] = string_value(endpoint, "name");
            match["primary"] = required_member(endpoint, "primary").as_bool();
            match["configured_address"] = address;
            match["configured_port"] = port;
            match["configured_endpoint"] = string_value(endpoint, "endpoint");
            match["match_method"] = "exact-address-and-port";
            result.emplace(endpoint_key(address, port), std::move(match));
            ++endpoint_count;
        }
    }
    return result;
}

}  // namespace

Json annotate_runtime_topology_endpoints(Json snapshot, const Json& session_config) {
    if (string_value(snapshot, "schema") != "tdx-runtime-topology-v1")
        throw Error("endpoint correlation requires a tdx-runtime-topology-v1 snapshot");
    std::uint64_t configured_groups = 0;
    std::uint64_t configured_endpoints = 0;
    const auto catalog = build_catalog(session_config, configured_groups,
                                       configured_endpoints);

    std::uint64_t correlatable_connections = 0;
    std::uint64_t matched_connections = 0;
    std::set<std::string> matched_sections;
    std::set<std::string> unmatched_established;
    auto& processes = snapshot["processes"].as_array();
    for (auto& process : processes) {
        auto& connections = process["connections"].as_array();
        std::uint64_t process_matches = 0;
        for (auto& connection : connections) {
            Json matches = Json::array();
            const auto remote_port = unsigned_number(connection, "remote_port");
            const auto remote_address = string_value(connection, "remote_address");
            const bool correlatable = string_value(connection, "protocol") == "tcp" &&
                                      remote_port != 0 && remote_address != "0.0.0.0" &&
                                      remote_address != "::";
            if (correlatable) {
                ++correlatable_connections;
                const auto range = catalog.equal_range(endpoint_key(remote_address, remote_port));
                for (auto found = range.first; found != range.second; ++found) {
                    matches.push_back(found->second);
                    matched_sections.insert(found->second.at("section").as_string());
                }
            }
            const auto count = static_cast<std::uint64_t>(matches.size());
            connection["configured_endpoint"] = count != 0;
            connection["configured_endpoint_match_count"] = count;
            connection["configured_endpoint_matches"] = std::move(matches);
            if (count) {
                ++matched_connections;
                ++process_matches;
            } else if (correlatable && string_value(connection, "state") == "established") {
                unmatched_established.insert(endpoint_label(connection));
            }
        }
        process["configured_remote_connection_count"] = process_matches;
    }

    auto& summary = snapshot["summary"];
    summary["correlatable_remote_connection_count"] = correlatable_connections;
    summary["configured_remote_connection_count"] = matched_connections;
    summary["unmatched_established_remote_endpoints"] = Json::array();
    for (const auto& endpoint : unmatched_established)
        summary["unmatched_established_remote_endpoints"].push_back(endpoint);
    summary["matched_config_sections"] = Json::array();
    for (const auto& section : matched_sections)
        summary["matched_config_sections"].push_back(section);

    Json correlation = Json::object();
    correlation["enabled"] = true;
    correlation["available"] = configured_endpoints != 0;
    correlation["configuration_schema"] = string_value(session_config, "schema");
    correlation["configuration_root"] = string_value(session_config, "root");
    correlation["configured_group_count"] = configured_groups;
    correlation["configured_endpoint_count"] = configured_endpoints;
    correlation["matched_connection_count"] = matched_connections;
    correlation["matching_method"] = "exact-configured-address-and-port";
    correlation["dns_resolved"] = false;
    correlation["network_sent"] = false;
    correlation["private_state_included"] = false;
    correlation["credentials_emitted"] = false;
    snapshot["endpoint_correlation"] = std::move(correlation);
    return snapshot;
}

}  // namespace tdx
