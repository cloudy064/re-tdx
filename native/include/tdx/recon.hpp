#pragma once

#include "tdx/json.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace tdx {

Json inventory_document(const std::filesystem::path& root,
                        bool recursive = false,
                        bool hash = false);
Json evaluate_api_contract_response(const std::string& contract_id,
                                    int status,
                                    const std::string& content_type,
                                    const std::string& body,
                                    const Json& context = Json::object());
Json run_api_contract_audit(const std::string& base_url,
                            const std::string& profile = "full",
                            const std::vector<std::string>& selected_cases = {},
                            int timeout_ms = 15000);

struct RuntimeTopologyOptions {
    std::string process_name{"TdxW.exe"};
    std::uint32_t process_id{};
    bool include_modules{true};
    bool all_modules{};
    bool include_connections{true};
    bool include_descendants{true};
};

Json runtime_topology_document(const RuntimeTopologyOptions& options = {});
Json compare_runtime_topology_documents(const Json& baseline, const Json& current);
Json annotate_runtime_topology_endpoints(Json snapshot, const Json& session_config);
Json runtime_topology_watch_document(const std::vector<Json>& samples,
                                     std::uint64_t requested_duration_ms,
                                     std::uint64_t interval_ms);
int command_recon_install(const std::vector<std::string>& args);
int command_recon_runtime_topology(const std::vector<std::string>& args);
int command_recon_api_contracts(const std::vector<std::string>& args);
int command_doctor(const std::vector<std::string>& args);

}  // namespace tdx
