#include "tdx/tpool.hpp"

#include "tdx/common.hpp"
#include "tpool_internal.hpp"

#include <array>
#include <cstdint>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <set>
#include <string>

namespace {
void require(bool condition, const std::string& message) {
    if (!condition) throw tdx::Error(message);
}
}

int main() {
    try {
        const std::string xml = R"xml(<?xml version="1.0" encoding="utf-8"?>
<root><flows><flow startid="1" endid="2" size="2" tran="0" /></flows>
<cells><cell id="1"><func nset="0" ntjindexno="1" accode="MACD" nperiod="7"
 nfirst="0" cfirst="DIF" noperate="2" nsecond="0" csecond="" fsecond="0.5" />
<stk setcode="0" code="000001" /><stk setcode="1" code="600000" /></cell></cells></root>)xml";
        const auto result = tdx::parse_tpool_xml_document(xml, "fixture.xml");
        require(result.at("flow_count").as_number() == 1, "flow count");
        require(result.at("function_count").as_number() == 1, "function count");
        require(result.at("stock_count").as_number() == 2, "stock count");
        require(result.at("security_count").as_number() == 2, "security count");
        require(result.at("native_formula_reference_count").as_number() == 1,
                "supported formula reference");
        const auto& function = result.at("functions").as_array().front();
        require(function.at("formula_code").as_string() == "MACD", "formula code");
        require(function.at("formula_supported").as_bool(), "formula compatibility");
        require(function.at("operator").as_string() == "less-than", "operator mapping");
        require(function.at("operator_mapping_verified").as_bool(), "verified operator");
        require(function.at("execution_ready").as_bool(), "supported comparison must be executable");
        require(function.at("cell_id").as_string() == "1", "function cell ownership");
        require(result.at("flows").as_array().front().at("start_type_name").as_string() ==
                    "immediate", "flow start timing mapping");
        require(!result.at("flows").as_array().front().at("transfer_enabled").as_bool(),
                "disabled transfer mapping");
        require(result.at("flow_graph").at("dangling_edges").size() == 1,
                "missing end cell must be diagnosed");
        const auto& stock = result.at("stocks").as_array().front();
        require(stock.at("security_id").as_string() == "sz000001", "market mapping");
        require(result.at("compatibility").at("safe_to_inspect").as_bool(), "safe inspection");
        require(result.at("compatibility").at("safe_to_evaluate_individual_rules").as_bool(),
                "individual rule evaluation gate");
        require(!result.at("compatibility").at("safe_to_execute").as_bool(),
                "full flow execution must stay disabled");

        const auto snapshot_history = tdx::parse_tpool_history_xml_document(
            R"xml(<?xml version="1.0" encoding="utf-8"?>
<root><data>
<stk market="0" code="000001" indate="20260812" intime="093001"
 inprice="10.25" income="1.50" now="10.40" rise="1.46" volume="123400"
 maxrate="2.75" maxperiod="7" maxtime="20260810" maxprice="10.53"
 idaynum="2" />
<stk market="1" code="600000" indate="20260812" intime="145959"
 inprice="9.80" income="-0.10" now="9.79" rise="-0.10" volume="88000"
 maxrate="0.90" maxperiod="3" maxtime="20260811" maxprice="9.89"
 idaynum="1" />
</data></root>)xml",
            "20260812.dat");
        require(snapshot_history.at("schema").as_string() ==
                    "tdx-tpool-history-file-v1" &&
                    snapshot_history.at("kind").as_string() == "daily-snapshot",
                "snapshot history schema and kind");
        require(snapshot_history.at("record_count").as_number() == 2 &&
                    snapshot_history.at("complete_record_count").as_number() == 2 &&
                    snapshot_history.at("expected_fields").size() == 14,
                "snapshot history field contract");
        const auto& snapshot = snapshot_history.at("records").as_array().front();
        require(snapshot.at("security_id").as_string() == "sz000001" &&
                    snapshot.at("entry_date_text").as_string() == "2026-08-12" &&
                    snapshot.at("entry_time_text").as_string() == "09:30:01" &&
                    snapshot.at("maximum_date").as_number() == 20260810 &&
                    snapshot.at("maximum_date_text").as_string() == "2026-08-10" &&
                    snapshot.at("invalid_fields").size() == 0 &&
                    snapshot.at("complete").as_bool(),
                "snapshot history typed normalization");
        tdx::tpool_detail::TpoolSecurityNames history_names{
            {{0, "000001"}, "平安银行"}, {{1, "600000"}, "浦发银行"}};
        const auto snapshot_text =
            tdx::tpool_detail::render_tpool_history_native_text(
                snapshot_history, history_names);
        require(snapshot_text.kind == "daily-snapshot" &&
                    snapshot_text.row_count == 2 &&
                    snapshot_text.resolved_name_count == 2 &&
                    snapshot_text.text_utf8 ==
                        "市场|代码|名称|进入日期|进入时间|进入价|最高收益率|最高周期|最高日期|最高价格"
                        "\r\n0|000001|平安银行|20260812|09:30|10.25|2.75%|7|08/10|10.53"
                        "\r\n1|600000|浦发银行|20260812|14:59|9.80|0.90%|3|08/11|9.89",
                "snapshot native text export must match TPool format exactly");
        const auto parsed_snapshot_text =
            tdx::parse_tpool_history_text_document(
                snapshot_text.text_utf8,
                "demo_20260812_status_his.txt");
        require(parsed_snapshot_text.at("source_format").as_string() ==
                    "native-pipe-text" &&
                    parsed_snapshot_text.at("kind").as_string() ==
                    "daily-snapshot" &&
                    parsed_snapshot_text.at("record_count").as_number() == 2 &&
                    parsed_snapshot_text.at("complete_record_count").as_number() == 2,
                "native status text parsing contract");
        const auto& parsed_snapshot =
            parsed_snapshot_text.at("records").as_array().front();
        require(parsed_snapshot.at("name").as_string() == "平安银行" &&
                    parsed_snapshot.at("entry_time").as_number() == 93000 &&
                    parsed_snapshot.at("entry_time_precision").as_string() == "minute" &&
                    parsed_snapshot.at("maximum_date").is_null() &&
                    parsed_snapshot.at("maximum_date_month_day").as_string() == "08/10" &&
                    parsed_snapshot.at("maximum_date_precision").as_string() == "month-day",
                "native status text must preserve its actual time/date precision");
        const auto snapshot_roundtrip =
            tdx::tpool_detail::render_tpool_history_native_text(
                parsed_snapshot_text, {});
        require(snapshot_roundtrip.text_utf8 == snapshot_text.text_utf8 &&
                    snapshot_roundtrip.resolved_name_count == 2,
                "native status text must round-trip without external name lookup");

        const std::string entry_native_text =
            "市场|代码|名称|进入日期|进入时间|进入价"
            "\r\n2|430047|北证样本|20260811|10:05|12.34";
        const auto parsed_entry_text =
            tdx::parse_tpool_history_text_document(
                entry_native_text, "demo_in_pool_his.txt");
        require(parsed_entry_text.at("kind").as_string() == "daily-entry-log" &&
                    parsed_entry_text.at("expected_column_count").as_number() == 6 &&
                    parsed_entry_text.at("records").as_array().front()
                        .at("security_id").as_string() == "bj430047" &&
                    tdx::tpool_detail::render_tpool_history_native_text(
                        parsed_entry_text, {}).text_utf8 == entry_native_text,
                "native entry text parse and round-trip contract");

        const auto malformed_native_text =
            tdx::parse_tpool_history_text_document(
                "市场|代码|名称|进入日期|进入时间|进入价\r\n"
                "9|000001||2026-08-12|25:61|bad",
                "malformed_in_pool_his.txt");
        require(malformed_native_text.at("complete_record_count").as_number() == 0 &&
                    malformed_native_text.at("records").as_array().front()
                        .at("invalid_columns").size() == 4,
                "native text parser must diagnose malformed typed columns");

        const auto entry_history = tdx::parse_tpool_history_xml_document(
            R"xml(<root><data>
<stk market="2" code="430047" indate="20260811" intime="100500"
 inprice="12.34" />
<stk market="2" code="430047" indate="20260812" intime="101500"
 inprice="12.56" />
<stk market="0" code="000002" indate="20260812" intime="101600" />
</data></root>)xml",
            "20260812.log");
        require(entry_history.at("kind").as_string() == "daily-entry-log" &&
                    entry_history.at("record_count").as_number() == 3 &&
                    entry_history.at("duplicate_record_count").as_number() == 1 &&
                    entry_history.at("complete_record_count").as_number() == 2 &&
                    entry_history.at("expected_fields").size() == 5,
                "entry history deduplication and completeness contract");
        require(entry_history.at("records").as_array().front()
                        .at("security_id").as_string() == "bj430047" &&
                    entry_history.at("records").as_array().back()
                        .at("missing_fields").size() == 1,
                "entry history market and missing-field normalization");
        bool incomplete_text_rejected = false;
        try {
            (void)tdx::tpool_detail::render_tpool_history_native_text(
                entry_history, history_names);
        } catch (const tdx::Error&) {
            incomplete_text_rejected = true;
        }
        require(incomplete_text_rejected,
                "native text export must not fabricate missing record values");

        const auto history_root = tdx::inspect_tpool_history_root_document(
            std::filesystem::path(TDX_TEST_ROOT) /
                "tests/fixtures/tpool-history-root",
            "demo-pool", "entry-cell", "all", 20260811, 20260812, 10);
        require(history_root.at("file_count").as_number() == 2 &&
                    history_root.at("matched_file_count").as_number() == 2 &&
                    history_root.at("record_count").as_number() == 3 &&
                    !history_root.at("truncated").as_bool(),
                "history root discovery and catalog aggregation");
        require(history_root.at("files").as_array().front()
                        .at("pool").as_string() == "demo-pool" &&
                    history_root.at("files").as_array().front()
                        .at("cell").as_string() == "entry-cell" &&
                    history_root.at("files").as_array().front()
                        .at("history_date").as_number() == 20260812 &&
                    history_root.at("order").as_string() == "date-desc",
                "history root path metadata");
        const auto snapshot_only = tdx::inspect_tpool_history_root_document(
            std::filesystem::path(TDX_TEST_ROOT) /
                "tests/fixtures/tpool-history-root",
            {}, {}, "snapshot", 20260812, 20260812, 10);
        require(snapshot_only.at("file_count").as_number() == 1 &&
                    snapshot_only.at("files").as_array().front()
                        .at("kind").as_string() == "daily-snapshot",
                "history root kind and date filters");

        bool invalid_history_rejected = false;
        try {
            (void)tdx::parse_tpool_history_xml_document(
                "<root><data /></root>", "history.xml");
        } catch (const tdx::Error&) {
            invalid_history_rejected = true;
        }
        require(invalid_history_rejected,
                "history parser must reject non-.dat/.log sources");
        const auto malformed_history = tdx::parse_tpool_history_xml_document(
            R"xml(<root><data><stk market="9" code="000001"
 indate="2026-08-12" intime="256100" inprice="bad" /></data></root>)xml",
            "malformed.log");
        require(malformed_history.at("complete_record_count").as_number() == 0 &&
                    malformed_history.at("records").as_array().front()
                        .at("invalid_fields").size() == 4,
                "malformed typed history fields must not be marked complete");

        const auto inline_evaluation = tdx::evaluate_tpool_xml_document(
            R"xml(<root><cells><cell id="empty"></cell></cells></root>)xml",
            "inline-fixture.xml", 1, 20, 1000, 1);
        require(inline_evaluation.at("schema").as_string() ==
                    "tdx-tpool-native-evaluation-v1",
                "inline evaluation schema");
        require(inline_evaluation.at("source").as_string() == "inline-fixture.xml" &&
                    inline_evaluation.at("inline_source").as_bool(),
                "inline evaluation source identity");
        require(!inline_evaluation.at("request_body_retained").as_bool() &&
                    !inline_evaluation.at("tdx_state_mutated").as_bool(),
                "inline evaluation must not retain input or mutate TDX state");
        require(inline_evaluation.at("evaluated_security_count").as_number() == 0,
                "empty inline pool must evaluate without network access");

        const auto volume_rule = tdx::parse_tpool_xml_document(
            R"xml(<root><cells><cell id="volume"><func accode="VOL" cfirst="VOLUME"
 nperiod="4" noperate="1" /></cell></cells></root>)xml", "volume.xml");
        require(volume_rule.at("native_formula_reference_count").as_number() == 1 &&
                    volume_rule.at("functions").as_array().front()
                        .at("formula_supported").as_bool(),
                "new v3 formulas must be accepted by TPool inspection");

        const auto source_rule = tdx::parse_tpool_xml_document(
            R"xml(<root><cells><cell id="source"><func accode="UDL" cfirst="UDL"
 nperiod="4" noperate="1" nsecond="-1" fsecond="0" /></cell></cells></root>)xml",
            "source.xml");
        require(!source_rule.at("functions").as_array().front().at("formula_supported").as_bool(),
                "UDL must demonstrate expansion beyond the old hand-coded subset");
        tdx::Json library = tdx::Json::object(); library["formulas"] = tdx::Json::array();
        tdx::Json udl = tdx::Json::object(); udl["code"] = "UDL"; udl["kind_key"] = "technical";
        udl["source_text"] = "UDL:MA(CLOSE,N);"; udl["parameters"] = tdx::Json::array();
        tdx::Json period = tdx::Json::object(); period["name"] = "N"; period["default"] = 3;
        udl["parameters"].push_back(std::move(period)); library["formulas"].push_back(std::move(udl));
        const auto upgraded = tdx::attach_tpool_formula_library_document(source_rule, library);
        const auto& upgraded_function = upgraded.at("functions").as_array().front();
        require(upgraded_function.at("formula_supported").as_bool() &&
                    upgraded_function.at("execution_ready").as_bool(),
                "source interpreter formula must upgrade TPool compatibility");
        require(upgraded_function.at("calculation_engine").as_string() ==
                    "tdx-source-interpreter-v1", "TPool source engine identity");

        const auto builtin_inspection = tdx::parse_tpool_xml_document(
            R"xml(<root><cells><cell id="builtin">
              <func nset="3" ntjindexno="0" noperate="1" fsecond="100" />
              <func nset="4" ntjindexno="7" noperate="3" fsecond="1"
                    nbeginday="20" nendday="5" />
              </cell></cells></root>)xml", "builtin.xml");
        require(builtin_inspection.at("native_formula_reference_count").as_number() == 0 &&
                    builtin_inspection.at("native_builtin_reference_count").as_number() == 2 &&
                    builtin_inspection.at("execution_ready_function_count").as_number() == 2,
                "nset=3/4 fields must be counted independently from formulas");
        const auto& finance_field = builtin_inspection.at("functions").as_array()[0];
        const auto& quote_field = builtin_inspection.at("functions").as_array()[1];
        require(finance_field.at("rule_kind").as_string() == "latest-finance" &&
                    finance_field.at("field_name").as_string() == "总股本" &&
                    finance_field.at("field_unit").as_string() == "ten-thousand-shares" &&
                    finance_field.at("formula_supported").is_null(),
                "latest-finance field catalog annotation");
        require(quote_field.at("rule_kind").as_string() == "realtime-quote" &&
                    quote_field.at("field_name").as_string() == "涨幅" &&
                    quote_field.at("operator").as_string() == "exact-rank" &&
                    quote_field.at("history_window_mode").as_string() == "current-snapshot" &&
                    !quote_field.at("history_attributes_applied").as_bool(),
                "realtime fields must use their own operators and ignore history attributes");
        std::set<std::string> builtin_field_keys;
        for (const auto [set, count] :
             std::vector<std::pair<int, int>>{{3, 30}, {4, 12}}) {
            for (int index = 0; index < count; ++index) {
                auto rule = tdx::Json::object();
                rule["nset"] = std::to_string(set);
                rule["ntjindexno"] = std::to_string(index);
                rule["noperate"] = "0";
                const auto plan = tdx::tpool_detail::annotate_tpool_builtin_rule(rule);
                require(plan.field_recognized && plan.execution_ready &&
                            !rule.at("field_key").as_string().empty(),
                        "every recovered built-in field must be executable");
                require(builtin_field_keys.insert(
                            std::to_string(set) + ":" +
                            rule.at("field_key").as_string()).second,
                        "built-in field keys must be unique within a rule source");
            }
        }
        require(builtin_field_keys.size() == 42,
                "complete 30 finance plus 12 quote field catalog");
        auto unknown_builtin = tdx::Json::parse(
            R"json({"nset":"4","ntjindexno":"12","noperate":"0"})json");
        const auto unknown_plan =
            tdx::tpool_detail::annotate_tpool_builtin_rule(unknown_builtin);
        require(!unknown_plan.field_recognized && !unknown_plan.execution_ready,
                "out-of-range built-in fields must be rejected before market access");

        const auto synthetic_quote = tdx::Json::parse(R"json({
          "last_price":20,"pre_close_price":10,"open_price":12,
          "high_price":22,"low_price":11,"total_hand":10000,"amount":3000000
        })json");
        const auto synthetic_finance = tdx::Json::parse(R"json({
          "shares":{"total":1000000000,"circulating":500000000,
                    "b_share":10000000,"h_share":20000000},
          "per_share":{"eps":2,"net_assets":8},
          "balance_sheet":{"total_assets_yuan":2000000000,
            "current_assets_yuan":1000000000,"intangible_assets_yuan":20000000,
            "current_liabilities_yuan":400000000,"long_term_liabilities_yuan":3000000,
            "capital_reserve_yuan":250000000,"net_assets_yuan":1200000000,
            "accounts_receivable_yuan":50000000,"inventory_yuan":60000000},
          "income_statement":{"revenue_yuan":900000000,"operating_profit_yuan":100000000,
            "investment_income_yuan":10000000,"total_profit_yuan":110000000,
            "after_tax_profit_yuan":90000000,"net_profit_yuan":80000000,
            "undistributed_profit_yuan":150000000},
          "cash_flow":{"operating_yuan":70000000,"total_yuan":60000000},
          "reserved_2":6.5
        })json");
        const auto ab_market_value = tdx::tpool_detail::evaluate_tpool_builtin_rule(
            tdx::Json::parse(R"json({"nset":"3","ntjindexno":"4",
              "noperate":"1","fsecond":"1900000"})json"),
            &synthetic_quote, &synthetic_finance, 25.0, 100.0, 200);
        require(ab_market_value.evaluated && ab_market_value.matched &&
                    std::abs(ab_market_value.left - 1960000.0) < 0.001,
                "AB market value must exclude H shares and use current price");
        const auto minority_alias = tdx::tpool_detail::evaluate_tpool_builtin_rule(
            tdx::Json::parse(R"json({"nset":"3","ntjindexno":"25",
              "noperate":"0","fsecond":"300"})json"),
            &synthetic_quote, &synthetic_finance, 25.0, 100.0, 200);
        require(minority_alias.evaluated && minority_alias.matched,
                "field 25 must preserve the DLL's long-liability offset alias");
        const auto change = tdx::tpool_detail::evaluate_tpool_builtin_rule(
            tdx::Json::parse(R"json({"nset":"4","ntjindexno":"7",
              "noperate":"0","fsecond":"100"})json"),
            &synthetic_quote, &synthetic_finance, 25.0, 100.0, 200);
        const auto amplitude = tdx::tpool_detail::evaluate_tpool_builtin_rule(
            tdx::Json::parse(R"json({"nset":"4","ntjindexno":"8",
              "noperate":"0","fsecond":"100"})json"),
            &synthetic_quote, &synthetic_finance, 25.0, 100.0, 200);
        require(change.evaluated && change.matched && amplitude.evaluated &&
                    amplitude.matched,
                "change and DLL low-price-denominator amplitude formulas");
        const auto dynamic_pe = tdx::tpool_detail::evaluate_tpool_builtin_rule(
            tdx::Json::parse(R"json({"nset":"4","ntjindexno":"9",
              "noperate":"0","fsecond":"10"})json"),
            &synthetic_quote, &synthetic_finance, 25.0, 100.0, 200);
        const auto turnover = tdx::tpool_detail::evaluate_tpool_builtin_rule(
            tdx::Json::parse(R"json({"nset":"4","ntjindexno":"10",
              "noperate":"0","fsecond":"0.2"})json"),
            &synthetic_quote, &synthetic_finance, 25.0, 100.0, 200);
        const auto volume_ratio = tdx::tpool_detail::evaluate_tpool_builtin_rule(
            tdx::Json::parse(R"json({"nset":"4","ntjindexno":"11",
              "noperate":"0","fsecond":"2"})json"),
            &synthetic_quote, &synthetic_finance, 25.0, 100.0, 200);
        require(dynamic_pe.matched && turnover.matched && volume_ratio.matched,
                "PE, turnover and volume-ratio source composition");
        const auto builtin_rank = tdx::tpool_detail::evaluate_tpool_builtin_rule(
            tdx::Json::parse(R"json({"nset":"4","ntjindexno":"0",
              "noperate":"3","fsecond":"1"})json"),
            &synthetic_quote, &synthetic_finance, 25.0, 100.0, 200);
        require(builtin_rank.ranking_pending &&
                    tdx::tpool_detail::tpool_canonical_ranking_operation(4, 3) == 5,
                "built-in exact-rank must map to the canonical ranking strategy");

        const auto synthetic_calculation = tdx::Json::parse(R"json({
          "outputs":["X"],
          "points":[
            {"values":{"X":1}},
            {"values":{"X":7}},
            {"values":{"X":2}},
            {"values":{"X":3}},
            {"values":{"X":4}}
          ]
        })json");
        const auto historical_match = tdx::evaluate_tpool_rule_document(
            tdx::Json::parse(R"json({"noperate":"1","nfirst":"0","nsecond":"-1",
              "fsecond":"5","nbeginday":"3","nendday":"1","nperiodnum":"5"})json"),
            synthetic_calculation);
        require(historical_match.at("status").as_string() == "evaluated" &&
                    historical_match.at("matched").as_bool(),
                "historical window must use any-match semantics");
        require(historical_match.at("matched_offset_from_latest").as_number() == 3 &&
                    historical_match.at("requested_anchor_count").as_number() == 3,
                "historical match offset and inclusive range");

        const auto latest_only = tdx::evaluate_tpool_rule_document(
            tdx::Json::parse(R"json({"noperate":"1","nfirst":"0","nsecond":"-1",
              "fsecond":"5"})json"), synthetic_calculation);
        require(!latest_only.at("matched").as_bool() &&
                    latest_only.at("selected_offset_from_latest").as_number() == 0,
                "absent history fields must retain latest-bar behavior");

        const auto cross_calculation = tdx::Json::parse(R"json({
          "outputs":["X"],
          "points":[{"values":{"X":1}},{"values":{"X":2}},
                    {"values":{"X":3}},{"values":{"X":4}}]
        })json");
        const auto historical_cross = tdx::evaluate_tpool_rule_document(
            tdx::Json::parse(R"json({"noperate":"3","nfirst":"0","nsecond":"-1",
              "fsecond":"2.5","nbeginday":"2","nendday":"1","nperiodnum":"4"})json"),
            cross_calculation);
        require(historical_cross.at("matched").as_bool() &&
                    historical_cross.at("matched_offset_from_latest").as_number() == 1,
                "cross must be anchored at each requested historical bar");

        const auto invalid_history = tdx::parse_tpool_xml_document(
            R"xml(<root><cells><cell id="bad"><func accode="MACD" nperiod="4"
              noperate="1" nbeginday="10" nendday="0" nperiodnum="5" />
              </cell></cells></root>)xml", "invalid-history.xml");
        const auto& invalid_function = invalid_history.at("functions").as_array().front();
        require(!invalid_function.at("execution_ready").as_bool() &&
                    invalid_function.at("blocking_reason").as_string().find("nperiodnum") !=
                        std::string::npos,
                "undersized calculation lookback must be diagnosed before market access");
        const auto ranking_history = tdx::parse_tpool_xml_document(
            R"xml(<root><cells><cell id="rank"><func accode="MACD" nperiod="4"
              noperate="6" nbeginday="2" nendday="0" nperiodnum="20" />
              </cell></cells></root>)xml", "ranking-history.xml");
        const auto& ranking_function =
            ranking_history.at("functions").as_array().front();
        require(ranking_function.at("history_window_supported").as_bool() &&
                    ranking_function.at("execution_ready").as_bool(),
                "historical cross-security ranking must be executable");
        require(ranking_function.at("history_window_mode").as_string() ==
                    "per-offset-cross-security-ranking-union",
                "historical ranking inspection must expose DLL grouping semantics");

        const auto ranking_rule = tdx::evaluate_tpool_rule_document(
            tdx::Json::parse(R"json({"noperate":"6","nfirst":"0","nsecond":"-1",
              "fsecond":"1","nbeginday":"2","nendday":"0","nperiodnum":"4"})json"),
            tdx::Json::parse(R"json({
              "outputs":["X"],
              "points":[{"values":{"X":10}},{"values":{"X":20}},
                        {"values":{"X":30}},{"values":{"X":40}}]
            })json"));
        require(ranking_rule.at("status").as_string() == "ranking-pending" &&
                    ranking_rule.at("ranking_observations").size() == 3 &&
                    ranking_rule.at("evaluated_anchor_count").as_number() == 3,
                "ranking rule must retain every evaluable historical observation");
        require(ranking_rule.at("ranking_observations").as_array().front()
                        .at("offset_from_latest").as_number() == 2 &&
                    ranking_rule.at("left_value").as_number() == 40,
                "ranking observations must preserve old-to-new extraction and latest display value");

        const std::string flow_xml = R"xml(<root><flows>
