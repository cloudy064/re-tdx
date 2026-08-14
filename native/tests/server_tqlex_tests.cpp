#include "server_market_internal.hpp"

#include "tdx/common.hpp"

#include <initializer_list>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

tdx::server_detail::RequestTarget target() {
    return {"/api/v1/tqlex/query", {}};
}

tdx::server_detail::RequestTarget target(
    std::initializer_list<std::pair<const std::string, std::string>> query) {
    tdx::server_detail::RequestTarget result{"/api/v1/tqlex/query", {}};
    for (const auto& [key, value] : query) result.query[key] = value;
    return result;
}

template <typename Function>
void require_error(Function&& function, const std::string& expected) {
    try {
        function();
    } catch (const tdx::Error& error) {
        require(std::string(error.what()).find(expected) != std::string::npos,
                "TQLEX HTTP rejection message");
        return;
    }
    throw std::runtime_error("TQLEX HTTP boundary was not rejected");
}

void test_single_page_plan() {
    const auto plan = tdx::server_detail::tqlex_http_query_plan(target({
        {"page", "1000000"}, {"page_size", "5000"},
        {"max_pages", "20"}, {"all_pages", "false"},
    }));
    require(plan.page == 1000000 && plan.page_size == 5000 &&
                !plan.all_pages && plan.requested_max_pages == 20 &&
                plan.effective_max_pages == 1,
            "single-page HTTP plan keeps strict inputs and executes one page");

    const auto defaults = tdx::server_detail::tqlex_http_query_plan(target());
    require(defaults.page == 0 && defaults.page_size == 20 &&
                defaults.requested_max_pages == 20 &&
                defaults.effective_max_pages == 1,
            "single-page HTTP defaults");
}

void test_all_pages_budget() {
    const auto exact = tdx::server_detail::tqlex_http_query_plan(target({
        {"page_size", "2500"}, {"max_pages", "20"},
        {"all_pages", "true"},
    }));
    require(exact.all_pages && exact.requested_max_pages == 20 &&
                exact.effective_max_pages == 20,
            "50,000-row all-pages HTTP budget is accepted");

    require_error([] {
        (void)tdx::server_detail::tqlex_http_query_plan(target({
            {"page_size", "2501"}, {"max_pages", "20"},
            {"all_pages", "1"},
        }));
    }, "must not exceed 50000 rows");
}

void test_individual_boundaries() {
    const auto page_size_boundary =
        tdx::server_detail::tqlex_http_query_plan(target({
            {"page_size", "5000"},
        }));
    require(page_size_boundary.page_size == 5000,
            "page_size 5000 boundary is accepted");
    require_error([] {
        (void)tdx::server_detail::tqlex_http_query_plan(target({
            {"page_size", "5001"},
        }));
    }, "page_size must be in 1..5000");
    const auto max_pages_boundary =
        tdx::server_detail::tqlex_http_query_plan(target({
            {"page_size", "1"}, {"max_pages", "20"},
            {"all_pages", "true"},
        }));
    require(max_pages_boundary.effective_max_pages == 20,
            "max_pages 20 boundary is accepted");
    require_error([] {
        (void)tdx::server_detail::tqlex_http_query_plan(target({
            {"max_pages", "21"},
        }));
    }, "max_pages must be in 1..20");
    require_error([] {
        (void)tdx::server_detail::tqlex_http_query_plan(target({
            {"max_pages", "0"}, {"all_pages", "false"},
        }));
    }, "max_pages must be in 1..20");
    require_error([] {
        (void)tdx::server_detail::tqlex_http_query_plan(target({
            {"page", "1000001"},
        }));
    }, "page must be in 0..1000000");
}

}  // namespace

int main() {
    try {
        test_single_page_plan();
        test_all_pages_budget();
        test_individual_boundaries();
        std::cout << "server TQLEX tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "server TQLEX tests failed: " << error.what() << '\n';
        return 1;
    }
}
