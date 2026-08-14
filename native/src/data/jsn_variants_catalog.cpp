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

const std::map<std::string, std::vector<std::string>>& detail_templates() {
    static const auto values = [] {
        std::map<std::string, std::vector<std::string>> result;
        auto add = [&](std::string key, std::initializer_list<const char*> paths) {
            auto& target = result[lower_ascii(std::move(key))];
            for (const auto* path : paths) target.emplace_back(path);
        };
        add("func_hsgt10$$unitid$$.jsn", {});
        add("cgfxmx1", {"cgfxmx1/$$$SC$$$$$ZQDM$$.jsn"});
        add("cgfxmx2", {"cgfxmx2/$$$SC$$$$$ZQDM$$.jsn"});
        add("lhbfx", {"lhbfx/$$$ZQDM$$.jsn"});
        add("zcjc", {"zcjc/$$$SC$$$$$ZQDM$$.jsn"});
        add("gqzy", {"gqzy/$$$SC$$$$$ZQDM$$.jsn"});
        add("xtzy", {"xtzy/$$$ZQDM$$.jsn"});
        add("dzjy1", {"dzjy1/$$$ZQDM$$.jsn"});
        add("dzjy2", {"dzjy2/$$$ZQDM$$.jsn"});
        add("dzjy3", {"dzjy3/$$$SC$$$$$ZQDM$$.jsn"});
        add("dzjy13", {"dzjy13/$$$SC$$$$$ZQDM$$.jsn"});
        add("cggg", {"cggg/$$$SC$$$$$ZQDM$$.jsn"});
        add("gdzjc1", {"gdzjc1/$$$ZQDM$$.jsn"});
        add("gghg", {"gghg/$$$SC$$$$$ZQDM$$.jsn"});
        add("gqgg", {"gqgg/$$$ZQDM$$.jsn"});
        add("rzrq1", {"rzrq1/$$$SC$$$$$ZQDM$$.jsn"});
        add("rzrq2", {"rzrq2/$$$SC$$$$$ZQDM$$.jsn"});
        add("rzrq3", {"rzrq3/$$$SC$$$$$ZQDM$$.jsn"});
        add("rzrq4", {"rzrq4/$$$SC$$$$$ZQDM$$.jsn"});
        add("rzrq5", {"rzrq5/$$$ZQDM$$.jsn"});
        add("rzrq6", {"rzrq6/$$$ZQDM$$.jsn"});
        add("rzrq7", {"rzrq7/$$$ZQDM$$.jsn"});
        add("rzrq8", {"rzrq8/$$$ZQDM$$.jsn"});
        add("hsgt", {"hsgt/$$$ZQDM$$.jsn"});
        add("hsgtcg1", {"hsgtcg1/$$$SC$$$$$ZQDM$$.jsn", "hsgtcg1/$$$ZQDM$$.jsn"});
        add("hsgtcg2", {"hsgtcg2/$$$SC$$$$$ZQDM$$.jsn", "hsgtcg2/$$$ZQDM$$.jsn"});
        add("ggthy", {"ggthy/$$$SC$$$$$ZQDM$$.jsn"});
        add("ggthy1", {"ggthy1/$$$SC$$$$$ZQDM$$.jsn"});
        add("cjrl", {"cjrl/$$$ZQDM$$.jsn"});
        add("nscg", {"nscg/$$$ZQDM$$.jsn"});
        add("qhtj1", {"qhtj1/$$$SC$$$$$ZQDM$$.jsn"});
        add("qhtj2", {"qhtj2/$$$SC$$$$$ZQDM$$.jsn"});
        add("ipotj102", {"ipotj102/$$$ZQDM$$.jsn"});
        add("ipotj103", {"ipotj103/$$$ZQDM$$.jsn"});
        add("ipotj104", {"ipotj104/$$$ZQDM$$.jsn"});
        add("bygtj1", {"bygtj1/$$$ZQDM$$.jsn"});
        add("bygtj3", {"bygtj3/$$$ZQDM$$.jsn"});
        add("zdtfx1", {"zdtfx1/$$$SC$$$$$ZQDM$$.jsn"});
        add("zdtfx2", {"zdtfx2/$$$ZQDM$$.jsn"});
        add("zdtfx3", {"zdtfx3/$$$ZQDM$$.jsn"});
        add("zjtc1", {"zjtc1/$$$ZQDM$$.jsn"});
        add("zjtc2", {"zjtc2/$$$ZQDM$$.jsn"});
        add("zjtc3", {"zjtc3/$$$ZQDM$$.jsn"});
        add("zjtc4", {"zjtc4/$$$ZQDM$$.jsn"});
        add("zjtc5", {"zjtc5/$$$ZQDM$$.jsn"});
        add("jjzb1", {"jjzb1/$$$ZQDM$$.jsn"});
        add("jjzb2", {"jjzb2/$$$ZQDM$$.jsn"});
        add("zttz", {"zttz/$$$ZQDM$$.jsn"});
        add("zttz1", {"zttz1/$$$ZQDM$$.jsn"});
        add("ggjx", {"ggjx/$$$SC$$$$$ZQDM$$.jsn"});
        add("ydyl1", {"ydyl1/$$$ZQDM$$.jsn"});
        add("xnxs", {"xnxs/$$$ZQDM$$.jsn"});
        add("ztxx", {"ztxx/$$$ZQDM$$.jsn"});
        add("ygzl", {"ygzl/$$$ZQDM$$.jsn"});
        add("sjqd", {"sjqd/$$$ZQDM$$.jsn"});
        add("jzgz1", {"jzgz1/$$$ZQDM$$.jsn"});
        add("yjyg", {"yjyg/$$$ZQDM$$.jsn"});
        add("zdjjzczc", {"zdjjzczc/$$$SC$$$$$ZQDM$$.jsn"});
        add("ggpj", {"ggpj/$$$SC$$$$$ZQDM$$.jsn"});
        add("hypj", {"hypj/$$$SC$$$$$ZQDM$$.jsn"});
        add("mgpj", {"mgpj/$$$SC$$$$$ZQDM$$.jsn"});
        add("kzz_hstk", {"kzz_hstk/$$$SC$$$$$ZQDM$$.jsn"});
        add("kzz_shtk", {"kzz_shtk/$$$SC$$$$$ZQDM$$.jsn"});
        add("kzz_xztk", {"kzz_xztk/$$$SC$$$$$ZQDM$$.jsn"});
        for (const int id : {21701, 21702, 21703})
            result["hgrztj$$unitid$$"].push_back(
                "hgrztj" + std::to_string(id) + "/$$$ZQDM$$.jsn");
        for (const int id : {22401, 22402, 22403, 22404})
            result["yybph$$unitid$$"].push_back(
                "yybph" + std::to_string(id) + "/$$$ZQDM$$.jsn");
        for (const int id : {13801, 13901, 14001})
            result["hylhb$$unitid$$"].push_back(
                "hylhb" + std::to_string(id) + "/$$$SC$$$$$ZQDM$$.jsn");
        for (const int id : {22801, 22802, 22803, 22804})
            result["lsyd$$unitid$$"].push_back(
                "lsyd" + std::to_string(id) + "/$$$SC$$$$$ZQDM$$.jsn");
        return result;
    }();
    return values;
}

