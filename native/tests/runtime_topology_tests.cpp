#include "tdx/recon.hpp"

#include "tdx/common.hpp"

#include <chrono>
#include <iostream>
#include <string>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) throw tdx::Error(message);
}

tdx::Json sample(std::uint64_t remote_port, bool extra_module) {
    auto document = tdx::Json::object();
    document["schema"] = "tdx-runtime-topology-v1";
    document["captured_at_utc"] = extra_module ? "2026-08-12T01:00:01Z" :
                                                  "2026-08-12T01:00:00Z";
    auto process = tdx::Json::object();
    process["pid"] = 42;
    process["modules"] = tdx::Json::array();
    auto first_module = tdx::Json::object();
    first_module["path"] = "C:/tdx/TdxW.exe";
    process["modules"].push_back(std::move(first_module));
    if (extra_module) {
        auto second_module = tdx::Json::object();
        second_module["path"] = "C:/tdx/tpbus.dll";
        process["modules"].push_back(std::move(second_module));
    }
    process["connections"] = tdx::Json::array();
    auto connection = tdx::Json::object();
    connection["protocol"] = "tcp";
    connection["address_family"] = "ipv4";
    connection["state"] = "established";
    connection["local_address"] = "127.0.0.1";
    connection["local_port"] = 10000;
    connection["remote_address"] = "127.0.0.2";
    connection["remote_port"] = remote_port;
    process["connections"].push_back(std::move(connection));
    document["processes"] = tdx::Json::array();
    document["processes"].push_back(std::move(process));
    document["connection_scan_errors"] = tdx::Json::array();
    document["summary"] = tdx::Json::object();
    document["summary"]["process_count"] = 1;
    document["summary"]["direct_match_count"] = 1;
    document["summary"]["root_process_count"] = 1;
    document["summary"]["descendant_process_count"] = 0;
    document["summary"]["additional_descendant_count"] = 0;
    document["summary"]["orphaned_parent_count"] = 0;
    document["summary"]["root_pids"] = tdx::Json::array();
    document["summary"]["root_pids"].push_back(42);
    document["summary"]["module_count"] = extra_module ? 2 : 1;
    document["summary"]["connection_count"] = 1;
    document["summary"]["established_connection_count"] = 1;
    document["summary"]["listener_count"] = 0;
    document["summary"]["module_names"] = tdx::Json::array();
    document["summary"]["module_names"].push_back("TdxW.exe");
    if (extra_module) document["summary"]["module_names"].push_back("tpbus.dll");
    document["summary"]["remote_endpoints"] = tdx::Json::array();
    document["summary"]["remote_endpoints"].push_back(
        "127.0.0.2:" + std::to_string(remote_port));
    return document;
}

tdx::Json session_config_sample() {
    auto document = tdx::Json::object();
    document["schema"] = "tdx-session-config-audit-native-v1";
    document["root"] = "C:/tdx";
    document["endpoint_groups"] = tdx::Json::array();
    auto group = tdx::Json::object();
    group["section"] = "hqhost";
    group["role"] = tdx::Json::object();
    group["role"]["role"] = "public_quote";
    group["role"]["status"] = "protocol-verified";
    group["role"]["protocol_family"] = "tdx-public-quote";
    group["endpoints"] = tdx::Json::array();
    auto endpoint = tdx::Json::object();
    endpoint["index"] = 2;
    endpoint["address"] = "127.0.0.2";
    endpoint["port"] = 7709;
    endpoint["endpoint"] = "127.0.0.2:7709";
    endpoint["name"] = "Primary quote";
    endpoint["primary"] = true;
    group["endpoints"].push_back(std::move(endpoint));
    document["endpoint_groups"].push_back(std::move(group));
    return document;
}

}  // namespace

