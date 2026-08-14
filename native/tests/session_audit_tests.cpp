#include "tdx/session_audit.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace fs = std::filesystem;

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

const tdx::Json& group_named(const tdx::Json& document, const std::string& name) {
    for (const auto& group : document.at("endpoint_groups").as_array())
        if (group.at("section").as_string() == name) return group;
    throw std::runtime_error("missing endpoint group: " + name);
}

}  // namespace

int main() {
    const auto suffix = std::to_string(std::chrono::steady_clock::now()
                                           .time_since_epoch().count());
    const auto root = fs::temp_directory_path() / ("tdx-session-audit-" + suffix);
    try {
        fs::create_directories(root / "T0002");
        {
            std::ofstream output(root / "connect.cfg", std::ios::binary);
            output << "[USER]\r\nUserName=private-user\r\n"
                      "[HQHOST]\r\nHostNum=2\r\nPrimaryHost=1\r\n"
                      "HostName01=First\r\nIPAddress01=127.0.0.1\r\nPort01=7709\r\n"
                      "HostName02=Second\r\nIPAddress02=127.0.0.2\r\nPort02=7710\r\n"
                      "[INFOHOST]\r\nHostNum=0\r\nPrimaryHost=-1\r\n"
                      "[INFOHOST2]\r\nHostNum=1\r\nPrimaryHost=0\r\n"
                      "HostName01=Information\r\nIPAddress01=127.0.0.3\r\n"
                      "[DSHOST]\r\nHostNum=1\r\nPrimaryHost=0\r\n"
                      "HostName01=Expansion\r\nIPAddress01=127.0.0.4\r\n"
                      "[HFHOST]\r\nHostNum=1\r\n"
                      "HostName01=HighFrequency\r\nIPAddress01=127.0.0.5\r\n"
                      "[WTHOST]\r\nHostNum=1\r\nPrimaryHost=0\r\n"
                      "HostName01=Unresolved\r\nIPAddress01=127.0.0.6\r\n";
        }
        {
            std::ofstream output(root / "T0002" / "user.ini", std::ios::binary);
            output << "[Other]\r\nTPSession=@ec0:DO_NOT_EMIT\r\n"
                      "TDXToken=TOKEN_DO_NOT_EMIT\r\nOID=OID_DO_NOT_EMIT\r\n";
        }
        const auto document = tdx::session_config_document(root);
        require(document.at("schema").as_string() ==
                    "tdx-session-config-audit-native-v1",
                "session audit schema");
        require(document.at("counts").at("endpoints").as_number() == 6,
                "endpoint count");
        const auto& group = group_named(document, "hqhost");
        require(group.at("selected_primary").at("endpoint").as_string() ==
                     "127.0.0.2:7710",
                "primary endpoint");
        require(group.at("primary_index").as_number() == 1 &&
                    group.at("primary_index_base").as_number() == 0 &&
                    group.at("configured_primary_index").as_number() == 1 &&
                    group.at("role").at("role").as_string() == "public_quote" &&
                    group.at("role").at("status").as_string() ==
                        "protocol-verified",
                "zero-based primary and HQ role");
        const auto& info = group_named(document, "infohost2");
        require(info.at("endpoints").as_array().front().at("port").as_number() == 7711 &&
                    info.at("role").at("role").as_string() ==
                        "information_service" &&
                    info.at("role").at("status").as_string() == "loader-semantic",
                "information loader role and default port");
        const auto& expansion = group_named(document, "dshost");
        require(expansion.at("endpoints").as_array().front().at("port").as_number() == 7721 &&
                    expansion.at("role").at("role").as_string() ==
                        "expansion_market_quote" &&
                    expansion.at("role").at("protocol_family").as_string() ==
                        "tdx-expansion-market",
                "expansion role and loader fallback");
        const auto& high_frequency = group_named(document, "hfhost");
        require(high_frequency.at("endpoints").as_array().front().at("port").as_number() == 7709 &&
                    high_frequency.at("selected_primary").is_null() &&
                    high_frequency.at("primary_index").is_null() &&
                    high_frequency.at("role").at("role").as_string() ==
                        "high_frequency_quote_host_pool" &&
                    high_frequency.at("role").at("primary_selectors").size() == 2,
                "HF host pool has separate L1 and L2 selectors");
        const auto& unresolved = group_named(document, "wthost");
        require(unresolved.at("endpoints").as_array().front().at("port").as_number() == 7708 &&
                    unresolved.at("role").at("role").as_string() == "unknown" &&
                    unresolved.at("role").at("status").as_string() == "unresolved",
                "unresolved WTHOST remains explicit");
        require(document.at("counts").at("resolved_endpoint_groups").as_number() == 5 &&
                    document.at("counts").at("protocol_verified_endpoint_groups").as_number() == 2 &&
                    document.at("counts").at("unresolved_endpoint_groups").as_number() == 1,
                "endpoint role counts");
        const auto selected = tdx::load_public_quote_endpoints(root, 2);
        require(selected.source == "connect.cfg:hqhost-primary-first" &&
                    selected.available_endpoint_count == 2 &&
                    selected.primary_configured && selected.endpoints.size() == 2 &&
                    selected.endpoints[0].address() == "127.0.0.2:7710" &&
                    selected.endpoints[1].address() == "127.0.0.1:7709" &&
                    selected.endpoints[0].name == "Second",
                "runtime quote endpoints must prefer PrimaryHost then wrap");
        const auto explicit_selection = tdx::select_public_quote_endpoints(
            root, {"192.0.2.1:7709", "192.0.2.2:7710"});
        require(explicit_selection.source == "explicit-host" &&
                    explicit_selection.available_endpoint_count == 2 &&
                    !explicit_selection.primary_configured &&
                    explicit_selection.endpoints[1].address() == "192.0.2.2:7710",
                "explicit endpoints must override connect.cfg without reordering");
        const auto capped_explicit_selection = tdx::select_public_quote_endpoints(
            root, {"192.0.2.1:7709", "192.0.2.2:7710"}, 1);
        require(capped_explicit_selection.available_endpoint_count == 2 &&
                    capped_explicit_selection.endpoints.size() == 1 &&
                    capped_explicit_selection.endpoints.front().address() ==
                        "192.0.2.1:7709",
                "explicit endpoint selection must honor the requested cap");
        const auto transport = tdx::public_quote_transport_document(
            selected, 5, 2, 2);
        require(transport.at("connection_attempts").as_number() == 5 &&
                    transport.at("transient_retries").as_number() == 2 &&
                    transport.at("endpoints_attempted").as_number() == 2 &&
                    transport.at("endpoint_failover").as_bool() &&
                    transport.at("recovered_after_retry").as_bool() &&
                    transport.at("endpoint_source").as_string() ==
                        "connect.cfg:hqhost-primary-first" &&
                    transport.at("available_endpoint_count").as_number() == 2,
                "quote transport metadata contract");
        const auto& private_state = document.at("private_state");
        require(private_state.at("values_redacted").as_bool(), "redaction marker");
        require(private_state.at("field_count").as_number() == 3,
                "private field count");
        const auto rendered = document.dump();
        require(rendered.find("DO_NOT_EMIT") == std::string::npos,
                "session value leaked");
        require(rendered.find("TOKEN_DO_NOT_EMIT") == std::string::npos,
                "token value leaked");
        require(rendered.find("private-user") == std::string::npos,
                "username leaked");
        require(!document.at("boundary").at("network_sent").as_bool(),
                "network boundary");
        fs::remove_all(root);
        std::cout << "session audit tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::error_code ignored;
        fs::remove_all(root, ignored);
        std::cerr << error.what() << '\n';
        return 1;
    }
}
