#include "tdx/ttplugin_redirect.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

}  // namespace

int main() {
    try {
        const auto contract = tdx::ttplugin_redirect_contract_document();
        require(contract.at("schema").as_string() ==
                    "tdx-ttplugin-redirect-contract-native-v1",
                "contract schema");
        require(contract.at("protocols").size() == 6, "protocol count");
        require(!contract.at("boundary").at("network_sent").as_bool(),
                "network boundary");

        const auto f10 = tdx::Json::parse(
            R"({"req":4611,"setcode":1,"code":"000001","reserved":2})");
        const auto f10_body = tdx::encode_ttplugin_redirect_request(f10);
        require(f10_body.size() == 14, "4611 wire size");
        require(tdx::read_u16_le(f10_body.data()) == 4611, "4611 req");
        require(tdx::read_u16_le(f10_body.data() + 2) == 1, "4611 setcode");
        require(std::string(reinterpret_cast<const char*>(f10_body.data() + 4), 6) ==
                    "000001",
                "4611 code");
        require(f10_body[12] == 2 && f10_body[13] == 0, "4611 tail");

        const auto file = tdx::Json::parse(
            R"({"req":4631,"flag":1,"pos":-4,"wantlen":4096,"filename":"test.txt"})");
        const auto file_body = tdx::encode_ttplugin_redirect_request(file);
        require(file_body.size() == 114, "4631 wire size");
        require(tdx::read_i32_le(file_body.data() + 6) == -4, "4631 signed position");
        require(tdx::read_i32_le(file_body.data() + 10) == 4096, "4631 length");

        bool unsupported = false;
        try {
            (void)tdx::encode_ttplugin_redirect_request(
                tdx::Json::parse(R"({"req":9221})"));
        } catch (const tdx::Error&) {
            unsupported = true;
        }
        require(unsupported, "unsupported request rejection");

        bool overflow = false;
        try {
            (void)tdx::encode_ttplugin_redirect_request(tdx::Json::parse(
                R"({"req":4611,"code":"12345678"})"));
        } catch (const tdx::Error&) {
            overflow = true;
        }
        require(overflow, "fixed text overflow rejection");
        std::cout << "TTPlugin redirect tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
