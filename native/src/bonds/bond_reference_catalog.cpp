#include "bond_reference_internal.hpp"

namespace tdx {
namespace {

struct SourceLiteral {
    std::string_view group;
    std::string_view bucket;
    std::string_view name;
    std::string_view resource;
};

constexpr std::array<SourceLiteral, 24> source_literals{{
    {"rating", "aaa", "AAA级", "list/zq_aaa201.jsn"},
    {"rating", "aa-plus", "AA+级", "list/zq_aaj201.jsn"},
    {"rating", "aa", "AA级", "list/zq_aa201.jsn"},
    {"rating", "aa-minus", "AA-级", "list/zq_aajj201.jsn"},
    {"rating", "a-plus", "A+级", "list/zq_aj201.jsn"},
    {"rating", "a", "A级", "list/zq_a201.jsn"},
    {"rating", "a-minus", "A-级", "list/zq_ajj201.jsn"},
    {"rating", "a-minus-1", "A-1级", "list/zq_a1201.jsn"},
    {"rating", "bbb-plus-or-lower", "BBB+级及以下", "list/zq_bbbjjyx201.jsn"},
    {"rate", "fixed", "固定利率", "list/zq_gdll201.jsn"},
    {"rate", "floating", "浮动利率", "list/zq_fdll201.jsn"},
    {"rate", "progressive", "累进利率", "list/zq_djll201.jsn"},
    {"rate", "principal-at-maturity", "利随本清", "list/zq_lsbq201.jsn"},
    {"rate", "discount", "贴现", "list/zq_tx201.jsn"},
    {"rate", "other", "其他类型", "list/zq_qtlx.jsn"},
    {"category", "all", "全市场债券", "list/zq_zqqb201.jsn"},
    {"category", "government", "国债", "list/zqgz201.jsn"},
    {"category", "local-government", "地方债", "list/zqdfzfz201.jsn"},
    {"category", "corporate", "公司债", "list/zqgsz201.jsn"},
    {"category", "enterprise", "企业债", "list/zqqyz201.jsn"},
    {"category", "private", "私募债", "list/gxjty_zq_smz101_1.jsn"},
    {"category", "asset-backed", "资产支持证券", "list/gxjty_zq_zczczq101_1.jsn"},
    {"category", "recent-convertible", "次新可转债", "list/gxjty_zq_cxkzz101_1.jsn"},
    {"category", "policy-financial", "政策性金融债", "list/zqjrz201.jsn"},
}};

constexpr bool unique_sources() {
    for (std::size_t left = 0; left < source_literals.size(); ++left)
        for (std::size_t right = left + 1; right < source_literals.size(); ++right)
            if (source_literals[left].resource == source_literals[right].resource ||
                (source_literals[left].group == source_literals[right].group &&
                 source_literals[left].bucket == source_literals[right].bucket))
                return false;
    return true;
}

static_assert(unique_sources());

}  // namespace

const std::vector<BondReferenceSource>& bond_reference_sources() {
    static const auto sources = [] {
        std::vector<BondReferenceSource> result;
        result.reserve(source_literals.size());
        for (const auto& source : source_literals)
            result.push_back({std::string(source.group), std::string(source.bucket),
                              std::string(source.name), std::string(source.resource)});
        return result;
    }();
    return sources;
}

namespace bond_reference_detail {
namespace {

constexpr std::array<ResourceProfile, 20> resource_profiles{{
    {"list/zqjrz201.jsn", true, ScaleSemantics::issue_100m_yuan},
    {"list/zqdfzfz201.jsn", true, ScaleSemantics::client_master_hidden_unit},
    {"list/zqgsz201.jsn", true, ScaleSemantics::client_master_hidden_unit},
    {"list/zqgz201.jsn", true, ScaleSemantics::client_master_hidden_unit},
    {"list/zqqyz201.jsn", true, ScaleSemantics::client_master_hidden_unit},
    {"list/zqsmz201.jsn", true, ScaleSemantics::client_master_hidden_unit},
    {"list/zqzczczq201.jsn", true, ScaleSemantics::client_master_hidden_unit},
    {"list/zq_dfzfz201_1.jsn", false, ScaleSemantics::outstanding_100m_yuan},
    {"list/zq_dfzfz201_2.jsn", false, ScaleSemantics::outstanding_100m_yuan},
    {"list/zq_gsz201_1.jsn", false, ScaleSemantics::outstanding_100m_yuan},
    {"list/zq_gsz201_2.jsn", false, ScaleSemantics::outstanding_100m_yuan},
    {"list/zq_gz201_1.jsn", false, ScaleSemantics::outstanding_100m_yuan},
    {"list/zq_gz201_2.jsn", false, ScaleSemantics::outstanding_100m_yuan},
    {"list/zq_qyz201_1.jsn", false, ScaleSemantics::outstanding_100m_yuan},
    {"list/zq_qyz201_2.jsn", false, ScaleSemantics::outstanding_100m_yuan},
    {"list/zq_smz201_1.jsn", false, ScaleSemantics::outstanding_100m_yuan},
    {"list/zq_smz201_2.jsn", false, ScaleSemantics::outstanding_100m_yuan},
    {"list/zq_zczczq201_2.jsn", false, ScaleSemantics::outstanding_100m_yuan},
    {"list/zq_jrz201_1.jsn", false, ScaleSemantics::issue_100m_yuan},
    {"list/zq_jrz201_2.jsn", false, ScaleSemantics::issue_100m_yuan},
}};

constexpr bool unique_profiles() {
    for (std::size_t left = 0; left < resource_profiles.size(); ++left)
        for (std::size_t right = left + 1; right < resource_profiles.size(); ++right)
            if (resource_profiles[left].resource == resource_profiles[right].resource)
                return false;
    return true;
}

static_assert(unique_profiles());

}  // namespace

ResourceProfile resource_profile(const BondReferenceSource& source) {
    const auto found = std::find_if(resource_profiles.begin(), resource_profiles.end(),
        [&](const ResourceProfile& profile) {
            return profile.resource == source.resource;
        });
    return found == resource_profiles.end()
        ? ResourceProfile{source.resource, false, ScaleSemantics::issue_yuan}
        : *found;
}

const std::vector<BondReferenceSource>& category_projections(
    const std::string& bucket) {
    static const std::map<std::string, std::vector<BondReferenceSource>> plans{
        {"all", {{"projection", "all-sh", "全部债券·沪市投影", "list/zq_qbzq201_1.jsn"},
                 {"projection", "all-sz", "全部债券·深市投影", "list/zq_qbzq201_2.jsn"}}},
        {"government", {{"projection", "government-sh", "国债·沪市投影", "list/zq_gz201_1.jsn"},
                        {"projection", "government-sz", "国债·深市投影", "list/zq_gz201_2.jsn"}}},
        {"local-government", {{"projection", "local-government-sh", "地方政府债·沪市投影", "list/zq_dfzfz201_1.jsn"},
                              {"projection", "local-government-sz", "地方政府债·深市投影", "list/zq_dfzfz201_2.jsn"}}},
        {"corporate", {{"projection", "corporate-sh", "公司债·沪市投影", "list/zq_gsz201_1.jsn"},
                       {"projection", "corporate-sz", "公司债·深市投影", "list/zq_gsz201_2.jsn"}}},
        {"enterprise", {{"projection", "enterprise-sh", "企业债·沪市投影", "list/zq_qyz201_1.jsn"},
                        {"projection", "enterprise-sz", "企业债·深市投影", "list/zq_qyz201_2.jsn"}}},
        {"private", {{"projection", "private-sh", "私募债·沪市投影", "list/zq_smz201_1.jsn"},
                     {"projection", "private-sz", "私募债·深市投影", "list/zq_smz201_2.jsn"}}},
        {"asset-backed", {{"projection", "asset-backed-sz", "资产支持证券·深市投影", "list/zq_zczczq201_2.jsn"}}},
        {"policy-financial", {{"projection", "policy-financial-sh", "政策性金融债·沪市投影", "list/zq_jrz201_1.jsn"},
                              {"projection", "policy-financial-sz", "政策性金融债·深市投影", "list/zq_jrz201_2.jsn"}}},
    };
    static const std::vector<BondReferenceSource> empty;
    const auto found = plans.find(bucket);
    return found == plans.end() ? empty : found->second;
}

const std::optional<BondReferenceSource>& category_client_master(
    const std::string& bucket) {
    static const std::map<std::string, std::optional<BondReferenceSource>> plans{
        {"private", BondReferenceSource{"comparison", "private-client-master",
            "客户端私募债合并表", "list/zqsmz201.jsn"}},
        {"asset-backed", BondReferenceSource{"comparison", "asset-backed-client-master",
            "客户端资产支持证券合并表", "list/zqzczczq201.jsn"}},
    };
    static const std::optional<BondReferenceSource> empty;
    const auto found = plans.find(bucket);
    return found == plans.end() ? empty : found->second;
}

}  // namespace bond_reference_detail
}  // namespace tdx
