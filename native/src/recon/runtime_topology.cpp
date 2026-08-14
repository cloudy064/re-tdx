#include "tdx/recon.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#include <iphlpapi.h>
#include <tlhelp32.h>
#include <ws2tcpip.h>
#endif

namespace fs = std::filesystem;

namespace tdx {
namespace {

std::string utc_time(std::time_t value) {
    std::tm parts{};
#ifdef _WIN32
    if (gmtime_s(&parts, &value) != 0) return {};
#else
    const auto* converted = std::gmtime(&value);
    if (!converted) return {};
    parts = *converted;
#endif
    std::ostringstream output;
    output << std::put_time(&parts, "%Y-%m-%dT%H:%M:%SZ");
    return output.str();
}

std::string captured_at_utc() {
    return utc_time(std::chrono::system_clock::to_time_t(
        std::chrono::system_clock::now()));
}

const Json* member(const Json& value, std::string_view name) {
    if (!value.is_object()) return nullptr;
    const auto found = value.as_object().find(name);
    return found == value.as_object().end() ? nullptr : &found->second;
}

std::uint64_t unsigned_number(const Json& value, std::string_view name) {
    const auto* item = member(value, name);
    if (!item || !item->is_number()) throw Error("runtime topology field is missing: " +
                                                  std::string(name));
    const auto number = item->as_number();
    if (number < 0) throw Error("runtime topology field is negative: " + std::string(name));
    return static_cast<std::uint64_t>(number);
}

std::string string_value(const Json& value, std::string_view name) {
    const auto* item = member(value, name);
    if (!item || !item->is_string()) throw Error("runtime topology field is missing: " +
                                                  std::string(name));
    return item->as_string();
}

std::string process_key(const Json& process) {
    auto result = std::to_string(unsigned_number(process, "pid"));
    const auto* started = member(process, "started_at_utc");
    if (started && started->is_string() && !started->as_string().empty())
        result += "|" + started->as_string();
    return result;
}

std::string module_key(const Json& process, const Json& module) {
    return process_key(process) + "|" + lower_ascii(string_value(module, "path"));
}

std::string connection_key(const Json& process, const Json& connection) {
    return process_key(process) + "|" + string_value(connection, "protocol") + "|" +
           string_value(connection, "address_family") + "|" +
           string_value(connection, "local_address") + ":" +
           std::to_string(unsigned_number(connection, "local_port")) + "|" +
           string_value(connection, "remote_address") + ":" +
           std::to_string(unsigned_number(connection, "remote_port")) + "|" +
           string_value(connection, "state");
}

using FlatItems = std::map<std::string, Json>;

FlatItems flatten(const Json& document, std::string_view child) {
    const auto* processes = member(document, "processes");
    if (!processes || !processes->is_array())
        throw Error("runtime topology document has no processes array");
    FlatItems result;
    for (const auto& process : processes->as_array()) {
        if (child.empty()) {
            result.emplace(process_key(process), process);
            continue;
        }
        const auto* items = member(process, child);
        if (!items || !items->is_array()) continue;
        for (const auto& item : items->as_array()) {
            const auto key = child == "modules"
                ? module_key(process, item)
                : connection_key(process, item);
            Json row = item;
            row["pid"] = unsigned_number(process, "pid");
            result.emplace(key, std::move(row));
        }
    }
    return result;
}

Json set_difference(const FlatItems& left, const FlatItems& right) {
    Json result = Json::array();
    for (const auto& [key, value] : left)
        if (right.find(key) == right.end()) result.push_back(value);
    return result;
}

#ifdef _WIN32

struct ProcessEntry {
    DWORD pid{};
    DWORD parent_pid{};
    DWORD thread_count{};
    std::string name;
    bool direct_match{};
    DWORD root_pid{};
    std::uint32_t tree_depth{};
    bool parent_present{};
    std::string parent_name;
};

struct ConnectionEntry {
    DWORD pid{};
    std::string family;
    std::string state;
    std::string local_address;
    std::uint16_t local_port{};
    std::string remote_address;
    std::uint16_t remote_port{};
};

std::string windows_error(DWORD code) {
    return "Win32 error " + std::to_string(static_cast<std::uint64_t>(code));
}

std::string filetime_utc(const FILETIME& value) {
    ULARGE_INTEGER ticks{};
    ticks.LowPart = value.dwLowDateTime;
    ticks.HighPart = value.dwHighDateTime;
    constexpr std::uint64_t epoch = 116444736000000000ULL;
    if (ticks.QuadPart < epoch) return {};
    return utc_time(static_cast<std::time_t>((ticks.QuadPart - epoch) / 10000000ULL));
}

std::string address_v4(DWORD value) {
    in_addr address{};
    address.s_addr = value;
    char text[INET_ADDRSTRLEN]{};
    return InetNtopA(AF_INET, &address, text, sizeof(text)) ? text : "";
}

std::string address_v6(const UCHAR* value) {
    in6_addr address{};
    std::copy(value, value + 16, address.u.Byte);
    char text[INET6_ADDRSTRLEN]{};
    return InetNtopA(AF_INET6, &address, text, sizeof(text)) ? text : "";
}

std::uint16_t port_value(DWORD value) {
    return ntohs(static_cast<u_short>(value));
}

std::string tcp_state(DWORD state) {
    switch (state) {
        case MIB_TCP_STATE_CLOSED: return "closed";
        case MIB_TCP_STATE_LISTEN: return "listen";
        case MIB_TCP_STATE_SYN_SENT: return "syn-sent";
        case MIB_TCP_STATE_SYN_RCVD: return "syn-received";
        case MIB_TCP_STATE_ESTAB: return "established";
        case MIB_TCP_STATE_FIN_WAIT1: return "fin-wait-1";
        case MIB_TCP_STATE_FIN_WAIT2: return "fin-wait-2";
        case MIB_TCP_STATE_CLOSE_WAIT: return "close-wait";
        case MIB_TCP_STATE_CLOSING: return "closing";
        case MIB_TCP_STATE_LAST_ACK: return "last-ack";
        case MIB_TCP_STATE_TIME_WAIT: return "time-wait";
        case MIB_TCP_STATE_DELETE_TCB: return "delete-tcb";
        default: return "state-" + std::to_string(static_cast<std::uint64_t>(state));
    }
}

void capture_ipv4(std::vector<ConnectionEntry>& result, Json& errors) {
    DWORD size = 0;
    auto status = GetExtendedTcpTable(nullptr, &size, FALSE, AF_INET,
                                      TCP_TABLE_OWNER_PID_ALL, 0);
    if (status != ERROR_INSUFFICIENT_BUFFER && status != NO_ERROR) {
        errors.push_back("IPv4 TCP table: " + windows_error(status));
        return;
    }
    std::vector<std::uint8_t> buffer(size);
    status = GetExtendedTcpTable(buffer.data(), &size, FALSE, AF_INET,
                                 TCP_TABLE_OWNER_PID_ALL, 0);
    if (status != NO_ERROR) {
        errors.push_back("IPv4 TCP table: " + windows_error(status));
        return;
    }
    const auto* table = reinterpret_cast<const MIB_TCPTABLE_OWNER_PID*>(buffer.data());
    for (DWORD index = 0; index < table->dwNumEntries; ++index) {
        const auto& row = table->table[index];
        result.push_back({row.dwOwningPid, "ipv4", tcp_state(row.dwState),
                          address_v4(row.dwLocalAddr), port_value(row.dwLocalPort),
                          address_v4(row.dwRemoteAddr), port_value(row.dwRemotePort)});
    }
}

void capture_ipv6(std::vector<ConnectionEntry>& result, Json& errors) {
    DWORD size = 0;
    auto status = GetExtendedTcpTable(nullptr, &size, FALSE, AF_INET6,
                                      TCP_TABLE_OWNER_PID_ALL, 0);
    if (status != ERROR_INSUFFICIENT_BUFFER && status != NO_ERROR) {
        errors.push_back("IPv6 TCP table: " + windows_error(status));
        return;
    }
    std::vector<std::uint8_t> buffer(size);
    status = GetExtendedTcpTable(buffer.data(), &size, FALSE, AF_INET6,
                                 TCP_TABLE_OWNER_PID_ALL, 0);
    if (status != NO_ERROR) {
        errors.push_back("IPv6 TCP table: " + windows_error(status));
        return;
    }
    const auto* table = reinterpret_cast<const MIB_TCP6TABLE_OWNER_PID*>(buffer.data());
    for (DWORD index = 0; index < table->dwNumEntries; ++index) {
        const auto& row = table->table[index];
        result.push_back({row.dwOwningPid, "ipv6", tcp_state(row.dwState),
                          address_v6(row.ucLocalAddr), port_value(row.dwLocalPort),
                          address_v6(row.ucRemoteAddr), port_value(row.dwRemotePort)});
    }
}

std::vector<ConnectionEntry> capture_connections(Json& errors) {
    std::vector<ConnectionEntry> result;
    capture_ipv4(result, errors);
    capture_ipv6(result, errors);
    std::sort(result.begin(), result.end(), [](const auto& left, const auto& right) {
        return std::tie(left.pid, left.family, left.local_address, left.local_port,
                        left.remote_address, left.remote_port, left.state) <
               std::tie(right.pid, right.family, right.local_address, right.local_port,
                        right.remote_address, right.remote_port, right.state);
    });
    return result;
}

bool process_name_matches(const std::string& actual, std::string requested) {
    auto normalized_actual = lower_ascii(actual);
    requested = lower_ascii(trim(std::move(requested)));
    if (requested.empty()) requested = "tdxw.exe";
    if (normalized_actual == requested) return true;
    if (requested.size() < 4 || requested.substr(requested.size() - 4) != ".exe")
        return normalized_actual == requested + ".exe";
    return false;
}

std::vector<ProcessEntry> enumerate_processes(const RuntimeTopologyOptions& options) {
    const auto snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
        throw Error("cannot enumerate processes: " + windows_error(GetLastError()));
    std::vector<ProcessEntry> all;
    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    if (Process32FirstW(snapshot, &entry)) {
        do {
            ProcessEntry item;
            item.pid = entry.th32ProcessID;
            item.parent_pid = entry.th32ParentProcessID;
            item.thread_count = entry.cntThreads;
            item.name = wide_to_utf8(entry.szExeFile);
            all.push_back(std::move(item));
        } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);

    std::set<DWORD> direct;
    for (const auto& entry : all) {
        if ((options.process_id && entry.pid == options.process_id) ||
            (!options.process_id && process_name_matches(entry.name, options.process_name)))
            direct.insert(entry.pid);
    }
    std::set<DWORD> included = direct;
    if (options.include_descendants) {
        bool changed = true;
        while (changed) {
            changed = false;
            for (const auto& entry : all) {
                if (!included.count(entry.pid) && included.count(entry.parent_pid)) {
                    included.insert(entry.pid);
                    changed = true;
                }
            }
        }
    }

    std::map<DWORD, const ProcessEntry*> by_pid;
    for (const auto& entry : all) by_pid.emplace(entry.pid, &entry);
    std::vector<ProcessEntry> result;
    for (const auto& source : all) {
        if (!included.count(source.pid)) continue;
        auto entry = source;
        entry.direct_match = direct.count(entry.pid) != 0;
        const auto direct_parent = by_pid.find(entry.parent_pid);
        if (direct_parent != by_pid.end()) {
            entry.parent_present = true;
            entry.parent_name = direct_parent->second->name;
        }
        entry.root_pid = entry.pid;
        std::set<DWORD> visited{entry.pid};
        auto parent = entry.parent_pid;
        while (included.count(parent) && !visited.count(parent)) {
            const auto found = by_pid.find(parent);
            if (found == by_pid.end()) break;
            visited.insert(parent);
            entry.root_pid = parent;
            ++entry.tree_depth;
            parent = found->second->parent_pid;
        }
        result.push_back(std::move(entry));
    }
    std::sort(result.begin(), result.end(), [](const auto& left, const auto& right) {
        return left.pid < right.pid;
    });
    return result;
}

bool path_is_local(std::string module_path, std::string root_path) {
    module_path = lower_ascii(std::move(module_path));
    root_path = lower_ascii(std::move(root_path));
    while (!root_path.empty() && (root_path.back() == '\\' || root_path.back() == '/'))
        root_path.pop_back();
    if (module_path == root_path) return true;
    if (module_path.size() <= root_path.size() ||
        module_path.compare(0, root_path.size(), root_path) != 0) return false;
    return module_path[root_path.size()] == '\\' || module_path[root_path.size()] == '/';
}

Json process_document(const ProcessEntry& entry, const RuntimeTopologyOptions& options,
                      const std::vector<ConnectionEntry>& connections) {
    Json result = Json::object();
    result["pid"] = static_cast<std::uint64_t>(entry.pid);
    result["parent_pid"] = static_cast<std::uint64_t>(entry.parent_pid);
    result["thread_count"] = static_cast<std::uint64_t>(entry.thread_count);
    result["image_name"] = entry.name;
    result["direct_match"] = entry.direct_match;
    result["root_pid"] = static_cast<std::uint64_t>(entry.root_pid);
    result["tree_depth"] = static_cast<std::uint64_t>(entry.tree_depth);
    result["descendant"] = entry.tree_depth != 0;
    result["parent_present"] = entry.parent_present;
    result["parent_name"] = entry.parent_name;
    result["orphaned_parent"] = entry.parent_pid != 0 && !entry.parent_present;
    result["queryable"] = false;

    const auto handle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, entry.pid);
    std::string image_path;
    std::string image_root;
    if (handle) {
        std::wstring buffer(32768, L'\0');
        DWORD size = static_cast<DWORD>(buffer.size());
        if (QueryFullProcessImageNameW(handle, 0, buffer.data(), &size)) {
            buffer.resize(size);
            image_path = path_utf8(fs::path(buffer));
            image_root = path_utf8(fs::path(buffer).parent_path());
            result["image_path"] = image_path;
            result["queryable"] = true;
        } else {
            result["query_error"] = windows_error(GetLastError());
        }
        FILETIME created{}, exited{}, kernel{}, user{};
        if (GetProcessTimes(handle, &created, &exited, &kernel, &user))
            result["started_at_utc"] = filetime_utc(created);
        BOOL wow64 = FALSE;
        if (IsWow64Process(handle, &wow64)) result["wow64"] = wow64 != FALSE;
        CloseHandle(handle);
    } else {
        result["query_error"] = windows_error(GetLastError());
    }

