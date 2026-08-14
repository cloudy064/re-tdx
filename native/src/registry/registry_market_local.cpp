#include "tdx/registry_internal.hpp"

#include "tdx/investment.hpp"
#include "tdx/shape_match.hpp"

namespace tdx::registry_detail {

void append_market_local_commands(std::vector<CommandSpec>& commands) {
    commands.push_back({
        "market investment", "本地投资组合与交易费率", "客户端本地私有数据",
        "只读解密 invest.dll 的组合目录与文件完整性，解析 trdpara.dat 费率规则并复算交易费用；不输出密码或交易明细私有字段。",
        false, "", command_market_investment});
    commands.push_back({
        "market shape-match", "形态匹配模板与相似度", "本地研究工具",
        "读取 TDXDeep shapematch.dat 的证券/手绘模板，并按原生尾部 Pearson 相关与四分量权重对单证券 K 线评分；不加载原 DLL。",
        false, "", command_market_shape_match});
}

}  // namespace tdx::registry_detail
