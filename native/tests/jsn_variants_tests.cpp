#include "tdx/common.hpp"
#include "tdx/jsn_variants.hpp"

#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw tdx::Error(message);
}

tdx::JsnResourceTemplate resource(
    std::string name, std::vector<std::string> sources = {"fixture.cfg"},
    std::vector<std::string> placeholders = {}) {
    return {std::move(name), std::move(sources), std::move(placeholders)};
}

const tdx::Json* file_row(const tdx::Json& document, const std::string& resource) {
    for (const auto& row : document.at("files").as_array())
        if (row.at("resource").as_string() == resource) return &row;
    return nullptr;
}

bool string_array_contains(const tdx::Json& value, const std::string& expected) {
    for (const auto& item : value.as_array())
        if (item.as_string() == expected) return true;
    return false;
}

}  // namespace

int main() {
    const auto fixture = fs::current_path() / "jsn-variants-test-fixture";
    try {
        std::error_code error;
        fs::remove_all(fixture, error);
        fs::create_directories(fixture / "tdx" / "T0002" / "cloud_cfg");
        tdx::atomic_write_text(
            fixture / "tdx" / "T0002" / "cloud_cfg" / "fixture.xml",
            "<root><datasource name=\"BI\" reqformat=\"11\" body=\"func_xml101_1.jsn\"/></root>\n");
        tdx::atomic_write_text(
            fixture / "tdx" / "T0002" / "cloud_cfg" / "fixture.cfg",
            "<root><!-- <unit file=\"ignored.jsn\"/> -->"
             "<unit id=\"1\" file=\"func_cgfx101_1.jsn\"/>"
             "<unit id=\"2\" file=\"func_cfg101_1.jsn\"/>"
             "<unit id=\"3\" refunit=\"1\" file=\"cgfxmx1\"/>"
             "<unit id=\"4\" file=\"func_mgpj101_1.jsn\"/>"
             "<unit id=\"5\" refunit=\"4\" file=\"mgpj\"/>"
             "<unit id=\"6\" refunit=\"1\" file=\"zttz\"/>"
             "<unit id=\"7\" refunit=\"1\" file=\"zttz1\"/></root>\n");
        const auto inventory = tdx::inventory_jsn_resource_templates(fixture / "tdx");
        require(inventory.size() == 8, "XML, direct CFG masters and dynamic CFG inventory");
        bool xml = false, cfg = false, dynamic = false, united_states_rating = false;
        bool theme_detail = false, theme_chart = false;
        for (const auto& item : inventory) {
            xml = xml || item.resource == "list/func_xml101_1.jsn";
            cfg = cfg || item.resource == "list/func_cfg101_1.jsn";
            dynamic = dynamic || (item.resource == "cgfxmx1/$$$SC$$$$$ZQDM$$.jsn" &&
                                   item.placeholders.size() == 2);
            united_states_rating = united_states_rating ||
                (item.resource == "mgpj/$$$SC$$$$$ZQDM$$.jsn" &&
                 item.placeholders.size() == 2);
            theme_detail = theme_detail ||
                (item.resource == "zttz/$$$ZQDM$$.jsn" &&
                 item.placeholders.size() == 1);
            theme_chart = theme_chart ||
                (item.resource == "zttz1/$$$ZQDM$$.jsn" &&
                 item.placeholders.size() == 1);
        }
        bool typed_master = false;
        for (const auto& item : inventory)
            typed_master = typed_master || item.resource == "list/func_cgfx101_1.jsn";
        require(xml && cfg && dynamic && united_states_rating && theme_detail &&
                    theme_chart && typed_master,
                "inventory resource paths, master and placeholders");

        const auto rating_coverage = tdx::jsn_variant_coverage_document(
            {resource("list/func_mgpj101_1.jsn"),
             resource("mgpj/$$$SC$$$$$ZQDM$$.jsn", {"fixture.cfg"},
                      {"$$$SC$$", "$$$ZQDM$$"})},
            fixture / "empty-downloads", false, 10);
        require(rating_coverage.at("summary").at("typed_template_count").as_number() == 2 &&
                rating_coverage.at("summary").at("generic_only_template_count").as_number() == 0,
                "US rating master and dynamic detail are typed resources");

        const auto downloads = fixture / "downloads";
        fs::create_directories(downloads / "list");
        fs::create_directories(downloads / "cgfxmx1");
        fs::create_directories(downloads / "mgpj");
        const std::string payload =
            "[{\"colheader\":[\"$SC\",\"$ZQDM\"],\"data\":[[\"0\",\"000001\"]]}]\n";
        const std::string united_states_rating_payload =
            "[{\"colheader\":[\"$SC\",\"$ZQDM\"],\"data\":[[\"74\",\"ABNB\"]]}]\n";
        tdx::atomic_write_text(downloads / "list" / "func_unknown101_1.jsn", payload);
        tdx::atomic_write_text(downloads / "cgfxmx1" / "0000001.jsn", payload);
        tdx::atomic_write_text(downloads / "mgpj" / "74ABNB.jsn",
                               united_states_rating_payload);
        std::vector<tdx::JsnResourceTemplate> templates{
            resource("list/func_zdjjzczc101_1.jsn"),
            resource("list/func_unknown101_1.jsn"),
            resource("cgfxmx1/$$$SC$$$$$ZQDM$$.jsn", {"fixture.cfg"},
                     {"$$$SC$$", "$$$ZQDM$$"})};
        const auto report = tdx::jsn_variant_coverage_document(
            templates, downloads, false, 10);
        const auto& summary = report.at("summary");
        require(summary.at("resource_template_count").as_number() == 3 &&
                summary.at("typed_template_count").as_number() == 2 &&
                summary.at("generic_only_template_count").as_number() == 1,
                "typed and generic coverage counts");
        require(summary.at("downloaded_file_count").as_number() == 3 &&
                 summary.at("matched_downloaded_file_count").as_number() == 2 &&
                summary.at("generic_downloaded_template_count").as_number() == 1,
                "downloaded template coverage counts");
        require(report.at("high_value_gaps").as_array().size() == 1 &&
                report.at("high_value_gaps").as_array().front().at("resource").as_string() ==
                    "list/func_unknown101_1.jsn",
                "downloaded generic resource ranking");
        const auto gaps = tdx::jsn_variant_coverage_document(templates, downloads, true, 10);
        require(gaps.at("resources").as_array().size() == 1,
                "gaps-only filters typed resources");

        tdx::atomic_write_text(downloads / "list" / "func_cfg101_1.jsn", payload);
        const auto baseline = fixture / "baseline.json";
        const auto initial = tdx::jsn_discovery_document(
            fixture / "tdx", downloads, baseline, true, 20);
        require(!initial.at("summary").at("baseline_available").as_bool() &&
                initial.at("summary").at("unbaselined_file_count").as_number() == 4 &&
                fs::is_regular_file(baseline),
                "first discovery scan captures an explicit baseline");
        const auto* dynamic_row = file_row(initial, "cgfxmx1/0000001.jsn");
        require(dynamic_row && dynamic_row->at("template").as_string() ==
                    "cgfxmx1/$$$SC$$$$$ZQDM$$.jsn" &&
                dynamic_row->at("key_values").at("SC").as_string() == "0" &&
                dynamic_row->at("key_values").at("ZQDM").as_string() == "000001",
                 "dynamic template extracts adjacent market and security keys");
        const auto* united_states_rating_row = file_row(initial, "mgpj/74ABNB.jsn");
        require(united_states_rating_row &&
                united_states_rating_row->at("template").as_string() ==
                    "mgpj/$$$SC$$$$$ZQDM$$.jsn" &&
                united_states_rating_row->at("coverage").as_string() == "typed-command" &&
                united_states_rating_row->at("key_values").at("SC").as_string() == "74" &&
                united_states_rating_row->at("key_values").at("ZQDM").as_string() == "ABNB",
                "US market 74 rating detail is recognized and typed");

        const std::string changed_payload =
            "[{\"colheader\":[\"$SC\",\"$ZQDM\",\"event_date\",\"url\"],"
            "\"data\":[[\"0\",\"000001\",\"2026-08-07\","
            "\"https://example.test/item\"]]}]\n";
        tdx::atomic_write_text(
            downloads / "list" / "func_cfg101_1.jsn", changed_payload);
        fs::remove(downloads / "list" / "func_unknown101_1.jsn");
        tdx::atomic_write_text(downloads / "list" / "func_fresh101_1.jsn", payload);
        const auto changed = tdx::jsn_discovery_document(
            fixture / "tdx", downloads, baseline, false, 20);
        const auto& discovery_summary = changed.at("summary");
        require(discovery_summary.at("baseline_available").as_bool() &&
                discovery_summary.at("added_file_count").as_number() == 1 &&
                discovery_summary.at("changed_file_count").as_number() == 1 &&
                discovery_summary.at("removed_file_count").as_number() == 1 &&
                discovery_summary.at("unchanged_file_count").as_number() == 2,
                "discovery classifies added changed removed and unchanged resources");
        const auto* changed_row = file_row(changed, "list/func_cfg101_1.jsn");
        require(changed_row && changed_row->at("status").as_string() == "changed" &&
                string_array_contains(changed_row->at("new_columns"), "event_date") &&
                changed_row->at("column_profiles").at("event_date")
                    .at("date_like").as_number() == 1 &&
                changed_row->at("column_profiles").at("url")
                    .at("url_like").as_number() == 1,
                "changed resource exposes structural and semantic field profiles");
        const auto* fresh_row = file_row(changed, "list/func_fresh101_1.jsn");
        require(fresh_row && fresh_row->at("coverage").as_string() == "unrecognized" &&
                changed.at("priority_changes").as_array().front()
                    .at("resource").as_string() == "list/func_fresh101_1.jsn",
                "unrecognized additions receive the highest discovery priority");

        const std::string candidate_master =
            "[{\"colheader\":[\"$SC\",\"$ZQDM\"],"
            "\"data\":[[\"0\",\"000001\"],[\"0\",\"000002\"]]}]\n";
        tdx::atomic_write_text(
            downloads / "list" / "func_cgfx101_1.jsn", candidate_master);
        tdx::atomic_write_text(
            fixture / "tdx" / "T0002" / "cloud_cfg" / "unrelated101.cfg",
            "<root><unit id=\"1\" file=\"func_cgfx103_1.jsn\"/></root>\n");
        tdx::atomic_write_text(
            fixture / "tdx" / "T0002" / "cloud_cfg" / "linked101.cfg",
            "<root><unit id=\"1\" file=\"func_cgfx104_1.jsn\"/></root>\n");
        fs::create_directories(fixture / "tdx" / "T0002" / "cloud_pad");
        tdx::atomic_write_text(
            fixture / "tdx" / "T0002" / "cloud_pad" / "linked.sp",
            "[STEP0]\nCfgName=fixture\n[STEP1]\nCfgName=linked101\n");
        tdx::atomic_write_text(
            downloads / "list" / "func_cgfx103_1.jsn",
            "[{\"colheader\":[\"$SC\",\"$ZQDM\"],"
            "\"data\":[[\"0\",\"000999\"]]}]\n");
        tdx::atomic_write_text(
            downloads / "list" / "func_cgfx104_1.jsn",
            "[{\"colheader\":[\"$SC\",\"$ZQDM\"],"
            "\"data\":[[\"0\",\"000888\"]]}]\n");
        const auto candidates = tdx::jsn_candidate_document(
            fixture / "tdx", downloads, "cgfxmx1", true, 20, false, 5, 1000);
        require(candidates.at("schema").as_string() ==
                    "tdx-jsn-candidates-native-v1" &&
                candidates.at("summary").at("dynamic_template_count").as_number() == 1 &&
                candidates.at("summary").at("candidate_count").as_number() == 3 &&
                candidates.at("summary").at("unique_missing_resource_count").as_number() == 2 &&
                candidates.at("candidates").as_array().size() == 2,
                "candidate queue follows typed refunit master rows and filters local files");
        const auto& candidate = candidates.at("candidates").as_array().front();
        require(candidate.at("resource").as_string() == "cgfxmx1/0000002.jsn" &&
                candidate.at("origin").as_string() == "refunit-row" &&
                candidate.at("probe_eligible").as_bool() &&
                candidate.at("key_values").at("SC").as_string() == "0" &&
                candidate.at("key_values").at("ZQDM").as_string() == "000002",
                "candidate queue expands adjacent SC/ZQDM with traceable keys");
        require(!string_array_contains(candidate.at("source_resources"),
                                       "list/func_cgfx103_1.jsn"),
                "candidate queue scopes repeated unit ids to the same CFG page family");
        bool linked_candidate = false, unrelated_candidate = false;
        for (const auto& row : candidates.at("candidates").as_array()) {
            linked_candidate = linked_candidate ||
                row.at("resource").as_string() == "cgfxmx1/0000888.jsn";
            unrelated_candidate = unrelated_candidate ||
                row.at("resource").as_string() == "cgfxmx1/0000999.jsn";
        }
        require(linked_candidate && !unrelated_candidate,
                "candidate queue accepts cross-family refunits only when a client page links them");

        const std::string boundary_sample =
            std::string(95, 'a') + "\xe4\xb8\xad" + "tail";
        tdx::Json utf8_group = tdx::Json::object();
        tdx::Json utf8_headers = tdx::Json::array();
        utf8_headers.push_back("description");
        tdx::Json utf8_row = tdx::Json::array();
        utf8_row.push_back(boundary_sample);
        tdx::Json utf8_rows = tdx::Json::array();
        utf8_rows.push_back(std::move(utf8_row));
        utf8_group["colheader"] = std::move(utf8_headers);
        utf8_group["data"] = std::move(utf8_rows);
        tdx::Json utf8_document = tdx::Json::array();
        utf8_document.push_back(std::move(utf8_group));
        tdx::atomic_write_bytes(
            downloads / "list" / "func_utf8_boundary101_1.jsn",
            tdx::encode_gbk(utf8_document.dump(-1)));
        const auto utf8_report = tdx::jsn_discovery_document(
            fixture / "tdx", downloads, {}, false, 20);
        const auto utf8_rendered = utf8_report.dump(-1);
        require(!tdx::utf8_to_wide(utf8_rendered).empty(),
                "discovery report remains strict UTF-8 after sample truncation");
        const auto* utf8_row_result = file_row(
            utf8_report, "list/func_utf8_boundary101_1.jsn");
        require(utf8_row_result &&
                utf8_row_result->at("column_profiles").at("description")
                    .at("samples").as_array().front().as_string() ==
                    std::string(95, 'a') + "...",
                "field profile sample truncation preserves a UTF-8 code-point boundary");
        fs::remove_all(fixture, error);
        std::cout << "JSN variant tests passed\n";
        return 0;
    } catch (const std::exception& failure) {
        std::error_code error;
        fs::remove_all(fixture, error);
        std::cerr << failure.what() << '\n';
        return 1;
    }
}
