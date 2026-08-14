# JSN refunit 动态候选队列

> 2026-08-12 更正：本阶段把 `unit id` 作为全目录 ID，导致同一类型化命令内的不同
> 客户端页面发生交叉匹配，GQZY 的 2,474 条也包含无页面关系的 GDRS 来源。当前实现已按
> CFG 族和 `.sp` 页面共同出现关系限定 `refunit`，详见
> [JSN 动态候选的 CFG 族与客户端页面作用域](2026-08-12-native-jsn-candidate-page-scope.md)。

## 目标

把上一轮“发现新增文件”的被动能力继续向前推进：从客户端自身的主从单元配置
推导尚未下载的动态路径，形成可解释、可筛选、可有界探测的队列。实现仍为纯
C++，不依赖 Python，不涉及 L2。

## 证据链与安全边界

当前 75 个动态模板只出现两种占位形式：41 个 `ZQDM`，34 个 `SC+ZQDM`。
新命令 `jsn candidates` 解析 CFG/XML `<unit>` 的 `id/file/refunit`，但不把所有
引用行直接视为安全候选，而是增加第二层业务约束：详情模板与引用主表必须属于
同一类型化 C++ 业务域。例如 `market institution-analysis` 会规范到
`market institution` 后与 `cgfxmx1/2` 对齐；不相关的通用页面引用只记为诊断，
不进入探测队列。

候选还必须满足：主表真实存在且能解析、要求的键列完整、键是安全 ASCII、展开
后满足 709 请求 40 字节路径上限。默认只输出本地缺失项且完全离线。

联网探测必须显式指定 `--family --probe`，`--max-probes` 范围为 1—25；只取大小
和 MD5，命令本身永不下载。下载继续使用已有 `jsn download --download`，保留
MD5 校验、临时文件和原子替换。

## 真实 GQZY 验证

`gqzy/SC+ZQDM` 由同属 `market ownership` 的五张本地主表提供证据：

- `func_gdrs101_1`；
- `func_gqzy101_1`；
- `func_gqzy102_1`；
- `func_gqzy103_1`；
- `func_gqzy109_1`。

它们生成 2,474 个去重具体路径。首批 10 个高证据候选探测结果为 10/10 非空，
文件大小为 3,084—22,604 字节。随后用既有原生下载命令保存这 10 个样本，合计
361 行；每个文件 16 列。增量发现相对 340 文件基线准确报告：

- 新增文件 10；
- 新字段出现次数 160；
- 当前文件 350；
- 模板识别 350/350；
- 类型化覆盖 350/350；
- 解析错误 0。

基线暂未覆盖更新，因此网页打开“资源变化”仍可直接看到这 10 个新增样本；用户
确认后可点“设为当前基线”。

## API 与网页

新增只读 API `/api/v1/jsn/candidates`，支持 `family/missing_only/limit/refresh`。
API 不提供批量联网探测；网页选中候选后仍复用现有单资源 GET 探测和确认 POST
下载，避免扩大写权限。服务端按筛选参数缓存候选结果，`gqzy` 首次约 1.0 秒，
缓存读取约 0.02 秒；下载后会同时失效候选与增量发现缓存。

Svelte JSN 页面顶部现在可切换：

- “资源变化”：基线差异和字段画像；
- “动态候选”：资源族筛选、动态键、来源主表、证据行和优先级。

候选详情侧栏展示具体键和来源主表，可直接探测或按现有确认流程下载。

## 跨资源族抽样

为排除规则只对 `gqzy` 偶然有效，又对候选量较小的 `dzjy1` 做了第二组只读
验证。该族由 `list/func_dzjy101_1.jsn` 产生 12 个候选，其中 1 个已有本地
样本、11 个缺失；前 5 个缺失项全部可用，大小为 3,522—3,881 字节。

这里也验证了一项重要边界：模板占位名是 `$ZQDM`，实际值却是 `2025-08`
这样的月份，而不是证券代码。候选器只把它当作来源字段键，不据名称臆测业务
语义；业务层仍由既有大宗交易服务解释为“月份→行业统计”。现有 C++ API 和
Svelte 页面本来就能按选中月份实时读取该资源，所以本轮不重复下载样本，也不
新增平行业务入口。探测证据保存为
`output/probes/jsn-candidates-dzjy1-probe.json`。

## 验收与部署

- C++：65/65 测试通过；
- Svelte：`svelte-check` 0 错误/0 警告，生产构建成功；
- 部署态 HTTP 契约：92/92；
- 功能目录：112 项；
- 服务：`http://127.0.0.1:8765`，PID `14616`；
- JSN 索引：350 个资源；
- 可执行文件 SHA-256：
  `8B7E91816EA34FB49F2C7282776CF263B44E5ED78BA9E47E06FDD641B70DF366`。

主要证据：

- `output/tdx-jsn-candidates.json`；
- `output/probes/jsn-candidates-gqzy-probe.json`；
- `output/probes/jsn-candidates-gqzy-download.json`；
- `output/probes/jsn-candidates-dzjy1-probe.json`；
- `output/probes/jsn-discovery-after-candidate-download.json`；
- `output/probes/jsn-variants-after-candidates-current.json`；
- `output/probes/api-contracts-after-jsn-candidates-current.json`。

## 下一步

按同样方法逐族做小样本收益率验证：优先挑选“候选量适中、主表证据集中、当前
本地实例少”的资源族，先探测不下载；只有非空率高且字段能补充现有业务模型时，
再下载少量样本并由增量发现自动做结构审计。空率高的资源族保留诊断，不进行
全量扫库。
