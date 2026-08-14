#pragma once

#include "tdx/json.hpp"
#include "tdx/transport.hpp"

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace tdx {

struct QuoteEndpointSelection {
    std::vector<Endpoint> endpoints;
    std::string source;
    std::size_t available_endpoint_count{};
    bool primary_configured{};
};

QuoteEndpointSelection load_public_quote_endpoints(
    const std::filesystem::path& root, std::size_t max_endpoints = 3);
QuoteEndpointSelection select_public_quote_endpoints(
    const std::filesystem::path& root,
    const std::vector<std::string>& explicit_hosts,
    std::size_t max_endpoints = 3);
Json public_quote_transport_document(
    const QuoteEndpointSelection& selection,
    int connection_attempts,
    int transient_retries,
    int endpoints_attempted,
    int max_attempts_per_endpoint = 3);
Json session_config_document(const std::filesystem::path& root);
int command_recon_session_config(const std::vector<std::string>& args);

}  // namespace tdx
