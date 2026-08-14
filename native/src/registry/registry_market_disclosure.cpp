#include "tdx/registry_internal.hpp"

#include "tdx/active_funds.hpp"
#include "tdx/active_lhb.hpp"
#include "tdx/disclosures.hpp"
#include "tdx/forecasts.hpp"
#include "tdx/foreign_alerts.hpp"
#include "tdx/futures_issuance.hpp"
#include "tdx/industry_profile.hpp"
#include "tdx/institution.hpp"
#include "tdx/institution_analysis.hpp"
#include "tdx/institution_lhb.hpp"
#include "tdx/state_owned_reform.hpp"

namespace tdx::registry_detail {

// Issuance, forecasts, institutional research and industry profiles.
void append_market_disclosure_commands(std::vector<CommandSpec>& commands) {
    commands.insert(commands.end(), {
        {"market futures-issuance", "期货、配股与发行统计", "公开资料",
         "聚合商品/股指期货、IPO 与债券发行人，并覆盖定向增发、配股生命周期和优先股发行/股息条款。",
         true, "/api/v1/market/futures-issuance", command_market_futures_issuance},
        {"market active-funds", "主动基金增持", "公开资料",
         "读取基金季报股票持仓汇总，并按股票动态展开持有基金、仓位占比和持仓排名。",
         true, "/api/v1/market/active-funds", command_market_active_funds},
        {"market forecasts", "全市场、行业与港股业绩预告", "公开资料",
         "聚合研究行业业绩预告统计，按行业报告期展开 A 股详情，并提供港股预告。",
         true, "/api/v1/market/forecasts", command_market_forecasts},
        {"market disclosures", "财报披露日历", "公开资料",
         "归档财报时点，支持批量续传回补、上市日感知覆盖审计，以及刷新到期队列的一键增量维护。",
         true, "/api/v1/market/disclosures, /api/v1/market/disclosures/archive",
         command_market_disclosures},
        {"market foreign-alerts", "外资持股预警", "公开资料",
         "读取当前外资持股预警股票，并按股票展开持股数量、占总股本比例和状态历史。",
         true, "/api/v1/market/foreign-alerts", command_market_foreign_alerts},
        {"market institution", "机构与股东关联", "公开资料",
         "聚合机构持仓与十大流通股东，并展开选定股东的跨股票和单票报告期历史。",
         true, "/api/v1/security/profile, /api/v1/market/holder", command_market_institution},
        {"market institution-analysis", "机构持仓全景", "公开资料",
         "类型化整合二十五张机构分类、养老金、浮筹、基金独门、汇金证金、国开/梧桐树/中科汇通持股与举牌披露，支持全市场与单票聚合。",
         true, "/api/v1/market/institution-analysis", command_market_institution_analysis},
        {"market institution-lhb", "机构龙虎统计", "公开资料",
         "聚合一周、一月、三月和一年机构席位买卖排行，并按股票展开异动日买卖总额。",
         true, "/api/v1/market/institution-lhb", command_market_institution_lhb},
        {"market active-lhb", "活跃龙虎榜", "公开资料",
         "聚合近5日、近一月和近半年龙虎榜上榜次数、累计买卖额与区间涨幅，并按股票展开历史异动。",
         true, "/api/v1/market/active-lhb", command_market_active_lhb},
        {"market state-owned-reform", "国企改革关系", "公开资料",
         "获取按行业、地区、整合预期和公司系组织的国企股票关系，展开实际控制人、控股比例、逻辑说明与重组预期。",
         true, "/api/v1/market/state-owned-reform", command_market_state_owned_reform},
        {"market industry-profile", "行业机构画像", "公开资料",
         "合并研究行业树、季度机构持仓历史、行业股东结构和逐股股东画像。",
         true, "/api/v1/market/industry-profile", command_market_industry_profile}
    });
}

}  // namespace tdx::registry_detail
