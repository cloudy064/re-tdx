#include "server_formula_internal.hpp"

#include "tdx/common.hpp"

#include <filesystem>
#include <iostream>
#include <set>

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw tdx::Error(message);
}

tdx::Json formula(std::string code, std::string name,
                  std::string origin) {
    auto row = tdx::Json::object();
    row["code"] = std::move(code);
    row["name"] = std::move(name);
    row["kind_key"] = "selection";
    row["category_name"] = "条件选股";
    row["source_text"] = "RESULT:CLOSE>OPEN;";
    row["source_text_origin"] = std::move(origin);
    return row;
}

tdx::Json library(bool include_user) {
    auto document = tdx::Json::object();
    document["schema"] = include_user
        ? "tdx-formula-installed-library-v1"
        : "tdx-formula-library-v1";
    document["source_text_count"] = include_user ? 2 : 1;
    document["source_text_missing_count"] = 0;
    document["runtime_library_origin"] = "bundled-snapshot";
    document["runtime_dll_accessed"] = false;
    document["formulas"] = tdx::Json::array();
    document["formulas"].push_back(
        formula("SYSTEM", "系统公式", "embedded"));
    if (include_user) {
        document["private_user_data"] = true;
        document["formulas"].push_back(
            formula("MYUSER", "我的公式", "user-file-decrypted"));
        document["user_library"] = tdx::Json::object();
        document["user_library"]["formula_count"] = 1;
    }
    return document;
}

tdx::server_detail::FormulaHttpState state_for(const tdx::Json& formulas) {
    static const std::filesystem::path root;
    static const std::filesystem::path jsn_root;
    static const tdx::BlockData blocks;
    return {root, blocks, formulas, jsn_root};
}

tdx::Json audit_formula(std::string code,
                        std::initializer_list<const char*> dependencies,
                        std::initializer_list<const char*> bindings,
                        bool executable_with_context = true) {
    auto row = tdx::Json::object();
    row["code"] = std::move(code);
    row["analysis"] = tdx::Json::object();
    row["analysis"]["executable_with_context"] = executable_with_context;
    row["analysis"]["external_dependencies"] = tdx::Json::array();
    row["analysis"]["context_bindings_required"] = tdx::Json::array();
    for (const auto* dependency : dependencies)
        row["analysis"]["external_dependencies"].push_back(dependency);
    for (const auto* binding : bindings)
        row["analysis"]["context_bindings_required"].push_back(binding);
    return row;
}

std::set<std::string> string_set(const tdx::Json& values) {
    std::set<std::string> result;
    for (const auto& value : values.as_array())
        result.insert(value.as_string());
    return result;
}

}  // namespace

