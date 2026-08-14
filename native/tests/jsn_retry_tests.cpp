#include "tdx/common.hpp"
#include "tdx/jsn.hpp"

#include <iostream>
#include <string>

namespace {
void require(bool value, const char* message) {
    if (!value) throw tdx::Error(message);
}

tdx::Json source(const std::string& resource) {
    auto document = tdx::Json::object();
    document["resource"] = resource;
    document["size"] = 100;
    document["row_count"] = 1;
    document["endpoint"] = "test:7709";
    document["rows"] = tdx::Json::array();
    return document;
}
}

int main() {
    try {
        int attempts = 0;
        const auto recovered = tdx::detail::retry_jsn_rows_fetch([&]() {
            ++attempts;
            if (attempts < 3) throw tdx::Error("server closed connection while receiving");
            auto result = tdx::Json::object();
            result["ok"] = true;
            return result;
        }, 3, 0);
        require(attempts == 3 && recovered.at("ok").as_bool(),
                "JSN retry did not recover on the third attempt");

        attempts = 0;
        bool exhausted = false;
        try {
            (void)tdx::detail::retry_jsn_rows_fetch([&]() -> tdx::Json {
                ++attempts;
                throw tdx::Error("cannot connect to 7709 test endpoint");
            }, 3, 0);
        } catch (const tdx::Error& error) {
            exhausted = std::string(error.what()).find("cannot connect") != std::string::npos;
        }
        require(exhausted && attempts == 3,
                "JSN retry exhaustion did not preserve the final transport error");

        const std::vector<std::string> keys{
            "test/resilience/master-a.jsn", "test/resilience/master-b.jsn"};
        attempts = 0;
        const auto live_documents = tdx::detail::resilient_jsn_rows_fetch(keys, [&]() {
            ++attempts;
            auto documents = tdx::Json::array();
            documents.push_back(source("master-a.jsn"));
            documents.push_back(source("master-b.jsn"));
            return documents;
        }, 3, 0);
        require(attempts == 1 &&
                    !live_documents.as_array()[0].at("stale").as_bool() &&
                    live_documents.as_array()[0].at("attempts").as_number() == 1,
                "JSN resilient fetch did not store a live batch");

        attempts = 0;
        const auto stale_documents = tdx::detail::resilient_jsn_rows_fetch(keys,
            [&]() -> tdx::Json {
                ++attempts;
                throw tdx::Error("server closed connection while receiving cached batch");
            }, 3, 0);
        require(attempts == 3 &&
                    stale_documents.as_array()[0].at("stale").as_bool() &&
                    stale_documents.as_array()[1].at("stale").as_bool() &&
                    stale_documents.as_array()[0].at("attempts").as_number() == 3 &&
                    stale_documents.as_array()[0].at("upstream_error").as_string().find(
                        "cached batch") != std::string::npos,
                "JSN resilient fetch did not return the complete stale batch");

        auto sources = tdx::Json::array();
        for (const auto& document : stale_documents.as_array())
            sources.push_back(tdx::jsn_source_metadata(document));
        const auto health = tdx::jsn_sources_health(sources);
        require(health.at("stale").as_bool() &&
                    health.at("stale_sources").as_number() == 2 &&
                    health.at("live_sources").as_number() == 0 &&
                    health.at("max_attempts").as_number() == 3 &&
                    health.at("upstream_errors").size() == 2,
                "JSN source health did not aggregate stale metadata");

        attempts = 0;
        bool cold_failed = false;
        try {
            (void)tdx::detail::resilient_jsn_rows_fetch(
                {"test/resilience/cold-only.jsn"}, [&]() -> tdx::Json {
                    ++attempts;
                    throw tdx::Error("cannot connect to cold JSN endpoint");
                }, 3, 0);
        } catch (const tdx::Error& error) {
            cold_failed = std::string(error.what()).find("cold JSN") != std::string::npos;
        }
        require(cold_failed && attempts == 3,
                "cold JSN resilient fetch must fail after bounded retries");

        std::cout << "JSN retry tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
