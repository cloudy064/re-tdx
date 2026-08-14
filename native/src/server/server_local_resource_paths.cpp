#include "server_local_resource_paths_internal.hpp"
#include "server_core_internal.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <string>
#include <string_view>

namespace fs = std::filesystem;

namespace tdx::server_detail {
namespace {

std::string root_relative_resource_path(const fs::path& root,
                                        const std::string& source) {
    std::error_code error;
    const auto canonical_root = fs::weakly_canonical(root, error);
    if (error) throw Error("failed to canonicalize TDX root");
    auto candidate = from_utf8(source);
    if (!candidate.is_absolute()) candidate = canonical_root / candidate;
    error.clear();
    candidate = fs::weakly_canonical(candidate, error);
    if (error) throw Error("failed to canonicalize local resource path");
    const auto relative = candidate.lexically_relative(canonical_root);
    if (relative.empty() || relative.is_absolute())
        throw Error("local resource path is outside the TDX root");
    for (const auto& part : relative)
        if (part == "..")
            throw Error("local resource path is outside the TDX root");
    auto result = path_utf8(relative);
    std::replace(result.begin(), result.end(), '\\', '/');
    if (result.empty() || result == ".")
        throw Error("local resource path has no safe root-relative value");
    return result;
}

void project_source(Json& source, const fs::path& root) {
    if (!source.is_object())
        throw Error("local resource source must be an object");
    if (source.as_object().count("path") && source.at("path").is_string())
        source["path"] = root_relative_resource_path(
            root, source.at("path").as_string());
    if (source.as_object().count("endpoint") &&
        source.at("endpoint").is_string()) {
        constexpr std::string_view prefix = "local-file:";
        const auto endpoint = source.at("endpoint").as_string();
        if (endpoint.rfind(prefix, 0) != 0)
            throw Error("local resource endpoint must use local-file:");
        source["endpoint"] = std::string(prefix) +
            root_relative_resource_path(
                root, endpoint.substr(prefix.size()));
    }
}

}  // namespace

void project_local_catalog_resource_paths(Json& document, const fs::path& root) {
    if (!document.is_object())
        throw Error("local resource document must be an object");
    bool projected = false;
    if (document.as_object().count("sources")) {
        if (!document.at("sources").is_array())
            throw Error("local resource sources must be an array");
        for (auto& source : document["sources"].as_array())
            project_source(source, root);
        projected = true;
    }
    if (document.as_object().count("source")) {
        project_source(document["source"], root);
        projected = true;
    }
    if (!projected)
        throw Error("local resource document must contain source or sources");
    document["path_scope"] = "tdx-root-relative";
}

}  // namespace tdx::server_detail
