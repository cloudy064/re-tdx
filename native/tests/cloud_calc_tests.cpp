#include "tdx/cloud_calc.hpp"

#include "tdx/common.hpp"
#include "tdx/seal_order.hpp"

#include <cmath>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>
#include <tuple>
#include <vector>

namespace fs = std::filesystem;

namespace {

void require(bool value, const char* message) {
    if (!value) throw tdx::Error(message);
}

const tdx::Json& result_for(const tdx::Json& evaluation, std::string_view code) {
    for (const auto& unit : evaluation.at("units").as_array())
        for (const auto& result : unit.at("results").as_array())
            if (result.at("code").as_string() == code) return result;
    throw tdx::Error("calculation result not found: " + std::string(code));
}

double value_for(const tdx::Json& evaluation, std::string_view code) {
    const auto& result = result_for(evaluation, code);
    require(result.at("status").as_string() == "evaluated", "calculation was not evaluated");
    return result.at("value").as_number();
}

bool array_has_code(const tdx::Json& values, std::string_view code) {
    if (!values.is_array()) return false;
    for (const auto& value : values.as_array())
        if (value.is_object()) {
            const auto found = value.as_object().find("code");
            if (found != value.as_object().end() && found->second.is_string() &&
                found->second.as_string() == code) return true;
        }
    return false;
}

}  // namespace