int main(int argc, char** argv) {
    try {
#ifdef _WIN32
        if (argc == 2 && std::string(argv[1]) == "--runtime-topology-child") {
            std::this_thread::sleep_for(std::chrono::milliseconds(1500));
            return 0;
        }
#endif
        const auto diff = tdx::compare_runtime_topology_documents(
            sample(7709, false), sample(7710, true));
        require(diff.at("changed").as_bool(), "topology diff changed");
        require(diff.at("added_module_count").as_number() == 1,
                "topology added module count");
        require(diff.at("added_connection_count").as_number() == 1 &&
                diff.at("removed_connection_count").as_number() == 1,
                "topology connection replacement");
        const auto matched = tdx::annotate_runtime_topology_endpoints(
            sample(7709, false), session_config_sample());
        const auto& matched_connection = matched.at("processes").as_array().front()
            .at("connections").as_array().front();
        require(matched_connection.at("configured_endpoint").as_bool() &&
                matched_connection.at("configured_endpoint_match_count").as_number() == 1 &&
                 matched_connection.at("configured_endpoint_matches").as_array().front()
                     .at("section").as_string() == "hqhost" &&
                 matched_connection.at("configured_endpoint_matches").as_array().front()
                     .at("role").as_string() == "public_quote" &&
                 matched_connection.at("configured_endpoint_matches").as_array().front()
                     .at("role_status").as_string() == "protocol-verified" &&
                 matched_connection.at("configured_endpoint_matches").as_array().front()
                     .at("protocol_family").as_string() == "tdx-public-quote" &&
                 matched.at("summary").at("configured_remote_connection_count").as_number() == 1,
                "runtime configured endpoint correlation");
        const auto unmatched = tdx::annotate_runtime_topology_endpoints(
            sample(7710, false), session_config_sample());
        require(!unmatched.at("processes").as_array().front().at("connections")
                    .as_array().front().at("configured_endpoint").as_bool() &&
                unmatched.at("summary").at("unmatched_established_remote_endpoints").size() == 1,
                "runtime unmatched endpoint remains explicit");
        const auto correlated_watch = tdx::runtime_topology_watch_document(
            {matched, unmatched}, 100, 100);
        require(correlated_watch.at("peak_configured_connection_count").as_number() == 1 &&
                correlated_watch.at("observed_config_sections").size() == 1 &&
                correlated_watch.at("observed_config_sections").as_array().front()
                    .as_string() == "hqhost" &&
                correlated_watch.at("endpoint_correlation").at("network_sent").as_bool() == false,
                "runtime watch retains configured endpoint evidence");
        auto old_generation = sample(7709, false);
        auto new_generation = sample(7709, false);
        old_generation["processes"].as_array().front()["started_at_utc"] =
            "2026-08-12T01:00:00Z";
        new_generation["processes"].as_array().front()["started_at_utc"] =
            "2026-08-12T01:00:02Z";
        const auto restarted = tdx::compare_runtime_topology_documents(
            old_generation, new_generation);
        require(restarted.at("added_process_count").as_number() == 1 &&
                restarted.at("removed_process_count").as_number() == 1,
                "topology pid generation identity");
        const auto watch = tdx::runtime_topology_watch_document(
            {sample(7709, false), sample(7710, true), sample(7710, true)}, 200, 100);
        require(watch.at("schema").as_string() == "tdx-runtime-topology-watch-v1",
                "topology watch schema");
        require(watch.at("samples_collected").as_number() == 3 &&
                watch.at("change_event_count").as_number() == 1 &&
                watch.at("stable_comparison_count").as_number() == 1,
                "topology watch event compaction");
        require(watch.at("peak_module_count").as_number() == 2 &&
                watch.at("observed_remote_endpoints").size() == 2,
                "topology watch observed peaks");
#ifdef _WIN32
        std::wstring executable(32768, L'\0');
        const auto executable_size = GetModuleFileNameW(
            nullptr, executable.data(), static_cast<DWORD>(executable.size()));
        require(executable_size != 0 && executable_size < executable.size(),
                "test executable path");
        executable.resize(executable_size);
        std::wstring child_command = L"\"" + executable +
                                     L"\" --runtime-topology-child";
        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        PROCESS_INFORMATION child{};
        require(CreateProcessW(nullptr, child_command.data(), nullptr, nullptr, FALSE,
                               CREATE_NO_WINDOW, nullptr, nullptr, &startup, &child) != FALSE,
                "spawn topology child");

        tdx::RuntimeTopologyOptions tree_options;
        tree_options.process_id = GetCurrentProcessId();
        tree_options.include_modules = false;
        tree_options.include_connections = false;
        tdx::Json tree;
        for (int attempt = 0; attempt < 40; ++attempt) {
            tree = tdx::runtime_topology_document(tree_options);
            if (tree.at("process_count").as_number() >= 2) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(25));
        }
        require(tree.at("process_count").as_number() >= 2,
                "recursive child process capture");
        require(tree.at("summary").at("direct_match_count").as_number() == 1 &&
                tree.at("summary").at("descendant_process_count").as_number() >= 1 &&
                tree.at("summary").at("additional_descendant_count").as_number() >= 1,
                "recursive child process summary");
        bool child_found = false;
        for (const auto& process : tree.at("processes").as_array()) {
            if (process.at("pid").as_number() != child.dwProcessId) continue;
            child_found = !process.at("direct_match").as_bool() &&
                          process.at("descendant").as_bool() &&
                          process.at("root_pid").as_number() == GetCurrentProcessId() &&
                          process.at("tree_depth").as_number() == 1 &&
                          process.at("parent_present").as_bool() &&
                          !process.at("parent_name").as_string().empty() &&
                          !process.at("orphaned_parent").as_bool();
        }
        require(child_found, "recursive child process identity");

        auto direct_only_options = tree_options;
        direct_only_options.include_descendants = false;
        const auto direct_only = tdx::runtime_topology_document(direct_only_options);
        require(direct_only.at("process_count").as_number() == 1 &&
                direct_only.at("summary").at("additional_descendant_count").as_number() == 0,
                "disable recursive child capture");
        require(WaitForSingleObject(child.hProcess, 5000) == WAIT_OBJECT_0,
                "wait topology child");
        CloseHandle(child.hThread);
        CloseHandle(child.hProcess);

        tdx::RuntimeTopologyOptions options;
        options.process_id = GetCurrentProcessId();
        options.include_connections = false;
        const auto live = tdx::runtime_topology_document(options);
        require(live.at("supported").as_bool(), "Windows topology support");
        require(live.at("read_only").as_bool() && !live.at("attached").as_bool() &&
                !live.at("process_memory_read").as_bool(), "read-only guarantees");
        require(live.at("process_count").as_number() == 1, "current process capture");
        const auto& process = live.at("processes").as_array().front();
        require(process.at("pid").as_number() == GetCurrentProcessId(),
                "current process pid");
        require(process.at("module_count").as_number() >= 1,
                "current process local module capture");
#endif
        std::cout << "Runtime topology tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
