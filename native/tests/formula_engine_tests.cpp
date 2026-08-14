#include "formula_engine_test_support.hpp"

#include <iostream>
#include <string_view>

namespace {

struct TestDomain {
    const char *name;
    void (*run)();
};

constexpr std::array<TestDomain, 8> test_domains{{
    {"context-builder", formula_engine_test::run_context_builder_tests},
    {"language", formula_engine_test::run_language_tests},
    {"native-operators", formula_engine_test::run_native_operator_tests},
    {"native-binary", formula_engine_test::run_native_binary_operator_tests},
    {"native-scalars", formula_engine_test::run_native_scalar_tests},
    {"workflow", formula_engine_test::run_workflow_tests},
    {"render-ir", formula_engine_test::run_render_tests},
    {"context-and-library", formula_engine_test::run_context_and_library_tests},
}};

} // namespace

int main(int argc, char** argv) {
    try {
        std::string_view requested_domain;
        if (argc == 3 && std::string_view(argv[1]) == "--domain") {
            requested_domain = argv[2];
        } else if (argc != 1) {
            throw tdx::Error("usage: tdx-formula-engine-tests [--domain <name>]");
        }

        bool matched = requested_domain.empty();
        for (const auto &domain : test_domains) {
            if (!requested_domain.empty() && requested_domain != domain.name) continue;
            matched = true;
            try {
                domain.run();
            } catch (const std::exception &error) {
                throw tdx::Error(std::string(domain.name) + ": " + error.what());
            }
        }
        if (!matched)
            throw tdx::Error("unknown formula engine test domain: " +
                             std::string(requested_domain));
        std::cout << "formula engine tests passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
