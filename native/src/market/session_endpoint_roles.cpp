#include "session_endpoint_roles_internal.hpp"

#include "tdx/common.hpp"

#include <array>
#include <cstddef>
#include <string>
#include <string_view>

namespace tdx::session_audit_detail {
namespace {

struct EndpointRoleDefinition {
    std::string_view section;
    std::string_view role;
    std::string_view status;
    std::string_view protocol_family;
    int loader_default_port;
    bool section_primary;
    std::array<std::string_view, 3> primary_selectors;
    std::array<std::string_view, 3> evidence;
    std::array<std::string_view, 2> limitations;
};

constexpr std::array<EndpointRoleDefinition, 6> endpoint_roles{{
    {
        "hqhost", "public_quote", "protocol-verified", "tdx-public-quote",
        7709, true,
        {"HQHOST.PrimaryHost", "", ""},
        {
            "TdxW sub_657A80 parses HQHOST and defaults PortNN to 7709",
            "native public quote commands select connect.cfg HQHOST endpoints",
            "the 7709 quote request and response families are implemented and tested",
        },
        {
            "configuration does not prove that an endpoint is currently reachable",
            "a public quote endpoint is separate from tpbus/TaApi application sessions",
        },
    },
    {
        "hfhost", "high_frequency_quote_host_pool", "client-binding-verified", "",
        7709, false,
        {"OTHERHOST.L1HFPrimaryHost", "OTHERHOST.L2HFPrimaryHost", ""},
        {
            "TdxW sub_75C630 loads HFHOST with a 7709 default into the L1 HF table",
            "TdxW sub_6536E0 selector 10 loads the same HFHOST section into the L2 HF table",
            "sub_75C630 applies separate L1HFPrimaryHost and L2HFPrimaryHost selectors",
        },
        {
            "the wire protocol and authorization requirements are not decoded here",
            "HFHOST must not be classified as exclusively L1 or exclusively L2",
        },
    },
    {
        "infohost", "information_service", "loader-semantic", "",
        7711, true,
        {"INFOHOST.PrimaryHost", "", ""},
        {
            "TdxW sub_657A80 has a dedicated INFOHOST loader",
            "the loader defaults PortNN to 7711 and reads HostTypeNN",
            "the exact application protocol has not been decoded",
        },
        {
            "the section name and loader establish intent but not a verified wire contract",
            "configuration does not prove endpoint availability",
        },
    },
    {
        "infohost2", "information_service", "loader-semantic", "",
        7711, true,
        {"INFOHOST2.PrimaryHost", "", ""},
        {
            "TdxW sub_657A80 has a separate INFOHOST2 loader with a 7711 fallback",
            "current installations may explicitly override PortNN, commonly to 7712",
            "configured HostNameNN labels identify these entries as information main stations",
        },
        {
            "the explicit PortNN value takes precedence over the loader fallback",
            "the exact application protocol has not been decoded",
        },
    },
    {
        "dshost", "expansion_market_quote", "protocol-verified", "tdx-expansion-market",
        7721, true,
        {"DSHOST.PrimaryHost", "DSHOST_EXTERN.PrimaryHost", ""},
        {
            "TdxW sub_6536E0 selector 6 and sub_657A80 load DSHOST with a 7721 fallback",
            "TdxW download startup selects DSHOST_EXTERN for the expansion-market path",
            "native 7727 expansion quote, directory, timeline, trade and K-line protocols are implemented and tested",
        },
        {
            "the loader fallback is not the active port when PortNN explicitly configures 7727",
            "configuration does not prove endpoint availability",
        },
    },
    {
        "wthost", "unknown", "unresolved", "",
        7708, true,
        {"WTHOST.PrimaryHost", "WTHOST.XYPrimaryHost/QHPrimaryHost/UCPrimaryHost", ""},
        {
            "TdxW sub_657A80 loads WTHOST with a 7708 fallback",
            "the loader also reads HostTypeNN, YYBIDSNN and SSLNN",
            "no verified request/response contract currently assigns a narrower role",
        },
        {
            "the WTHOST abbreviation is not expanded by guesswork",
            "broker-specific fields alone do not prove a trading protocol",
        },
    },
}};

const EndpointRoleDefinition* find_role(std::string_view section) {
    const auto normalized = lower_ascii(std::string(section));
    for (const auto& definition : endpoint_roles)
        if (definition.section == normalized) return &definition;
    return nullptr;
}

template <std::size_t Size>
Json string_array(const std::array<std::string_view, Size>& values) {
    Json result = Json::array();
    for (const auto value : values)
        if (!value.empty()) result.push_back(std::string(value));
    return result;
}

}  // namespace

Json endpoint_group_role_document(std::string_view section) {
    const auto normalized = lower_ascii(std::string(section));
    const auto* definition = find_role(normalized);
    Json result = Json::object();
    result["schema"] = "tdx-endpoint-group-role-v1";
    result["section"] = normalized;
    if (!definition) {
        result["role"] = "unknown";
        result["status"] = "unresolved";
        result["protocol_family"] = Json(nullptr);
        result["loader_default_port"] = 7709;
        result["primary_index_base"] = 0;
        result["primary_selectors"] = Json::array();
        result["evidence"] = Json::array();
        result["limitations"] = Json::array();
        result["limitations"].push_back(
            "no typed role evidence catalog entry exists for this section");
    } else {
        result["role"] = std::string(definition->role);
        result["status"] = std::string(definition->status);
        result["protocol_family"] = definition->protocol_family.empty()
            ? Json(nullptr) : Json(std::string(definition->protocol_family));
        result["loader_default_port"] = definition->loader_default_port;
        result["primary_index_base"] = 0;
        result["primary_selectors"] = string_array(definition->primary_selectors);
        result["evidence"] = string_array(definition->evidence);
        result["limitations"] = string_array(definition->limitations);
    }
    result["network_probed"] = false;
    result["availability_inferred"] = false;
    result["authorization_inferred"] = false;
    return result;
}

int endpoint_group_loader_default_port(std::string_view section) {
    const auto* definition = find_role(section);
    return definition ? definition->loader_default_port : 7709;
}

bool endpoint_group_uses_section_primary(std::string_view section) {
    const auto* definition = find_role(section);
    return !definition || definition->section_primary;
}

}  // namespace tdx::session_audit_detail
