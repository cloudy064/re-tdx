#pragma once

#include "tdx/json.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace tdx {

struct JsnResourceTemplate {
    std::string resource;
    std::vector<std::string> source_files;
    std::vector<std::string> placeholders;
};

std::vector<JsnResourceTemplate> inventory_jsn_resource_templates(
    const std::filesystem::path& root);
Json jsn_variant_coverage_document(
    const std::vector<JsnResourceTemplate>& templates,
    const std::filesystem::path& downloaded_root = {},
    bool gaps_only = false,
    int top = 50);
Json jsn_variant_coverage_document(
    const std::filesystem::path& root,
    const std::filesystem::path& downloaded_root = {},
    bool gaps_only = false,
    int top = 50);
Json jsn_discovery_document(
    const std::filesystem::path& root,
    const std::filesystem::path& downloaded_root,
    const std::filesystem::path& baseline_path = {},
    bool capture_baseline = false,
    int top = 100);
Json jsn_candidate_document(
    const std::filesystem::path& root,
    const std::filesystem::path& downloaded_root,
    const std::string& family = {},
    bool missing_only = false,
    int limit = 200,
    bool probe = false,
    int max_probes = 10,
    int timeout_ms = 10000);
int command_recon_jsn_variants(const std::vector<std::string>& args);
int command_recon_jsn_discovery(const std::vector<std::string>& args);
int command_jsn_candidates(const std::vector<std::string>& args);

}  // namespace tdx
