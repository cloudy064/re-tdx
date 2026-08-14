#include "tdx/cloud_variants.hpp"

#include "tdx/common.hpp"
#include "tdx/utf8.hpp"

#include <iostream>

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw tdx::Error(message);
}

}  // namespace

int main() {
    try {
        std::vector<tdx::TqlexConfig> tqlex{
            {"a.xml", "HQServ.hq_nlp_factor", "2", "200626", {},
             "[{'ReqId':'200626','Page':'0','PageSize':'100','modname':'mod_Factor.dll'}]"},
            {"b.xml", "HQServ.hq_nlp_factor", "2", "200626", {},
             "[ { 'ReqId' : '200626', 'Page' : '0', 'PageSize' : '100', 'modname' : 'mod_Factor.dll' } ]"},
            {"road.xml", "CWSearch.tzx_rcache", "2", "", {},
             "{\"action\":\"get\",\"key\":\"ly:1_zxly\"}"},
            {"unknown.xml", "HQServ.unknown", "2", "999999", {},
             "[{\"ReqId\":\"999999\",\"Mode\":\"1\"}]"}
        };
        std::vector<tdx::PbrpcConfig> pbrpc{
            {"signal.xml", "HQServ.PBRPC_DXQX", "200250", "mod_dxqx.dll", {},
             "pb_rpc_req:Head.CharSet=1;Moduledll=mod_dxqx.dll;ReqByte={\"ReqId\":\"200250\",\"XgName\":\"CXGXG\",\"modname\":\"mod_dxqx.dll\"}"}
        };
        const auto report = tdx::cloud_variant_coverage_document(tqlex, pbrpc);
        const auto& summary = report.at("summary");
        require(summary.at("template_count").as_number() == 5 &&
                    summary.at("semantic_variant_count").as_number() == 4 &&
                    summary.at("duplicate_template_count").as_number() == 1,
                "cloud variants semantic deduplication");
        require(summary.at("fixed_variant_count").as_number() == 3 &&
                    summary.at("generic_only_variant_count").as_number() == 1 &&
                    !summary.at("fully_fixed").as_bool(),
                "cloud variants fixed and generic coverage");
        bool roadshow = false, unknown = false, duplicate = false;
        for (const auto& row : report.at("variants").as_array()) {
            if (!row.at("fixed_command").is_null() &&
                row.at("fixed_command").as_string() == "market roadshows") roadshow = true;
            if (row.at("coverage").as_string() == "generic-only") unknown = true;
            if (row.at("template_occurrences").as_number() == 2) duplicate = true;
        }
        require(roadshow && unknown && duplicate, "cloud variant record evidence");
        const auto gaps = tdx::cloud_variant_coverage_document(tqlex, pbrpc, true);
        require(gaps.at("variants").as_array().size() == 1,
                "cloud variants gaps-only output");

        const std::string boundary_selector =
            std::string(255, 'a') + "\xe4\xb8\xad" + "tail";
        const auto boundary_report = tdx::cloud_variant_coverage_document(
            {tdx::TqlexConfig{
                "utf8.xml", "HQServ.unknown", "2", "999998", {},
                "{\"ReqId\":\"999998\",\"Label\":\"" +
                    boundary_selector + "\"}"}}, {});
        const auto& boundary_variant = boundary_report.at("variants").as_array().front();
        require(boundary_variant.at("selectors").at("Label").as_string() ==
                    std::string(255, 'a') + "... [262 bytes]" &&
                !tdx::utf8_to_wide(boundary_report.dump(-1)).empty(),
                "cloud selector preview is byte-bounded without splitting UTF-8");
        std::cout << "cloud variant tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
