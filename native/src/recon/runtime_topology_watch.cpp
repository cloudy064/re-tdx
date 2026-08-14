#include "tdx/recon.hpp"

#include "tdx/common.hpp"

#include <algorithm>
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
    if (!result) throw Error("runtime topology watch sample has no " + std::string(name));
    return *result;
}

std::uint64_t unsigned_number(const Json& value, std::string_view name) {
    const auto& item = required_member(value, name);
    if (!item.is_number() || item.as_number() < 0)
        throw Error("runtime topology watch field is not unsigned: " + std::string(name));
    return static_cast<std::uint64_t>(item.as_number());
}

std::uint64_t optional_unsigned(const Json& value, std::string_view name) {
    const auto* item = member(value, name);
    if (!item) return 0;
    if (!item->is_number() || item->as_number() < 0)
        throw Error("runtime topology watch optional field is not unsigned: " +
                    std::string(name));
    return static_cast<std::uint64_t>(item->as_number());
}

std::string string_value(const Json& value, std::string_view name) {
    const auto& item = required_member(value, name);
    if (!item.is_string())
        throw Error("runtime topology watch field is not text: " + std::string(name));
    return item.as_string();
}

void collect_strings(const Json& value, std::string_view name,
                     std::set<std::string>& output) {
    const auto* items = member(value, name);
    if (!items || !items->is_array()) return;
    for (const auto& item : items->as_array())
        if (item.is_string()) output.insert(item.as_string());
}

Json sample_summary(const Json& sample, std::uint64_t index) {
    const auto& summary = required_member(sample, "summary");
    Json result = Json::object();
    result["sample_index"] = index;
    result["captured_at_utc"] = string_value(sample, "captured_at_utc");
    result["process_count"] = unsigned_number(summary, "process_count");
    result["direct_match_count"] = optional_unsigned(summary, "direct_match_count");
    result["descendant_process_count"] =
        optional_unsigned(summary, "descendant_process_count");
    result["additional_descendant_count"] =
        optional_unsigned(summary, "additional_descendant_count");
    result["orphaned_parent_count"] = optional_unsigned(summary, "orphaned_parent_count");
    result["module_count"] = unsigned_number(summary, "module_count");
    result["connection_count"] = unsigned_number(summary, "connection_count");
    result["established_connection_count"] =
        unsigned_number(summary, "established_connection_count");
    result["listener_count"] = unsigned_number(summary, "listener_count");
    result["configured_remote_connection_count"] =
        optional_unsigned(summary, "configured_remote_connection_count");
    const auto* endpoints = member(summary, "remote_endpoints");
    result["remote_endpoints"] = endpoints && endpoints->is_array()
        ? *endpoints : Json::array();
    return result;
}

}  // namespace

