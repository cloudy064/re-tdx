#include "jsn_variants_internal.hpp"

#include "tdx/common.hpp"
#include "tdx/jsn.hpp"
#include "tdx/jsn_data.hpp"
#include "tdx/tqlex.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iterator>
#include <iostream>
#include <map>
#include <optional>
#include <regex>
#include <set>
#include <sstream>
#include <string_view>

namespace fs = std::filesystem;

namespace tdx::jsn_variant_detail {

struct CandidateRelation {
    std::set<std::string> detail_configs;
    std::set<std::string> detail_unit_ids;
    std::set<std::string> reference_unit_ids;
    std::set<std::string> master_resources;
    std::map<std::string, std::set<std::string>> master_unit_ids;
    std::map<std::string, std::set<std::string>> master_configs;
};

struct CandidateAggregate {
    std::string template_resource;
    std::string resource;
    std::map<std::string, std::string> keys;
    std::set<std::string> source_resources;
    std::set<std::string> source_unit_ids;
    std::set<std::string> source_configs;
    std::uint64_t evidence_rows{};
    bool derived{};
    bool observed{};
    bool local_present{};
    std::uint64_t local_bytes{};
};

std::map<std::string, std::set<std::string>> page_config_relations(
    const fs::path& root) {
    std::map<std::string, std::set<std::string>> result;
    const auto t0002 = root / "T0002";
    if (!fs::is_directory(t0002)) return result;
    std::error_code error;
    for (fs::recursive_directory_iterator iterator(t0002, error), end;
         iterator != end; iterator.increment(error)) {
        if (error) throw Error("cannot scan TDX page definitions: " + error.message());
        if (!iterator->is_regular_file() ||
            lower_ascii(iterator->path().extension().string()) != ".sp") continue;
        std::set<std::string> configs;
        std::istringstream stream(config_text(iterator->path()));
        std::string line;
        while (std::getline(stream, line)) {
            const auto separator = line.find('=');
            if (separator == std::string::npos ||
                lower_ascii(trim(line.substr(0, separator))) != "cfgname") continue;
            auto value = trim(line.substr(separator + 1));
            if (value.empty()) continue;
            value = lower_ascii(path_utf8(fs::path(value).stem()));
            if (!value.empty()) configs.insert(std::move(value));
        }
        for (const auto& left : configs)
            result[left].insert(configs.begin(), configs.end());
    }
    return result;
}

bool relation_visible_to_detail(
    const UnitDefinition& detail,
    const UnitDefinition& master,
    const std::map<std::string, std::set<std::string>>& page_relations) {
    if (detail.relation_scope == master.relation_scope) return true;
    const auto page = page_relations.find(detail.config_name);
    return page != page_relations.end() && page->second.count(master.config_name) != 0;
}

std::optional<std::size_t> candidate_column(
    const std::vector<std::string>& headers, const std::string& name) {
    const auto target = lower_ascii(name);
    const std::array<std::string, 4> preferred = target == "sc"
        ? std::array<std::string, 4>{"$sc", "sc", "$sc1", "sc1"}
        : std::array<std::string, 4>{"$zqdm", "zqdm", "$zqdm1", "zqdm1"};
    for (const auto& candidate : preferred)
        for (std::size_t index = 0; index < headers.size(); ++index)
            if (lower_ascii(headers[index]) == candidate) return index;
    return std::nullopt;
}

bool safe_candidate_key(const std::string& value) {
    if (value.empty() || value == "." || value == "..") return false;
    for (unsigned char ch : value)
        if (ch <= 0x20 || ch >= 0x7F || ch == '/' || ch == '\\' || ch == ':') return false;
    return true;
}

std::optional<std::string> expand_candidate_resource(
    const JsnResourceTemplate& resource,
    const std::map<std::string, std::string>& keys) {
    auto result = resource.resource;
    for (const auto& token : resource.placeholders) {
        const auto name = token.substr(3, token.size() - 5);
        const auto found = keys.find(name);
        if (found == keys.end() || !safe_candidate_key(found->second)) return std::nullopt;
        std::size_t offset = 0;
        while ((offset = result.find(token, offset)) != std::string::npos) {
            result.replace(offset, token.size(), found->second);
            offset += found->second.size();
        }
    }
    constexpr std::size_t jsn_info_path_size = 40;
    if (!placeholders(result).empty() || result.size() + 3 >= jsn_info_path_size)
        return std::nullopt;
    try {
        return normalize_resource(result);
    } catch (...) {
        return std::nullopt;
    }
}

std::map<std::string, CandidateRelation> candidate_relations(
    const fs::path& root,
    const std::vector<JsnResourceTemplate>& templates) {
    const auto units = unit_definitions(root);
    const auto page_relations = page_config_relations(root);
    std::map<std::string, std::vector<const UnitDefinition*>> by_id;
    for (const auto& unit : units)
        if (!unit.id.empty()) by_id[unit.id].push_back(&unit);
    std::set<std::string> dynamic_templates;
    for (const auto& resource : templates)
        if (!resource.placeholders.empty())
            dynamic_templates.insert(lower_ascii(resource.resource));

    std::map<std::string, CandidateRelation> result;
    for (const auto& detail : units) {
        const auto known = detail_templates().find(lower_ascii(detail.file));
        if (known == detail_templates().end()) continue;
        for (const auto& template_resource : known->second) {
            const auto key = lower_ascii(template_resource);
            if (!dynamic_templates.count(key)) continue;
            auto& relation = result[key];
            relation.detail_configs.insert(detail.source_file);
            if (!detail.id.empty()) relation.detail_unit_ids.insert(detail.id);
            for (const auto& reference : detail.reference_ids) {
                relation.reference_unit_ids.insert(reference);
                const auto found = by_id.find(reference);
                if (found == by_id.end()) continue;
                for (const auto* master : found->second) {
                    if (!relation_visible_to_detail(detail, *master, page_relations)) continue;
                    if (lower_ascii(fs::path(master->file).extension().string()) != ".jsn")
                        continue;
                    try {
                        const auto resource = normalize_resource(master->file);
                        relation.master_resources.insert(resource);
                        if (!master->id.empty())
                            relation.master_unit_ids[resource].insert(master->id);
                        if (!master->source_file.empty())
                            relation.master_configs[resource].insert(master->source_file);
                    } catch (...) {}
                }
            }
        }
    }
    return result;
}

struct LocalCandidateFile {
    std::string resource;
    std::uint64_t bytes{};
};

std::map<std::string, LocalCandidateFile> local_candidate_files(const fs::path& root) {
    std::map<std::string, LocalCandidateFile> result;
    if (!fs::is_directory(root)) return result;
    std::error_code error;
    for (fs::recursive_directory_iterator iterator(root, error), end;
         iterator != end; iterator.increment(error)) {
        if (error) throw Error("cannot scan JSN candidate directory: " + error.message());
        if (!iterator->is_regular_file() ||
            lower_ascii(iterator->path().extension().string()) != ".jsn") continue;
        LocalCandidateFile file;
        file.resource = relative_resource(iterator->path(), root);
        file.bytes = static_cast<std::uint64_t>(iterator->file_size());
        result[lower_ascii(file.resource)] = std::move(file);
    }
    return result;
}

Json candidate_document_impl(const fs::path& root,
                             const fs::path& downloaded_root,
                             std::string family,
                             bool missing_only,
                             int limit,
                             bool probe,
                             int max_probes,
                             int timeout_ms) {
    if (limit < 0 || limit > 100000) throw Error("limit must be in 0..100000");
    if (max_probes < 1 || max_probes > 25)
        throw Error("max_probes must be in 1..25");
    if (timeout_ms < 100 || timeout_ms > 60000)
        throw Error("timeout_ms must be in 100..60000");
    family = lower_ascii(trim(std::move(family)));
    if (probe && family.empty())
        throw Error("probing candidate resources requires an explicit family");

    const auto templates = inventory_jsn_resource_templates(root);
    const auto relations = candidate_relations(root, templates);
    const auto local_files = local_candidate_files(downloaded_root);
    const auto bindings = coverage_bindings();
    std::map<std::string, CandidateAggregate> candidates;
    std::map<std::string, std::set<std::string>> relation_issues;
    std::map<std::string, std::uint64_t> loaded_masters;
    std::map<std::string, std::vector<JsnTable>> master_table_cache;
    std::map<std::string, std::string> master_table_errors;

    auto selected_template = [&](const JsnResourceTemplate& value) {
        return !value.placeholders.empty() &&
            (family.empty() || family_name(value.resource) == family ||
             lower_ascii(value.resource).rfind(family + "/", 0) == 0);
    };
    auto commands_for = [&](const std::string& resource) {
        std::set<std::string> result;
        for (const auto& binding : bindings) {
            if (!binding_matches(resource, binding.pattern)) continue;
            auto command = binding.command;
            constexpr std::string_view analysis_suffix = "-analysis";
            if (command.size() > analysis_suffix.size() &&
                command.substr(command.size() - analysis_suffix.size()) == analysis_suffix)
                command.resize(command.size() - analysis_suffix.size());
            result.insert(std::move(command));
        }
        return result;
    };
    auto add_candidate = [&](const JsnResourceTemplate& template_value,
                             const std::string& concrete,
                             const std::map<std::string, std::string>& keys,
                             const std::string& source_resource,
                             const std::string& source_unit,
                             const std::string& source_config,
                             bool derived,
                             bool observed) {
        const auto map_key = lower_ascii(template_value.resource) + "\n" +
            lower_ascii(concrete);
        auto& value = candidates[map_key];
        value.template_resource = template_value.resource;
        value.resource = concrete;
        if (value.keys.empty()) value.keys = keys;
        if (!source_resource.empty()) value.source_resources.insert(source_resource);
        if (!source_unit.empty()) value.source_unit_ids.insert(source_unit);
        if (!source_config.empty()) value.source_configs.insert(source_config);
        value.derived = value.derived || derived;
        value.observed = value.observed || observed;
        if (derived) ++value.evidence_rows;
        const auto local = local_files.find(lower_ascii(concrete));
        if (local != local_files.end()) {
            value.local_present = true;
            value.local_bytes = local->second.bytes;
        }
    };

    for (const auto& [key, local] : local_files) {
        (void)key;
        for (const auto& match : concrete_template_matches(local.resource, templates)) {
            if (!selected_template(*match.value)) continue;
            add_candidate(*match.value, local.resource, match.keys, {}, {}, {}, false, true);
        }
    }

    for (const auto& template_value : templates) {
        if (!selected_template(template_value)) continue;
        const auto template_key = lower_ascii(template_value.resource);
        const auto template_commands = commands_for(template_value.resource);
        const auto relation = relations.find(template_key);
        if (relation == relations.end()) {
            relation_issues[template_key].insert("no-refunit-relation");
            continue;
        }
        if (relation->second.master_resources.empty()) {
            relation_issues[template_key].insert("no-static-master-resource");
            continue;
        }
        for (const auto& master_resource : relation->second.master_resources) {
            const auto master_commands = commands_for(master_resource);
            std::vector<std::string> shared_commands;
            std::set_intersection(template_commands.begin(), template_commands.end(),
                                  master_commands.begin(), master_commands.end(),
                                  std::back_inserter(shared_commands));
            if (shared_commands.empty()) {
                relation_issues[template_key].insert(
                    "refunit-command-mismatch:" + master_resource);
                continue;
            }
            const auto local = local_files.find(lower_ascii(master_resource));
            if (local == local_files.end() || !local->second.bytes) {
                relation_issues[template_key].insert("master-not-downloaded:" + master_resource);
                continue;
            }
            if (!master_table_cache.count(master_resource) &&
                !master_table_errors.count(master_resource)) {
                try {
                    master_table_cache.emplace(master_resource,
                        load_jsn_tables(downloaded_root / from_utf8(master_resource)));
                } catch (const std::exception& error) {
                    master_table_errors[master_resource] = error.what();
                }
            }
            const auto parse_error = master_table_errors.find(master_resource);
            if (parse_error != master_table_errors.end()) {
                relation_issues[template_key].insert(
                    "master-parse-error:" + master_resource + ":" + parse_error->second);
                continue;
            }
            ++loaded_masters[template_key];
            for (const auto& table : master_table_cache.at(master_resource)) {
                std::map<std::string, std::optional<std::size_t>> indexes;
                for (const auto& token : template_value.placeholders) {
                    const auto name = token.substr(3, token.size() - 5);
                    indexes[name] = candidate_column(table.headers, name);
                }
                const bool complete = std::all_of(indexes.begin(), indexes.end(),
                    [](const auto& item) { return item.second.has_value(); });
                if (!complete) {
                    relation_issues[template_key].insert(
                        "master-key-columns-missing:" + master_resource);
                    continue;
                }
                for (const auto& row : table.rows) {
                    std::map<std::string, std::string> keys;
                    bool valid = true;
                    for (const auto& [name, index] : indexes) {
                        if (!index || *index >= row.size()) { valid = false; break; }
                        auto value = trim(jsn_scalar_text(row[*index]));
                        if (!safe_candidate_key(value)) { valid = false; break; }
                        keys[name] = std::move(value);
                    }
                    if (!valid) continue;
                    const auto concrete = expand_candidate_resource(template_value, keys);
                    if (!concrete) continue;
                    const auto units = relation->second.master_unit_ids.find(master_resource);
                    const auto source_unit = units == relation->second.master_unit_ids.end() ||
                        units->second.empty() ? std::string{} : *units->second.begin();
                    const auto source_config = relation->second.detail_configs.empty()
                        ? std::string{} : *relation->second.detail_configs.begin();
                    add_candidate(template_value, *concrete, keys, master_resource,
                                  source_unit, source_config, true, false);
                    const auto configs = relation->second.master_configs.find(master_resource);
                    if (configs != relation->second.master_configs.end()) {
                        auto& aggregate = candidates[lower_ascii(template_value.resource) +
                            "\n" + lower_ascii(*concrete)];
                        aggregate.source_configs.insert(
                            configs->second.begin(), configs->second.end());
                    }
                }
            }
        }
    }

    std::map<std::string, std::uint64_t> template_candidates;
    std::map<std::string, std::uint64_t> template_observed;
    std::map<std::string, std::uint64_t> template_missing;
    std::set<std::string> unique_resources, missing_resources;
    std::uint64_t observed_candidates = 0, derived_candidates = 0;
    struct RankedCandidate { int priority{}; Json row; std::string resource; bool missing{}; };
    std::vector<RankedCandidate> ranked;
    for (const auto& [key, value] : candidates) {
        (void)key;
        ++template_candidates[lower_ascii(value.template_resource)];
        if (value.observed) { ++template_observed[lower_ascii(value.template_resource)]; ++observed_candidates; }
        if (value.derived) ++derived_candidates;
        if (!value.local_present) ++template_missing[lower_ascii(value.template_resource)];
        unique_resources.insert(lower_ascii(value.resource));
        if (!value.local_present) missing_resources.insert(lower_ascii(value.resource));
        int priority = value.local_present ? 10 : 100;
        if (value.derived) priority += 50;
        if (value.observed) priority += 20;
        if (value.keys.count("SC")) priority += 10;
        priority += static_cast<int>(std::min<std::size_t>(value.source_resources.size() * 5, 25));
        Json row = Json::object();
        row["template"] = value.template_resource;
        row["resource"] = value.resource;
        row["family"] = family_name(value.template_resource);
        row["key_values"] = key_values_json(value.keys);
        row["status"] = value.local_present ? "local-present" : "local-missing";
        row["origin"] = value.derived && value.observed ? "refunit+observed"
            : value.derived ? "refunit-row" : "observed-only";
        row["probe_eligible"] = value.derived && !value.local_present;
        row["local_bytes"] = value.local_present ? Json(value.local_bytes) : Json(nullptr);
        row["source_resources"] = strings(value.source_resources);
        row["source_unit_ids"] = strings(value.source_unit_ids);
        row["source_configs"] = strings(value.source_configs);
        row["evidence_rows"] = value.evidence_rows;
        row["priority_score"] = priority;
        ranked.push_back({priority, std::move(row), value.resource, !value.local_present});
    }
    std::sort(ranked.begin(), ranked.end(), [](const auto& left, const auto& right) {
        if (left.priority != right.priority) return left.priority > right.priority;
        return lower_ascii(left.resource) < lower_ascii(right.resource);
    });

    Json rows = Json::array();
    std::vector<std::string> probe_queue;
    std::set<std::string> queued;
    for (const auto& candidate : ranked) {
        if (missing_only && !candidate.missing) continue;
        if (rows.size() < static_cast<std::size_t>(limit)) rows.push_back(candidate.row);
        if (candidate.missing && candidate.row.at("probe_eligible").as_bool() &&
            queued.insert(lower_ascii(candidate.resource)).second &&
            probe_queue.size() < static_cast<std::size_t>(max_probes))
            probe_queue.push_back(candidate.resource);
    }

    Json diagnostics = Json::array();
    std::uint64_t selected_templates = 0, relation_templates = 0;
    for (const auto& template_value : templates) {
        if (!selected_template(template_value)) continue;
        ++selected_templates;
        const auto key = lower_ascii(template_value.resource);
        const auto relation = relations.find(key);
        if (relation != relations.end()) ++relation_templates;
        Json row = Json::object();
        row["template"] = template_value.resource;
        row["family"] = family_name(template_value.resource);
        row["relation_found"] = relation != relations.end();
        row["master_resources"] = relation == relations.end()
            ? Json::array() : strings(relation->second.master_resources);
        row["loaded_master_resources"] = loaded_masters[key];
        row["candidate_count"] = template_candidates[key];
        row["observed_count"] = template_observed[key];
        row["missing_count"] = template_missing[key];
        row["issues"] = strings(relation_issues[key]);
        diagnostics.push_back(std::move(row));
    }

    Json probes = Json::array();
    if (probe) {
        for (const auto& resource : probe_queue) {
            try {
                auto result = transfer_jsn_resource(
                    resource, "bi", downloaded_root, false, timeout_ms, root);
                result["status"] = "available";
                probes.push_back(std::move(result));
            } catch (const std::exception& error) {
                Json result = Json::object();
                result["resource"] = resource;
                const auto message = std::string(error.what());
                result["status"] = message.find("zero-length") != std::string::npos
                    ? "missing" : "error";
                result["message"] = message;
                probes.push_back(std::move(result));
            }
        }
    }

    Json summary = Json::object();
    summary["dynamic_template_count"] = selected_templates;
    summary["refunit_template_count"] = relation_templates;
    summary["candidate_count"] = static_cast<std::uint64_t>(candidates.size());
    summary["unique_resource_count"] = static_cast<std::uint64_t>(unique_resources.size());
    summary["derived_candidate_count"] = derived_candidates;
    summary["observed_candidate_count"] = observed_candidates;
    summary["local_missing_candidate_count"] = static_cast<std::uint64_t>(
        std::count_if(candidates.begin(), candidates.end(), [](const auto& item) {
            return !item.second.local_present;
        }));
    summary["unique_missing_resource_count"] = static_cast<std::uint64_t>(missing_resources.size());
    summary["returned_candidate_count"] = static_cast<std::uint64_t>(rows.size());
    summary["probe_queue_count"] = static_cast<std::uint64_t>(probe_queue.size());
    summary["probed_count"] = static_cast<std::uint64_t>(probes.size());

    Json queue = Json::array();
    for (const auto& resource : probe_queue) queue.push_back(resource);
    Json result = Json::object();
    result["schema"] = "tdx-jsn-candidates-native-v1";
    result["generated_at"] = timestamp_text();
    result["scan_root"] = path_utf8(downloaded_root);
    result["family"] = family.empty() ? Json(nullptr) : Json(family);
    result["missing_only"] = missing_only;
    result["network_used"] = probe;
    result["safety"] = "Candidates are generated only from CFG refunit master rows or already observed concrete files. Network probing requires an explicit family and is capped at 25; this command never downloads.";
    result["summary"] = std::move(summary);
    result["probe_queue"] = std::move(queue);
    result["probe_results"] = std::move(probes);
    result["templates"] = std::move(diagnostics);
    result["candidates"] = std::move(rows);
    return result;
}

}  // namespace tdx::jsn_variant_detail

namespace tdx {

using namespace jsn_variant_detail;

Json jsn_candidate_document(const fs::path& root,
                            const fs::path& downloaded_root,
                            const std::string& family,
                            bool missing_only,
                            int limit,
                            bool probe,
                            int max_probes,
                            int timeout_ms) {
    return candidate_document_impl(root, downloaded_root, family, missing_only,
                                   limit, probe, max_probes, timeout_ms);
}

}  // namespace tdx
