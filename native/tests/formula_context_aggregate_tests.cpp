#include "formula_engine_test_support.hpp"

#include <iostream>

int main() {
    try {
        formula_engine_test::run_context_builder_tests();
        std::cout << "formula context aggregate tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
