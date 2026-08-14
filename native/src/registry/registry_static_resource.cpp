#include "tdx/registry_internal.hpp"

#include "tdx/hyzt.hpp"
#include "tdx/image_data.hpp"
#include "tdx/jsn.hpp"
#include "tdx/jsn_data.hpp"
#include "tdx/jsn_variants.hpp"

namespace tdx::registry_detail {

// Local image data, HYZT and the JSN static-resource family.
void append_static_resource_commands(std::vector<CommandSpec>& commands) {
    commands.insert(commands.end(), {
        {"image-data decode", "本地盘口快照解码", "本地行情",
         "纯 C++ 解压 zst_cache 的 img/TCK 包装并恢复 1032 字节盘口快照、十档和买卖一委托队列。",
         false, "CLI only", command_image_data_decode},
        {"hyzt extract", "行业主题关系", "板块与行业",
         "生成行业层级、股票所属行业、主题及其股票反向索引。",
         false, "/api/v1/industry/tree, /api/v1/jsn/security", command_hyzt_extract},
        {"jsn catalog", "JSN 资源编目", "云端静态资源",
         "离线扫描全部已下载 JSN，统计表结构、行数、证券和成员覆盖。",
         false, "/api/v1/jsn/catalog", command_jsn_catalog},
        {"recon jsn-variants", "JSN 语义覆盖审计", "系统",
         "归并 XML/CFG 静态与动态资源模板，核对下载状态和类型化 C++ 业务命令覆盖。",
         false, "/api/v1/jsn/variants", command_recon_jsn_variants},
        {"recon jsn-discovery", "JSN 增量能力发现", "系统",
         "为已下载 JSN 建立指纹基线，提取动态键与字段画像，并按新增、变化、移除和类型化覆盖排序。",
         false, "/api/v1/jsn/discovery", command_recon_jsn_discovery},
        {"jsn candidates", "JSN 动态候选队列", "云端静态资源",
         "沿 CFG refunit 主从关系从真实主表行生成动态资源键；默认离线，显式限定资源族后可做有界元数据探测。",
         true, "/api/v1/jsn/candidates", command_jsn_candidates},
        {"jsn download", "JSN 资源下载", "云端静态资源",
         "通过 7709 的 709/1721 查询、分片下载并校验 JSN。",
         true, "/api/v1/jsn/resource", command_jsn_download},
        {"jsn query", "按股票查询 JSN", "云端静态资源",
         "按证券横向查询全部 JSN 中的直接、引用和成员关系。",
         false, "/api/v1/jsn/security", command_jsn_query}
    });
}

}  // namespace tdx::registry_detail