Json runtime_topology_watch_document(const std::vector<Json>& samples,
                                     std::uint64_t requested_duration_ms,
                                     std::uint64_t interval_ms) {
    if (samples.empty()) throw Error("runtime topology watch requires at least one sample");
    if (!interval_ms) throw Error("runtime topology watch interval must be positive");
    for (const auto& sample : samples)
        if (string_value(sample, "schema") != "tdx-runtime-topology-v1")
            throw Error("runtime topology watch requires tdx-runtime-topology-v1 samples");

    Json events = Json::array();
    Json summaries = Json::array();
    std::set<std::string> observed_modules;
    std::set<std::string> observed_endpoints;
    std::set<std::string> observed_config_sections;
    std::set<std::uint64_t> observed_pids;
    std::uint64_t peak_processes = 0;
    std::uint64_t peak_descendants = 0;
    std::uint64_t peak_additional_descendants = 0;
    std::uint64_t peak_orphaned_parents = 0;
    std::uint64_t peak_modules = 0;
    std::uint64_t peak_connections = 0;
    std::uint64_t peak_established = 0;
    std::uint64_t peak_configured_connections = 0;
    std::uint64_t samples_with_capture_errors = 0;

    for (std::size_t index = 0; index < samples.size(); ++index) {
        const auto& sample = samples[index];
        const auto& summary = required_member(sample, "summary");
        summaries.push_back(sample_summary(sample, static_cast<std::uint64_t>(index)));
        peak_processes = std::max(peak_processes, unsigned_number(summary, "process_count"));
        peak_descendants = std::max(
            peak_descendants, optional_unsigned(summary, "descendant_process_count"));
        peak_additional_descendants = std::max(
            peak_additional_descendants,
            optional_unsigned(summary, "additional_descendant_count"));
        peak_orphaned_parents = std::max(
            peak_orphaned_parents, optional_unsigned(summary, "orphaned_parent_count"));
        peak_modules = std::max(peak_modules, unsigned_number(summary, "module_count"));
        peak_connections = std::max(peak_connections,
                                    unsigned_number(summary, "connection_count"));
        peak_established = std::max(peak_established,
                                    unsigned_number(summary, "established_connection_count"));
        peak_configured_connections = std::max(
            peak_configured_connections,
            optional_unsigned(summary, "configured_remote_connection_count"));
        collect_strings(summary, "module_names", observed_modules);
        collect_strings(summary, "remote_endpoints", observed_endpoints);
        collect_strings(summary, "matched_config_sections", observed_config_sections);
        const auto& processes = required_member(sample, "processes");
        if (!processes.is_array())
            throw Error("runtime topology watch sample processes is not an array");
        for (const auto& process : processes.as_array())
            observed_pids.insert(unsigned_number(process, "pid"));
        const auto* errors = member(sample, "connection_scan_errors");
        if (errors && errors->is_array() && !errors->as_array().empty())
            ++samples_with_capture_errors;

        if (!index) continue;
        auto diff = compare_runtime_topology_documents(samples[index - 1], sample);
        if (!diff.at("changed").as_bool()) continue;
        Json event = Json::object();
        event["sample_index"] = static_cast<std::uint64_t>(index);
        event["captured_at_utc"] = string_value(sample, "captured_at_utc");
        event["diff"] = std::move(diff);
        events.push_back(std::move(event));
    }

    Json result = Json::object();
    result["schema"] = "tdx-runtime-topology-watch-v1";
    result["read_only"] = true;
    result["process_memory_read"] = false;
    result["attached"] = false;
    result["injected"] = false;
    result["requested_duration_ms"] = requested_duration_ms;
    result["interval_ms"] = interval_ms;
    result["samples_collected"] = static_cast<std::uint64_t>(samples.size());
    result["comparisons"] = static_cast<std::uint64_t>(samples.size() - 1);
    result["change_event_count"] = static_cast<std::uint64_t>(events.size());
    result["stable_comparison_count"] =
        static_cast<std::uint64_t>(samples.size() - 1 - events.size());
    result["changed"] = !events.as_array().empty();
    result["started_at_utc"] = string_value(samples.front(), "captured_at_utc");
    result["finished_at_utc"] = string_value(samples.back(), "captured_at_utc");
    result["peak_process_count"] = peak_processes;
    result["peak_descendant_process_count"] = peak_descendants;
    result["peak_additional_descendant_count"] = peak_additional_descendants;
    result["peak_orphaned_parent_count"] = peak_orphaned_parents;
    result["peak_module_count"] = peak_modules;
    result["peak_connection_count"] = peak_connections;
    result["peak_established_connection_count"] = peak_established;
    result["peak_configured_connection_count"] = peak_configured_connections;
    result["samples_with_connection_scan_errors"] = samples_with_capture_errors;
    result["observed_pids"] = Json::array();
    for (const auto pid : observed_pids) result["observed_pids"].push_back(pid);
    result["observed_module_names"] = Json::array();
    for (const auto& name : observed_modules) result["observed_module_names"].push_back(name);
    result["observed_remote_endpoints"] = Json::array();
    for (const auto& endpoint : observed_endpoints)
        result["observed_remote_endpoints"].push_back(endpoint);
    result["observed_config_sections"] = Json::array();
    for (const auto& section : observed_config_sections)
        result["observed_config_sections"].push_back(section);
    result["sample_summaries"] = std::move(summaries);
    result["events"] = std::move(events);
    result["first_snapshot"] = samples.front();
    result["final_snapshot"] = samples.back();
    const auto* correlation = member(samples.back(), "endpoint_correlation");
    if (correlation) result["endpoint_correlation"] = *correlation;
    return result;
}

}  // namespace tdx
