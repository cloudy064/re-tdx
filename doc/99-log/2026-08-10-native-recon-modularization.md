# 原生 API 契约巡检模块化

## 目标

消除 `native/src/recon.cpp` 同时承担安装盘点、契约目录、HTTP 请求模板、全部
响应断言和 CLI 输出造成的超大翻译单元，并保持公开 JSON/API 行为不变。

## 结果

- `recon.cpp` 从 12,967 行缩减到约 700 行，只保留安装盘点、HTTP 巡检编排和
  CLI；
- 216 余项契约的 ID、标题、路径和 profile 移入 `recon_contract_catalog.cpp`；
- 41 个静态 POST 请求正文和 `X-TDX-Action` 由
  `recon_contract_requests.cpp` 的只读映射表查找，删除重复的 ID 条件链；
- 公式契约与市场契约分离；市场契约再按行情/机构债券、主题研究、公司事件日历
  三个领域拆分，由固定策略表按顺序分派；
- JSON 断言辅助函数收敛在内部头文件，不改变 `tdx/recon.hpp` 公共接口。

拆分后最大的契约实现文件约 4,400 行；`recon.cpp` 不再因新增业务契约而线性
膨胀。仓库规则新增 5,000 行软阈值，以及编排、常量、验证和协议职责分离约定。

## 增量验证

- `tdx-recon-contract-tests`：通过；
- `tdx-formula-engine-tests`：通过；
- 未运行全量 CTest；候选服务抽查健康、RAND/评分公式、行情、主题研究和公司
  事件五项契约，5/5 通过。

本次测试同时补齐 `specgpext.txt` 评分字段夹具，覆盖 `SAFESCORE/SHINESCORE`
的本地上下文，不引入 Python 或外部数据库。

## 后续拆分结果

原有 `formula_engine.cpp`（约 9,100 行）和 `server.cpp`（约 6,100 行）超过
新的软阈值，因此分别优先抽出函数注册/运行时，以及后续的路由注册/参数解析。

随后已完成解释器首轮拆分：`formula_engine.cpp` 从 9,064 行降至约 4,800 行，
约 3,900 行的原生函数运行时迁入 `formula_functions.cpp`，函数/符号能力集合迁入
约 370 行的 `formula_registry.cpp`。共享类型和少量运行入口只在内部头文件暴露；
公式引擎与契约测试再次通过。
随后跨证券指标又独立为 `formula_nested.cpp`，指标目录查找和输出序号校验抽到
`formula_catalog_internal.cpp`；`INSORT/INSUM/CALCSTOCKINDEX` 不再各自维护
重复的目录规则。

`server.cpp` 的拆分也已完成：功能目录/OpenAPI 移入 `server_catalog.cpp`，公式、
云计算、TPool、扫描、策略和回测 HTTP 控制器移入 `server_formula.cpp`，总控文件
从约 6,100 行降至约 4,700 行。控制器只接收窄化的 `FormulaHttpState`，不公开
新的公共 C++ API。受影响构建、自检及 4 个候选接口均通过，未运行全量测试。
