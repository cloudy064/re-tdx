#pragma once

#include "tdx/cloud_endpoints.hpp"
#include "tdx/json.hpp"

#include <filesystem>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace tdx {

struct CloudWorkflowStep {
    std::string transport;
    std::string request_id;
    std::vector<std::pair<std::string, std::string>> field_map;
    std::vector<std::pair<std::string, std::string>> parameter_aliases;
    std::vector<std::string> body_contains;
};

struct CloudWorkflowSpec {
    std::string name;
    std::string description;
    std::string key_field;
    CloudWorkflowStep master;
    std::vector<CloudWorkflowStep> details;
};

const std::vector<CloudWorkflowSpec>& cloud_workflow_specs();
Json cloud_workflow_defaults(const std::string& name,
                             const std::string& today_yyyymmdd = {});
Json cloud_workflows_document(const std::string& today_yyyymmdd = {});
Json cloud_result_rows(const Json& response, std::size_t result_set_index = 0);
Json run_cloud_workflow(
    const std::filesystem::path& root, const std::string& workflow_name,
    const std::map<std::string, std::string>& parameter_overrides = {},
    const std::vector<std::string>& selected = {}, int limit = 1,
    bool master_all_pages = false, int page_size = 20, int max_pages = 100,
    const std::string& base_url = cloud_endpoints::tqlex,
    int timeout_ms = 15000);
int command_cloud_workflow(const std::vector<std::string>& args);

}  // namespace tdx