    Json modules = Json::array();
    std::uint64_t module_total = 0;
    if (options.include_modules) {
        const auto snapshot = CreateToolhelp32Snapshot(
            TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, entry.pid);
        if (snapshot == INVALID_HANDLE_VALUE) {
            result["module_scan_error"] = windows_error(GetLastError());
        } else {
            MODULEENTRY32W module{};
            module.dwSize = sizeof(module);
            if (Module32FirstW(snapshot, &module)) {
                do {
                    ++module_total;
                    const auto path = wide_to_utf8(module.szExePath);
                    const bool local = !image_root.empty() && path_is_local(path, image_root);
                    if (!options.all_modules && !local) continue;
                    Json item = Json::object();
                    item["name"] = wide_to_utf8(module.szModule);
                    item["path"] = path;
                    item["size_bytes"] = static_cast<std::uint64_t>(module.modBaseSize);
                    std::ostringstream address;
                    address << "0x" << std::hex << reinterpret_cast<std::uintptr_t>(module.modBaseAddr);
                    item["base_address"] = address.str();
                    item["local_to_process_image"] = local;
                    modules.push_back(std::move(item));
                } while (Module32NextW(snapshot, &module));
            }
            CloseHandle(snapshot);
        }
        std::sort(modules.as_array().begin(), modules.as_array().end(), [](const auto& left,
                                                                          const auto& right) {
            return lower_ascii(left.at("path").as_string()) <
                   lower_ascii(right.at("path").as_string());
        });
    }
    result["module_scan_enabled"] = options.include_modules;
    result["module_scope"] = !options.include_modules ? "none" :
                             options.all_modules ? "all" : "local";
    result["module_total_count"] = module_total;
    result["module_count"] = static_cast<std::uint64_t>(modules.size());
    result["modules"] = std::move(modules);

