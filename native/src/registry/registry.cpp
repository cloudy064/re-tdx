#include "tdx/registry.hpp"

#include "tdx/registry_internal.hpp"

#include <algorithm>

namespace tdx {

// The table is assembled from per-domain appenders (registry_*.cpp) instead of
// one 450-line literal, so a domain header change only recompiles its own unit.
// Call order defines the public order of `--help`, /api/v1/features and the
// OpenAPI path list.
const std::vector<CommandSpec>& command_registry() {
    static const std::vector<CommandSpec> commands = [] {
        std::vector<CommandSpec> table;
        table.reserve(158);
        registry_detail::append_cloud_gateway_commands(table);
        registry_detail::append_formula_commands(table);
        registry_detail::append_static_resource_commands(table);
        registry_detail::append_market_reference_commands(table);
        registry_detail::append_market_local_commands(table);
        registry_detail::append_market_corporate_commands(table);
        registry_detail::append_market_quote_commands(table);
        registry_detail::append_market_disclosure_commands(table);
        registry_detail::append_market_anomaly_commands(table);
        registry_detail::append_market_session_commands(table);
        registry_detail::append_market_valuation_commands(table);
        registry_detail::append_system_commands(table);
        return table;
    }();
    return commands;
}

const CommandSpec* find_command(const std::string& name) {
    const auto& commands = command_registry();
    const auto found = std::find_if(commands.begin(), commands.end(),
                                    [&](const CommandSpec& item) { return item.name == name; });
    return found == commands.end() ? nullptr : &*found;
}

}  // namespace tdx