<flow startid="input" endid="result" tran="1" emptyps="0" starttype="0" cxtype="2" />
</flows><cells><cell id="input"><func accode="MACD" nperiod="4" noperate="1"
 bnost="1" bnotp="1" bnotq="1" /></cell><cell id="result" type="8">
<psatt bdel="1" ndelnum="3" ndeltype="2" baimpool="1" bsound="1"
 nsoundtype="1" soundfile="sound/custom.wav" btip="1" bsavetoblock="1"
 blockfile="watch.blk" bclearblock="1" bsavehis="1" /></cell></cells></root>)xml";
        const auto flow_inspection = tdx::parse_tpool_xml_document(flow_xml, "flow.xml");
        const auto& filtered_function = flow_inspection.at("functions").as_array().front();
        require(filtered_function.at("execution_ready").as_bool(),
                "TPool exclusion fields must no longer disable native formula evaluation");
        require(filtered_function.at("exclude_st").as_bool(), "ST exclusion mapping");
        require(filtered_function.at("exclude_suspended").as_bool(), "suspension exclusion mapping");
        require(filtered_function.at("exclude_delisted").as_bool(), "delisting exclusion mapping");
        require(flow_inspection.at("action_policy_count").as_number() == 1 &&
                    flow_inspection.at("configured_action_count").as_number() == 6,
                "psatt policy and enabled actions must be counted");
        const auto& action_policy = flow_inspection.at("action_policies").as_array().front();
        require(action_policy.at("cell_id").as_string() == "result" &&
                    action_policy.at("delete").at("count").as_number() == 3 &&
                    action_policy.at("delete").at("retention_unit").as_string() ==
                        "minutes" &&
                    action_policy.at("delete").at("retention_seconds").as_number() ==
                        180 &&
                    action_policy.at("delete").at("record_size_bytes").as_number() ==
                        85 &&
                    action_policy.at("delete").at("effective").as_bool() &&
                    action_policy.at("aim_pool").at("kind").as_string() ==
                        "record-entry-log" &&
                    action_policy.at("aim_pool").at("deduplicate_by").size() == 2 &&
                    action_policy.at("tip").at("batch_record_size_bytes").as_number() ==
                        187 &&
                    action_policy.at("tip").at("payload_layout").as_array()[1]
                            .at("offset").as_number() == 51 &&
                    action_policy.at("tip").at("payload_layout").as_array()[2]
                            .at("offset").as_number() == 102 &&
                    action_policy.at("tip").at("stock_state_fields").size() == 14 &&
                    !action_policy.at("tip").at("host_callback").as_bool() &&
                    action_policy.at("sound").at("mode").as_string() == "custom" &&
                    action_policy.at("block").at("clear_enabled").as_bool() &&
                    action_policy.at("block").at("callback_argument_count")
                            .as_number() == 7 &&
                    action_policy.at("block").at("callback_arguments").size() == 7 &&
                    action_policy.at("block").at("callback_operation_id")
                            .as_number() == 88 &&
                    action_policy.at("block").at("security_record_size_bytes")
                            .as_number() == 7,
                "psatt fields must retain their cell-scoped DLL layout");
        const auto& callback_contracts =
            flow_inspection.at("host_callback_contracts");
        const auto& callbacks = callback_contracts.at("callbacks").as_array();
        require(flow_inspection.at("host_callback_count").as_number() == 3 &&
                    callback_contracts.at("callback_count").as_number() == 3 &&
                    !callback_contracts.at("callbacks_invoked").as_bool() &&
                    callbacks.size() == 3 &&
                    callbacks[0].at("logical_argument_count").as_number() == 11 &&
                    callbacks[0].at("observed_operation_ids").size() == 10 &&
                    callbacks[1].at("logical_argument_count").as_number() == 7 &&
                    callbacks[1].at("observed_operation_ids").as_array().back()
                            .as_number() == 88 &&
                    callbacks[2].at("logical_argument_count").as_number() == 4 &&
                    callbacks[2].at("observed_operation_ids").size() == 2 &&
                    callbacks[2].at("observed_operation_ids").as_array()[1]
                            .as_number() == 31,
                "TPool inspection must expose all three registered host callback contracts");
        require(!flow_inspection.at("compatibility").at("safe_to_execute").as_bool() &&
                    flow_inspection.at("compatibility").at("host_actions").as_string().find(
                        "planned-only") != std::string::npos,
                "host action execution must remain disabled");

        tdx::Json evaluation = tdx::Json::object();
        evaluation["inspection"] = flow_inspection;
        tdx::Json securities = tdx::Json::array();
        tdx::Json security = tdx::Json::object();
        security["security_id"] = "sz000001";
        security["cell_id"] = "input";
        tdx::Json rules = tdx::Json::array();
        tdx::Json matched_rule = tdx::Json::object();
        matched_rule["cell_id"] = "input";
        matched_rule["status"] = "evaluated";
        matched_rule["matched"] = true;
        rules.push_back(std::move(matched_rule));
        security["rules"] = std::move(rules);
        securities.push_back(std::move(security));
        evaluation["securities"] = std::move(securities);
        const auto projection = tdx::project_tpool_flow_document(evaluation);
        require(projection.at("evaluated").as_bool(), "acyclic flow projection");
        require(projection.at("transitions").as_array().front()
                    .at("matched_count").as_number() == 1,
                "matched input must project to enabled destination");
        const auto& projected_actions = projection.at("transitions").as_array().front()
                                            .at("planned_actions").as_array();
        require(projection.at("planned_action_count").as_number() == 4 &&
                    projected_actions.size() == 4 &&
                    projected_actions[0].at("kind").as_string() ==
                        "record-entry-log" &&
                    projected_actions[0].at("path_template").as_string() ==
                        "tpool/<pool>/<cell>/<YYYYMMDD>.log" &&
                    projected_actions[0].at("security_count").as_number() == 1 &&
                    projected_actions[2].at("kind").as_string() == "tip" &&
                    projected_actions[2].at("payload_bytes").as_number() == 187 &&
                    projected_actions[3].at("kind").as_string() == "save-block" &&
                    projected_actions[3].at("security_buffer_bytes").as_number() ==
                        7 &&
                    !projected_actions[0].at("executed").as_bool(),
                "cell-entry projection must exclude periodic history maintenance and avoid host actions");

        const auto unsupported_retention = tdx::parse_tpool_xml_document(
            R"xml(<root><cells><cell id="retention" type="8"><psatt
 bdel="1" ndelnum="4" ndeltype="9" baimpool="0" bsound="0"
 nsoundtype="0" soundfile="" btip="0" bsavetoblock="0" blockfile=""
 bclearblock="0" bsavehis="0" /></cell></cells></root>)xml",
            "unsupported-retention.xml");
        const auto& unsupported_delete = unsupported_retention.at("action_policies")
                                             .as_array().front().at("delete");
        require(!unsupported_delete.at("supported_unit").as_bool() &&
                    !unsupported_delete.at("effective").as_bool() &&
                    unsupported_delete.at("retention_seconds").is_null(),
                "unknown ndeltype must remain configured but cannot become effective");

        struct RetentionCase {
            int type_id;
            const char* unit;
            std::int64_t seconds;
        };
        constexpr std::array<RetentionCase, 4> retention_cases{{
            {0, "days", 172800},
            {1, "hours", 7200},
            {2, "minutes", 120},
            {3, "seconds", 2},
        }};
        for (const auto& expected : retention_cases) {
            tdx::Json raw = tdx::Json::object();
            raw["bdel"] = "1";
            raw["ndelnum"] = "2";
            raw["ndeltype"] = std::to_string(expected.type_id);
            const auto policy = tdx::tpool_detail::tpool_action_policy_document(
                raw, "retention-unit");
            const auto& retention = policy.at("delete");
            require(retention.at("retention_unit").as_string() == expected.unit &&
                        retention.at("retention_seconds").as_number() ==
                            expected.seconds &&
                        retention.at("effective").as_bool() &&
                        policy.at("actions").as_array().front()
                                .at("trigger_scope").as_string() ==
                            "periodic-history-maintenance",
                    "TPool retention unit registry must match sub_1001E610");
        }

        tdx::Json gated_raw = tdx::Json::object();
        gated_raw["baimpool"] = "0";
        gated_raw["bsound"] = "1";
        gated_raw["nsoundtype"] = "0";
        gated_raw["btip"] = "1";
        gated_raw["bsavetoblock"] = "0";
        gated_raw["blockfile"] = "ignored.blk";
        gated_raw["bclearblock"] = "1";
        gated_raw["bsavehis"] = "1";
        const auto gated_policy = tdx::tpool_detail::tpool_action_policy_document(
            gated_raw, "gated-actions");
        require(gated_policy.at("configured_action_count").as_number() == 3 &&
                    !gated_policy.at("sound").at("effective").as_bool() &&
                    !gated_policy.at("tip_effective").as_bool() &&
                    !gated_policy.at("history_effective").as_bool() &&
                    !gated_policy.at("block").at("effective").as_bool(),
                "baimpool must gate sound, tip and history while clear stays a save option");
        for (const auto& configured : gated_policy.at("actions").as_array()) {
            require(tdx::tpool_detail::json_text(configured, "kind") !=
                        "clear-block" &&
                        !configured.at("effective").as_bool(),
                    "disabled baimpool actions must stay inspectable but not executable");
        }
        evaluation["flow_projection"] = projection;
        const auto initial_alerts = tdx::diff_tpool_alerts_document(tdx::Json::object(), evaluation);
        require(initial_alerts.at("entered_count").as_number() == 2,
                "snapshot must contain a rule and a flow alert");
        const auto stable_alerts = tdx::diff_tpool_alerts_document(evaluation, evaluation);
        require(stable_alerts.at("entered_count").as_number() == 0 &&
                    stable_alerts.at("exited_count").as_number() == 0,
                "identical snapshots must not emit changes");

        const auto state_inspection = tdx::parse_tpool_xml_document(R"xml(<root><flows>
<flow startid="A" endid="B" tran="1" starttype="0" cxtype="0" jgtime="5" />
<flow startid="B" endid="C" tran="1" starttype="0" cxtype="2" />
<flow startid="C" endid="A" tran="1" starttype="1" starttime="2"
 starttimetype="0" cxtype="2" />
<flow startid="A" endid="D" tran="1" starttype="3" starttime="0"
 starttimetype="0" cxtype="1" cxtime="10" cxtimetype="0" />
</flows><cells><cell id="A"><stk setcode="0" code="000001" /></cell>
<cell id="B" type="8"><psatt bdel="0" ndelnum="0" ndeltype="0"
 baimpool="0" bsound="1" nsoundtype="0" soundfile="" btip="0"
 bsavetoblock="0" blockfile="" bclearblock="0" bsavehis="0" /></cell>
<cell id="C"></cell><cell id="D"></cell></cells></root>)xml",
            "state-flow.xml");
        tdx::Json state_evaluation = tdx::Json::object();
        state_evaluation["inspection"] = state_inspection;
        state_evaluation["securities"] = tdx::Json::array();
        tdx::Json state_security = tdx::Json::object();
        state_security["security_id"] = "sz000001";
        state_security["cell_id"] = "A";
        state_security["rules"] = tdx::Json::array();
        state_evaluation["securities"].push_back(std::move(state_security));
        const tdx::TpoolFlowClock initial_clock{1000, 20260803, 100000, 2};
        const auto first_state = tdx::advance_tpool_flow_state_document(
            state_evaluation, tdx::Json::object(), initial_clock);
        require(first_state.at("flow_run_event_count").as_number() == 2,
                "immediate flow chain must advance in XML order");
        require(first_state.at("flows").as_array()[0].at("run_count").as_number() == 1,
                "repeating flow first run");
        require(first_state.at("flows").as_array()[1].at("completed").as_bool(),
                "one-shot flow must complete after first run");
        require(first_state.at("flows").as_array()[2].at("status").as_string() == "waiting",
                "pool tick threshold must wait");
        require(first_state.at("flows").as_array()[3].at("completed").as_bool(),
                "missed first activation window must complete without running");
        require(first_state.at("memberships").as_array()[2].at("cell_id").as_string() == "C" &&
                    first_state.at("memberships").as_array()[2].at("count").as_number() == 1,
                "downstream cell membership must persist");
        bool found_default_sound_plan = false;
        for (const auto& event : first_state.at("events").as_array()) {
            const auto* plans = tdx::tpool_detail::optional(event, "planned_actions");
            if (!plans || !plans->is_array()) continue;
            for (const auto& plan : plans->as_array()) {
                if (tdx::tpool_detail::json_text(plan, "kind") == "sound" &&
                    tdx::tpool_detail::json_text(plan, "effective_file") ==
                        "sound\\default.wav" &&
                    tdx::tpool_detail::json_text(plan, "trigger") ==
                        "stateful-cell-entry")
                    found_default_sound_plan = true;
            }
        }
        require(first_state.at("planned_action_count").as_number() == 0 &&
                    !first_state.at("host_actions_executed").as_bool() &&
                    !found_default_sound_plan &&
                    !state_inspection.at("action_policies").as_array().front()
                         .at("sound").at("effective").as_bool(),
                "sound must remain configured but ineffective when baimpool is disabled");

        const auto second_state = tdx::advance_tpool_flow_state_document(
            state_evaluation, first_state, tdx::TpoolFlowClock{1002, 20260803, 100002, 2});
        require(second_state.at("pool_tick_seconds").as_number() == 2,
                "persisted pool tick must use elapsed wall clock");
        require(second_state.at("flows").as_array()[2].at("run_count").as_number() == 1 &&
                    second_state.at("flows").as_array()[2].at("completed").as_bool(),
                "delayed one-shot cycle edge must run at its threshold");
        const auto repeated_state = tdx::advance_tpool_flow_state_document(
            state_evaluation, second_state, tdx::TpoolFlowClock{1005, 20260803, 100005, 2});
        require(repeated_state.at("flows").as_array()[0].at("run_count").as_number() == 2,
                "repeat interval must resume from persisted state");
        state_evaluation["flow_runtime"] = repeated_state;
        const auto state_alerts = tdx::diff_tpool_alerts_document(
            tdx::Json::object(), state_evaluation);
        require(state_alerts.at("active_count").as_number() == 3,
                "flow-state memberships must participate in alert diffs");

        auto changed_evaluation = state_evaluation;
        changed_evaluation["inspection"] = tdx::parse_tpool_xml_document(
            R"xml(<root><cells><cell id="A"><stk setcode="0" code="000001" /></cell></cells></root>)xml",
            "changed-state-flow.xml");
        const auto reset_state = tdx::advance_tpool_flow_state_document(
            changed_evaluation, repeated_state,
            tdx::TpoolFlowClock{1006, 20260803, 100006, 2});
        require(reset_state.at("state_reset").as_bool() &&
                    reset_state.at("pool_tick_seconds").as_number() == 0,
                "source changes must reset incompatible persisted state");

        tdx::Json candidates = tdx::Json::array();
        for (const auto& item : std::vector<std::pair<std::string, double>>{
                 {"A", 10.0}, {"B", 20.0}, {"C", 20.0}, {"D", 5.0}}) {
            tdx::Json candidate = tdx::Json::object();
            candidate["input_index"] = static_cast<std::uint64_t>(candidates.size());
            candidate["security_id"] = item.first;
            candidate["value"] = item.second;
            candidates.push_back(std::move(candidate));
        }
        const auto exact = tdx::rank_tpool_candidates_document(candidates, 5, 1.0);
        require(exact.at("selected_count").as_number() == 1, "exact rank selection count");
        require(exact.at("rankings").as_array().front().at("security_id").as_string() == "C",
                "equal rank must prefer later input");
        require(exact.at("rankings").as_array().front().at("matched").as_bool(),
                "highest exact rank");
        const auto bottom = tdx::rank_tpool_candidates_document(candidates, 6, -2.0);
        require(bottom.at("selected_count").as_number() == 2, "bottom two selection");
        const auto tail = tdx::rank_tpool_candidates_document(candidates, 7, -2.0);
        require(tail.at("selected_count").as_number() == 3, "tail from descending rank two");

        tdx::Json observations = tdx::Json::array();
        const auto add_observation = [&observations](std::uint64_t input_index,
                                                     const char* security_id,
                                                     int offset,
                                                     double value) {
            tdx::Json observation = tdx::Json::object();
            observation["input_index"] = input_index;
            observation["security_id"] = security_id;
            observation["offset_from_latest"] = offset;
            observation["value"] = value;
            observations.push_back(std::move(observation));
        };
        add_observation(0, "A", 0, 30.0);
        add_observation(1, "B", 0, 20.0);
        add_observation(2, "C", 0, 10.0);
        add_observation(0, "A", 1, 10.0);
        add_observation(1, "B", 1, 30.0);
        add_observation(2, "C", 1, 20.0);
        const auto historical_ranking =
            tdx::tpool_detail::rank_tpool_observation_groups_document(
                observations, 6, 1.0, 1, 0);
        require(historical_ranking.at("group_count").as_number() == 2 &&
                    historical_ranking.at("selected_security_count").as_number() == 2,
                "historical ranking must union independently selected offset groups");
        const auto& ranking_groups = historical_ranking.at("groups").as_array();
        require(ranking_groups[0].at("offset_from_latest").as_number() == 0 &&
                    ranking_groups[0].at("dll_map_key").as_number() == 0 &&
                    ranking_groups[0].at("rankings").as_array().front()
                        .at("security_id").as_string() == "A",
                "newest requested offset must be the first DLL map group");
        require(ranking_groups[1].at("offset_from_latest").as_number() == 1 &&
                    ranking_groups[1].at("rankings").as_array().front()
                        .at("security_id").as_string() == "B",
                "each older offset must rank its own cross-security population");
        std::cout << "TPool inspection tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
