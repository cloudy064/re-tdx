#include "formula_engine_test_support.hpp"

namespace formula_engine_test {

void run_context_builder_tests() {
    const auto main_business_catalog =
        tdx::parse_main_business_catalog("0|000001|零售金融业务|92|7|\n"
                                         "1|600521|原料药和制剂|88|6|\n",
                                         "fixture/specgpext.txt");
    require(main_business_catalog.records.size() == 2 &&
                tdx::main_business_for(main_business_catalog, 0, "000001") == "零售金融业务" &&
                tdx::main_business_for(main_business_catalog, 1, "600521") == "原料药和制剂" &&
                tdx::main_business_for(main_business_catalog, 0, "999999").empty(),
            "TdxW specgpext market/code lookup and third-field MAINBUSINESS parsing");
    require(tdx::safety_score_for(main_business_catalog, 0, "000001") == 92.0 &&
                tdx::shine_score_for(main_business_catalog, 0, "000001") == 7.0 &&
                tdx::safety_score_for(main_business_catalog, 1, "600521") == 88.0 &&
                !tdx::shine_score_for(main_business_catalog, 0, "999999"),
            "TdxW specgpext fields four/five expose SAFESCORE and SHINESCORE");
    const auto custom_root = fs::temp_directory_path() / "tdx-formula-custom-block-test";
    fs::remove_all(custom_root);
    fs::create_directories(custom_root / "T0002" / "blocknew");
    fs::create_directories(custom_root / "T0002" / "hq_cache");
    fs::create_directories(custom_root / "T0002" / "signals" / "signals_user_73");
    tdx::atomic_write_text(custom_root / "T0002" / "hq_cache" / "specgpext.txt",
                           "0|000001|零售金融业务|92|7|\n"
                           "1|600521|原料药和制剂|88|6|\n");
    require(tdx::tdx_user_industry_mode(custom_root) == 2,
            "UseTdxL3HY defaults to research-industry mode 2");
    tdx::atomic_write_text(custom_root / "T0002" / "user.ini", "[Other]\nUseTdxL3HY=1\n");
    require(tdx::tdx_user_industry_mode(custom_root) == 1,
            "UseTdxL3HY is read from T0002/user.ini");
    tdx::Bytes custom_record(120, 0);
    const auto custom_name = tdx::encode_gbk("测试板块");
    std::copy(custom_name.begin(), custom_name.end(), custom_record.begin());
    const std::string custom_key = "custom";
    std::copy(custom_key.begin(), custom_key.end(), custom_record.begin() + 50);
    tdx::atomic_write_bytes(custom_root / "T0002" / "blocknew" / "blocknew.cfg", custom_record);
    tdx::atomic_write_text(custom_root / "T0002" / "blocknew" / "zxg.blk", "0000001\n1600521\n");
    tdx::atomic_write_text(custom_root / "T0002" / "blocknew" / "tjg.blk", "");
    tdx::atomic_write_text(custom_root / "T0002" / "blocknew" / "custom.blk", "0000001\n1600521\n");
    const auto custom_membership = tdx::custom_block_membership_for(custom_root, 0, "000001");
    require(custom_membership.count == 2 && custom_membership.directory_count == 3 &&
                custom_membership.text == "自选股 测试板块 " &&
                custom_membership.catalog_source.find("blocknew.cfg") != std::string::npos,
            "ZDBLOCK parses built-in and 120-byte custom block directories in host order");
    require(tdx::custom_block_membership_for(custom_root, 0, "999999").text == " ",
            "empty ZDBLOCK preserves TCalc's pipe-to-single-space result");
    const auto custom_counts = tdx::custom_block_member_counts(custom_root);
    require(custom_counts.entries.size() == 3 && custom_counts.entries[0].name == "自选股" &&
                custom_counts.entries[0].count == 2 && custom_counts.entries[0].built_in &&
                custom_counts.entries[1].count == 0 &&
                custom_counts.entries[2].name == "测试板块" &&
                custom_counts.entries[2].count == 2 && !custom_counts.entries[2].built_in,
            "BLOCKSETNUM custom-block catalog preserves directory order and raw member counts");
    const auto custom_members = tdx::load_custom_block_members(custom_root, "custom");
    require(custom_members.size() == 2 && custom_members[0].market_id == 0 &&
                custom_members[0].code == "000001" && custom_members[1].market_id == 1 &&
                custom_members[1].code == "600521",
            "custom block member enumeration preserves market/code rows");
    fs::create_directories(custom_root / "T0002" / "lc");
    tdx::Bytes combination_directory(2 * 320, 0);
    const auto write_combination_directory = [&](std::size_t index, std::string_view key,
                                                 std::string_view name) {
        auto *record = combination_directory.data() + index * 320;
        std::copy(key.begin(), key.end(), record + 4);
        const auto encoded_name = tdx::encode_gbk(name);
        std::copy(encoded_name.begin(), encoded_name.end(), record + 11);
    };
    write_combination_directory(0, "combo1", "组合一");
    write_combination_directory(1, "combo2", "组合二");
    tdx::atomic_write_bytes(custom_root / "T0002" / "lc" / "lcidx.lii", combination_directory);
    const auto make_combination_members =
        [](const std::vector<std::pair<int, std::string>> &members) {
            tdx::Bytes output(members.size() * 16, 0);
            for (std::size_t index = 0; index < members.size(); ++index) {
                auto *record = output.data() + index * 16;
                const auto market = static_cast<std::uint16_t>(members[index].first);
                record[0] = static_cast<std::uint8_t>(market & 0xffu);
                record[1] = static_cast<std::uint8_t>((market >> 8) & 0xffu);
                std::copy(members[index].second.begin(), members[index].second.end(), record + 2);
            }
            return output;
        };
    tdx::atomic_write_bytes(custom_root / "T0002" / "lc" / "combo1.cis",
                            make_combination_members({{0, "000001"}, {1, "600521"}}));
    tdx::atomic_write_bytes(custom_root / "T0002" / "lc" / "combo2.cis",
                            make_combination_members({{0, "000001"}}));
    const auto combination_membership =
        tdx::combination_block_membership_for(custom_root, 0, "000001");
    require(combination_membership.text == "组合一 组合二 " && combination_membership.count == 2 &&
                combination_membership.directory_count == 2 &&
                combination_membership.readable_member_file_count == 2,
            "ZHBLOCK parses lcidx.lii/cis membership in ascending catalog order");
    require(tdx::combination_block_membership_for(custom_root, 1, "600521").text == "组合一 " &&
                tdx::combination_block_membership_for(custom_root, 0, "999999").text == " ",
            "ZHBLOCK preserves trailing separators and the empty single-space sentinel");
    const auto combination_analysis =
        tdx::analyze_formula_source("Z:STRCMP(ZHBLOCK,'组合一 组合二 ');N:ZHBLOCKNUM;"
                                    "S:STRCMP(SIMIBLOCK,' ');");
    require(combination_analysis.at("executable_with_context").as_bool() &&
                combination_analysis.at("automatic_context_dependencies").size() == 3,
            "ZHBLOCK/ZHBLOCKNUM/SIMIBLOCK are automatic context dependencies");
    const auto combination_context = tdx::build_formula_market_context_document(
        custom_root, "sz", "000001", combination_analysis);
    require(combination_context.at("formula_text_symbols").at("ZHBLOCK").as_string() ==
                    "组合一 组合二 " &&
                combination_context.at("formula_text_symbols").at("SIMIBLOCK").as_string() == " " &&
                combination_context.at("symbols").at("ZHBLOCKNUM").as_number() == 2.0 &&
                combination_context.at("combination_block_directory_count").as_number() == 2.0 &&
                combination_context.at("combination_block_membership_count").as_number() == 2.0,
            "automatic formula context binds positive combination membership and exact SIMIBLOCK "
            "sentinel");
    const auto make_daily = [](const std::vector<std::array<double, 9>> &rows) {
        tdx::Bytes output;
        const auto append_u32 = [&](std::uint32_t value) {
            for (int shift = 0; shift < 32; shift += 8)
                output.push_back(static_cast<std::uint8_t>((value >> shift) & 0xffu));
        };
        const auto append_float = [&](float value) {
            std::uint32_t bits = 0;
            std::memcpy(&bits, &value, sizeof(bits));
            append_u32(bits);
        };
        for (const auto &row : rows) {
            append_u32(static_cast<std::uint32_t>(row[0]));
            for (std::size_t index = 1; index <= 4; ++index)
                append_u32(
                    static_cast<std::uint32_t>(static_cast<std::int32_t>(row[index] * 100.0)));
            append_float(static_cast<float>(row[5]));
            append_u32(static_cast<std::uint32_t>(row[6]));
            append_u32(0);
        }
        return output;
    };
    fs::create_directories(custom_root / "vipdoc" / "sz" / "lday");
    fs::create_directories(custom_root / "vipdoc" / "sh" / "lday");
    tdx::atomic_write_bytes(custom_root / "vipdoc" / "sz" / "lday" / "sz000001.day",
                            make_daily({{20240102, 4, 6, 3, 5, 50, 500, 0, 0},
                                        {20240104, 6, 8, 5, 7, 70, 700, 0, 0},
                                        {20240105, 7, 9, 6, 8, 80, 800, 0, 0}}));
    tdx::atomic_write_bytes(custom_root / "vipdoc" / "sh" / "lday" / "sh600521.day",
                            make_daily({{20240103, 19, 21, 18, 20, 200, 2000, 0, 0},
                                        {20240105, 29, 31, 28, 30, 300, 3000, 0, 0}}));
    tdx::Json horcalc_kline = tdx::Json::object();
    horcalc_kline["market"] = "sz";
    horcalc_kline["code"] = "999999";
    horcalc_kline["period"] = "day";
    horcalc_kline["bars"] = tdx::Json::array();
    for (const auto &row :
         std::vector<std::array<double, 7>>{{20240102, 9, 11, 8, 10, 100, 1000},
                                            {20240103, 14, 16, 13, 15, 150, 1500},
                                            {20240104, 17, 19, 16, 18, 180, 1800},
                                            {20240105, 24, 26, 23, 25, 250, 2500},
                                            {20240106, 27, 29, 26, 28, 280, 2800}}) {
        const int date = static_cast<int>(row[0]);
        tdx::Json bar = tdx::Json::object();
        bar["date"] = std::to_string(date).substr(0, 4) + "-" + std::to_string(date).substr(4, 2) +
                      "-" + std::to_string(date).substr(6, 2);
        bar["time"] = "15:00";
        bar["open"] = row[1];
        bar["high"] = row[2];
        bar["low"] = row[3];
        bar["close"] = row[4];
        bar["amount"] = row[5];
        bar["volume"] = row[6];
        horcalc_kline["bars"].push_back(std::move(bar));
    }
    const std::string horcalc_source = "S:HORCALC('MY.测试板块',103,0,2);"
                                       "R:HORCALC('MY.测试板块',103,1,2);"
                                       "A:HORCALC('MY.测试板块',103,2,2);"
                                       "P:HORCALC('MY.测试板块',105,0,2);";
    const auto horcalc_analysis_fixture = tdx::analyze_formula_source(horcalc_source);
    const tdx::BlockData empty_blocks;
    const auto horcalc_market_context = tdx::build_formula_market_context_document(
        custom_root, "sz", "999999", horcalc_analysis_fixture, 1000, &empty_blocks, &horcalc_kline,
        false);
    const auto horcalc_native_fixture = tdx::evaluate_formula_source_document(
        horcalc_kline, horcalc_source, {}, "HORCALC_FIXTURE", &horcalc_market_context);
    const std::array<double, 5> expected_sum{5, 25, 27, 38, 38};
    const std::array<double, 5> expected_rank{1, 2, 2, 2, 2};
    const std::array<double, 5> expected_average{5, 12.5, 13.5, 19, 19};
    const std::array<double, 5> expected_change{0, 0, 0.400000006, 0.642857134, 0.642857134};
    for (std::size_t index = 0; index < 5; ++index) {
        require(std::abs(point_value(horcalc_native_fixture, index, "S").as_number() -
                         expected_sum[index]) < 1e-5 &&
                    std::abs(point_value(horcalc_native_fixture, index, "R").as_number() -
                             expected_rank[index]) < 1e-5 &&
                    std::abs(point_value(horcalc_native_fixture, index, "A").as_number() -
                             expected_average[index]) < 1e-5 &&
                    std::abs(point_value(horcalc_native_fixture, index, "P").as_number() -
                             expected_change[index]) < 1e-5,
                "HORCALC local-day aggregation matches native probe");
    }
    require(
        horcalc_native_fixture.at("context_metadata").at("horcalc_member_file_count").as_number() ==
                2.0 &&
            horcalc_native_fixture.at("context_metadata")
                    .at("horcalc_member_series_count")
                    .as_number() == 2.0,
        "HORCALC reports exact local member coverage");

    auto aggregate_indicator =
        tdx::make_formula_source_definition("X:CLOSE;", "PROBEFORM", "technical");
    aggregate_indicator["outputs"] = tdx::Json::array();
    aggregate_indicator["outputs"].push_back("X");
    tdx::Json aggregate_library = tdx::Json::object();
    aggregate_library["formulas"] = tdx::Json::array();
    aggregate_library["formulas"].push_back(std::move(aggregate_indicator));
    const std::string indicator_aggregate_source = "RD:INSORT('MY.测试板块','PROBEFORM',1,0);"
                                                   "RA:INSORT('MY.测试板块','PROBEFORM',1,1);"
                                                   "S:INSUM('MY.测试板块','PROBEFORM',1,0);"
                                                   "A:INSUM('MY.测试板块','PROBEFORM',1,1);"
                                                   "MX:INSUM('MY.测试板块','PROBEFORM',1,2);"
                                                   "MN:INSUM('MY.测试板块','PROBEFORM',1,3);"
                                                   "IX:INSUM('MY.测试板块','PROBEFORM',1,4);"
                                                   "IN:INSUM('MY.测试板块','PROBEFORM',1,5);";
    const auto indicator_aggregate_analysis =
        tdx::analyze_formula_source(indicator_aggregate_source);
    const auto indicator_aggregate_context = tdx::build_formula_market_context_document(
        custom_root, "sz", "999999", indicator_aggregate_analysis, 1000, &empty_blocks,
        &horcalc_kline, false, &aggregate_library);
    const auto indicator_aggregate_fixture = tdx::evaluate_formula_source_document(
        horcalc_kline, indicator_aggregate_source, {}, "INDICATOR_AGGREGATE_FIXTURE",
        &indicator_aggregate_context);
    const std::array<double, 5> expected_descending_rank{1, 2, 2, 2, 2};
    const std::array<double, 5> expected_ascending_rank{2, 2, 2, 2, 2};
    const std::array<double, 4> expected_insum{5, 20, 7, 38};
    const std::array<double, 4> expected_insum_average{2.5, 10, 3.5, 19};
    const std::array<double, 4> expected_insum_maximum{5, 20, 7, 30};
    const std::array<double, 4> expected_insum_minimum{5, 20, 7, 8};
    const std::array<double, 4> expected_insum_maximum_index{1, 2, 1, 2};
    const std::array<double, 4> expected_insum_minimum_index{1, 2, 1, 1};
    for (std::size_t index = 0; index < 5; ++index) {
        require(point_value(indicator_aggregate_fixture, index, "RD").as_number() ==
                        expected_descending_rank[index] &&
                    point_value(indicator_aggregate_fixture, index, "RA").as_number() ==
                        expected_ascending_rank[index],
                "INSORT matches native rank direction and missing-date carry semantics");
    }
    for (std::size_t index = 0; index < 4; ++index) {
        require(std::abs(point_value(indicator_aggregate_fixture, index, "S").as_number() -
                         expected_insum[index]) < 1e-5 &&
                    std::abs(point_value(indicator_aggregate_fixture, index, "A").as_number() -
                             expected_insum_average[index]) < 1e-5 &&
                    point_value(indicator_aggregate_fixture, index, "MX").as_number() ==
                        expected_insum_maximum[index] &&
                    point_value(indicator_aggregate_fixture, index, "MN").as_number() ==
                        expected_insum_minimum[index] &&
                    point_value(indicator_aggregate_fixture, index, "IX").as_number() ==
                        expected_insum_maximum_index[index] &&
                    point_value(indicator_aggregate_fixture, index, "IN").as_number() ==
                        expected_insum_minimum_index[index],
                "INSUM matches native sum/average/extreme/member-index semantics");
    }
    for (const auto *output : {"S", "A", "MX", "MN", "IX", "IN"})
        require(point_value(indicator_aggregate_fixture, 4, output).is_null(),
                std::string("INSUM does not carry a missing member date: ") + output);
    require(
        indicator_aggregate_fixture.at("context_metadata").at("insort_binding_count").as_number() ==
                2.0 &&
            indicator_aggregate_fixture.at("context_metadata")
                    .at("insum_binding_count")
                    .as_number() == 6.0 &&
            indicator_aggregate_fixture.at("context_metadata")
                    .at("indicator_aggregate_member_file_count")
                    .as_number() == 2.0 &&
            indicator_aggregate_fixture.at("context_metadata")
                    .at("indicator_aggregate_member_series_count")
                    .as_number() == 2.0 &&
            indicator_aggregate_fixture.at("context_metadata")
                    .at("indicator_aggregate_formula_evaluation_count")
                    .as_number() == 3.0,
        "INSORT/INSUM report exact bindings, member files, and cached evaluations");
    fs::create_directories(custom_root / "T0002" / "signals");
    tdx::atomic_write_bytes(custom_root / "T0002" / "signals" / "extern_user.txt",
                            tdx::encode_gbk("0|000001|7|  用户文本  |12.5\n"
                                            "0|000001|7|后续重复值|88\n"
                                            "71|IFL9|9|期货文本|3.25\n"));
    tdx::atomic_write_bytes(custom_root / "T0002" / "signals" / "extern_sys.txt",
                            tdx::encode_gbk("0|000001|7|系统文本|99.25\n"));
    const auto external_catalog = tdx::load_external_signal_catalog(custom_root);
    const auto *user_external = tdx::find_external_signal(external_catalog, false, 0, "000001", 7);
    const auto *system_external = tdx::find_external_signal(external_catalog, true, 0, "000001", 7);
    const auto *derivative_external =
        tdx::find_external_signal(external_catalog, false, 31, "IFL9", 9);
    require(external_catalog.records.size() == 4 && user_external &&
                std::abs(user_external->value - 12.5f) < 1e-6 &&
                tdx::trim(user_external->text) == "用户文本" && system_external &&
                std::abs(system_external->value - 99.25f) < 1e-6 && derivative_external &&
                tdx::trim(derivative_external->text) == "期货文本",
            "extern_user/extern_sys preserve file order and 31/71 market compatibility");
    const auto external_signal_analysis =
        tdx::analyze_formula_source("U:EXTERNVALUE(0,7);S:EXTERNVALUE(1,7);"
                                    "L:EXTERNVALUE(256,7);M:EXTERNVALUE(0,999);"
                                    "UT:STRCMP(EXTERNSTR(0,7),'用户文本');"
                                    "ST:STRCMP(EXTERNSTR(1,7),'系统文本');"
                                    "MT:STRCMP(EXTERNSTR(0,999),' ');");
    require(external_signal_analysis.at("executable_with_context").as_bool() &&
                external_signal_analysis.at("automatic_context_dependencies").size() == 2,
            "EXTERNVALUE/EXTERNSTR are automatic local-file dependencies");
    const auto external_signal_context = tdx::build_formula_market_context_document(
        custom_root, "sz", "000001", external_signal_analysis);
    const auto external_fixture =
        tdx::evaluate_formula_source_document(sample(3),
                                              "U:EXTERNVALUE(0,7);S:EXTERNVALUE(1,7);"
                                              "L:EXTERNVALUE(256,7);M:EXTERNVALUE(0,999);"
                                              "UT:STRCMP(EXTERNSTR(0,7),'用户文本');"
                                              "ST:STRCMP(EXTERNSTR(1,7),'系统文本');"
                                              "MT:STRCMP(EXTERNSTR(0,999),' ');",
                                              {}, "EXTERNALSIGNALS", &external_signal_context);
    require(point_value(external_fixture, 2, "U").as_number() == 12.5 &&
                point_value(external_fixture, 2, "S").as_number() == 99.25 &&
                point_value(external_fixture, 2, "L").as_number() == 12.5 &&
                point_value(external_fixture, 2, "M").as_number() == 0.0 &&
                point_value(external_fixture, 2, "UT").as_number() == 1.0 &&
                point_value(external_fixture, 2, "ST").as_number() == 1.0 &&
                point_value(external_fixture, 2, "MT").as_number() == 1.0 &&
                external_fixture.at("context_metadata")
                        .at("formula_external_signal_matched_binding_count")
                        .as_number() == 2.0,
            "EXTERNVALUE/EXTERNSTR reproduce namespace, low-byte selector, first-match, trim, and "
            "missing sentinels");
    fs::create_directories(custom_root / "T0002" / "extdata");
    const auto append_u16 = [](tdx::Bytes &bytes, std::uint16_t value) {
        bytes.push_back(static_cast<std::uint8_t>(value));
        bytes.push_back(static_cast<std::uint8_t>(value >> 8));
    };
    const auto append_u32 = [](tdx::Bytes &bytes, std::uint32_t value) {
        const auto start = bytes.size();
        bytes.resize(start + sizeof(value));
        for (std::size_t index = 0; index < sizeof(value); ++index)
            bytes[start + index] =
                static_cast<std::uint8_t>(value >> (index * 8));
    };
    const auto append_index = [&](tdx::Bytes &bytes, std::uint16_t market, const std::string &code,
                                  std::int32_t count) {
        append_u16(bytes, market);
        const auto start = bytes.size();
        bytes.resize(start + 23, 0);
        std::copy_n(code.begin(), std::min<std::size_t>(code.size(), 22),
                    bytes.begin() + static_cast<std::ptrdiff_t>(start));
        append_u32(bytes, static_cast<std::uint32_t>(count));
    };
    const auto append_point = [&](tdx::Bytes &bytes, std::int32_t date, std::int32_t time,
                                  float value) {
        append_u32(bytes, static_cast<std::uint32_t>(date));
        append_u32(bytes, static_cast<std::uint32_t>(time));
        std::uint32_t bits{};
        std::memcpy(&bits, &value, sizeof(bits));
        append_u32(bytes, bits);
    };
    tdx::Bytes external_index;
    append_index(external_index, 1, "600000", 1);
    append_index(external_index, 0, "000001", 2);
    tdx::atomic_write_bytes(custom_root / "T0002" / "extdata" / "extdata_73.idx", external_index);
    tdx::Bytes external_data;
    append_point(external_data, 20260101, 150000, 999.0f);
    append_point(external_data, 20260102, 150000, 20.0f);
    append_point(external_data, 20260104, 150000, 40.0f);
    tdx::atomic_write_bytes(custom_root / "T0002" / "extdata" / "extdata_73.dat", external_data);
    const auto selected_external_series =
        tdx::load_external_series(custom_root, 73, 0, "000001", 30000);
    const auto limited_external_series = tdx::load_external_series(custom_root, 73, 0, "000001", 1);
    require(selected_external_series.index_records.size() == 2 &&
                selected_external_series.security_found &&
                selected_external_series.selected->start_point == 1 &&
                selected_external_series.points.size() == 2 &&
                selected_external_series.points.front().date == 20260102 &&
                std::abs(selected_external_series.points.back().value - 40.0f) < 1e-6 &&
                limited_external_series.points.size() == 1 &&
                limited_external_series.host_limit_truncated,
            "EXTDATA_USER idx order selects the exact 12-byte dat segment and enforces the host "
            "point limit");
    tdx::Json external_kline = tdx::Json::object();
    external_kline["market"] = "sz";
    external_kline["code"] = "000001";
    external_kline["period"] = "day";
    external_kline["bars"] = tdx::Json::array();
    for (int day = 1; day <= 5; ++day) {
        tdx::Json bar = tdx::Json::object();
        bar["date"] = "2026-01-0" + std::to_string(day);
        bar["time"] = "15:00";
        bar["open"] = 10.0;
        bar["high"] = 11.0;
        bar["low"] = 9.0;
        bar["close"] = 10.0;
        bar["amount"] = 1000.0;
        bar["volume"] = 100.0;
        external_kline["bars"].push_back(std::move(bar));
    }
    const auto score_analysis = tdx::analyze_formula_source("SAFE:SAFESCORE();SHINE:SHINESCORE();");
    require(score_analysis.at("executable_with_context").as_bool() &&
                score_analysis.at("automatic_context_dependencies").size() == 2,
            "SAFESCORE/SHINESCORE are automatic local type-167 dependencies");
    const auto score_context =
        tdx::build_formula_market_context_document(custom_root, "sz", "000001", score_analysis);
    const auto score_fixture = tdx::evaluate_formula_source_document(
        external_kline, "SAFE:SAFESCORE();SHINE:SHINESCORE();", {}, "SECURITYSCORES",
        &score_context);
    require(point_value(score_fixture, 0, "SAFE").as_number() == 92.0 &&
                point_value(score_fixture, 4, "SAFE").as_number() == 92.0 &&
                point_value(score_fixture, 0, "SHINE").as_number() == 7.0 &&
                score_fixture.at("context_metadata").at("security_score_mode").as_string() ==
                    "tdxw-type167-specgpext-fields4-5-local-reconstruction",
            "SAFESCORE/SHINESCORE broadcast local specgpext float fields");
    const auto missing_score_context =
        tdx::build_formula_market_context_document(custom_root, "sz", "999999", score_analysis);
    const auto missing_score_fixture = tdx::evaluate_formula_source_document(
        external_kline, "SAFE:SAFESCORE();SHINE:SHINESCORE();", {}, "MISSINGSECURITYSCORES",
        &missing_score_context);
    require(point_value(missing_score_fixture, 0, "SAFE").is_null() &&
                point_value(missing_score_fixture, 4, "SHINE").is_null(),
            "missing specgpext security preserves native missing-score output");
    const auto external_series_analysis =
        tdx::analyze_formula_source("F:EXTDATA_USER(73.9,1.9);Z:EXTDATA_USER(73,2);"
                                    "B:EXTDATA_USER(73,3);N:EXTDATA_USER(73,0);");
    require(
        external_series_analysis.at("executable_with_context").as_bool() &&
            external_series_analysis.at("automatic_context_dependencies").size() == 1 &&
            external_series_analysis.at("context_bindings_required")
                    .as_array()
                    .front()
                    .as_string()
                    .rfind("EXTDATA_USER#73#", 0) == 0,
        "EXTDATA_USER is an automatic local-file dependency with TCalc float32-to-int bindings");
    const auto external_series_context = tdx::build_formula_market_context_document(
        custom_root, "sz", "000001", external_series_analysis, 10000, nullptr, &external_kline);
    const auto external_series_fixture =
        tdx::evaluate_formula_source_document(external_kline,
                                              "F:EXTDATA_USER(73.9,1.9);Z:EXTDATA_USER(73,2);"
                                              "B:EXTDATA_USER(73,3);N:EXTDATA_USER(73,0);",
                                              {}, "EXTDATAUSER", &external_series_context);
    require(
        point_value(external_series_fixture, 0, "F").is_null() &&
            point_value(external_series_fixture, 0, "Z").as_number() == 0.0 &&
            point_value(external_series_fixture, 0, "B").as_number() == 20.0 &&
            point_value(external_series_fixture, 0, "N").is_null() &&
            point_value(external_series_fixture, 1, "F").as_number() == 20.0 &&
            point_value(external_series_fixture, 2, "F").as_number() == 20.0 &&
            point_value(external_series_fixture, 2, "B").as_number() == 40.0 &&
            point_value(external_series_fixture, 3, "N").as_number() == 40.0 &&
            point_value(external_series_fixture, 4, "F").as_number() == 40.0 &&
            point_value(external_series_fixture, 4, "B").is_null() &&
            external_series_fixture.at("context_metadata").at("formula_external_series").size() ==
                4,
        "EXTDATA_USER reproduces exact, forward-fill, zero and backward-fill alignment modes");

    tdx::atomic_write_text(custom_root / "T0002" / "signals" / "datacfg.sys",
                           "10001|0|7|System signal\n");
    tdx::Bytes user_signal_catalog(60, 0);
    user_signal_catalog[0] = 73;
    const std::string user_signal_name = "User signal";
    std::copy(user_signal_name.begin(), user_signal_name.end(),
              user_signal_catalog.begin() + 8);
    tdx::atomic_write_bytes(custom_root / "T0002" / "signals" / "datacfg.dat",
                            user_signal_catalog);
    tdx::atomic_write_text(custom_root / "T0002" / "signals" / "signals_sys_10001.dat",
                           "0|000001|20260102|12.5\n"
                           "1|600000|20260103|999\n"
                           "0|000001|20260104|14.5\n");
    tdx::Bytes user_signal_points;
    const auto append_user_signal_point = [&](std::int32_t date, float value) {
        append_u32(user_signal_points, static_cast<std::uint32_t>(date));
        std::uint32_t bits{};
        std::memcpy(&bits, &value, sizeof(bits));
        append_u32(user_signal_points, bits);
    };
    append_user_signal_point(20260103, 23.5f);
    append_user_signal_point(20260105, 25.5f);
    tdx::atomic_write_bytes(custom_root / "T0002" / "signals" /
                                "signals_user_73" / "0_000001.dat",
                            user_signal_points);
    const auto local_signal_catalog = tdx::load_local_signal_catalog(custom_root);
    const auto local_system_signal = tdx::load_local_signal_series(
        custom_root, tdx::LocalSignalNamespace::system, 10001, 0, "000001");
    const auto local_user_signal = tdx::load_local_signal_series(
        custom_root, tdx::LocalSignalNamespace::user, 73, 0, "000001");
    require(local_signal_catalog.entries.size() == 2 &&
                local_signal_catalog.entries[0].signal_id == 10001 &&
                local_signal_catalog.entries[1].signal_id == 73 &&
                local_system_signal.source_record_count == 3 &&
                local_system_signal.matched_record_count == 2 &&
                local_system_signal.points.size() == 2 &&
                local_user_signal.source_record_count == 2 &&
                local_user_signal.points.size() == 2,
            "TDXDeep/TdxW local signal catalogs and selector-34/36 files parse exactly");
    const auto local_signal_analysis = tdx::analyze_formula_source(
        "SF:SIGNALS_SYS(10001.9,1.9);SZ:SIGNALS_SYS(10001,2);"
        "UE:SIGNALS_USER(73,0);UF:SIGNALS_USER(73,1);");
    require(local_signal_analysis.at("executable_with_context").as_bool() &&
                local_signal_analysis.at("automatic_context_dependencies").size() == 2 &&
                local_signal_analysis.at("context_bindings_required").size() == 4,
            "SIGNALS_SYS/SIGNALS_USER are automatic local-file dependencies");
    const auto local_signal_context = tdx::build_formula_market_context_document(
        custom_root, "sz", "000001", local_signal_analysis, 10000, nullptr,
        &external_kline);
    const auto local_signal_fixture = tdx::evaluate_formula_source_document(
        external_kline,
        "SF:SIGNALS_SYS(10001.9,1.9);SZ:SIGNALS_SYS(10001,2);"
        "UE:SIGNALS_USER(73,0);UF:SIGNALS_USER(73,1);",
        {}, "LOCALSIGNALS", &local_signal_context);
    require(point_value(local_signal_fixture, 0, "SF").is_null() &&
                point_value(local_signal_fixture, 0, "SZ").as_number() == 0.0 &&
                point_value(local_signal_fixture, 1, "SF").as_number() == 12.5 &&
                point_value(local_signal_fixture, 2, "SF").as_number() == 12.5 &&
                point_value(local_signal_fixture, 3, "SF").as_number() == 14.5 &&
                point_value(local_signal_fixture, 2, "UE").as_number() == 23.5 &&
                point_value(local_signal_fixture, 3, "UE").is_null() &&
                point_value(local_signal_fixture, 3, "UF").as_number() == 23.5 &&
                point_value(local_signal_fixture, 4, "UF").as_number() == 25.5 &&
                local_signal_fixture.at("context_metadata")
                        .at("formula_local_signals")
                        .size() == 4,
            "SIGNALS_SYS/SIGNALS_USER reproduce exact, forward-fill and zero modes");

    const auto valuation_jsn_root = custom_root / "jsn";
    tdx::atomic_write_text(
        valuation_jsn_root / "list" / "func_gx_hyzt101_1.jsn",
        R"([{"colheader":["$ZQDM","$SC","TDXHY","$ZQDM1","$SC1","hyPE","hyPB","$S_ZQDM","sszt"],"data":[["000001","0","Bank","880471","1","5.2105","0.5302","0|000001",""]]}])");
    tdx::BlockData valuation_blocks;
    tdx::Block valuation_block;
    valuation_block.block_id = "industry:880471";
    valuation_block.family = "industry";
    valuation_block.family_name = "通达信行业";
    valuation_block.block_code = "880471";
    valuation_block.name = "Bank";
    valuation_block.is_leaf = true;
    valuation_blocks.blocks.push_back(valuation_block);
    tdx::BlockMember valuation_member;
    valuation_member.block_id = valuation_block.block_id;
    valuation_member.family = valuation_block.family;
    valuation_member.family_name = valuation_block.family_name;
    valuation_member.block_code = valuation_block.block_code;
    valuation_member.block_name = valuation_block.name;
    valuation_member.security_id = "SZ000001";
    valuation_member.market_id = 0;
    valuation_member.market = "SZ";
    valuation_member.code = "000001";
    valuation_member.membership = "direct";
    valuation_blocks.members.push_back(valuation_member);
    tdx::Block research_valuation_block;
    research_valuation_block.block_id = "research-industry:881230";
    research_valuation_block.family = "research-industry";
    research_valuation_block.family_name = "研究行业";
    research_valuation_block.block_code = "881230";
    research_valuation_block.name = "Research Bank";
    research_valuation_block.is_leaf = true;
    valuation_blocks.blocks.push_back(research_valuation_block);
    auto research_valuation_member = valuation_member;
    research_valuation_member.block_id = research_valuation_block.block_id;
    research_valuation_member.family = research_valuation_block.family;
    research_valuation_member.family_name = research_valuation_block.family_name;
    research_valuation_member.block_code = research_valuation_block.block_code;
    research_valuation_member.block_name = research_valuation_block.name;
    valuation_blocks.members.push_back(research_valuation_member);
    const auto industry_valuation_analysis =
        tdx::analyze_formula_source("PE:HYSYL;PEC:HYSYL();PB:HYSJL;PBC:HYSJL();");
    const auto industry_valuation_context = tdx::build_formula_market_context_document(
        custom_root, "sz", "000001", industry_valuation_analysis, 1000, &valuation_blocks,
        &external_kline, false, nullptr, valuation_jsn_root);
    const auto industry_valuation_fixture = tdx::evaluate_formula_source_document(
        external_kline, "PE:HYSYL;PEC:HYSYL();PB:HYSJL;PBC:HYSJL();", {}, "INDUSTRYVALUATION",
        &industry_valuation_context);
    const double expected_industry_pe = static_cast<double>(static_cast<float>(5.2105));
    const double expected_industry_pb = static_cast<double>(static_cast<float>(0.5302));
    require(
        std::abs(point_value(industry_valuation_fixture, 0, "PE").as_number() -
                 expected_industry_pe) < 1e-12 &&
            std::abs(point_value(industry_valuation_fixture, 4, "PEC").as_number() -
                     expected_industry_pe) < 1e-12 &&
            std::abs(point_value(industry_valuation_fixture, 0, "PB").as_number() -
                     expected_industry_pb) < 1e-12 &&
            std::abs(point_value(industry_valuation_fixture, 4, "PBC").as_number() -
                     expected_industry_pb) < 1e-12 &&
            industry_valuation_context.at("industry_valuation").at("selected_code").as_string() ==
                "880471" &&
            industry_valuation_context.at("industry_valuation").at("availability").as_string() ==
                "public-hyzt-record" &&
            industry_valuation_context.at("industry_valuation")
                    .at("bindings")
                    .at("HYSYL")
                    .at("tcalc_opcode")
                    .as_number() == 1328.0 &&
            industry_valuation_context.at("industry_valuation")
                    .at("bindings")
                    .at("HYSJL")
                    .at("tdxw_return_offset")
                    .as_number() == 380.0,
        "HYSYL/HYSJL select the configured industry and broadcast cached HYZT valuation fields");
    tdx::atomic_write_text(custom_root / "T0002" / "user.ini", "[Other]\nUseTdxL3HY=2\n");
    const auto industry_valuation_fallback_context = tdx::build_formula_market_context_document(
        custom_root, "sz", "000001", industry_valuation_analysis, 1000, &valuation_blocks,
        &external_kline, false, nullptr, valuation_jsn_root);
    const auto &fallback_metadata = industry_valuation_fallback_context.at("industry_valuation");
    require(
        fallback_metadata.at("configured_selected_code").as_string() == "881230" &&
            fallback_metadata.at("selected_code").as_string() == "880471" &&
            fallback_metadata.at("normal_industry_fallback").as_bool() &&
            !fallback_metadata.at("native_host_family_exact").as_bool() &&
            fallback_metadata.at("availability").as_string() ==
                "public-hyzt-normal-industry-fallback" &&
            std::abs(industry_valuation_fallback_context.at("symbols").at("HYSYL").as_number() -
                     expected_industry_pe) < 1e-12,
        "missing public 881xxx valuation falls back transparently to the audited 880xxx record");
    fs::remove_all(custom_root);
}

} // namespace formula_engine_test
