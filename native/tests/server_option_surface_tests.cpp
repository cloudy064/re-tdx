#include "server_market_realtime_internal.hpp"

#include "tdx/common.hpp"

#include <chrono>
#include <filesystem>
#include <functional>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) throw tdx::Error(message);
}

void require_error(const std::function<void()>& action,
                   const std::string& expected) {
    try {
        action();
    } catch (const std::exception& error) {
        if (std::string(error.what()).find(expected) != std::string::npos)
            return;
        throw tdx::Error("unexpected option surface error: " +
                         std::string(error.what()));
    }
    throw tdx::Error("expected option surface error containing: " + expected);
}

struct TemporaryRoot {
    fs::path path = fs::temp_directory_path() /
        ("tdx-server-option-surface-" + std::to_string(
            std::chrono::high_resolution_clock::now()
                .time_since_epoch().count()));

    TemporaryRoot() {
        fs::create_directories(path / "T0002" / "hq_cache");
    }
    ~TemporaryRoot() {
        std::error_code ignored;
        fs::remove_all(path, ignored);
    }
};

}  // namespace

int main() {
    try {
        TemporaryRoot root;
        const auto rules = root.path / "T0002" / "hq_cache" /
            "code2name_qq.ini";
        const auto holidays = root.path / "T0002" / "hq_cache" /
            "neednote.dat";

        tdx::Json expiry = tdx::Json::object();
        expiry["schema"] = "tdx-option-expiry-v1";
        expiry["rules_source"] = tdx::path_utf8(rules);
        expiry["holiday_source"] = tdx::path_utf8(holidays);
        expiry["status"] = "exact_cutoff_override";
        tdx::server_detail::project_option_resource_paths(expiry, root.path);
        require(expiry.at("path_scope").as_string() ==
                    "tdx-root-relative" &&
                    expiry.at("rules_source").as_string() ==
                    "T0002/hq_cache/code2name_qq.ini" &&
                    expiry.at("holiday_source").as_string() ==
                    "T0002/hq_cache/neednote.dat" &&
                    expiry.at("status").as_string() ==
                    "exact_cutoff_override",
                "option expiry resources are root-relative without changing domain fields");
        require(expiry.dump(-1).find(root.path.filename().string()) ==
                    std::string::npos,
                "option expiry projection does not expose the absolute root");

        tdx::Json chain = tdx::Json::object();
        chain["schema"] = "tdx-option-chain-v1";
        chain["expiry_resolution"] = tdx::Json::object();
        chain["expiry_resolution"]["rules_source"] = tdx::path_utf8(rules);
        chain["expiry_resolution"]["holiday_source"] =
            tdx::path_utf8(holidays);
        chain["expiry_resolution"]["expiry"] = "2026-08-18";
        tdx::server_detail::project_option_resource_paths(chain, root.path);
        require(chain.at("path_scope").as_string() ==
                    "tdx-root-relative" &&
                    chain.at("expiry_resolution").at("rules_source").as_string() ==
                    "T0002/hq_cache/code2name_qq.ini" &&
                    chain.at("expiry_resolution").at("expiry").as_string() ==
                    "2026-08-18",
                "chain and volatility nested expiry resources use the same projection");

        tdx::Json escaped = tdx::Json::object();
        escaped["rules_source"] = tdx::path_utf8(
            root.path.parent_path() / "outside.ini");
        require_error(
            [&] {
                tdx::server_detail::project_option_resource_paths(
                    escaped, root.path);
            },
            "outside the TDX root");

        std::cout << "server option surface tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