std::vector<Binding> coverage_bindings() {
    std::vector<Binding> result;
    auto add = [&](const std::string& command, const std::string& endpoint,
                   std::initializer_list<const char*> patterns) {
        for (const auto* pattern : patterns) result.push_back({pattern, command, endpoint});
    };
    add("market active-funds", "/api/v1/market/active-funds",
        {"list/func_zdjjzczc101_1.jsn", "zdjjzczc/*"});
    add("market block-trades", "/api/v1/market/block-trades",
        {"list/func_dzjy101_1.jsn", "list/func_dzjy104_1.jsn",
         "list/func_dzjy107_1.jsn", "list/func_dzjy108_1.jsn",
         "list/func_dzjy109_1.jsn", "list/func_dzjy1010_1.jsn",
         "list/func_dzjy1012_1.jsn", "dzjy1/*", "dzjy2/*", "dzjy3/*",
         "dzjy13/*", "yybph22401/*", "yybph22402/*", "yybph22403/*",
         "yybph22404/*"});
    add("market calendar", "/api/v1/market/calendar",
        {"list/func_cjrl101_1.jsn", "list/func_cjrl105_1.jsn",
         "list/func_ggrl101_1.jsn", "list/func_gsrl201_1.jsn",
         "list/func_gsrl202_1.jsn", "list/func_gsrl203_1.jsn",
         "list/func_gsrl204_1.jsn", "list/func_gsrl205_1.jsn",
         "list/func_gsrl206_1.jsn", "list/func_gsrl207_1.jsn",
         "list/func_gsrl208_1.jsn",
         "list/func_gsrl209_1.jsn",
         "list/func_dsjtx101_1.jsn", "list/func_xgrl101_1.jsn",
         "list/func_xgrl102_1.jsn", "list/func_xgrl103_1.jsn",
         "list/func_xgrl104_1.jsn", "list/func_xgrl105_1.jsn",
         "list/func_zdgz_qzkcbd101_1.jsn", "list/func_zdgz_kcbsq101_1.jsn",
         "list/func_zdgz_kcbsq103_1.jsn",
         "list/func_sbxg101_1.jsn", "list/func_sbxg102_1.jsn",
         "list/func_mgrl101_1.jsn", "list/func_mgrl102_1.jsn",
         "list/func_mgxg101_1.jsn", "list/func_mgxg102_1.jsn",
         "list/func_qhrl400_1.jsn",
         "cjrl/*"});
    add("market economic-indicators", "/api/v1/market/economic-indicators",
        {"list/func_jjzb101_1.jsn", "jjzb1/*", "jjzb2/*"});
    add("market strategic-themes", "/api/v1/market/strategic-themes",
        {"list/func_gfjg101_1.jsn", "list/func_gfjg102_1.jsn",
         "list/func_hlw101_1.jsn", "list/func_jkzg101_1.jsn",
         "list/func_mlzg101_1.jsn", "list/func_jtqg101_1.jsn",
         "list/func_gy101_1.jsn", "list/func_qccy101_1.jsn",
         "list/func_rgzn101_1.jsn", "list/func_5G101_1.jsn",
         "list/func_xjj101_1.jsn", "list/func_djr101_1.jsn",
         "list/func_fjcjj101_1.jsn", "list/func_bdt101_1.jsn",
         "list/func_tzh_1.jsn", "list/func_szjj101_1.jsn",
         "list/func_slcy101_1.jsn", "list/func_jqr101_1.jsn",
         "list/func_dxf101_1.jsn", "list/func_fglc101_1.jsn",
         "list/func_xcl101_1.jsn", "list/func_xxdl101_1.jsn",
         "list/func_gcr101_1.jsn", "list/func_dzq101_1.jsn",
         "list/func_nyaq101_1.jsn", "list/func_dcl101_1.jsn",
         "zttzty/*"});
    add("market theme-library", "/api/v1/market/theme-library",
        {"list/func_zttz102_1.jsn", "list/func_zttz103_1.jsn",
         "list/func_zttz104_1.jsn", "list/func_zttz105_1.jsn",
         "list/func_zttz106_1.jsn", "zttz/*", "zttz1/*"});
    add("market thematic-opportunities", "/api/v1/market/thematic-opportunities",
        {"list/func_ydyl101_1.jsn", "list/func_ydyl102_1.jsn", "ydyl1/*",
         "list/func_rdhs101_1.jsn", "list/func_rdhs102_1.jsn",
         "list/func_xnxs101_1.jsn", "xnxs/*"});
    add("market tender-offers", "/api/v1/market/tender-offers",
        {"list/func_yysg101_1.jsn"});
    add("market consensus", "/api/v1/market/consensus",
        {"list/func_yzyq101_1.jsn", "list/func_yzyq102_1.jsn",
         "list/func_yzyq103_1.jsn", "list/func_yzyq104_1.jsn",
         "list/func_yzyq105_1.jsn", "list/func_yzyq106_1.jsn",
         "list/func_yzyq107_1.jsn", "list/func_yzyq108_1.jsn",
         "list/func_yzyq109_1.jsn", "yzyq/*"});
    add("market convertible-bonds", "/api/v1/market/convertible-bonds",
        {"list/kzz_kzzsy201_1.jsn", "list/func_kzz_tkjd201.jsn",
         "list/func_kzz_lltk201.jsn", "list/func_kzz_hstk201.jsn",
         "list/func_kzz_shtk201.jsn", "list/func_kzz_xztk201.jsn",
         "list/kjhz_kjhzsy201_1.jsn", "list/dfkzz201_1.jsn",
         "list/gxjty_zq_kzzsy101_1.jsn", "list/func_kkzss101_1.jsn",
         "list/func_kzz102_1.jsn", "list/gxjty_zq_dfkzz102_1.jsn",
         "list/gxjty_zq_xkzz102_1.jsn", "list/func_kzz103_1.jsn",
         "kzz_hstk/*", "kzz_shtk/*", "kzz_xztk/*"});
    add("market bond-reference", "/api/v1/market/bond-reference",
        {"list/zq_aaa201.jsn", "list/zq_aaj201.jsn", "list/zq_aa201.jsn",
         "list/zq_aajj201.jsn", "list/zq_aj201.jsn", "list/zq_a201.jsn",
         "list/zq_ajj201.jsn", "list/zq_a1201.jsn",
         "list/zq_bbbjjyx201.jsn", "list/zq_gdll201.jsn",
         "list/zq_fdll201.jsn", "list/zq_djll201.jsn",
         "list/zq_lsbq201.jsn", "list/zq_tx201.jsn", "list/zq_qtlx.jsn"});
    add("market bond-reference", "/api/v1/market/bond-reference",
        {"list/zq_zqqb201.jsn", "list/gxjty_zq_gz101_1.jsn",
         "list/gxjty_zq_dfz101_1.jsn",
         "list/gxjty_zq_gsz101_1.jsn", "list/gxjty_zq_qyz101_1.jsn",
         "list/gxjty_zq_smz101_1.jsn", "list/gxjty_zq_zczczq101_1.jsn",
         "list/gxjty_zq_cxkzz101_1.jsn", "list/zqjrz201.jsn",
         "list/zq_jrz201_1.jsn", "list/zq_jrz201_2.jsn",
         "list/zqdfzfz201.jsn", "list/zqgsz201.jsn", "list/zqgz201.jsn",
         "list/zqqyz201.jsn", "list/zqsmz201.jsn", "list/zqzczczq201.jsn",
         "list/zq_dfzfz201_1.jsn", "list/zq_dfzfz201_2.jsn",
         "list/zq_gsz201_1.jsn", "list/zq_gsz201_2.jsn",
         "list/zq_gz201_1.jsn", "list/zq_gz201_2.jsn",
         "list/zq_qbzq201_1.jsn", "list/zq_qbzq201_2.jsn",
         "list/zq_qyz201_1.jsn", "list/zq_qyz201_2.jsn",
         "list/zq_smz201_1.jsn", "list/zq_smz201_2.jsn",
         "list/zq_zczczq201_2.jsn"});
    add("market disclosures", "/api/v1/market/disclosures",
        {"list/func_cbpl102_1.jsn", "list/func_cbpl103_1.jsn",
         "list/func_cbpl104_1.jsn", "list/func_ggplsj101_1.jsn",
         "list/func_ggyjpl101_1.jsn"});
    add("market employees", "/api/v1/market/employees",
        {"list/func_ygxc101_1.jsn", "list/func_qxfa501_1.jsn", "ggxc/*"});
    add("market hk-events", "/api/v1/market/hk-events",
        {"list/func_ggrl102_1.jsn", "list/func_ggrl103_1.jsn",
         "list/func_ggrl104_1.jsn", "list/func_ggrl105_1.jsn"});
    add("market special-situations", "/api/v1/market/special-situations",
        {"list/func_agtl101_1.jsn", "list/func_agtl102_1.jsn",
         "list/func_cdgc101_1.jsn", "list/func_qxfa105_1.jsn",
         "list/func_qxfa106_1.jsn", "list/func_qxfa110_1.jsn",
         "list/func_qxfa111_1.jsn", "list/func_xsbtj101_1.jsn",
         "list/func_xsbtj102_1.jsn", "list/func_yzb101_1.jsn"});
    add("market exchange-funds", "/api/v1/market/exchange-funds",
        {"list/func_etfhq101.jsn", "list/func_tlfeyxetf101_1.jsn",
         "list/gxjty_etfjj101.jsn",
         "list/gxjty_etfjj102.jsn", "list/gxjty_etfjj103.jsn",
         "list/gxjty_etfjj104.jsn", "list/gxjty_etfjj105.jsn",
         "list/gxjty_etfjj106.jsn", "list/gxjty_etfjj107.jsn",
         "list/gxjty_lofjj101.jsn", "list/gxjty_lofjj102.jsn",
         "list/gxjty_lofjj103.jsn", "list/gxjty_lofjj104.jsn",
         "list/gxjty_lofjj105.jsn", "list/gxjty_lofjj106.jsn",
         "list/gxjty_lofjj107.jsn", "list/gxjty_fbjj101.jsn",
         "list/gxjty_fbjj102.jsn", "list/gxjty_xjgl101.jsn",
         "list/gxjty_xjgl104.jsn", "list/func_reits101_1.jsn",
         "list/func_reits102_1.jsn"});
    add("market curated-data", "/api/v1/market/curated-data",
        {"list/func_cmyl101_1.jsn", "list/func_dgzxz101.jsn",
         "list/func_fhmz101_1.jsn", "list/func_gfhgtj101_1.jsn",
         "list/func_gfhl101_1.jsn", "list/func_ggthq101_1.jsn",
         "list/func_ggzrt101_1.jsn", "list/func_gqpjg101_1.jsn"});
    add("market special-attention", "/api/v1/market/special-attention",
        {"list/func_tbgz102_1.jsn", "list/func_tbgz103_1.jsn",
         "list/func_tbgz104_1.jsn", "list/func_tbgz106_1.jsn",
         "list/func_tbgz110_1.jsn"});
    add("market fund-statistics", "/api/v1/market/fund-statistics",
        {"list/func_jjtj101_1.jsn", "list/func_jjtj102_1.jsn",
         "list/func_jjtj103_1.jsn", "list/func_jjtj104_1.jsn",
         "list/func_jjtj104_2.jsn", "list/func_jjtj105_1.jsn",
         "list/func_jjtj105_2.jsn", "list/func_jjtj108_1.jsn",
         "list/func_jjtj109_1.jsn"});
    add("market fund-calendar", "/api/v1/market/fund-calendar",
        {"list/func_jjrl301_1.jsn"});
    add("market specialized-metrics", "/api/v1/market/specialized-metrics",
        {"list/func_hyjyfx101_1.jsn", "list/func_hyjyfx102_1.jsn",
         "list/func_hyjyfx103_1.jsn"});
    add("market company-changes", "/api/v1/market/company-changes",
        {"list/func_zqbg101_1.jsn", "list/func_zqbg102_1.jsn",
         "list/func_zqbg103_1.jsn", "list/func_zqbg104_1.jsn",
         "list/func_zqbg105_1.jsn", "list/func_zqbg107_1.jsn",
         "list/func_zqbg108_1.jsn", "list/func_zqbg109_1.jsn",
         "list/func_zqbg110_1.jsn"});
    add("market financial-screen", "/api/v1/market/financial-screen",
        {"list/func_cwzb101_1.jsn", "list/func_cwzb102_1.jsn",
         "list/func_cwzb104_1.jsn", "list/func_cwzb105_1.jsn",
         "list/func_cwzb107_1.jsn", "list/func_xpcz101_1.jsn",
         "list/func_xpcz103_1.jsn", "list/func_xpcz104_1.jsn"});
    add("market financial-insights", "/api/v1/market/financial-insights",
        {"list/func_cbpl101_8.jsn", "list/func_cbpl106_1.jsn",
         "list/func_cbpl107_1.jsn", "list/func_dpsgp_1.jsn",
         "list/func_fhbdb101.jsn", "list/func_gqtz101_1.jsn",
         "list/func_gxjcb101.jsn", "list/func_gyszk101_1.jsn",
         "list/func_knzx101_1.jsn", "list/func_xjl101_1.jsn",
         "list/func_yjfz101_1.jsn", "list/func_wjcg101_1.jsn",
         "list/func_lxsnzz101_1.jsn", "list/func_tqwclr101.jsn",
         "list/func_qxfa101_1.jsn"});
    add("market gdr", "/api/v1/market/gdr", {"list/func_gdr101.jsn"});
    add("market equity-performance", "/api/v1/market/equity-performance",
        {"list/func_aghq101.jsn"});
    add("market corporate-orders", "/api/v1/market/corporate-orders",
        {"list/func_zb101_1.jsn", "list/func_zdht101_1.jsn"});
    add("market event-impact", "/api/v1/market/event-impact",
        {"list/func_zdsj101_1.jsn", "list/func_zdsj102_1.jsn",
         "list/func_zdsj103_1.jsn"});
    add("market global-performance", "/api/v1/market/global-performance",
        {"list/func_zyzh101.jsn", "list/func_zgghq101.jsn"});
    add("market shareholder-signals", "/api/v1/market/shareholder-signals",
        {"list/func_cwnscg101_1.jsn", "list/func_jgxc101_1.jsn",
         "list/func_jgzd101_1.jsn", "list/func_xszgp101_1.jsn",
         "list/func_nscg101_1.jsn", "nscg/*"});
    add("market recent-watch", "/api/v1/market/recent-watch",
        {"list/func_jqgz101_1.jsn", "list/func_jqgz104_1.jsn",
         "list/func_jqgz111_1.jsn"});
    add("market patent-statistics", "/api/v1/market/patent-statistics",
        {"list/func_gszl101_1.jsn"});
    add("market overview-factors", "/api/v1/market/overview-factors",
        {"list/func_dpfx101_1.jsn"});
    add("market benchmark-analysis", "/api/v1/market/benchmark-analysis",
        {"list/func_jzfx201_1.jsn", "list/func_jzfx202_1.jsn",
         "list/func_jzfx203_1.jsn", "list/func_jzfx204_1.jsn",
         "list/func_jzfx208_1.jsn", "list/func_jzfx209_1.jsn",
         "list/func_jzfx210_1.jsn", "list/func_jzfx211_1.jsn",
         "list/func_jzfx212_1.jsn", "list/func_jzfx213_1.jsn",
         "list/func_jzfx214_1.jsn", "list/func_jzfx215_1.jsn",
         "list/func_jzfx216_1.jsn", "list/func_jzfx301_1.jsn",
         "list/func_jzfx302_1.jsn", "list/func_jzfx303_1.jsn",
         "list/func_jzfx304_1.jsn", "list/func_jzfx308_1.jsn",
         "list/func_jzfx309_1.jsn", "list/func_jzfx310_1.jsn",
         "list/func_jzfx311_1.jsn", "list/func_jzfx312_1.jsn",
         "list/func_jzfx313_1.jsn", "list/func_jzfx314_1.jsn",
         "list/func_jzfx315_1.jsn", "list/func_jzfx316_1.jsn",
         "list/func_jzfx401_1.jsn", "list/func_jzfx501_1.jsn"});
    add("market etf-flows", "/api/v1/market/etf-flows",
        {"list/func_etfsg101_1.jsn", "list/func_etfsg102_1.jsn"});
    add("formulas evaluate", "/api/v1/formulas/evaluate",
        {"list/func_qxfa401_1.jsn"});
    add("market forecasts", "/api/v1/market/forecasts",
        {"list/func_yjygtj101_1.jsn", "list/func_ggyjyg101_1.jsn",
         "list/func_cbpl101_1.jsn", "yjyg/*"});
    add("market foreign-alerts", "/api/v1/market/foreign-alerts",
        {"list/func_wzmryj101_1.jsn", "wzmryj/*"});
    add("market futures-issuance", "/api/v1/market/futures-issuance",
        {"list/func_qhtj101_1.jsn", "list/func_qhtj103_1.jsn",
         "list/func_qhtj104_1.jsn", "list/func_ipotj101_1.jsn",
         "list/zq_ssfxr201.jsn", "list/zq_wssfxr201.jsn",
         "list/func_qxfa201_1.jsn", "list/func_qxfa202_1.jsn",
         "list/func_qxfa301_1.jsn", "list/func_qxfa302_1.jsn",
         "list/func_qxfa402_1.jsn", "list/func_qxfa601_1.jsn",
         "list/func_qxfa107_1.jsn", "list/func_qxfa108_1.jsn",
         "list/func_qxfa109_1.jsn", "list/func_yxg101_3.jsn", "qhtj1/*",
         "qhtj2/*", "ipotj102/*", "ipotj103/*", "ipotj104/*"});
    add("hyzt extract", "/api/v1/industry/tree",
        {"list/func_gx_hyzt101_1.jsn"});
    add("market industry-profile", "/api/v1/market/industry-profile",
        {"list/func_cgfxhy101_1.jsn", "list/func_cgfxhy103_1.jsn",
         "list/func_cgfxhy104_1.jsn", "list/func_cgfxhy105_1.jsn",
         "list/func_hygdrs101_1.jsn", "hycgmx/*", "hygdrs/*"});
    add("market institution-lhb", "/api/v1/market/institution-lhb",
        {"list/func_jgzc101_1.jsn", "list/func_jgzc102_1.jsn",
         "list/func_jgzc103_1.jsn", "list/func_jgzc104_1.jsn",
         "lsyd22801/*", "lsyd22802/*", "lsyd22803/*", "lsyd22804/*"});
    add("market institution", "/api/v1/market/institution",
        {"cgfxmx1/*", "cgfxmx2/*"});
    add("market institution-analysis", "/api/v1/market/institution-analysis",
        {"list/func_cgfx101_1.jsn", "list/func_cgfx102_1.jsn",
         "list/func_cgfx103_1.jsn", "list/func_cgfx104_1.jsn",
         "list/func_cgfx105_1.jsn", "list/func_cgfx106_1.jsn",
         "list/func_cgfx107_1.jsn", "list/func_cgfx108_1.jsn",
         "list/func_cgfx109_1.jsn", "list/func_cgfx110_1.jsn",
         "list/func_cgfx111_1.jsn", "list/func_cgfx112_1.jsn",
         "list/func_cgfx113_1.jsn", "list/func_cgfx114_1.jsn",
         "list/func_cgfx115_1.jsn", "list/func_cgfx116_1.jsn",
         "list/func_cgfx117_1.jsn", "list/func_tbgz108_1.jsn",
         "list/func_jgcg108_1.jsn",
         "list/func_tzcg104_1.jsn", "list/func_tzcg105_1.jsn",
         "list/func_tzcg106_1.jsn", "list/func_tzcg108_1.jsn",
         "list/func_tzcg109_1.jsn",
         "list/func_sbltgd101_1.jsn"});
    add("market limit-review", "/api/v1/market/limit-review",
        {"list/func_zdtfx101_1.jsn", "list/func_zdtfx102_1.jsn",
         "list/func_zdtfx103_1.jsn", "list/func_zdtfx106_1.jsn",
         "list/func_zdtfx107_1.jsn", "zdtfx1/*", "zdtfx2/*", "zdtfx3/*"});
    add("market session-turnover", "/api/v1/market/session-turnover",
        {"list/func_phcje101_1.jsn", "list/func_phcje101_2.jsn",
         "list/func_phcje103_1.jsn", "list/func_phcje104_1.jsn"});
    add("market block-rotation", "/api/v1/market/block-rotation",
        {"list/func_bkld101_1.jsn", "list/func_bkld102_1.jsn",
         "list/func_bkld103_1.jsn", "list/func_bkld104_1.jsn"});
    add("market limit-ladder", "/api/v1/market/limit-ladder",
        {"list/func_lbtt101_1.jsn"});
    add("market threshold-stocks", "/api/v1/market/threshold-stocks",
        {"list/func_bygtj102_1.jsn", "list/func_qyjlb102_1.jsn",
         "bygtj1/*", "bygtj3/*"});
    add("market capital-strength", "/api/v1/market/capital-strength",
        {"list/func_qszj101_1.jsn", "list/func_qszj102_1.jsn",
         "list/func_qszj103_1.jsn", "list/func_qszj104_1.jsn",
         "list/func_qszj105_1.jsn"});
    add("market strong-stocks", "/api/v1/market/strong-stocks",
        {"list/func_ygzl101_1.jsn", "ygzl/*"});
    add("market commodity-links", "/api/v1/market/commodity-links",
        {"list/func_zjtc101_1.jsn", "list/func_zjtc103_1.jsn",
         "zjtc1/*", "zjtc2/*", "zjtc3/*", "zjtc4/*", "zjtc5/*"});
    add("market announcement-signals", "/api/v1/market/announcement-signals",
        {"list/func_zxjx101_1.jsn", "list/func_zxjx103_1.jsn", "ggjx/*"});
    add("market reverse-repo", "/api/v1/market/reverse-repo",
        {"list/func_gznhg100_1.jsn", "list/func_gznhg101_1.jsn",
         "list/func_gznhg102_1.jsn", "list/gxjty_zq_gznhg101_1.jsn"});
    add("market exchange-supervision", "/api/v1/market/exchange-supervision",
        {"list/func_jysjk101_1.jsn", "list/func_jysjk102_1.jsn"});
    add("market intelligence", "/api/v1/market/intelligence",
         {"list/func_scrd101_1.jsn", "list/func_bxgc101_1.jsn",
          "list/func_qzbl101_1.jsn", "list/func_sxbzx101_1.jsn",
          "list/func_ldph101_1.jsn",
          "list/func_sjqd101_1.jsn", "sjqd/*",
          "list/func_bwyq101_1.jsn", "list/func_rdyc101_1.jsn",
          "list/func_ztxx101_1.jsn", "ztxx/*",
          "list/func_xwlb101_1.jsn", "list/func_dpyd101_1.jsn",
          "list/func_jzgz101_1.jsn", "jzgz1/*"});
    add("market active-lhb", "/api/v1/market/active-lhb",
        {"list/func_hylhb101_1.jsn", "list/func_hylhb102_1.jsn",
         "list/func_hylhb104_1.jsn", "hylhb13801/*", "hylhb13901/*",
         "hylhb14001/*"});
    add("market state-owned-reform", "/api/v1/market/state-owned-reform",
        {"list/func_gqgg101_1.jsn", "list/func_gqgg102_1.jsn",
         "list/func_gqgg103_1.jsn", "list/func_gqgg104_1.jsn",
         "list/func_gqgg106_1.jsn", "gqgg/*"});
    add("market margin", "/api/v1/market/margin",
        {"list/func_rzt101_1.jsn",
         "list/func_rzrq101_1.jsn", "list/func_rzrq102_1.jsn",
         "list/func_rzrq103_1.jsn", "list/func_rzrq104_1.jsn",
         "list/func_rzrq107_1.jsn", "list/func_rzrq108_1.jsn",
         "list/func_rzrq109_1.jsn", "list/func_rzrq110_1.jsn",
         "list/func_rzrq111_1.jsn", "list/func_rzrq112_1.jsn",
          "list/func_rzrq113_1.jsn", "list/func_rzrq114_1.jsn",
          "list/func_rzrq120_1.jsn", "list/func_rzrq201_1.jsn",
          "rzrq1/*", "rzrq2/*", "rzrq3/*", "rzrq4/*", "rzrq5/*",
          "rzrq6/*", "rzrq7/*", "rzrq8/*"});
    add("market stock-connect", "/api/v1/market/stock-connect",
        {"list/func_hsgt101_1.jsn", "list/func_hsgt102_1.jsn",
         "list/func_hsgt103_1.jsn", "list/func_hsgt104_1.jsn",
         "list/func_hsgt107_1.jsn", "list/func_hsgt110_1.jsn",
         "list/func_hsgt111_1.jsn", "list/func_hsgt112_1.jsn",
         "list/func_hsgt113_1.jsn", "list/func_hsgt114_1.jsn",
         "list/func_gghq_hsgt_lgt_1.jsn", "list/func_gghq_hsgt_ggt_1.jsn",
         "list/func_hsgt201_1.jsn", "list/func_hsgt202_1.jsn",
         "list/func_hsgt203_1.jsn", "list/func_hsgt204_1.jsn",
         "list/func_hsgt205_1.jsn", "list/func_hsgt206_1.jsn",
         "list/func_hsgt207_1.jsn", "list/func_hsgt208_1.jsn",
         "list/func_hsgt209_1.jsn", "list/func_hsgt210_1.jsn",
         "list/func_hsgt211_1.jsn", "list/func_hsgt212_1.jsn",
          "list/func_hsgt301_1.jsn", "hsgt/*", "hsgtcg1/*", "hsgtcg2/*",
          "ggthy/*", "ggthy1/*"});
    add("market lhb", "/api/v1/market/lhb",
        {"list/func_lhbfx101_1.jsn", "list/func_lhbfx103_1.jsn",
         "list/func_lhbfx104_1.jsn", "list/func_lhbfx105_1.jsn",
         "list/func_lhbfx106_1.jsn", "list/func_lhbfx107_1.jsn",
         "list/func_lhbfx108_1.jsn", "list/func_lhbfx110_1.jsn", "lhbfx/*"});
    add("market ownership", "/api/v1/market/ownership",
        {"list/func_zcjc101_1.jsn", "list/func_zcjc102_1.jsn",
         "list/func_zcjc103_1.jsn", "list/func_zcjc104_1.jsn",
         "list/func_zcjc105_1.jsn",
         "list/func_zcjc106_1.jsn", "list/func_zcjc107_1.jsn",
         "list/func_zcjc108_1.jsn", "list/func_zcjc109_1.jsn",
         "list/func_zcjc110_1.jsn",
         "list/func_zcjc111_1.jsn", "list/func_cggg101_1.jsn",
         "list/func_gdrs101_1.jsn", "list/func_gdrs102_1.jsn",
         "list/func_gdrs103_1.jsn", "list/func_gdrs104_1.jsn",
         "list/func_gdrs106_1.jsn", "list/func_gdrs107_1.jsn",
          "list/func_gdzjc102_1.jsn", "list/func_gdzjc105_1.jsn", "gdzjc1/*",
         "list/func_gqzy101_1.jsn", "list/func_gqzy102_1.jsn",
         "list/func_gqzy103_1.jsn", "list/func_gqzy105_1.jsn",
         "list/func_gqzy106_1.jsn", "list/func_gqzy108_1.jsn",
         "list/func_gqzy109_1.jsn", "zcjc/*", "gqzy/*", "cggg/*", "xtzy/*"});
    add("market ratings", "/api/v1/market/ratings",
        {"list/func_ggpj101_1.jsn", "list/func_hypj101_1.jsn",
         "list/func_mgpj101_1.jsn", "ggpj/*", "hypj/*", "mgpj/*"});
    add("market repurchases", "/api/v1/market/repurchases",
        {"list/func_qxfa104_1.jsn", "list/func_hgtj101_1.jsn",
         "list/func_gghg101_1.jsn", "list/func_hgrztj101_1.jsn",
         "list/func_hgrztj102_1.jsn", "list/func_hgrztj103_1.jsn",
         "hgrztj21701/*", "hgrztj21702/*", "hgrztj21703/*", "gghg/*"});
    add("market research", "/api/v1/market/research",
        {"list/func_tzzhd103_1.jsn", "list/func_tzzhd105_1.jsn",
         "list/func_tzzhd109_1.jsn", "list/func_tzzhd110_1.jsn",
         "list/func_tzzhd112_1.jsn", "list/func_tzzhd114_1.jsn",
         "list/func_tzzhd115_1.jsn", "list/func_tzzhd117_1.jsn",
         "list/func_tzzhd190_1.jsn", "tzzhd/*", "tzzhd2/*", "cfjg/*", "zmjg/*"});
    add("market unlocks", "/api/v1/market/unlocks",
        {"list/func_dbljj04_1.jsn", "list/func_jqgz103_1.jsn",
         "list/func_dxfjj101_1.jsn", "dbljj/*"});
    add("market valuation", "/api/v1/market/valuation",
        {"list/func_zsgz101_1.jsn", "zsgz1/*", "zsgz3/*", "zsgz4/*"});
    add("market panorama", "/api/v1/market/panorama",
        {"list/func_aqfph101_1.jsn", "list/func_gx_zjlx101_1.jsn",
         "list/func_gx_fxgz101_1.jsn", "list/func_gx_cbsj101_1.jsn",
         "list/func_gx_cbyg101_1.jsn", "list/func_gx_fxspj101_1.jsn",
         "list/func_gx_zfsp101_1.jsn", "list/func_gx_lhbd101_1.jsn",
         "list/func_gx_rzrq101_1.jsn", "list/func_gx_zcjc101_1.jsn"});
    return result;
}

}  // namespace tdx::jsn_variant_detail
