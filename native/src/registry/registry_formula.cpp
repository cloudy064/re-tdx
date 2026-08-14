#include "tdx/registry_internal.hpp"

#include "tdx/cloud_calc.hpp"
#include "tdx/external_series.hpp"
#include "tdx/external_signals.hpp"
#include "tdx/formula_calc.hpp"
#include "tdx/formula_engine.hpp"
#include "tdx/formula_strategy.hpp"
#include "tdx/formulas.hpp"
#include "tdx/local_signals.hpp"
#include "tdx/tpool.hpp"

namespace tdx::registry_detail {

// Standalone formula resources, calculation, strategy and pool commands.
void append_formula_commands(std::vector<CommandSpec>& commands) {
    commands.insert(commands.end(), {
        {"formulas extract", "内置公式资源", "公式与指标",
         "导出随程序发布并校验哈希的四类公式签名与完整正文；不读取或加载 TCalc.dll。",
         false, "/api/v1/formulas", command_formulas_extract},
        {"formulas icons", "内置绘图图标", "公式与指标",
         "导出随程序发布并校验哈希的 DRAWICON 精灵图及清单；不读取或加载 TCalc.dll。",
         false, "/api/v1/formulas/icons",
         command_formulas_icons},
        {"formulas user-library", "用户公式只读库", "公式与指标",
         "只读解析 PriGS.dat，恢复用户公式元数据与源码；可输出兼容库，或由各解释器工作流通过 --include-user 直接装载，不加载 TCalc.dll、不写回用户目录。",
         false, "CLI only", command_formulas_user_library},
        {"formulas extern-signals", "公式外部信号表", "公式与指标",
         "只读解析 TdxW 的 signals/extern_user.txt 与 extern_sys.txt，并按证券和外部编号复现 EXTERNVALUE/EXTERNSTR 查找。",
         false, "CLI only", command_formulas_external_signals},
        {"formulas extdata-user", "公式用户扩展序列", "公式与指标",
         "只读解析 TdxW 的 extdata_N.idx/.dat，按证券导出 EXTDATA_USER 日期、时间和值序列。",
         false, "CLI only", command_formulas_external_series},
        {"formulas local-signals", "公式本地信号序列", "公式与指标",
         "只读解析 TdxW 的 datacfg、signals_sys_N.dat 与 signals_user_N 目录，导出 SIGNALS_SYS/SIGNALS_USER 本地序列。",
         false, "CLI only", command_formulas_local_signals},
        {"formulas calculate", "原生指标计算", "公式与指标",
         "下载多周期 K 线并以纯 C++ 计算 26 类 TCalc 兼容高价值指标。",
         true, "/api/v1/formulas/calculate", command_formulas_calculate},
        {"formulas analyze", "公式覆盖与依赖分析", "公式与指标",
         "解析已恢复公式源码，标记可执行覆盖、外部依赖、未来函数和绘图 IR 能力。",
         false, "/api/v1/formulas/coverage", command_formulas_analyze},
        {"formulas context-template", "公式外部上下文模板", "公式与指标",
         "为券商私有、合法 L2 序列或调用方宿主 raw 标量生成精确的显式上下文模板；纯标量模板跳过 K 线获取，且不获取、推导或伪造受限数据。",
         false, "/api/v1/formulas/context-template", command_formulas_context_template},
        {"formulas context-import", "公式授权捕获导入", "公式与指标",
         "把调用方合法持有的逐时间戳序列和宿主 raw 标量严格对齐到显式上下文模板；仅做离线校验与物化，不调用 SDK、订阅、账户或委托。",
         false, "/api/v1/formulas/context-import", command_formulas_context_import},
        {"formulas audit", "公式运行兼容体检", "公式与指标",
         "在同一组 K 线上执行全部可运行公式，汇总运行错误、空输出与末值覆盖。",
         false, "/api/v1/formulas/audit", command_formulas_audit},
        {"formulas cloud-calc", "TBigData 计算列解释器", "公式与指标",
         "审计 cloud_cfg 的 calc/calcref 与 syscol/refzqdm，自动生成最小输入模板，纯 C++ 复算单行或 1—128 行榜单；批量 HTTP 共享去重后的公开 L1/财务计划，并支持离线快照、本地行业层级及原生板块聚合补宿主字段。",
         true, "/api/v1/formulas/cloud-calc", command_formulas_cloud_calc},
        {"formulas evaluate", "通达信公式解释器", "公式与指标",
         "直接执行已恢复的通达信公式源码，支持参数、序列运算、IVOLAT 跨证券上下文及稀疏绘图 IR。",
         true, "/api/v1/formulas/evaluate", command_formulas_evaluate},
        {"formulas scan", "全市场条件选股", "公式与指标",
         "以解释器并发扫描内置或自定义源码，支持本地、指定证券或全 A 股 K 线、缓存、最近周期触发和严格财务披露时点。",
         true, "/api/v1/formulas/scan", command_formulas_scan},
        {"formulas watch", "条件公式策略池告警", "公式与指标",
         "周期重算内置或自定义条件公式，原子恢复活跃成员，输出进入、退出、信号更新 JSONL，并可显式导出通达信 .blk。",
         true, "/api/v1/formulas/scan", command_formulas_watch},
        {"formulas strategy", "多公式组合策略", "公式与指标",
         "在同一证券同一日期时间 K 线上组合多个条件公式，支持 all/any/at-least 扫描、下一开盘等权组合回测和逐股收益归因。",
         true, "/api/v1/formulas/strategy/scan, /api/v1/formulas/strategy/backtest", command_formulas_strategy},
        {"formulas backtest", "专家公式回测", "公式与指标",
         "按收盘信号、下一根开盘成交回测 ENTERLONG/EXITLONG，计入佣金、滑点、最大回撤及严格财务披露时点。",
         true, "/api/v1/formulas/backtest", command_formulas_backtest},
        {"pool inspect", "TPool 股票池诊断", "公式与指标",
         "只读解析 TPool XML，导出流程、公式规则、证券列表和原生执行兼容性；HTTP 投影仅返回 TDX 根相对路径，不回显服务器绝对根目录。",
         false, "/api/v1/pools", command_pool_inspect},
        {"pool history", "TPool 原生日历史", "公式与指标",
         "只读解析 TPool 的 .dat/.log 与原版 *_his.txt，并支持 GBK 管道文本双向转换；不写回原目录。",
         false, "/api/v1/pools/history", command_pool_history},
        {"pool evaluate", "TPool 规则本地计算", "公式与指标",
         "按单元格下载 K 线，以纯 C++ 计算比较、交叉、拐点、过滤、排名和只读流程投影；HTTP 可读取安装目录内 XML 或接收不落盘的内联 XML。",
         true, "/api/v1/pools/evaluate", command_pool_evaluate},
        {"pool watch", "TPool 持续告警", "公式与指标",
         "周期重算 TPool，推进多节点/带环流程并输出增量事件；可用独立 JSON 原子续跑，不写回通达信。",
         true, "CLI only", command_pool_watch}
    });
}

}  // namespace tdx::registry_detail
