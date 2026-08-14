#include "tdx/cloud_variants.hpp"

#include "tdx/common.hpp"
#include "tdx/utf8.hpp"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string_view>
#include <tuple>

namespace fs = std::filesystem;

namespace tdx {
namespace {

struct Variant {
    std::string transport;
    std::string request_id;
    std::string entry;
    std::string module;
    std::string digest;
    std::size_t body_size{};
    std::size_t occurrences{};
    std::set<std::string> source_files;
    std::vector<std::string> placeholders;
    Json request_fields{Json::array()};
    Json selectors{Json::object()};
    std::string parse_error;
    std::string fixed_command;
};

std::string canonical_body(std::string_view body) {
    std::string result;
    result.reserve(body.size());
    char quote = 0;
    bool escaped = false;
    for (char ch : body) {
        if (quote) {
            result.push_back(ch);
            if (escaped) escaped = false;
            else if (ch == '\\') escaped = true;
            else if (ch == quote) quote = 0;
        } else if (ch == '\'' || ch == '"') {
            quote = ch;
            result.push_back(ch);
        } else if (!std::isspace(static_cast<unsigned char>(ch))) {
            result.push_back(ch);
        }
    }
    return result;
}

std::string digest_body(const std::string& body) {
    const Bytes bytes(body.begin(), body.end());
    return md5_bytes(bytes);
}

const Json* request_object(const Json& request) {
    if (request.is_object()) return &request;
    if (request.is_array() && !request.as_array().empty() &&
        request.as_array().front().is_object()) return &request.as_array().front();
    return nullptr;
}

std::string value_text(const Json& value) {
    if (value.is_string()) return value.as_string();
    if (value.is_number()) {
        std::ostringstream output;
        output << value.as_number();
        return output.str();
    }
    if (value.is_bool()) return value.as_bool() ? "true" : "false";
    if (value.is_null()) return "null";
    return value.dump(-1);
}

bool infrastructure_field(const std::string& name) {
    const auto folded = lower_ascii(name);
    return folded == "reqid" || folded == "page" || folded == "pagesize" ||
           folded == "modname" || folded == "fields";
}

void describe_request(const Json& request, Json& fields, Json& selectors) {
    const auto* object = request_object(request);
    if (!object) throw Error("cloud template request does not contain an object");
    for (const auto& [name, value] : object->as_object()) {
        fields.push_back(name);
        const auto text = value_text(value);
        if (infrastructure_field(name) || text.rfind("__TDX_", 0) == 0) continue;
        const auto prefix = utf8_prefix(text, 256);
        selectors[name] = prefix.size() < text.size()
            ? Json(prefix + "... [" + std::to_string(text.size()) + " bytes]")
            : Json(prefix);
    }
}

std::map<std::string, std::string> placeholder_values(
    const std::vector<std::string>& placeholders) {
    std::map<std::string, std::string> result;
    for (const auto& name : placeholders) result[name] = "__TDX_" + name + "__";
    return result;
}

const std::map<std::string, std::string>& tqlex_bindings() {
    static const std::map<std::string, std::string> values{
        {"200000", "market relative-valuation"},
        {"200003", "market index-volatility"}, {"200004", "market index-volatility"},
        {"200009", "market flow-followup"}, {"200010", "market flow-followup"},
        {"200626", "market factors"}, {"200636", "market factors"},
        {"200646", "market factors"}, {"200650", "market factors"},
        {"200651", "market factors"}, {"200660", "market factors"},
        {"200661", "market technical-signals"},
        {"200662", "market technical-signals"},
        {"200720", "market anomaly-risk"}, {"2044", "market anomaly-risk"},
        {"200770", "market total-return-gap"},
        {"500030", "market fund-analytics"}, {"500031", "market fund-analytics"},
        {"500032", "market fund-analytics"}, {"500033", "market fund-analytics"},
        {"500035", "market fund-analytics"}, {"500050", "market fund-analytics"},
        {"500051", "market fund-analytics"}, {"500052", "market fund-analytics"},
        {"500055", "market fund-analytics"}, {"500056", "market fund-analytics"},
        {"500057", "market fund-analytics"}, {"500060", "market fund-analytics"},
        {"500062", "market fund-analytics"},
        {"500108", "market abnormal-details"}, {"500109", "market abnormal-details"},
        {"500501", "market flow-followup"}, {"500502", "market flow-followup"},
        {"500601", "market profit-gaps"}
    };
    return values;
}

const std::map<std::string, std::string>& pbrpc_bindings() {
    static const std::map<std::string, std::string> values{
        {"200001", "market relative-valuation"}, {"200011", "market flow-followup"},
        {"200199", "market block-backtest"},
        {"200225", "market technical-signals"}, {"200250", "market technical-signals"},
        {"200300", "market equity-valuation"}, {"200301", "market equity-valuation"},
        {"200302", "market equity-valuation"}, {"200303", "market equity-valuation"},
        {"200305", "market equity-valuation"},
        {"200316", "market technical-signals"}, {"200320", "market technical-signals"},
        {"200325", "market technical-signals"}, {"200327", "market technical-signals"},
        {"200329", "market technical-signals"},
        {"200340", "market funds"}, {"200341", "market funds"},
        {"200400", "market technical-signals"}, {"200401", "market technical-signals"},
        {"200402", "market technical-signals"}, {"200403", "market technical-signals"},
        {"200404", "market technical-signals"}, {"200405", "market technical-signals"},
        {"200406", "market technical-signals"},
        {"200451", "market technical-signals"}, {"200452", "market technical-signals"},
        {"200453", "market technical-signals"},
        {"500107", "market abnormal-moves"}, {"500503", "market flow-followup"}
    };
    return values;
}

std::string fixed_command(const Variant& variant, const std::string& canonical) {
    const auto& bindings = variant.transport == "tqlex" ? tqlex_bindings() : pbrpc_bindings();
    if (!variant.request_id.empty()) {
        const auto found = bindings.find(variant.request_id);
        return found == bindings.end() ? std::string{} : found->second;
    }
    if (variant.transport == "tqlex" &&
        lower_ascii(variant.entry) == "cwsearch.tzx_rcache" &&
        lower_ascii(canonical).find("\"key\":\"ly:") != std::string::npos)
        return "market roadshows";
    return {};
}

Json strings(const std::set<std::string>& values) {
    Json result = Json::array();
    for (const auto& value : values) result.push_back(value);
    return result;
}

Json strings(const std::vector<std::string>& values) {
    Json result = Json::array();
    for (const auto& value : values) result.push_back(value);
    return result;
}

void add_variant(std::map<std::string, Variant>& variants, const TqlexConfig& config) {
    const auto canonical = canonical_body(config.body);
    Variant candidate;
    candidate.transport = "tqlex"; candidate.request_id = config.request_id;
    candidate.entry = config.entry; candidate.body_size = canonical.size();
    candidate.placeholders = config.placeholders;
    std::string fingerprint = canonical;
    try {
        const auto request = parse_tqlex_config_body(
            config.body, placeholder_values(config.placeholders), 0, 20);
        describe_request(request, candidate.request_fields, candidate.selectors);
        fingerprint = request.dump(-1);
    } catch (const std::exception& error) { candidate.parse_error = error.what(); }
    candidate.digest = digest_body(fingerprint);
    candidate.fixed_command = fixed_command(candidate, canonical);
    const auto key = "tqlex|" + lower_ascii(config.entry) + '|' + config.request_id + '|' +
                     candidate.digest;
    auto [found, inserted] = variants.emplace(key, std::move(candidate));
    auto& value = found->second;
    ++value.occurrences;
    value.source_files.insert(config.source_file);
}

void add_variant(std::map<std::string, Variant>& variants, const PbrpcConfig& config) {
    const auto canonical = canonical_body(config.body);
    Variant candidate;
    candidate.transport = "pbrpc"; candidate.request_id = config.request_id;
    candidate.entry = config.entry; candidate.module = config.module;
    candidate.body_size = canonical.size(); candidate.placeholders = config.placeholders;
    std::string fingerprint = canonical;
    try {
        const auto descriptor = parse_pbrpc_descriptor(
            config.body, placeholder_values(config.placeholders));
        describe_request(descriptor.second, candidate.request_fields, candidate.selectors);
        fingerprint = descriptor.second.dump(-1);
    } catch (const std::exception& error) { candidate.parse_error = error.what(); }
    candidate.digest = digest_body(fingerprint);
    candidate.fixed_command = fixed_command(candidate, canonical);
    const auto key = "pbrpc|" + lower_ascii(config.entry) + '|' + lower_ascii(config.module) +
                     '|' + config.request_id + '|' + candidate.digest;
    auto [found, inserted] = variants.emplace(key, std::move(candidate));
    auto& value = found->second;
    ++value.occurrences;
    value.source_files.insert(config.source_file);
}

Json variant_json(const Variant& value) {
    Json result = Json::object();
    result["transport"] = value.transport;
    result["request_id"] = value.request_id.empty() ? Json(nullptr) : Json(value.request_id);
    result["entry"] = value.entry;
    result["module"] = value.module.empty() ? Json(nullptr) : Json(value.module);
    result["variant_id"] = value.digest;
    result["body_size"] = static_cast<std::uint64_t>(value.body_size);
    result["template_occurrences"] = static_cast<std::uint64_t>(value.occurrences);
    result["source_files"] = strings(value.source_files);
    result["placeholders"] = strings(value.placeholders);
    result["request_fields"] = value.request_fields;
    result["selectors"] = value.selectors;
    result["parse_error"] = value.parse_error.empty() ? Json(nullptr) : Json(value.parse_error);
    result["coverage"] = value.fixed_command.empty() ? "generic-only" : "fixed-command";
    result["fixed_command"] = value.fixed_command.empty() ? Json(nullptr) : Json(value.fixed_command);
    result["selection_key"] = value.request_id.empty() ? "entry+body" : "request-id+selectors";
    return result;
}

}  // namespace

Json cloud_variant_coverage_document(const std::vector<TqlexConfig>& tqlex_configs,
                                     const std::vector<PbrpcConfig>& pbrpc_configs,
                                     bool gaps_only) {
    std::map<std::string, Variant> variants;
    for (const auto& config : tqlex_configs) add_variant(variants, config);
    for (const auto& config : pbrpc_configs) add_variant(variants, config);

    std::size_t fixed = 0, generic = 0, parse_errors = 0, duplicate_templates = 0;
    std::set<std::string> tqlex_ids, pbrpc_ids, fixed_tqlex_ids, fixed_pbrpc_ids;
    std::map<std::string, std::size_t> by_command;
    Json records = Json::array();
    for (const auto& [key, value] : variants) {
        (void)key;
        if (value.occurrences > 1) duplicate_templates += value.occurrences - 1;
        if (!value.parse_error.empty()) ++parse_errors;
        auto& ids = value.transport == "tqlex" ? tqlex_ids : pbrpc_ids;
        auto& fixed_ids = value.transport == "tqlex" ? fixed_tqlex_ids : fixed_pbrpc_ids;
        if (!value.request_id.empty()) ids.insert(value.request_id);
        if (value.fixed_command.empty()) ++generic;
        else {
            ++fixed; ++by_command[value.fixed_command];
            if (!value.request_id.empty()) fixed_ids.insert(value.request_id);
        }
        if (!gaps_only || value.fixed_command.empty()) records.push_back(variant_json(value));
    }

    Json commands = Json::array();
    for (const auto& [command, count] : by_command) {
        Json row = Json::object(); row["command"] = command;
        row["semantic_variants"] = static_cast<std::uint64_t>(count);
        commands.push_back(std::move(row));
    }
    Json summary = Json::object();
    summary["template_count"] = static_cast<std::uint64_t>(tqlex_configs.size() + pbrpc_configs.size());
    summary["tqlex_template_count"] = static_cast<std::uint64_t>(tqlex_configs.size());
    summary["pbrpc_template_count"] = static_cast<std::uint64_t>(pbrpc_configs.size());
    summary["semantic_variant_count"] = static_cast<std::uint64_t>(variants.size());
    summary["fixed_variant_count"] = static_cast<std::uint64_t>(fixed);
    summary["generic_only_variant_count"] = static_cast<std::uint64_t>(generic);
    summary["duplicate_template_count"] = static_cast<std::uint64_t>(duplicate_templates);
    summary["parse_error_count"] = static_cast<std::uint64_t>(parse_errors);
    summary["tqlex_request_id_count"] = static_cast<std::uint64_t>(tqlex_ids.size());
    summary["fixed_tqlex_request_id_count"] = static_cast<std::uint64_t>(fixed_tqlex_ids.size());
    summary["pbrpc_request_id_count"] = static_cast<std::uint64_t>(pbrpc_ids.size());
    summary["fixed_pbrpc_request_id_count"] = static_cast<std::uint64_t>(fixed_pbrpc_ids.size());
    summary["fully_fixed"] = generic == 0 && parse_errors == 0;

    Json result = Json::object();
    result["schema"] = "tdx-cloud-variant-coverage-native-v1";
    result["gaps_only"] = gaps_only;
    result["semantics"] = "Templates are deduplicated by transport, route and parsed canonical request JSON; fixed-command means a typed C++ business command owns the variant, not merely that the generic cloud query can send it.";
    result["summary"] = std::move(summary);
    result["commands"] = std::move(commands);
    result["variants"] = std::move(records);
    return result;
}

Json cloud_variant_coverage_document(const fs::path& root, bool gaps_only) {
    return cloud_variant_coverage_document(inventory_tqlex_configs(root),
                                           inventory_pbrpc_configs(root), gaps_only);
}

int command_recon_cloud_variants(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout <<
            "Usage: tdx-tool recon cloud-variants [options]\n\n"
            "  --root PATH     TDX installation root\n"
            "  --gaps-only     Only emit templates without a typed C++ business command\n"
            "  --output FILE   Default output/tdx-cloud-variants.json\n"
            "  --compact\n";
        return 0;
    }
    const auto root_text = args.take_option("--root");
    const bool gaps_only = args.take_flag("--gaps-only");
    const auto output_text = args.take_option("--output", "output/tdx-cloud-variants.json");
    const bool compact = args.take_flag("--compact");
    args.require_empty();
    const auto root = find_tdx_root(root_text.empty() ? fs::path{} : fs::u8path(root_text));
    const auto result = cloud_variant_coverage_document(root, gaps_only);
    const auto output = fs::u8path(output_text);
    atomic_write_text(output, result.dump(compact ? -1 : 2) + "\n");
    const auto& summary = result.at("summary");
    std::cout << "cloud variants: " << summary.at("fixed_variant_count").as_number()
              << '/' << summary.at("semantic_variant_count").as_number()
              << " fixed -> " << path_utf8(output) << '\n';
    return summary.at("generic_only_variant_count").as_number() == 0 ? 0 : 1;
}

}  // namespace tdx
