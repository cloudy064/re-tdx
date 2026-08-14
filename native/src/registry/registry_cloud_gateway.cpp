#include "tdx/registry_internal.hpp"

#include "tdx/blocks.hpp"
#include "tdx/cloud_routes.hpp"
#include "tdx/cloud_variants.hpp"
#include "tdx/cloud_workflow.hpp"
#include "tdx/pbrpc.hpp"
#include "tdx/recon.hpp"
#include "tdx/tqlex.hpp"

namespace tdx::registry_detail {

// Cloud gateways, block export/query and the environment doctor.
void append_cloud_gateway_commands(std::vector<CommandSpec>& commands) {
    commands.insert(commands.end(), {
        {"cloud workflow", "云端主从工作流", "云端查询",
         "自动执行 TQLEX/PBRPC 主表、字段映射和明细请求，保留完整关联证据。",
         true, "/api/v1/cloud/workflows, /api/v1/cloud/workflow", command_cloud_workflow},
        {"cloud routes", "旧云路由诊断", "云端查询",
         "盘点 reqformat=1 服务名，关联 2/11/20/22 同名路由并检查 TPData 位数与消息循环边界。",
         false, "/api/v1/cloud/routes", command_cloud_routes},
        {"recon cloud-variants", "云模板参数变体覆盖", "系统",
         "将 TQLEX/PBRPC 模板按规范请求体去重，并核对每个语义变体是否已有类型化 C++ 业务命令。",
         false, "/api/v1/cloud/variants", command_recon_cloud_variants},
        {"cloud pbrpc", "PBRPC 云查询", "云端查询",
         "扫描 reqformat=22 配置，编码 protobuf 外层并在可下调的 max_assembled_bytes 预算内完成 RpcID 分段传输。",
         true, "/api/v1/pbrpc/configs, /api/v1/pbrpc/query", command_cloud_pbrpc},
        {"cloud tqlex", "TQLEX JSON 查询", "云端查询",
         "扫描 reqformat=2 配置，展开模板并通过原生 HTTP 客户端分页查询 JSON 服务。",
         true, "/api/v1/tqlex/configs, /api/v1/tqlex/query", command_cloud_tqlex},
        {"blocks export", "板块数据导出", "板块与行业",
         "导出多级行业、研究行业、概念、风格、指数及成分关系。",
         false, "/api/v1/blocks", command_blocks_export},
        {"blocks query", "板块与股票查询", "板块与行业",
         "查询板块下的股票，或反查一只股票所属的全部板块。",
         false, "/api/v1/blocks, /api/v1/securities/blocks", command_blocks_query},
        {"doctor", "环境诊断", "系统",
         "检查纯原生运行状态、通达信安装目录和关键数据目录。",
         false, "/api/v1/health", command_doctor}
    });
}

}  // namespace tdx::registry_detail