    Json rows = Json::array();
    if (options.include_connections) {
        for (const auto& connection : connections) {
            if (connection.pid != entry.pid) continue;
            Json item = Json::object();
            item["protocol"] = "tcp";
            item["address_family"] = connection.family;
            item["state"] = connection.state;
            item["local_address"] = connection.local_address;
            item["local_port"] = connection.local_port;
            item["remote_address"] = connection.remote_address;
            item["remote_port"] = connection.remote_port;
            rows.push_back(std::move(item));
        }
    }
    result["connection_scan_enabled"] = options.include_connections;
    result["connection_count"] = static_cast<std::uint64_t>(rows.size());
    result["connections"] = std::move(rows);
    return result;
}

#endif

}  // namespace

Json runtime_topology_document(const RuntimeTopologyOptions& options) {
    Json result = Json::object();
    result["schema"] = "tdx-runtime-topology-v1";
    result["captured_at_utc"] = captured_at_utc();
    result["read_only"] = true;
    result["process_memory_read"] = false;
    result["attached"] = false;
    result["injected"] = false;
    result["requested_process"] = options.process_name;
    result["requested_pid"] = static_cast<std::uint64_t>(options.process_id);
    result["module_scope"] = !options.include_modules ? "none" :
                             options.all_modules ? "all" : "local";
    result["connections_requested"] = options.include_connections;
    result["descendants_requested"] = options.include_descendants;
    result["tool_pointer_bits"] = static_cast<std::uint64_t>(sizeof(void*) * 8);
    Json processes = Json::array();
    Json connection_errors = Json::array();
#ifdef _WIN32
    result["supported"] = true;
    const auto entries = enumerate_processes(options);
    const auto connections = options.include_connections
        ? capture_connections(connection_errors)
        : std::vector<ConnectionEntry>{};
    for (const auto& entry : entries)
        processes.push_back(process_document(entry, options, connections));
#else
    result["supported"] = false;
    result["reason"] = "runtime topology capture is only available on Windows";
#endif
    result["connection_scan_errors"] = std::move(connection_errors);
    result["process_count"] = static_cast<std::uint64_t>(processes.size());
    result["running"] = !processes.as_array().empty();

    std::uint64_t module_count = 0;
    std::uint64_t connection_count = 0;
    std::uint64_t established_count = 0;
    std::uint64_t listener_count = 0;
    std::uint64_t direct_match_count = 0;
    std::uint64_t descendant_process_count = 0;
    std::uint64_t additional_descendant_count = 0;
    std::uint64_t orphaned_parent_count = 0;
    std::set<std::string> module_names;
    std::set<std::string> remote_endpoints;
    std::set<std::uint64_t> root_pids;
    for (const auto& process : processes.as_array()) {
        if (process.at("direct_match").as_bool()) ++direct_match_count;
        if (process.at("descendant").as_bool()) ++descendant_process_count;
        if (!process.at("direct_match").as_bool()) ++additional_descendant_count;
        if (process.at("orphaned_parent").as_bool()) ++orphaned_parent_count;
        root_pids.insert(unsigned_number(process, "root_pid"));
        const auto* modules = member(process, "modules");
        if (modules && modules->is_array()) {
            module_count += modules->size();
            for (const auto& module : modules->as_array())
                module_names.insert(string_value(module, "name"));
        }
        const auto* connections = member(process, "connections");
        if (!connections || !connections->is_array()) continue;
        connection_count += connections->size();
        for (const auto& connection : connections->as_array()) {
            const auto state = string_value(connection, "state");
            if (state == "established") ++established_count;
            if (state == "listen") ++listener_count;
            const auto port = unsigned_number(connection, "remote_port");
            const auto address = string_value(connection, "remote_address");
            if (state == "established" && port && address != "0.0.0.0" && address != "::") {
                const auto family = string_value(connection, "address_family");
                remote_endpoints.insert(family == "ipv6"
                    ? "[" + address + "]:" + std::to_string(port)
                    : address + ":" + std::to_string(port));
            }
        }
    }
    Json summary = Json::object();
    summary["process_count"] = static_cast<std::uint64_t>(processes.size());
    summary["direct_match_count"] = direct_match_count;
    summary["root_process_count"] = static_cast<std::uint64_t>(root_pids.size());
    summary["descendant_process_count"] = descendant_process_count;
    summary["additional_descendant_count"] = additional_descendant_count;
    summary["orphaned_parent_count"] = orphaned_parent_count;
    summary["root_pids"] = Json::array();
    for (const auto pid : root_pids) summary["root_pids"].push_back(pid);
    summary["module_count"] = module_count;
    summary["connection_count"] = connection_count;
    summary["established_connection_count"] = established_count;
    summary["listener_count"] = listener_count;
    summary["module_names"] = Json::array();
    for (const auto& name : module_names) summary["module_names"].push_back(name);
    summary["remote_endpoints"] = Json::array();
    for (const auto& endpoint : remote_endpoints) summary["remote_endpoints"].push_back(endpoint);
    result["summary"] = std::move(summary);
    result["processes"] = std::move(processes);
    return result;
}