int main() {
    try {
        const auto directory = fs::temp_directory_path() / "tdx-cloud-calc-native-test";
        fs::remove_all(directory);
        fs::create_directories(directory);
        const auto cfg = directory / "sample.cfg";
        tdx::atomic_write_text(cfg,
            "<?xml version=\"1.0\" encoding=\"gbk\"?>\n"
            "<root><unit id=\"1\" file=\"sample.jsn\">\n"
            "<item code=\"A\" calc=\"X+Y*2\" calcref=\"X,Y\"/>\n"
            "<item code=\"B\" calc=\"ABS(A-10)\" calcref=\"A\"/>\n"
            "<item code=\"MISS\" calc=\"MISSING+2\" calcref=\"MISSING\" calcflag=\"1\"/>\n"
            "<item code=\"DAY\" calc=\"$B_GetDateDiff_d$\" calcref=\"D1,D2\"/>\n"
            "<item code=\"RT\" calc=\"$B_CalcRemainTime$\" calcref=\"D1,D3,DEN\"/>\n"
            "<item code=\"AIT\" calc=\"$B_CalcAITime_All$\" "
            "calcref=\"AITYPE,ACURRENT,ASTART,DEN\"/>\n"
            "<item code=\"AI\" calc=\"$B_CalcAI_All$\" "
            "calcref=\"AITYPE,FACE,RATE,FREQ,AIT,ISSUE,ASTART,ACURRENT,AEND\"/>\n"
            "<item code=\"NOW\" calc=\"$SF_CurrDate$\" calcref=\"\"/>\n"
            "<item code=\"PV\" calc=\"$B_CalcPV_All$\" "
            "calcref=\"TYPE,FACE,RATES,FREQ,YIELD,NEXT,COUNT,REMAIN,CURRENT,TERM\"/>\n"
            "<item code=\"YTM\" calc=\"$B_CalcYTM_All$\" "
            "calcref=\"TYPE,FACE,RATES,FREQ,PV,NEXT,COUNT,REMAIN,CURRENT,TERM\"/>\n"
            "</unit></root>\n");

        const auto audit = tdx::audit_cloud_calc_configs(directory);
        const auto& summary = audit.at("summary");
        require(summary.at("cfg_files").as_number() == 1.0, "CFG count mismatch");
        require(summary.at("cfg_with_calc").as_number() == 1.0, "calculated CFG count mismatch");
        require(summary.at("calc_columns").as_number() == 10.0, "calculation count mismatch");
        require(summary.at("expressions").as_number() == 3.0, "expression count mismatch");
        require(summary.at("builtin_formulas").as_number() == 7.0, "builtin count mismatch");
        require(summary.at("invalid").as_number() == 0.0, "valid formulas were rejected");
        require(summary.at("current_config_unimplemented").as_number() == 0.0,
                "used builtin was not executable");
        require(summary.at("registered_builtin_count").as_number() == 36.0,
                "static builtin catalog count mismatch");
        require(summary.at("registered_builtin_implemented").as_number() == 36.0,
                "native builtin implementation count mismatch");

        const auto lifecycle_cfg = directory / "private-placement-lifecycle.cfg";
        tdx::atomic_write_text(lifecycle_cfg,
            "<root><unit id=\"locked\"><item code=\"price1\"/>"
            "<item code=\"RETURN\" calc=\"price2/price1-1\" "
            "calcref=\"price2,price1\"/></unit></root>");
        const auto lifecycle_audit = tdx::audit_cloud_calc_configs(lifecycle_cfg);
        require(lifecycle_audit.at("external_formula_inputs").size() == 1 &&
                    lifecycle_audit.at("external_formula_inputs").as_array()[0]
                        .at("resolver").as_string() ==
                        "upstream-lifecycle-conditional" &&
                    !lifecycle_audit.at("external_formula_inputs").as_array()[0]
                        .at("manual_input_recommended").as_bool(),
                "future private-placement unlock close was mislabeled as manual input");

        tdx::Json row = tdx::Json::object();
        row["X"] = 3.0;
        row["Y"] = 4.0;
        row["D1"] = "2026-01-01";
        row["D2"] = "20260111";
        row["D3"] = "20270101";
        row["DEN"] = 365;
        row["AITYPE"] = 5;
        row["ASTART"] = "20260101";
        row["ACURRENT"] = "20260702";
        row["AEND"] = "20270101";
        row["RATE"] = 0.03;
        row["ISSUE"] = 80.0;
        row["TYPE"] = 1;
        row["FACE"] = 100.0;
        row["RATES"] = "0.03,0.03";
        row["FREQ"] = 1;
        row["YIELD"] = 0.05;
        row["NEXT"] = 1.0;
        row["COUNT"] = 2;
        row["REMAIN"] = 2.0;
        row["CURRENT"] = 0.03;
        row["TERM"] = 2.0;
        const auto evaluation = tdx::evaluate_cloud_calc_row(cfg, row, 20260807);
        require(evaluation.at("counts").at("errors").as_number() == 0.0,
                "evaluation produced errors");
        require(evaluation.at("counts").at("unavailable").as_number() == 0.0,
                "evaluation unexpectedly lacked inputs");
        require(std::fabs(value_for(evaluation, "A") - 11.0) < 1e-12,
                "arithmetic precedence mismatch");
        require(std::fabs(value_for(evaluation, "B") - 1.0) < 1e-12,
                "ABS/dependency mismatch");
        require(std::fabs(value_for(evaluation, "MISS") - 2.0) < 1e-12,
                "calcflag=1 missing-as-zero mismatch");
        require(std::fabs(value_for(evaluation, "DAY") - 10.0) < 1e-12,
                "date difference mismatch");
        require(std::fabs(value_for(evaluation, "RT") - 1.0) < 1e-12,
                "remaining-time mismatch");
        require(std::fabs(value_for(evaluation, "AIT") - 182.0 / 365.0) < 1e-12,
                "accrual-time mismatch");
        require(std::fabs(value_for(evaluation, "AI") - 20.0 * 182.0 / 365.0) < 1e-12,
                "zero-coupon accrued-interest branch mismatch");
        require(std::fabs(value_for(evaluation, "NOW") - 20260807.0) < 1e-12,
                "deterministic current date mismatch");
        require(value_for(evaluation, "PV") > 95.0 && value_for(evaluation, "PV") < 100.0,
                "present value is outside expected range");
        require(std::fabs(value_for(evaluation, "YTM") - 0.05) < 1e-5,
                "PV/YTM round trip mismatch");

        const auto request_audit = tdx::audit_cloud_calc_request(directory, "sample");
        require(request_audit.at("summary").at("cfg_files").as_number() == 1.0 &&
                    request_audit.at("summary").at("calc_columns").as_number() == 10.0,
                "safe cloud-calc request audit mismatch");
        const auto request_template = tdx::generate_cloud_calc_template_request(
            directory, "sample");
        require(request_template.at("schema").as_string() ==
                    "tdx-tbigdata-cloud-calc-template-v1" &&
                    request_template.at("counts").at("calculated_fields").as_number() == 10.0 &&
                    request_template.at("counts").at("host_fields").as_number() == 0.0 &&
                    request_template.at("counts").at("derived_fields").as_number() == 0.0 &&
                    array_has_code(request_template.at("input_fields"), "X") &&
                    array_has_code(request_template.at("input_fields"), "Y") &&
                    request_template.at("row_template").as_object().count("X") == 1 &&
                    request_template.at("row_template").as_object().count("A") == 0,
                "cloud-calc minimal input template mismatch");
        tdx::CloudCalcEvaluationOptions request_options;
        request_options.as_of_yyyymmdd = 20260807;
        request_options.overrides["X"] = 5.0;
        const auto request_evaluation = tdx::evaluate_cloud_calc_request(
            directory, "sample", row, request_options);
        require(request_evaluation.at("row_source").as_string() == "inline-request" &&
                    request_evaluation.at("execution_mode").as_string() ==
                        "native-cpp-offline" &&
                    request_evaluation.at("override_count").as_number() == 1.0 &&
                    request_evaluation.as_object().find("cfg") ==
                        request_evaluation.as_object().end() &&
                    std::fabs(value_for(request_evaluation, "A") - 13.0) < 1e-12,
                "inline cloud-calc request did not share CLI evaluation semantics");
        tdx::Json batch_rows = tdx::Json::array();
        batch_rows.push_back(row);
        auto second_row = row;
        second_row["Y"] = 6.0;
        batch_rows.push_back(std::move(second_row));
        const auto batch = tdx::evaluate_cloud_calc_batch_request(
            directory, "sample", batch_rows, request_options);
        require(batch.at("schema").as_string() ==
                    "tdx-tbigdata-cloud-calc-batch-v1" &&
                    batch.at("counts").at("rows").as_number() == 2.0 &&
                    batch.at("counts").at("succeeded").as_number() == 2.0 &&
                    batch.at("counts").at("failed").as_number() == 0.0 &&
                    batch.at("counts").at("evaluated").as_number() == 20.0 &&
                    batch.at("fetch_plan").at("quote_document_fetches").as_number() == 0.0 &&
                    batch.at("request_body_retained").as_bool() == false,
                "offline cloud-calc batch summary mismatch");
        const auto& batch_entries = batch.at("rows").as_array();
        require(batch_entries[0].at("row_index").as_number() == 0.0 &&
                    batch_entries[1].at("row_index").as_number() == 1.0 &&
                    std::fabs(value_for(batch_entries[0].at("result"), "A") - 13.0) < 1e-12 &&
                    std::fabs(value_for(batch_entries[1].at("result"), "A") - 17.0) < 1e-12,
                "cloud-calc batch order or shared override mismatch");
        bool empty_batch_rejected = false;
        try {
            (void)tdx::evaluate_cloud_calc_batch_request(
                directory, "sample", tdx::Json::array(), request_options);
        } catch (const tdx::Error&) { empty_batch_rejected = true; }
        require(empty_batch_rejected, "cloud-calc batch must reject an empty row set");
        tdx::Json oversized_batch = tdx::Json::array();
        for (int index = 0; index < 129; ++index) oversized_batch.push_back(row);
        bool oversized_batch_rejected = false;
        try {
            (void)tdx::evaluate_cloud_calc_batch_request(
                directory, "sample", oversized_batch, request_options);
        } catch (const tdx::Error&) { oversized_batch_rejected = true; }
        require(oversized_batch_rejected,
                "cloud-calc batch must enforce the 128-row request bound");
        bool escaped_cfg_rejected = false;
        try {
            (void)tdx::audit_cloud_calc_request(directory, "../sample.cfg");
        } catch (const tdx::Error&) { escaped_cfg_rejected = true; }
        require(escaped_cfg_rejected,
                "cloud-calc request must reject directory traversal cfg names");

        const auto builtin_cfg = directory / "all-builtins.cfg";
        tdx::atomic_write_text(builtin_cfg,
            "<root><unit id=\"all\">\n"
            "<item code=\"YDY\" calc=\"$B_GetDateDiff_yd_y$\" calcref=\"D0,DH\"/>\n"
            "<item code=\"YDD\" calc=\"$B_GetDateDiff_yd_d$\" calcref=\"D0,DH\"/>\n"
            "<item code=\"Y1Y\" calc=\"$B_GetDateDiff_yd1_y$\" calcref=\"D0,DH\"/>\n"
            "<item code=\"Y1D\" calc=\"$B_GetDateDiff_yd1_d$\" calcref=\"D0,DH\"/>\n"
            "<item code=\"LEAPY\" calc=\"$B_GetDateDiff_yd1_y$\" calcref=\"LS,LE\"/>\n"
            "<item code=\"LEAPD\" calc=\"$B_GetDateDiff_yd1_d$\" calcref=\"LS,LE\"/>\n"
            "<item code=\"YT\" calc=\"$B_GetYearTime$\" calcref=\"D0,DH,DEN\"/>\n"
            "<item code=\"YT2\" calc=\"$B_GetYearTime2$\" calcref=\"D0,DH,DP\"/>\n"
            "<item code=\"BEFORE\" calc=\"$B_GetDay_Before$\" calcref=\"D0,DAYS\"/>\n"
            "<item code=\"AFTER\" calc=\"$B_GetDay_After$\" calcref=\"D0,DAYS\"/>\n"
            "<item code=\"WD\" calc=\"$B_GetDayofWeek$\" calcref=\"D0\"/>\n"
            "<item code=\"RT2\" calc=\"$B_CalcRemainTime2$\" calcref=\"D0,D3\"/>\n"
            "<item code=\"AITD\" calc=\"$B_CalcAITime$\" calcref=\"D0,DH,DEN\"/>\n"
            "<item code=\"AIT2\" calc=\"$B_CalcAITime2$\" calcref=\"DH,D0,D3\"/>\n"
            "<item code=\"NDT\" calc=\"$B_CalcNDTime$\" calcref=\"D3,D0,DH\"/>\n"
            "<item code=\"PAYC\" calc=\"$B_GetPayCount$\" calcref=\"TIME,FREQ\"/>\n"
            "<item code=\"NEXTP\" calc=\"$B_GetNextPayTime$\" calcref=\"TIME,FREQ\"/>\n"
            "<item code=\"AID\" calc=\"$B_CalcAI$\" calcref=\"FACE,RATE,FREQ,AIFRAC\"/>\n"
            "<item code=\"AICI\" calc=\"$B_CalcAI_CI_E$\" calcref=\"FACE,RATE,AIFRAC\"/>\n"
            "<item code=\"AIZ\" calc=\"$B_CalcAI_Z_E$\" calcref=\"ISSUE,D0,DH,D3\"/>\n"
            "<item code=\"PVD\" calc=\"$B_CalcPV$\" calcref=\"FACE,RATES,FREQ,YIELD,NEXT\"/>\n"
            "<item code=\"PVF\" calc=\"$B_CalcPV_Fixed$\" calcref=\"FACE,RATE,FREQ,YIELD,NEXT,COUNT\"/>\n"
            "<item code=\"PVE\" calc=\"$B_CalcPV_E$\" calcref=\"FACE,RATE,FREQ,YIELD,NEXT\"/>\n"
            "<item code=\"PVCI\" calc=\"$B_CalcPV_CI$\" calcref=\"FACE,RATE,ATIME,YIELD,REMAIN\"/>\n"
            "<item code=\"PVZ\" calc=\"$B_CalcPV_Z_E$\" calcref=\"FACE,YIELD,REMAIN\"/>\n"
            "<item code=\"CV\" calc=\"$B_CalcCV$\" calcref=\"FULL,ACCRUED\"/>\n"
            "<item code=\"YTMD\" calc=\"$B_CalcYTM$\" calcref=\"FACE,RATES,FREQ,PVD,NEXT\"/>\n"
            "<item code=\"YTMF\" calc=\"$B_CalcYTM_Fixed$\" calcref=\"FACE,RATE,FREQ,PVF,NEXT,COUNT\"/>\n"
            "<item code=\"YTME\" calc=\"$B_CalcYTM_E$\" calcref=\"FACE,RATE,FREQ,PVE,NEXT\"/>\n"
            "<item code=\"YTMCI\" calc=\"$B_CalcYTM_CI$\" calcref=\"FACE,RATE,ATIME,PVCI,REMAIN\"/>\n"
            "<item code=\"YTMZ\" calc=\"$B_CalcYTM_Z_E$\" calcref=\"FACE,PVZ,REMAIN\"/>\n"
            "</unit></root>\n");
        tdx::Json builtin_row = tdx::Json::object();
        builtin_row["D0"] = 20260101;
        builtin_row["DH"] = 20260702;
        builtin_row["DP"] = 20261231;
        builtin_row["D3"] = 20270101;
        builtin_row["LS"] = 20200229;
        builtin_row["LE"] = 20210228;
        builtin_row["DEN"] = 365;
        builtin_row["DAYS"] = 10;
        builtin_row["TIME"] = 1.25;
        builtin_row["FREQ"] = 2;
        builtin_row["FACE"] = 100.0;
        builtin_row["RATE"] = 0.06;
        builtin_row["RATES"] = "0.06,0.06";
        builtin_row["YIELD"] = 0.05;
        builtin_row["NEXT"] = 1.0;
        builtin_row["COUNT"] = 2;
        builtin_row["AIFRAC"] = 0.5;
        builtin_row["ATIME"] = 1.0;
        builtin_row["REMAIN"] = 2.0;
        builtin_row["ISSUE"] = 80.0;
        builtin_row["FULL"] = 120.0;
        builtin_row["ACCRUED"] = 2.0;
        const auto all_builtins = tdx::evaluate_cloud_calc_row(builtin_cfg, builtin_row, 20260807);
        require(all_builtins.at("counts").at("evaluated").as_number() == 31.0,
                "not every formerly catalog-only builtin evaluated");
        require(all_builtins.at("counts").at("errors").as_number() == 0.0,
                "a recovered builtin produced an error");
        require(value_for(all_builtins, "YDY") == 0.0 && value_for(all_builtins, "YDD") == 182.0,
                "year/day split mismatch");
        require(value_for(all_builtins, "LEAPY") == 0.0 &&
                value_for(all_builtins, "LEAPD") == 365.0,
                "Feb-29 anniversary split mismatch");
        require(value_for(all_builtins, "BEFORE") == 20251222.0 &&
                value_for(all_builtins, "AFTER") == 20260111.0,
                "calendar offset mismatch");
        require(value_for(all_builtins, "WD") == 4.0, "weekday mismatch");
        require(std::fabs(value_for(all_builtins, "YT") - 182.0 / 365.0) < 1e-12,
                "year-time mismatch");
        require(std::fabs(value_for(all_builtins, "YT2") - 182.0 / 364.0) < 1e-12,
                "relative year-time mismatch");
        require(value_for(all_builtins, "PAYC") == 3.0 &&
                std::fabs(value_for(all_builtins, "NEXTP") - 0.25) < 1e-12,
                "payment schedule mismatch");
        require(std::fabs(value_for(all_builtins, "AID") - 1.5) < 1e-12 &&
                std::fabs(value_for(all_builtins, "AICI") - 3.0) < 1e-12,
                "direct accrued-interest mismatch");
        require(std::fabs(value_for(all_builtins, "YTMD") - 0.05) < 1e-5 &&
                std::fabs(value_for(all_builtins, "YTMF") - 0.05) < 1e-5 &&
                std::fabs(value_for(all_builtins, "YTME") - 0.05) < 1e-12 &&
                std::fabs(value_for(all_builtins, "YTMCI") - 0.05) < 1e-12 &&
                std::fabs(value_for(all_builtins, "YTMZ") - 0.05) < 1e-12,
                "PV/YTM recovered family round trip mismatch");
        require(value_for(all_builtins, "CV") == 118.0, "clean-value mismatch");

        const auto host_cfg = directory / "host-fields.cfg";
        tdx::atomic_write_text(host_cfg,
            "<root><unit id=\"host\">\n"
            "<item code=\"$NOW3\" datatype=\"F\"/>\n"
            "<item code=\"$BONDAI\" datatype=\"F\"/>\n"
            "<item code=\"ZGXJ\" syscol=\"$NOW\" refzqdm=\"1\" datatype=\"F\"/>\n"
            "<item code=\"NOW2VAL\" syscol=\"$NOW2\" refzqdm=\"2\" datatype=\"F\"/>\n"
            "<item code=\"ZGZF\" syscol=\"$ZAF\" refzqdm=\"1\" datatype=\"F\"/>\n"
            "<item code=\"ZGSZ\" syscol=\"$CLOSE\" refzqdm=\"1\" datatype=\"F\"/>\n"
            "<item code=\"ZGKP\" syscol=\"$OPEN\" refzqdm=\"1\" datatype=\"F\"/>\n"
            "<item code=\"ZGZG\" syscol=\"$MAX\" refzqdm=\"1\" datatype=\"F\"/>\n"
            "<item code=\"ZGZD\" syscol=\"$MIN\" refzqdm=\"1\" datatype=\"F\"/>\n"
            "<item code=\"ZGXSL\" syscol=\"$NOWV\" refzqdm=\"1\" datatype=\"F\"/>\n"
            "<item code=\"ZGZDE\" syscol=\"$QRSD\" refzqdm=\"1\" datatype=\"F\"/>\n"
            "<item code=\"ZGZF2\" syscol=\"$ZEF\" refzqdm=\"1\" datatype=\"F\"/>\n"
            "<item code=\"ZGIN\" syscol=\"$INP\" refzqdm=\"1\" datatype=\"F\"/>\n"
            "<item code=\"ZGOUT\" syscol=\"$OUTP\" refzqdm=\"1\" datatype=\"F\"/>\n"
            "<item code=\"ZGBP1\" syscol=\"$BP1\" refzqdm=\"1\" datatype=\"F\"/>\n"
            "<item code=\"ZGSP1\" syscol=\"$SP1\" refzqdm=\"1\" datatype=\"F\"/>\n"
            "<item code=\"ZGBPV1\" syscol=\"$BPV1\" refzqdm=\"1\" datatype=\"F\"/>\n"
            "<item code=\"ZGSPV1\" syscol=\"$SPV1\" refzqdm=\"1\" datatype=\"F\"/>\n"
            "<item code=\"ZGJC\" syscol=\"$ZQJC\" refzqdm=\"1\" datatype=\"S\"/>\n"
            "<item code=\"TDXHY\" syscol=\"$TDXHY\" refzqdm=\"1\" datatype=\"S\"/>\n"
            "<item code=\"LTGB\" syscol=\"$J_LTGB\" refzqdm=\"1\" datatype=\"F\"/>\n"
            "<item code=\"ZGB\" syscol=\"$J_ZGB\" refzqdm=\"1\" datatype=\"F\"/>\n"
            "<item code=\"LTSZ\" syscol=\"$J_LTSZ\" refzqdm=\"1\" datatype=\"F\"/>\n"
            "<item code=\"ZSZ\" syscol=\"$J_ZSZ\" refzqdm=\"1\" datatype=\"F\"/>\n"
            "<item code=\"HSL\" syscol=\"$HSL\" refzqdm=\"1\" datatype=\"F\"/>\n"
            "<item code=\"PEVAL\" syscol=\"$PE\" refzqdm=\"1\" datatype=\"F\"/>\n"
            "<item code=\"SPEED\" syscol=\"$ZS\" refzqdm=\"1\" datatype=\"F\"/>\n"
            "<item code=\"QJ\" calc=\"$NOW3+$BONDAI\" calcref=\"$NOW3,$BONDAI\"/>\n"
            "<item code=\"ZGJZ\" calc=\"ZGXJ*MZ/ZGJ\" calcref=\"ZGXJ,MZ,ZGJ\"/>\n"
            "<item code=\"RATEP\" calc=\"DQLL2*100\" calcref=\"DQLL2\"/>\n"
            "</unit>\n"
            "<unit id=\"ref-inheritance\" refunit=\"1\">\n"
            "<item code=\"$BP1\" datatype=\"F\"/>\n"
            "</unit></root>\n");
        tdx::Json host_row = tdx::Json::object();
        host_row["$SC"] = "1";
        host_row["$ZQDM"] = "110076";
        host_row["$SC1"] = "1";
        host_row["$ZQDM1"] = "600521";
        host_row["$SC2"] = "0";
        host_row["$ZQDM2"] = "000002";
        host_row["MZ"] = 100.0;
        host_row["ZGJ"] = 16.5;
        host_row["SGFXRQ"] = "20260101";
        host_row["XGFXRQ"] = "20270101";
        host_row["SYFXLLXL"] = "0.06,0.07";
        tdx::Json snapshots = tdx::Json::object();
        snapshots["records"] = tdx::Json::array();
        tdx::Json bond_quote = tdx::Json::object();
        bond_quote["market_id"] = 1;
        bond_quote["code"] = "110076";
        bond_quote["last_price"] = 120.0;
        bond_quote["pre_close_price"] = 119.0;
        snapshots["records"].push_back(std::move(bond_quote));
        tdx::Json fallback_quote = tdx::Json::object();
        fallback_quote["market_id"] = 0;
        fallback_quote["code"] = "000002";
        fallback_quote["last_price"] = 0.0;
        fallback_quote["pre_close_price"] = 5.0;
        snapshots["records"].push_back(std::move(fallback_quote));
        tdx::Json stock_quote = tdx::Json::object();
        stock_quote["market_id"] = 1;
        stock_quote["code"] = "600521";
        stock_quote["name"] = "underlying";
        stock_quote["last_price"] = 7.0;
        stock_quote["pre_close_price"] = 6.5;
        stock_quote["open_price"] = 6.6;
        stock_quote["high_price"] = 7.2;
        stock_quote["low_price"] = 6.4;
        stock_quote["change_pct"] = 7.6923076923;
        stock_quote["rise_speed_pct"] = 1.23;
        stock_quote["total_hand"] = 300000.0;
        stock_quote["current_hand"] = 321.0;
        stock_quote["inside_dish"] = 12345.0;
        stock_quote["outer_disc"] = 23456.0;
        stock_quote["buy_levels"] = tdx::Json::array();
        tdx::Json bid1 = tdx::Json::object();
        bid1["price"] = 6.99;
        bid1["volume_hand"] = 456.0;
        stock_quote["buy_levels"].push_back(std::move(bid1));
        stock_quote["sell_levels"] = tdx::Json::array();
        tdx::Json ask1 = tdx::Json::object();
        ask1["price"] = 7.01;
        ask1["volume_hand"] = 654.0;
        stock_quote["sell_levels"].push_back(std::move(ask1));
        snapshots["records"].push_back(std::move(stock_quote));
        tdx::Json finance = tdx::Json::object();
        finance["records"] = tdx::Json::array();
        tdx::Json finance_record = tdx::Json::object();
        finance_record["market_id"] = 1;
        finance_record["code"] = "600521";
        finance_record["shares"] = tdx::Json::object();
        finance_record["shares"]["circulating"] = 1500000000.0;
        finance_record["shares"]["total"] = 2000000000.0;
        finance_record["shares"]["b_share"] = 0.0;
        finance_record["shares"]["h_share"] = 0.0;
        finance_record["income_statement"] = tdx::Json::object();
        finance_record["income_statement"]["net_profit_yuan"] = 200000000.0;
        finance_record["reserved_2"] = 3.0;
        finance["records"].push_back(std::move(finance_record));
        tdx::BlockData block_data;
        block_data.blocks.push_back(tdx::Block{"industry:abc", "industry", "通达信行业",
            "880001", "synthetic-industry", "abc", "", 1, true, std::nullopt, 1,
            "", "", "synthetic"});
        block_data.members.push_back(tdx::BlockMember{"industry:abc", "industry",
            "通达信行业", "880001", "synthetic-industry", "SH600521", 1, "sh",
            "600521", "underlying", "direct", true});
        const auto host = tdx::resolve_cloud_calc_host_fields(
            host_cfg, host_row, snapshots, finance, &block_data, 20260808);
        require(host.at("bindings").size() == 29, "host field binding count mismatch");
        require(host.at("unresolved").size() == 0, "supported host fields were left unresolved");
        require(host.at("requested_depth_securities").size() == 1 &&
                host.at("requested_depth_securities").as_array()[0].as_string() == "sh:600521",
                "depth host field request mismatch");
        require(host.at("requested_speed_securities").size() == 1 &&
                host.at("requested_speed_securities").as_array()[0].as_string() == "sh:600521",
                "rise-speed host field request mismatch");
        const auto& enriched = host.at("row");
        require(std::fabs(enriched.at("$NOW3").as_number() - 120.0) < 1e-12,
                "bond last-price binding mismatch");
        require(std::fabs(enriched.at("NOW2VAL").as_number() - 5.0) < 1e-12,
                "NOW2 pre-close fallback mismatch");
        auto zero_bond_snapshots = snapshots;
        zero_bond_snapshots["records"].as_array()[0]["last_price"] = 0.0;
        const auto zero_bond_host = tdx::resolve_cloud_calc_host_fields(
            host_cfg, host_row, zero_bond_snapshots, finance, &block_data, 20260808);
        bool now3_unresolved = false;
        for (const auto& item : zero_bond_host.at("unresolved").as_array())
            if (item.at("code").as_string() == "$NOW3") now3_unresolved = true;
        require(now3_unresolved, "$NOW3 must not inherit the $NOW2 pre-close fallback");
        require(std::fabs(enriched.at("$BONDAI").as_number() - 3.6) < 1e-12,
                "Actual/365 host accrued interest mismatch");
        require(std::fabs(enriched.at("ZGXJ").as_number() - 7.0) < 1e-12,
                "referenced security price mismatch");
        require(std::fabs(enriched.at("PEVAL").as_number() - 17.5) < 1e-6,
                "DYNAINFO(39) dynamic PE binding mismatch");
        require(std::fabs(enriched.at("ZGSZ").as_number() - 6.5) < 1e-12 &&
                std::fabs(enriched.at("ZGKP").as_number() - 6.6) < 1e-12 &&
                std::fabs(enriched.at("ZGZG").as_number() - 7.2) < 1e-12 &&
                std::fabs(enriched.at("ZGZD").as_number() - 6.4) < 1e-12,
                "OHLC host binding mismatch");
        require(std::fabs(enriched.at("ZGXSL").as_number() - 321.0) < 1e-12 &&
                std::fabs(enriched.at("ZGZDE").as_number() - 0.5) < 1e-12 &&
                std::fabs(enriched.at("ZGZF2").as_number() - 12.3076923076923) < 1e-10,
                "current-volume/change/amplitude host binding mismatch");
        require(std::fabs(enriched.at("SPEED").as_number() - 1.23) < 1e-12,
                "0x053E rise-speed host binding mismatch");
        require(std::fabs(enriched.at("ZGIN").as_number() - 12345.0) < 1e-12 &&
                std::fabs(enriched.at("ZGOUT").as_number() - 23456.0) < 1e-12,
                "inside/outside host binding mismatch");
        require(std::fabs(enriched.at("ZGBP1").as_number() - 6.99) < 1e-12 &&
                std::fabs(enriched.at("ZGSP1").as_number() - 7.01) < 1e-12 &&
                std::fabs(enriched.at("ZGBPV1").as_number() - 456.0) < 1e-12 &&
                std::fabs(enriched.at("ZGSPV1").as_number() - 654.0) < 1e-12 &&
                std::fabs(enriched.at("$BP1").as_number() - 6.99) < 1e-12,
                "best bid/ask host binding mismatch");
        require(std::fabs(enriched.at("DQLL2").as_number() - 0.06) < 1e-12,
                "current coupon host binding mismatch");
        require(enriched.at("TDXHY").as_string() == "synthetic-industry",
                "local industry host binding mismatch");
        require(std::fabs(enriched.at("LTGB").as_number() - 1500000000.0) < 1e-6 &&
                std::fabs(enriched.at("ZGB").as_number() - 2000000000.0) < 1e-6,
                "0x0010 share-capital host binding mismatch");
        require(std::fabs(enriched.at("LTSZ").as_number() - 10500000000.0) < 1e-6 &&
                std::fabs(enriched.at("ZSZ").as_number() - 14000000000.0) < 1e-6,
                "TBigData market-cap host binding mismatch");
        require(std::fabs(enriched.at("HSL").as_number() - 2.0) < 1e-12,
                "turnover host binding mismatch");
        auto b_share_finance = finance;
        b_share_finance["records"].as_array()[0]["shares"]["b_share"] = 100000000.0;
        const auto b_share_host = tdx::resolve_cloud_calc_host_fields(
            host_cfg, host_row, snapshots, b_share_finance, &block_data, 20260808);
        bool b_share_cap_unresolved = false;
        for (const auto& item : b_share_host.at("unresolved").as_array())
            if (item.at("code").as_string() == "ZSZ") b_share_cap_unresolved = true;
        require(b_share_cap_unresolved,
                "B-share total market cap must remain unresolved without its quote");
        const auto host_evaluation = tdx::evaluate_cloud_calc_row(
            host_cfg, enriched, 20260808);
        require(host_evaluation.at("counts").at("evaluated").as_number() == 3.0,
                "host-enriched calculations did not all evaluate");
        require(host_evaluation.at("counts").at("unavailable").as_number() == 0.0,
                "host-enriched calculations still need manual input");
        require(std::fabs(value_for(host_evaluation, "QJ") - 123.6) < 1e-12,
                "host-enriched full price mismatch");
        require(std::fabs(value_for(host_evaluation, "RATEP") - 6.0) < 1e-12,
                "host-enriched coupon calculation mismatch");

        const auto iopv_cfg = directory / "iopv-field.cfg";
        tdx::atomic_write_text(iopv_cfg,
            "<root><unit id=\"iopv\">\n"
            "<item code=\"$NOW\" datatype=\"F\"/>\n"
            "<item code=\"$JJJZ\" datatype=\"F\"/>\n"
            "<item code=\"PREMIUM\" calc=\"($NOW-$JJJZ)/$JJJZ*100\" "
            "calcref=\"$NOW,$JJJZ\"/>\n"
            "</unit></root>\n");
        tdx::Json iopv_row = tdx::Json::object();
        iopv_row["$SC"] = "1";
        iopv_row["$ZQDM"] = "510300";
        tdx::Json iopv_quotes = tdx::Json::object();
        iopv_quotes["records"] = tdx::Json::array();
        tdx::Json iopv_quote = tdx::Json::object();
        iopv_quote["market_id"] = 1;
        iopv_quote["code"] = "510300";
        iopv_quote["last_price"] = 4.05;
        iopv_quote["fund_iopv"] = 4.01;
        iopv_quotes["records"].push_back(std::move(iopv_quote));
        const auto iopv_host = tdx::resolve_cloud_calc_host_fields(
            iopv_cfg, iopv_row, iopv_quotes, tdx::Json::array(), nullptr, 20260808);
        require(iopv_host.at("unresolved").size() == 0 &&
                    std::fabs(iopv_host.at("row").at("$JJJZ").as_number() - 4.01) < 1e-12,
                "$JJJZ ETF IOPV host binding mismatch");
        const auto iopv_evaluation = tdx::evaluate_cloud_calc_row(
            iopv_cfg, iopv_host.at("row"), 20260808);
        require(std::fabs(value_for(iopv_evaluation, "PREMIUM") -
                          (4.05 - 4.01) / 4.01 * 100.0) < 1e-12,
                "$JJJZ ETF premium calculation mismatch");
        const auto iopv_audit = tdx::audit_cloud_calc_configs(iopv_cfg);
        require(iopv_audit.at("summary").at("auto_resolvable_host_columns").as_number() == 2.0 &&
                    iopv_audit.at("summary").at("unresolved_host_columns").as_number() == 0.0,
                "$JJJZ was not classified as auto-resolvable");

        tdx::LimitRuleConfig limit_rules;
        limit_rules.sz_st_10_date = 0;
        limit_rules.sh_st_10_date = 0;
        const auto main_limits = tdx::calculate_limit_prices(
            0, "000001", "平安银行", 10.0, 20260808, 0, limit_rules);
        require(main_limits.available && std::fabs(main_limits.upper - 11.0) < 1e-5 &&
                    std::fabs(main_limits.lower - 9.0) < 1e-5,
                "main-board native price-limit rounding mismatch");
        const auto chinext_limits = tdx::calculate_limit_prices(
            0, "300001", "特锐德", 10.03, 20260808, 0, limit_rules);
        require(std::fabs(chinext_limits.upper - 12.04) < 1e-4 &&
                    std::fabs(chinext_limits.lower - 8.02) < 1e-4,
                "ChiNext double-rounding mismatch");
        const auto st_limits = tdx::calculate_limit_prices(
            0, "000001", "*ST测试", 10.0, 20260808, 0, limit_rules);
        require(std::fabs(st_limits.upper - 10.5) < 1e-5 &&
                    std::fabs(st_limits.lower - 9.5) < 1e-5,
                "enabled ST 5% rule mismatch");
        limit_rules.sz_st_10_date = 99991231;
        const auto post_st_limits = tdx::calculate_limit_prices(
            0, "000001", "*ST测试", 10.0, 20260808, 0, limit_rules);
        require(std::fabs(post_st_limits.upper - 11.0) < 1e-5,
                "future ST transition must retain the ordinary limit");
        const auto bj_limits = tdx::calculate_limit_prices(
            2, "920001", "北交测试", 10.0, 20260808, 0, limit_rules);
        require(std::fabs(bj_limits.upper - 13.0) < 1e-5 &&
                    std::fabs(bj_limits.lower - 7.0) < 1e-5,
                "Beijing exchange biased rounding mismatch");
        const auto special_limits = tdx::calculate_limit_prices(
            0, "000001", "测试", 10.0, 20260808, 0, limit_rules,
            tdx::SpecialLimitPrice{10.77, 8.81});
        require(special_limits.source == "public-special-limit-0x0452" &&
                    std::fabs(special_limits.upper - 10.77) < 1e-4,
                "0x0452 per-security limit override mismatch");

        tdx::SealOrderInput seal_up;
        seal_up.last_price = 11.0;
        seal_up.total_hand = 10000;
        seal_up.trade_unit = 100.0;
        seal_up.buys[0] = {11.0, 1234};
        const auto up = tdx::calculate_seal_order(seal_up, main_limits);
        require(up.direction == 1 && up.mode == "continuous" &&
                    std::fabs(up.amount_yuan - 1357400.0) < 1e-8 &&
                    std::fabs(up.ratio - 0.1234) < 1e-12,
                "continuous limit-up seal projection mismatch");
        tdx::SealOrderInput seal_down;
        seal_down.last_price = 9.0;
        seal_down.total_hand = 20000;
        seal_down.trade_unit = 100.0;
        seal_down.sells[0] = {9.0, 800};
        const auto down = tdx::calculate_seal_order(seal_down, main_limits);
        require(down.direction == -1 && std::fabs(down.amount_yuan + 720000.0) < 1e-8 &&
                    std::fabs(down.ratio + 0.04) < 1e-12,
                "continuous limit-down sign projection mismatch");
        tdx::SealOrderInput auction;
        auction.total_hand = 5000;
        auction.trade_unit = 100.0;
        auction.auction_imbalance_hand = 250;
        auction.buys[0] = {11.0, 1000};
        auction.sells[0] = {11.0, 1000};
        const auto auction_result = tdx::calculate_seal_order(auction, main_limits);
        require(auction_result.direction == 1 && auction_result.mode == "auction-imbalance" &&
                    auction_result.queue_hand == 250 &&
                    std::fabs(auction_result.ratio - 0.05) < 1e-12,
                "auction imbalance seal projection mismatch");
        seal_up.quote_flags = 0x1c;
        const auto rejected = tdx::calculate_seal_order(seal_up, main_limits);
        require(rejected.available && rejected.direction == 0 && rejected.amount_yuan == 0.0,
                "TdxW rejected quote state must not report a continuous seal");

        tdx::Bytes tnf(410, 0);
        std::memcpy(tnf.data() + 50, "000001", 6);
        std::memcpy(tnf.data() + 50 + 31, "TEST", 4);
        const float lot = 100.0F;
        std::memcpy(tnf.data() + 50 + 78, &lot, sizeof(lot));
        tnf[50 + 282] = 12;
        const auto parsed_tnf = tdx::parse_tnf(tnf, 0);
        require(parsed_tnf.size() == 1 && parsed_tnf[0].trade_unit == 100.0 &&
                    parsed_tnf[0].tdx_category == 12,
                "360-byte TNF trade-unit/category offsets mismatch");

        const auto seal_cfg = directory / "seal-fields.cfg";
        tdx::atomic_write_text(seal_cfg,
            "<root><unit id=\"seal\">\n"
            "<item code=\"$FCAMO\" datatype=\"F\"/>\n"
            "<item code=\"$FCB\" datatype=\"F\"/>\n"
            "</unit></root>\n");
        tdx::Json seal_row = tdx::Json::object();
        seal_row["$SC"] = "0";
        seal_row["$ZQDM"] = "000001";
        tdx::Json seal_quotes = tdx::Json::object();
        seal_quotes["records"] = tdx::Json::array();
        tdx::Json seal_quote = tdx::Json::object();
        seal_quote["market_id"] = 0;
        seal_quote["code"] = "000001";
        seal_quote["seal_amount_yuan"] = up.amount_yuan;
        seal_quote["seal_ratio"] = up.ratio;
        seal_quotes["records"].push_back(std::move(seal_quote));
        const auto seal_host = tdx::resolve_cloud_calc_host_fields(
            seal_cfg, seal_row, seal_quotes, tdx::Json::array(), nullptr, 20260808);
        require(seal_host.at("unresolved").size() == 0 &&
                    seal_host.at("requested_depth_securities").size() == 1 &&
                    seal_host.at("requested_seal_securities").size() == 1 &&
                    std::fabs(seal_host.at("row").at("$FCAMO").as_number() - 1357400.0) < 1e-8 &&
                    std::fabs(seal_host.at("row").at("$FCB").as_number() - 0.1234) < 1e-12,
                "$FCAMO/$FCB host binding mismatch");
        const auto seal_audit = tdx::audit_cloud_calc_configs(seal_cfg);
        require(seal_audit.at("summary").at("auto_resolvable_host_columns").as_number() == 2.0 &&
                    seal_audit.at("summary").at("unresolved_host_columns").as_number() == 0.0,
                "$FCAMO/$FCB were not classified as auto-resolvable");

        const auto group_cfg = directory / "group-fields.cfg";
        tdx::atomic_write_text(group_cfg,
            "<root><unit id=\"group\">\n"
            "<item code=\"$S_AVGZF\" datatype=\"F\"/>\n"
            "<item code=\"$S_JQZF\" datatype=\"F\"/>\n"
            "<item code=\"$S_LTG\" datatype=\"S\"/>\n"
            "<item code=\"$S_MAXZF\" datatype=\"F\"/>\n"
            "<item code=\"$S_NUM\" datatype=\"I\"/>\n"
            "<item code=\"$S_UPRATE\" datatype=\"F\"/>\n"
            "</unit></root>\n");
        tdx::Json group_row = tdx::Json::object();
        // The duplicate is intentional: native TBigData uses the raw parsed
        // member vector for both the divisor and aggregation iteration.
        group_row["$S_ZQDM"] = "0 |000001,1 |600000,0 |000002,0 |000001";
        tdx::Json group_quotes = tdx::Json::object();
        group_quotes["records"] = tdx::Json::array();
        tdx::Json group_a = tdx::Json::object();
        group_a["market_id"] = 0;
        group_a["code"] = "000001";
        group_a["name"] = "Alpha";
        group_a["last_price"] = 11.0;
        group_a["pre_close_price"] = 10.0;
        group_quotes["records"].push_back(std::move(group_a));
        tdx::Json group_b = tdx::Json::object();
        group_b["market_id"] = 1;
        group_b["code"] = "600000";
        group_b["name"] = "Beta";
        group_b["last_price"] = 9.0;
        group_b["pre_close_price"] = 10.0;
        group_quotes["records"].push_back(std::move(group_b));
        tdx::Json group_finance = tdx::Json::object();
        group_finance["records"] = tdx::Json::array();
        for (const auto& item : std::vector<std::tuple<int, std::string, double>>{
                 {0, "000001", 100.0}, {1, "600000", 300.0}}) {
            tdx::Json record = tdx::Json::object();
            record["market_id"] = std::get<0>(item);
            record["code"] = std::get<1>(item);
            record["shares"] = tdx::Json::object();
            record["shares"]["total"] = std::get<2>(item);
            group_finance["records"].push_back(std::move(record));
        }
        const auto group_request = tdx::resolve_cloud_calc_host_fields(
            group_cfg, group_row, tdx::Json::array(), tdx::Json::array(), nullptr, 20260808);
        require(group_request.at("requested_securities").size() == 3 &&
                group_request.at("requested_finance_securities").size() == 3,
                "group request discovery must deduplicate network securities");
        require(group_request.at("bindings").size() == 1 &&
                group_request.at("row").at("$S_NUM").as_number() == 4.0,
                "group count must resolve without market data");
        const auto group_host = tdx::resolve_cloud_calc_host_fields(
            group_cfg, group_row, group_quotes, group_finance, nullptr, 20260808);
        require(group_host.at("bindings").size() == 6 &&
                group_host.at("unresolved").size() == 0,
                "native block aggregate fields did not all bind");
        const auto& group_enriched = group_host.at("row");
        require(std::fabs(group_enriched.at("$S_AVGZF").as_number() - 2.5) < 1e-12,
                "native average-change divisor mismatch");
        require(std::fabs(group_enriched.at("$S_JQZF").as_number() + 2.0) < 1e-12,
                "native total-share weighted change mismatch");
        require(group_enriched.at("$S_LTG").as_string() == "Alpha" &&
                std::fabs(group_enriched.at("$S_MAXZF").as_number() - 10.0) < 1e-12,
                "native leader/max-change mismatch");
        require(group_enriched.at("$S_NUM").as_number() == 4.0 &&
                std::fabs(group_enriched.at("$S_UPRATE").as_number() - 50.0) < 1e-12,
                "native member-count/up-rate mismatch");
        const auto& group_context = group_host.at("group_aggregation");
        require(group_context.at("mapped_quote_count").as_number() == 3.0 &&
                group_context.at("valid_change_count").as_number() == 3.0 &&
                group_context.at("weighted_member_count").as_number() == 3.0,
                "group aggregation coverage accounting mismatch");
        const auto group_audit = tdx::audit_cloud_calc_configs(group_cfg);
        require(group_audit.at("summary").at("auto_resolvable_host_columns").as_number() == 6.0 &&
                group_audit.at("summary").at("unresolved_host_columns").as_number() == 0.0,
                "group aggregate syscols were not classified as auto-resolvable");

        fs::remove_all(directory);
        std::cout << "cloud calc tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
