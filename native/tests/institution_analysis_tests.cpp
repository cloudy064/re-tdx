#include "tdx/institution_analysis.hpp"

#include "tdx/common.hpp"

#include <cmath>
#include <iostream>

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw tdx::Error(message);
}

tdx::Json row(std::initializer_list<std::pair<std::string, tdx::Json>> fields) {
    tdx::Json result = tdx::Json::object();
    for (const auto& [name, value] : fields) result[name] = value;
    return result;
}

tdx::Json source(std::string resource, tdx::Json rows) {
    tdx::Json result = tdx::Json::object();
    result["resource"] = std::move(resource);
    result["size"] = 100;
    result["row_count"] = static_cast<std::uint64_t>(rows.size());
    result["endpoint"] = "fixture:7709";
    result["server"] = "fixture";
    result["rows"] = std::move(rows);
    tdx::Json transport = tdx::Json::object();
    transport["attempt_count"] = 1;
    transport["cache_status"] = "fresh";
    result["transport"] = std::move(transport);
    return result;
}

}  // namespace

int main() {
    try {
        tdx::InstitutionAnalysisQuery catalog;
        const auto catalog_document = tdx::compose_institution_analysis_document(
            catalog, tdx::Json::array());
        require(catalog_document.at("view_count").as_number() == 25,
                "institution catalog must expose seventeen CGFX plus eight special views");

        tdx::Json rows = tdx::Json::array();
        rows.push_back(row({{"$SC", "0"}, {"$ZQDM", "000001"},
                            {"bgq", "20260331"}, {"sz", "1000"},
                            {"jgsl", "3"}, {"jgsl_bd", "1"},
                            {"gs", "120"}, {"gs_bd", "20"},
                            {"zltgb", "0.12"}, {"zltgb_bd", "0.02"},
                            {"zzgbb", "0.10"}, {"zzgbb_bd", "0.01"}}));
        tdx::Json documents = tdx::Json::array();
        documents.push_back(source("list/func_cgfx101_1.jsn", std::move(rows)));
        tdx::InstitutionAnalysisQuery query;
        query.view = "all";
        query.market = "sz";
        query.code = "000001";
        query.limit = 20;
        std::map<std::pair<int, std::string>, tdx::Security> securities;
        tdx::Security security;
        security.market_id = 0;
        security.market = "SZ";
        security.market_name = "深市";
        security.code = "000001";
        security.name = "平安银行";
        securities[{0, "000001"}] = security;
        const auto result = tdx::compose_institution_analysis_document(
            query, documents, securities);
        const auto& record = result.at("sections").as_array().front()
            .at("records").as_array().front();
        require(result.at("schema").as_string() == "tdx-institution-analysis-native-v1" &&
                result.at("counts").at("matched").as_number() == 1 &&
                record.at("name").as_string() == "平安银行",
                "institution analysis security projection");
        const auto ratio = record.at("data").at("holding_shares_change_ratio").as_number();
        require(std::abs(ratio - 0.2) < 1e-12 &&
                record.at("raw").at("gs_bd").as_string() == "20",
                "institution analysis derived ratio and raw evidence");

        tdx::Json northbound_rows = tdx::Json::array();
        northbound_rows.push_back(row({{"$SC", "0"}, {"$ZQDM", "000001"},
                                      {"bgq", "20260331"}, {"zzgbb", "6.5"},
                                      {"zzgbb_bd", "0.5"}}));
        tdx::Json northbound_documents = tdx::Json::array();
        northbound_documents.push_back(source(
            "list/func_cgfx117_1.jsn", std::move(northbound_rows)));
        query.view = "northbound";
        const auto northbound = tdx::compose_institution_analysis_document(
            query, northbound_documents, securities);
        const auto& normalized = northbound.at("sections").as_array().front()
            .at("records").as_array().front().at("data");
        require(std::abs(normalized.at("total_share_ratio").as_number() - 0.065) < 1e-12 &&
                std::abs(normalized.at("total_share_ratio_change").as_number() - 0.005) < 1e-12,
                "northbound total-share ratio must follow client divide-by-100 rule");

        tdx::Json exclusive_rows = tdx::Json::array();
        exclusive_rows.push_back(row({{"$SC", "0"}, {"$ZQDM", "000001"},
                                      {"glgs", "甲基金管理有限公司"},
                                      {"bgq", "20260630"}, {"cgsl", "12.5"},
                                      {"zb", "0.25"}}));
        exclusive_rows.push_back(row({{"$SC", "0"}, {"$ZQDM", "000001"},
                                      {"glgs", "乙基金管理有限公司"},
                                      {"bgq", "20260630"}, {"cgsl", "20"},
                                      {"zb", "0.40"}}));
        tdx::Json exclusive_documents = tdx::Json::array();
        exclusive_documents.push_back(source(
            "list/func_tbgz108_1.jsn", std::move(exclusive_rows)));
        query.view = "exclusive-funds";
        const auto exclusive = tdx::compose_institution_analysis_document(
            query, exclusive_documents, securities);
        const auto& exclusive_section = exclusive.at("sections").as_array().front();
        require(exclusive_section.at("records").size() == 2 &&
                    exclusive_section.at("summary").at("unique_securities").as_number() == 1 &&
                    exclusive_section.at("summary").at("fund_management_companies").as_number() == 2,
                "exclusive-fund rows must preserve multiple fund companies per security");
        const auto& exclusive_data = exclusive_section.at("records").as_array().front().at("data");
        require(exclusive_data.at("fund_management_company").as_string() ==
                    "乙基金管理有限公司" &&
                    exclusive_data.at("holding_shares").as_number() == 200000,
                "exclusive-fund scaling and descending ratio sort failed");

        tdx::Json notable_rows = tdx::Json::array();
        notable_rows.push_back(row({{"$SC", "0"}, {"$ZQDM", "000001"},
                                    {"mc", "高毅资产(邱国鹭)"},
                                    {"bgq", "20260630"}, {"sz", "1234.5"},
                                    {"zb", "1.25"}, {"hy", "银行"}}));
        notable_rows.push_back(row({{"$SC", "0"}, {"$ZQDM", "000001"},
                                    {"mc", "重阳投资(裘国根)"},
                                    {"bgq", "20260630"}, {"sz", "2345.6"},
                                    {"zb", "2.50"}, {"hy", "银行"}}));
        tdx::Json notable_documents = tdx::Json::array();
        notable_documents.push_back(source(
            "list/func_jgcg108_1.jsn", std::move(notable_rows)));
        query.view = "notable-private-funds";
        const auto notable = tdx::compose_institution_analysis_document(
            query, notable_documents, securities);
        const auto& notable_section = notable.at("sections").as_array().front();
        const auto& notable_data = notable_section.at("records").as_array().front().at("data");
        require(notable_section.at("layout").as_string() == "notable-private-funds" &&
                    notable_section.at("summary").at("private_fund_managers").as_number() == 2 &&
                    notable_section.at("summary").at("duplicate_manager_security_rows").as_number() == 1 &&
                    notable_data.at("private_fund_manager").as_string() == "重阳投资(裘国根)" &&
                    notable_data.at("holding_market_value").as_number() == 23456000,
                "notable private-fund semantics, scaling and sort failed");

        tdx::Json national_rows = tdx::Json::array();
        national_rows.push_back(row({{"$SC", "0"}, {"$ZQDM", "000001"},
                                     {"zjcg", "1.75"}, {"hjcg", "0.25"},
                                     {"hj", "2.00"}, {"gdmc", "第五、十大股东"},
                                     {"hy", "银行"}, {"date", "20260331"}}));
        tdx::Json national_documents = tdx::Json::array();
        national_documents.push_back(source(
            "list/func_tzcg104_1.jsn", std::move(national_rows)));
        query.view = "national-team";
        const auto national = tdx::compose_institution_analysis_document(
            query, national_documents, securities);
        const auto& national_section = national.at("sections").as_array().front();
        const auto& national_data = national_section.at("records").as_array().front().at("data");
        require(national_data.at("combined_ratio_formula_matches").as_bool() &&
                    national_data.at("calculated_combined_ratio_pct").as_number() == 2.0 &&
                    national_section.at("summary").at("combined_formula_mismatches").as_number() == 0,
                "Huijin/CSF combined ratio formula failed");

        tdx::Json social_rows = tdx::Json::array();
        social_rows.push_back(row({{"$SC", "0"}, {"$ZQDM", "000001"},
                                   {"bgq", "20260630"}, {"jgsl", "4"},
                                   {"ccgs", "88800000"}, {"zzgb", "4.25"}}));
        tdx::Json social_documents = tdx::Json::array();
        social_documents.push_back(source(
            "list/func_sbltgd101_1.jsn", std::move(social_rows)));
        query.view = "social-security-summary";
        const auto social = tdx::compose_institution_analysis_document(
            query, social_documents, securities);
        const auto& social_data = social.at("sections").as_array().front()
            .at("records").as_array().front().at("data");
        require(social_data.at("holding_shares").as_number() == 88800000.0 &&
                    social_data.at("total_share_pct").as_number() == 4.25,
                "social-security float-holder summary normalization failed");

        tdx::Json development_rows = tdx::Json::array();
        development_rows.push_back(row({
            {"$SC", "0"}, {"$ZQDM", "001979"}, {"date", "20260331"},
            {"cgxx", "国开金融有限责任公司在十大股东中排第9名"},
            {"xxsm", "截止2026-03-31,国开金融持有6187.92万股"}}));
        tdx::Json development_documents = tdx::Json::array();
        development_documents.push_back(source(
            "list/func_tzcg105_1.jsn", std::move(development_rows)));
        query.view = "development-bank-holdings";
        query.market.clear();
        query.code.clear();
        const auto development = tdx::compose_institution_analysis_document(
            query, development_documents, securities);
        const auto& development_section = development.at("sections").as_array().front();
        const auto& development_data = development_section.at("records").as_array()
            .front().at("data");
        require(development_section.at("layout").as_string() ==
                    "development-bank-holdings" &&
                    development_data.at("report_date").as_string() == "20260331" &&
                    development_data.at("shareholder_position").as_string().find("第9名") !=
                        std::string::npos &&
                    development_data.at("holding_detail").as_string().find("6187.92万股") !=
                        std::string::npos,
                "development-bank holdings must retain disclosure text and date");

        tdx::Json stake_rows = tdx::Json::array();
        stake_rows.push_back(row({
            {"$SC", "0"}, {"$ZQDM", "000001"}, {"ggrq", "20260731"},
            {"qsr", "20260701"}, {"jzr", "20260730"},
            {"gd", "示例股东"}, {"price1", "10"}, {"price2", "12"},
            {"cjjj", "11.2"}, {"zcgs", "1000000"}, {"zczb", "0.75"},
            {"zchgs", "7000000"}, {"zchzb", "5.25"},
            {"sfjxzc", "是"}, {"sfwxz", "否"}}));
        tdx::Json stake_documents = tdx::Json::array();
        stake_documents.push_back(source(
            "list/func_tzcg108_1.jsn", std::move(stake_rows)));
        query.view = "stake-building";
        const auto stake = tdx::compose_institution_analysis_document(
            query, stake_documents, securities);
        const auto& stake_section = stake.at("sections").as_array().front();
        const auto& stake_data = stake_section.at("records").as_array().front().at("data");
        require(stake_section.at("layout").as_string() == "stake-building" &&
                    std::abs(stake_data.at("period_return_pct").as_number() - 20.0) <
                        1e-12 &&
                    stake_data.at("increase_shares").as_number() == 1000000.0 &&
                    stake_data.at("further_increase").as_string() == "是" &&
                    stake_section.at("summary").at("further_increase_yes").as_number() ==
                        1.0 &&
                    stake_section.at("summary").at("insurance_capital_yes").as_number() ==
                        0.0,
                "stake-building disclosure fields, formula and flags failed");

        bool rejected = false;
        try {
            tdx::InstitutionAnalysisQuery invalid;
            invalid.view = "security";
            (void)tdx::compose_institution_analysis_document(invalid, tdx::Json::array());
        } catch (const tdx::Error&) { rejected = true; }
        require(rejected, "institution security view requires market and code");

        std::cout << "institution analysis tests passed\n";
        return 0;
    } catch (const std::exception& failure) {
        std::cerr << failure.what() << '\n';
        return 1;
    }
}
