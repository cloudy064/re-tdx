#include "server_local_resource_paths_internal.hpp"

#include "tdx/common.hpp"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

struct Fixture {
    fs::path root = fs::temp_directory_path() /
        ("tdx-local-resource-surface-" + std::to_string(
            std::chrono::high_resolution_clock::now()
                .time_since_epoch().count()));

    Fixture() { fs::create_directories(root / "T0002" / "hq_cache"); }
    ~Fixture() {
        std::error_code ignored;
        fs::remove_all(root, ignored);
    }
};

template <typename Callback>
void require_error(Callback&& callback, const char* needle) {
    try {
        callback();
    } catch (const std::exception& error) {
        require(std::string(error.what()).find(needle) != std::string::npos,
                "unexpected error text");
        return;
    }
    throw std::runtime_error("expected error");
}

}  // namespace

int main() {
    try {
        Fixture fixture;
        const auto first = fixture.root / "T0002" / "hq_cache" / "pttab.dat";
        const auto second =
            fixture.root / "T0002" / "hq_cache" / "speczsevent.txt";
        tdx::Json document = tdx::Json::object();
        document["schema"] = "fixture-v1";
        document["sources"] = tdx::Json::array();
        for (const auto& path : {first, second}) {
            tdx::Json source = tdx::Json::object();
            source["file"] = path.filename().string();
            source["path"] = tdx::path_utf8(path);
            source["endpoint"] = "local-file:" + tdx::path_utf8(path);
            source["extra"] = 7;
            document["sources"].push_back(std::move(source));
        }
        tdx::server_detail::project_local_catalog_resource_paths(
            document, fixture.root);
        require(document.at("path_scope").as_string() ==
                    "tdx-root-relative", "path scope");
        const auto& sources = document.at("sources").as_array();
        require(sources[0].at("path").as_string() ==
                    "T0002/hq_cache/pttab.dat" &&
                    sources[0].at("endpoint").as_string() ==
                    "local-file:T0002/hq_cache/pttab.dat" &&
                    sources[1].at("path").as_string() ==
                    "T0002/hq_cache/speczsevent.txt" &&
                    sources[0].at("extra").as_number() == 7,
                "local resource paths are projected without losing metadata");
        require(document.dump(-1).find(tdx::path_utf8(fixture.root)) ==
                    std::string::npos, "temporary root is not exposed");

        tdx::Json singular = tdx::Json::object();
        singular["source"] = tdx::Json::object();
        singular["source"]["file"] = "hkcwdata.dat";
        singular["source"]["path"] = tdx::path_utf8(
            fixture.root / "T0002" / "hq_cache" / "hkcwdata.dat");
        singular["source"]["endpoint"] = "local-file:" +
            singular.at("source").at("path").as_string();
        singular["source"]["encrypted"] = true;
        tdx::server_detail::project_local_catalog_resource_paths(
            singular, fixture.root);
        require(singular.at("source").at("path").as_string() ==
                    "T0002/hq_cache/hkcwdata.dat" &&
                    singular.at("source").at("endpoint").as_string() ==
                    "local-file:T0002/hq_cache/hkcwdata.dat" &&
                    singular.at("source").at("encrypted").as_bool(),
                "singular local resource source is projected");
        require(singular.dump(-1).find(tdx::path_utf8(fixture.root)) ==
                    std::string::npos,
                "singular source does not expose the temporary root");

        tdx::Json escaped = tdx::Json::object();
        escaped["sources"] = tdx::Json::array();
        tdx::Json source = tdx::Json::object();
        source["path"] = tdx::path_utf8(
            fixture.root.parent_path() / "outside.dat");
        source["endpoint"] = "local-file:" + source.at("path").as_string();
        escaped["sources"].push_back(std::move(source));
        require_error([&] {
            tdx::server_detail::project_local_catalog_resource_paths(
                escaped, fixture.root);
        }, "outside the TDX root");

        tdx::Json missing = tdx::Json::object();
        require_error([&] {
            tdx::server_detail::project_local_catalog_resource_paths(
                missing, fixture.root);
        }, "source or sources");

        std::cout << "server local resource surface tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
