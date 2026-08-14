#pragma once

#include "tdx/registry.hpp"

#include <vector>

// Each appender contributes one contiguous slice of the command table. The
// order of the calls in command_registry() is the public order of the table:
// `tdx-tool --help`, /api/v1/features and the OpenAPI path seeding all iterate
// the vector as-is, so appenders must never be reordered or sorted.
namespace tdx::registry_detail {

void append_cloud_gateway_commands(std::vector<CommandSpec>& commands);
void append_formula_commands(std::vector<CommandSpec>& commands);
void append_static_resource_commands(std::vector<CommandSpec>& commands);
void append_market_reference_commands(std::vector<CommandSpec>& commands);
void append_market_local_commands(std::vector<CommandSpec>& commands);
void append_market_corporate_commands(std::vector<CommandSpec>& commands);
void append_market_quote_commands(std::vector<CommandSpec>& commands);
void append_market_disclosure_commands(std::vector<CommandSpec>& commands);
void append_market_anomaly_commands(std::vector<CommandSpec>& commands);
void append_market_session_commands(std::vector<CommandSpec>& commands);
void append_market_valuation_commands(std::vector<CommandSpec>& commands);
void append_system_commands(std::vector<CommandSpec>& commands);

}  // namespace tdx::registry_detail
