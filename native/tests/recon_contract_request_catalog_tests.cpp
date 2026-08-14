#include "recon_contract_internal.hpp"

#include "tdx/common.hpp"

#include <iostream>
#include <string_view>

namespace {

void require(bool condition, std::string_view message) {
    if (!condition) throw tdx::Error(std::string(message));
}

void require_request(std::string_view id, std::string_view action) {
    const auto* request =
        tdx::recon_contract_detail::api_contract_post_request(id);
    require(request != nullptr, "expected recon POST request is missing");
    require(request->action == action, "recon POST action does not match");
    require(tdx::Json::parse(request->body).is_object(),
            "recon POST body must remain valid JSON object text");
}

}  // namespace

int main() {
    try {
        require_request("formula-inline-post", "formula-evaluate");
        require_request("formula-strategy-backtest-post",
                        "formula-strategy-backtest");
        require_request("tpool-inline-evaluate-post", "pool-evaluate");
        require(
            tdx::recon_contract_detail::api_contract_post_request(
                "not-a-contract") == nullptr,
            "unknown recon POST request must not resolve");
        std::cout << "recon request catalog tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