int main() {
    try {
        const auto system = library(false);
        const auto system_result = tdx::server_detail::query_formulas(
            state_for(system), {"/api/v1/formulas", {{"kind", "all"}}});
        require(!system_result.at("user_library_enabled").as_bool() &&
                    system_result.at("user_formula_count").as_number() == 0.0 &&
                    system_result.at("origin").as_string() == "all" &&
                    system_result.at("library_origin").as_string() ==
                        "bundled-snapshot" &&
                    !system_result.at("runtime_dll_accessed").as_bool() &&
                    system_result.at("library_schema").as_string() ==
                        "tdx-formula-library-v1",
                "system-only formula API metadata");

        const auto installed = library(true);
        const auto all_result = tdx::server_detail::query_formulas(
            state_for(installed), {"/api/v1/formulas", {{"kind", "all"}}});
        require(all_result.at("origin").as_string() == "all" &&
                    all_result.at("match_count").as_number() == 2.0 &&
                    all_result.at("returned").as_number() == 2.0,
                "default formula origin preserves the combined library");
        const auto user_result = tdx::server_detail::query_formulas(
            state_for(installed),
            {"/api/v1/formulas", {{"kind", "selection"}, {"q", "myuser"}}});
        require(user_result.at("user_library_enabled").as_bool() &&
                    user_result.at("user_formula_count").as_number() == 1.0 &&
                    user_result.at("match_count").as_number() == 1.0 &&
                    user_result.at("formulas").as_array().front()
                            .at("source_text_origin").as_string() ==
                        "user-file-decrypted",
                "opt-in PriGS formula API contract");

        const auto only_user = tdx::server_detail::query_formulas(
            state_for(installed),
            {"/api/v1/formulas", {{"kind", "all"}, {"origin", "user"}}});
        require(only_user.at("origin").as_string() == "user" &&
                    only_user.at("match_count").as_number() == 1.0 &&
                    only_user.at("formulas").as_array().front()
                            .at("code").as_string() == "MYUSER",
                "PriGS-only formula API filter");

        const auto only_system = tdx::server_detail::query_formulas(
            state_for(installed),
            {"/api/v1/formulas", {{"kind", "all"}, {"origin", "system"}}});
        require(only_system.at("origin").as_string() == "system" &&
                    only_system.at("match_count").as_number() == 1.0 &&
                    only_system.at("formulas").as_array().front()
                            .at("code").as_string() == "SYSTEM",
                "system-only formula API filter");

        bool invalid_origin_rejected = false;
        try {
            (void)tdx::server_detail::query_formulas(
                state_for(installed),
                {"/api/v1/formulas", {{"origin", "private"}}});
        } catch (const tdx::Error&) {
            invalid_origin_rejected = true;
        }
        require(invalid_origin_rejected, "invalid formula origin must be rejected");

        auto limit_context_library = tdx::Json::object();
        limit_context_library["formulas"] = tdx::Json::array();
        auto limit_context_formula = formula(
            "LIMITCTX", "涨跌停上下文", "embedded");
        limit_context_formula["kind_key"] = "technical";
        limit_context_formula["source_text"] = "Z:ZTPRICE(CLOSE,0.1);";
        limit_context_formula["parameters"] = tdx::Json::array();
        limit_context_library["formulas"].push_back(
            std::move(limit_context_formula));
        const auto limit_context =
            tdx::server_detail::query_formula_context_template(
                state_for(limit_context_library),
                {"/api/v1/formulas/context-template",
                 {{"formula", "LIMITCTX"},
                  {"market", "sz"},
                  {"code", "000001"},
                  {"period", "day"},
                  {"page_size", "800"}}});
        require(limit_context.at("formula_scalar_bindings").size() == 2 &&
                    limit_context.at("series").as_object().empty() &&
                    limit_context.at("_template")
                        .at("scalar_binding_count").as_number() == 2.0 &&
                    limit_context.at("_template")
                        .at("series_binding_count").as_number() == 0.0 &&
                    limit_context.at("_template")
                        .at("stamp_count").as_number() == 0.0 &&
                    limit_context.at("_template")
                        .at("stamp_source").as_string() ==
                        "not-required-scalar-only" &&
                    limit_context.at("_template")
                        .at("kline_fetch_skipped").as_bool() &&
                    limit_context.at("_template")
                        .at("requested_code").as_string() == "000001" &&
                    limit_context.at("_template")
                        .at("bar_count").as_number() == 0.0,
                "scalar-only context-template skips K-line retrieval and per-bar placeholders");

        auto audit_library = tdx::Json::object();
        audit_library["formulas"] = tdx::Json::array();
        audit_library["formulas"].push_back(
            audit_formula("HSL", {"FINANCE"}, {"FINANCE#7"}));
        audit_library["formulas"].push_back(
            audit_formula("ZJL", {"FINANCE"}, {"FINANCE#34"}));
        audit_library["formulas"].push_back(
            audit_formula("UNSUPPORTED", {"FINANCE"}, {"FINANCE#99"}));
        audit_library["formulas"].push_back(
            audit_formula("MULTIPLIER", {"MULTIPLIER"}, {}));
        audit_library["formulas"].push_back(
            audit_formula("DISABLED", {"FINANCE"}, {"FINANCE#3"}, false));

        const auto hk_requirements =
            tdx::server_detail::formula_audit_context_requirements(
                audit_library, true, "31");
        require(string_set(hk_requirements.at("external_dependencies")) ==
                    std::set<std::string>{"FINANCE", "MULTIPLIER"} &&
                    string_set(hk_requirements.at("context_bindings_required")) ==
                    std::set<std::string>{"FINANCE#34", "FINANCE#7"},
                "HK audit must materialize every proven FINANCE selector");

        const auto futures_requirements =
            tdx::server_detail::formula_audit_context_requirements(
                audit_library, true, "47");
        require(string_set(futures_requirements.at("external_dependencies")) ==
                    std::set<std::string>{"MULTIPLIER"} &&
                    futures_requirements.at("context_bindings_required")
                        .as_array().empty(),
                "non-HK expansion audit must not materialize HK FINANCE");

        const auto domestic_requirements =
            tdx::server_detail::formula_audit_context_requirements(
                audit_library, false, "sz");
        require(string_set(domestic_requirements.at("external_dependencies")) ==
                    std::set<std::string>{"FINANCE", "MULTIPLIER"} &&
                    string_set(domestic_requirements.at("context_bindings_required")) ==
                    std::set<std::string>{"FINANCE#34", "FINANCE#7", "FINANCE#99"},
                "domestic audit context requirements must remain unchanged");

        std::cout << "server formula-library tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
