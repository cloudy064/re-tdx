#include "tdx/ttplugin_servers.hpp"

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

}  // namespace

int main() {
    const auto suffix = std::to_string(std::chrono::steady_clock::now()
                                           .time_since_epoch().count());
    const auto root = fs::temp_directory_path() / ("tdx-ttplugin-servers-" + suffix);
    try {
        fs::create_directories(root);
        const auto input = root / "servers.ini";
        {
            std::ofstream output(input, std::ios::binary);
            output << "[CTP]\r\n"
                      "TDXProxy=PROXY_SECRET,SECOND_SECRET,2\r\n"
                      "MDUserServerNormal_Num=2\r\n"
                      "MDUserServerNormal_1=tcp://127.0.0.1:7709\r\n"
                      "MDUserServerNormal_2=tcp6://[::1]:7709\r\n"
                      "MDUserServerExtend_Num=1\r\n"
                      "MDUserServerExtend_1=socks5://proxy.example:1080/user:PASS_SECRET@target.example:7709\r\n";
        }
        const auto document = tdx::ttplugin_server_config_document(
            {input}, {"ssl://quotes.example:443",
                      "tcp://adhoc:ADHOC_SECRET@quotes.example:7709",
                      "bad://hidden.example:1"});
        require(document.at("schema").as_string() ==
                    "tdx-ttplugin-server-config-native-v1",
                "server config schema");
        require(document.at("counts").at("matching_sections").as_number() == 1,
                "matching section count");
        require(document.at("counts").at("declared_servers").as_number() == 3,
                "declared server count");
        require(document.at("counts").at("valid_server_urls").as_number() == 5,
                "valid URL count");
        require(document.at("counts").at("invalid_server_urls").as_number() == 1,
                "invalid URL count");
        require(document.at("counts").at("server_urls_with_credentials").as_number() == 2,
                "credential URL count");
        const auto rendered = document.dump();
        require(rendered.find("PASS_SECRET") == std::string::npos,
                "URL credential leaked");
        require(rendered.find("ADHOC_SECRET") == std::string::npos,
                "ad hoc URL credential leaked");
        require(rendered.find("PROXY_SECRET") == std::string::npos,
                "TDXProxy value leaked");
        require(rendered.find("SECOND_SECRET") == std::string::npos,
                "TDXProxy secondary value leaked");
        require(rendered.find("target.example") != std::string::npos,
                "proxy target missing");
        require(!document.at("boundary").at("network_sent").as_bool(),
                "network boundary");
        fs::remove_all(root);
        std::cout << "TTPlugin server tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::error_code ignored;
        fs::remove_all(root, ignored);
        std::cerr << error.what() << '\n';
        return 1;
    }
}
