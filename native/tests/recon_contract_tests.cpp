#include "recon_contract_test_support.hpp"

#include <array>
#include <iostream>
#include <string_view>

namespace {

struct ContractDomain {
    const char *name;
    void (*run)();
};

constexpr std::array<ContractDomain, 7> contract_domains{{
    {"formula-evaluation", recon_contract_test::run_formula_evaluation_contracts},
    {"formula-render-workflow", recon_contract_test::run_formula_render_workflow_contracts},
    {"market-core", recon_contract_test::run_market_core_contracts},
    {"research-signals", recon_contract_test::run_research_signal_contracts},
    {"realtime-corporate", recon_contract_test::run_realtime_corporate_contracts},
    {"curated-issuance", recon_contract_test::run_curated_issuance_contracts},
    {"calendar-resilience-web", recon_contract_test::run_calendar_resilience_web_contracts},
}};

} // namespace

int main(int argc, char** argv) {
    try {
        std::string_view requested_domain;
        if (argc == 3 && std::string_view(argv[1]) == "--domain") {
            requested_domain = argv[2];
        } else if (argc != 1) {
            throw tdx::Error("usage: tdx-recon-contract-tests [--domain <name>]");
        }

        bool matched = requested_domain.empty();
        for (const auto &domain : contract_domains) {
            if (!requested_domain.empty() && requested_domain != domain.name) continue;
            matched = true;
            try {
                domain.run();
            } catch (const std::exception &error) {
                throw tdx::Error(std::string(domain.name) + ": " + error.what());
            }
        }
        if (!matched)
            throw tdx::Error("unknown contract test domain: " +
                             std::string(requested_domain));
        std::cout << "API contract evaluator tests passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
