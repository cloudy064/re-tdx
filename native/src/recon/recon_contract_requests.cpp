#include "recon_contract_internal.hpp"

#include "tdx/common.hpp"

#include <cstdlib>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace tdx::recon_contract_detail {
namespace {

constexpr std::uintmax_t maximum_request_catalog_size = 512U * 1024U;
constexpr std::size_t maximum_request_count = 1000;
constexpr std::string_view request_catalog_file =
    "api-contract-post-requests.json";

const Json* catalog_member(const Json& object, std::string_view key) {
    if (!object.is_object()) return nullptr;
    const auto found = object.as_object().find(key);
    return found == object.as_object().end() ? nullptr : &found->second;
}

std::string required_string(const Json& object, std::string_view key) {
    const auto* value = catalog_member(object, key);
    if (!value || !value->is_string())
        throw Error("recon request catalog entry requires string " +
                    std::string(key));
    return value->as_string();
}

fs::path locate_request_catalog() {
    std::vector<fs::path> candidates;
    if (const char* environment = std::getenv("TDX_RECON_ASSETS");
        environment && *environment)
        candidates.push_back(fs::u8path(environment));
    const auto executable = running_executable_path();
    if (!executable.empty()) {
        candidates.push_back(executable.parent_path() / "recon-assets");
        candidates.push_back(executable.parent_path().parent_path() /
                             "share" / "tdx-tool" / "recon-assets");
    }
    candidates.push_back(fs::path("recon-assets"));
    candidates.push_back(fs::path("native") / "resources" / "recon");
#ifdef TDX_RECON_ASSET_SOURCE_DIR
    candidates.emplace_back(TDX_RECON_ASSET_SOURCE_DIR);
#endif
    for (const auto& directory : candidates) {
        std::error_code error;
        const auto path = directory / request_catalog_file;
        if (!fs::is_regular_file(path, error) || error) continue;
        const auto size = fs::file_size(path, error);
        if (error || size > maximum_request_catalog_size)
            throw Error("recon request catalog exceeds its safe size: " +
                        path_utf8(path));
        return fs::weakly_canonical(path);
    }
    throw Error("cannot locate recon-assets/api-contract-post-requests.json; "
                "set TDX_RECON_ASSETS or install the recon assets");
}

using RequestCatalog =
    std::map<std::string, ApiContractPostRequest, std::less<>>;

RequestCatalog load_request_catalog() {
    const auto path = locate_request_catalog();
    const auto document = Json::parse(read_text_utf8(path));
    if (!document.is_object() ||
        required_string(document, "schema") !=
            "tdx-recon-api-contract-requests-v1")
        throw Error("recon request catalog schema is unsupported");
    const auto* entries = catalog_member(document, "requests");
    if (!entries || !entries->is_array() ||
        entries->size() > maximum_request_count)
        throw Error("recon request catalog requires a bounded requests array");

    RequestCatalog catalog;
    for (const auto& entry : entries->as_array()) {
        if (!entry.is_object())
            throw Error("recon request catalog entries must be objects");
        const auto id = required_string(entry, "id");
        const auto action = required_string(entry, "action");
        const auto body = required_string(entry, "body");
        if (id.empty() || action.empty())
            throw Error("recon request catalog id/action must not be empty");
        Json::parse(body);
        const auto [_, inserted] = catalog.emplace(
            id, ApiContractPostRequest{action, body});
        if (!inserted)
            throw Error("duplicate recon request catalog id: " + id);
    }
    return catalog;
}

}  // namespace

const ApiContractPostRequest* api_contract_post_request(std::string_view id) {
    static const RequestCatalog requests = load_request_catalog();
    const auto found = requests.find(id);
    return found == requests.end() ? nullptr : &found->second;
}

}  // namespace tdx::recon_contract_detail
