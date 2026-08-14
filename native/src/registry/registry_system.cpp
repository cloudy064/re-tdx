#include "tdx/registry_internal.hpp"

#include "tdx/minute.hpp"
#include "tdx/recon.hpp"
#include "tdx/server.hpp"
#include "tdx/session_audit.hpp"
#include "tdx/ttplugin_redirect.hpp"
#include "tdx/ttplugin_servers.hpp"

namespace tdx::registry_detail {

// Minute K-line, install/session recon and the local HTTP server.
void append_system_commands(std::vector<CommandSpec>& commands) {
    commands.insert(commands.end(), {
        {"minute download", "一分钟线下载", "K 线",
         "通过 0x052D 分页下载一分钟线，合并并生成 LC1/JSON/CSV。",
         true, "/api/v1/minute?source=online", command_minute_download},
        {"minute extract", "本地一分钟线", "K 线",
         "解析本地 LC1 分钟缓存并按日期导出 JSON/CSV。",
         false, "/api/v1/minute?source=local", command_minute_extract},
        {"recon install", "安装目录盘点", "系统",
         "只读盘点安装目录中的 EXE、DLL、大小和版本证据。",
         false, "/api/v1/install", command_recon_install},
        {"recon runtime-topology", "运行拓扑快照", "系统",
         "只读枚举 TdxW 进程、父子关系、本地模块和 TCP 端点，并可与前一份快照比较；不注入、不读取进程内存。",
         false, "CLI only", command_recon_runtime_topology},
        {"recon session-config", "会话与服务器配置审计", "系统",
         "只读盘点 connect.cfg 公开服务器组、tpbus 会话默认值，并仅报告本地私密会话字段的存在性和长度。",
         false, "CLI only", command_recon_session_config},
        {"recon ttplugin-redirect", "TTPlugin 重定向协议契约", "系统",
         "离线输出 RedirectData 的六种请求结构、响应字段与任务桥接链，并可把调用方 JSON 严格编码为旧协议请求体；不加载 DLL、不联网。",
         false, "CLI only", command_recon_ttplugin_redirect},
        {"recon ttplugin-servers", "TTPlugin 服务器配置审计", "系统",
         "离线解析扩展行情/交易服务器 URL、直连与 HTTP/SOCKS 代理结构；凭据和 TDXProxy 内容始终脱敏，不加载 DLL、不联网。",
         false, "CLI only", command_recon_ttplugin_servers},
        {"recon api-contracts", "固定 API 契约巡检", "系统",
         "巡检健康状态、功能/OpenAPI、正常空关系、参数拒绝和报告期缓存隔离，输出可重复的结构化证据。",
         true, "CLI only", command_recon_api_contracts},
        {"serve", "网页与只读 API", "系统",
         "启动纯 C++ 本地 HTTP 服务，为网页提供功能目录、板块和指标查询接口。",
         false, "/api/v1/features", command_serve}
    });
}

}  // namespace tdx::registry_detail