Json compare_runtime_topology_documents(const Json& baseline, const Json& current) {
    if (string_value(baseline, "schema") != "tdx-runtime-topology-v1" ||
        string_value(current, "schema") != "tdx-runtime-topology-v1")
        throw Error("runtime topology comparison requires two tdx-runtime-topology-v1 documents");
    const auto old_processes = flatten(baseline, {});
    const auto new_processes = flatten(current, {});
    const auto old_modules = flatten(baseline, "modules");
    const auto new_modules = flatten(current, "modules");
    const auto old_connections = flatten(baseline, "connections");
    const auto new_connections = flatten(current, "connections");

    Json result = Json::object();
    result["schema"] = "tdx-runtime-topology-diff-v1";
    result["baseline_captured_at_utc"] = string_value(baseline, "captured_at_utc");
    result["current_captured_at_utc"] = string_value(current, "captured_at_utc");
    result["added_processes"] = set_difference(new_processes, old_processes);
    result["removed_processes"] = set_difference(old_processes, new_processes);
    result["added_modules"] = set_difference(new_modules, old_modules);
    result["removed_modules"] = set_difference(old_modules, new_modules);
    result["added_connections"] = set_difference(new_connections, old_connections);
    result["removed_connections"] = set_difference(old_connections, new_connections);
    result["added_process_count"] = static_cast<std::uint64_t>(result.at("added_processes").size());
    result["removed_process_count"] = static_cast<std::uint64_t>(result.at("removed_processes").size());
    result["added_module_count"] = static_cast<std::uint64_t>(result.at("added_modules").size());
    result["removed_module_count"] = static_cast<std::uint64_t>(result.at("removed_modules").size());
    result["added_connection_count"] = static_cast<std::uint64_t>(result.at("added_connections").size());
    result["removed_connection_count"] = static_cast<std::uint64_t>(result.at("removed_connections").size());
    result["changed"] = result.at("added_processes").size() ||
                        result.at("removed_processes").size() ||
                        result.at("added_modules").size() ||
                        result.at("removed_modules").size() ||
                        result.at("added_connections").size() ||
                        result.at("removed_connections").size();
    return result;
}

}  // namespace tdx
