#include "institution_analysis_internal.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <cstdint>
#include <utility>

namespace tdx::institution_analysis_detail {

const std::array<ViewSpec, 25>& views() { return view_catalog; }

const ViewSpec* find_view(const std::string& id) {
    const auto folded = lower_ascii(trim(id));
    const auto found = std::find_if(views().begin(), views().end(), [&](const ViewSpec& view) {
        return view.id == folded;
    });
    return found == views().end() ? nullptr : &*found;
}

const char* layout_name(Layout value) {
    switch (value) {
        case Layout::holdings: return "holdings";
        case Layout::pensions: return "pensions";
        case Layout::float_structure: return "float-structure";
        case Layout::crowded_oversold: return "crowded-oversold";
        case Layout::exclusive_funds: return "exclusive-funds";
        case Layout::notable_private_funds: return "notable-private-funds";
        case Layout::national_team: return "national-team";
        case Layout::social_security_summary: return "social-security-summary";
        case Layout::development_bank: return "development-bank-holdings";
        case Layout::named_holding: return "named-holding";
        case Layout::named_holding_summary: return "named-holding-summary";
        case Layout::stake_building: return "stake-building";
    }
    return "unknown";
}

Json fields_for(Layout layout) {
    Json result = Json::array();
    auto add = [&](const char* name, const char* source, const char* unit) {
        Json field = Json::object();
        field["name"] = name;
        field["source"] = source;
        field["unit"] = unit;
        result.push_back(std::move(field));
    };
    if (layout == Layout::holdings || layout == Layout::crowded_oversold) {
        add("report_date", "bgq", "YYYYMMDD");
        add("holding_market_value", "sz", "yuan");
        add("institution_count", "jgsl", "count");
        add("institution_count_change", "jgsl_bd", "count");
        add("holding_shares", "gs", "share");
        add("holding_shares_change", "gs_bd", "share");
        add("holding_shares_change_ratio", "derived:gs_bd/(gs-gs_bd)", "ratio");
        add("float_share_ratio", "zltgb", "ratio");
        add("float_share_ratio_change", "zltgb_bd", "ratio");
        add("total_share_ratio", "zzgbb", "ratio");
        add("total_share_ratio_change", "zzgbb_bd", "ratio");
        if (layout == Layout::crowded_oversold) {
            add("one_year_high_adjusted", "price", "price");
            add("latest_close_adjusted", "price1", "price");
            add("drawdown_from_high_pct", "ngdzj", "percent");
        }
    } else if (layout == Layout::pensions) {
        add("report_date", "bgq", "YYYYMMDD");
        add("holding_shares_10k", "gs", "10k-share");
        add("float_share_pct", "zltgb", "percent");
        add("holding_market_value_10k", "sz", "10k-yuan");
        add("net_profit", "bqlr", "upstream-unit");
        add("net_profit_growth_pct", "JLR", "percent");
    } else if (layout == Layout::float_structure) {
        add("report_date", "bgq", "YYYYMMDD");
        add("report_total_shares", "bgqzgb", "share");
        add("report_float_shares", "bgqltgb", "share");
        add("free_float_shares", "derived:bgqltgb-gsll-tdgs", "share");
        add("free_float_to_float_ratio", "derived", "ratio");
        add("free_float_to_total_ratio", "derived", "ratio");
        add("institution_shares", "gsll", "share");
        add("institution_to_float_ratio", "derived", "ratio");
        add("institution_to_total_ratio", "derived", "ratio");
        add("major_holder_shares", "tdgs", "share");
        add("major_holder_to_float_ratio", "derived", "ratio");
        add("major_holder_to_total_ratio", "derived", "ratio");
    } else if (layout == Layout::exclusive_funds) {
        add("fund_management_company", "glgs", "text");
        add("report_date", "bgq", "YYYYMMDD");
        add("holding_shares_10k", "cgsl", "10k-share");
        add("holding_shares", "derived:cgsl*10000", "share");
        add("float_share_pct", "zb", "percent");
    } else if (layout == Layout::notable_private_funds) {
        add("private_fund_manager", "mc", "text");
        add("report_date", "bgq", "YYYYMMDD");
        add("holding_market_value_10k", "sz", "10k-yuan");
        add("holding_market_value", "derived:sz*10000", "yuan");
        add("float_share_pct", "zb", "percent");
        add("industry", "hy", "text");
    } else if (layout == Layout::social_security_summary) {
        add("report_date", "bgq", "YYYYMMDD");
        add("institution_count", "jgsl", "count");
        add("holding_shares", "ccgs", "share");
        add("total_share_pct", "zzgb", "percent");
    } else if (layout == Layout::development_bank) {
        add("report_date", "date", "YYYYMMDD");
        add("shareholder_position", "cgxx", "text");
        add("holding_detail", "xxsm", "text");
    } else if (layout == Layout::named_holding) {
        add("report_date", "date", "YYYYMMDD");
        add("shareholder_position", "cgxx", "text");
    } else if (layout == Layout::named_holding_summary) {
        add("report_date", "DATE", "YYYYMMDD");
        add("holding_shares_10k", "SL", "10k-share");
        add("holding_shares", "derived:SL*10000", "share");
        add("holding_type", "LX", "text");
        add("float_share_pct", "ZB", "percent");
        add("holding_market_value_10k", "CGSZ", "10k-yuan");
        add("holding_market_value", "derived:CGSZ*10000", "yuan");
        add("net_profit", "bqlr", "yuan");
        add("net_profit_growth_pct", "JLR", "percent");
        add("industry", "HY", "text");
    } else if (layout == Layout::stake_building) {
        add("announcement_date", "ggrq", "YYYYMMDD");
        add("start_date", "qsr", "YYYYMMDD");
        add("end_date", "jzr", "YYYYMMDD");
        add("shareholder", "gd", "text");
        add("start_adjusted_close", "price1", "yuan/share");
        add("end_adjusted_close", "price2", "yuan/share");
        add("period_return_pct", "derived:(price2/price1-1)*100", "percent");
        add("average_price", "cjjj", "yuan/share");
        add("increase_shares", "zcgs", "share");
        add("increase_total_pct", "zczb", "percent");
        add("post_holding_shares", "zchgs", "share");
        add("post_holding_total_pct", "zchzb", "percent");
        add("further_increase", "sfjxzc", "text");
        add("insurance_capital", "sfwxz", "yes-no");
    } else {
        add("report_date", "date", "YYYYMMDD");
        add("china_securities_finance_ratio_pct", "zjcg", "percent");
        add("central_huijin_ratio_pct", "hjcg", "percent");
        add("combined_ratio_pct", "hj", "percent");
        add("calculated_combined_ratio_pct", "derived:zjcg+hjcg", "percent");
        add("holder_rank", "gdmc", "text");
        add("industry", "hy", "text");
    }
    return result;
}

Json catalog_document() {
    Json rows = Json::array();
    for (const auto& view : views()) {
        Json row = Json::object();
        row["id"] = view.id;
        row["label"] = view.label;
        row["resource"] = view.resource;
        row["layout"] = layout_name(view.layout);
        row["fields"] = fields_for(view.layout);
        if (view.total_ratio_divide_100)
            row["normalization_note"] =
                "Source zzgbb/zzgbb_bd are divided by 100 exactly as the client CFG calculates zzgbb1.";
        rows.push_back(std::move(row));
    }
    Json result = Json::object();
    result["schema"] = "tdx-institution-analysis-catalog-native-v1";
    result["view_count"] = static_cast<std::uint64_t>(views().size());
    result["views"] = std::move(rows);
    result["semantics"] =
        "Twenty-five TDX institution and investment-holding resources are typed separately from per-security cgfxmx history. They include seventeen CGFX tables plus fund-company exclusive holdings, notable private-fund positions, Central Huijin/China Securities Finance, a social-security float-holder summary, China Development Bank/Wutong/Zhongke Huitong holdings, and disclosed stake-building events; ratios and source units are explicit and raw rows remain auditable.";
    return result;
}

}  // namespace tdx::institution_analysis_detail
