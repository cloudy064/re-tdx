# 纯 C 工程评审修复与一致性归档

## 目标与范围

基于 `feat/c-l1-stream` 的 `255541cfc4d381759e014bcd05e89175fdfa1b5b`，完成纯 C 实现的十组工程修复，并将代码、公开契约、文档和验证结果一起归档。
范围为 `c/`、C 的 GitHub CI 工作流及相关文档入口。C++ 源码和既有独立研究脚本不属于此次提交。

逐项实现与测试入口见 [修复清单](../../c/docs/engineering-review-fixes.md)，所有权、模块关系和输出兼容边界见 [工程约定](../../c/docs/engineering.md)。

## 已落地的边界

- 服务器实例拥有连接与线程；停止时中断连接、等待 handler，然后销毁 Hub、连接池和 fetch context。
- 缓冲和文档先初始化、后复用、最终释放；重复请求不再覆盖既有内存所有权。
- JSON/JSN 使用有界解析与按长度转义，领域 renderer 独立于命令编排；输入中的重复列名确定性消歧。
- SSE 按证券合并最新完整状态；缓存快照公开采集时间、年龄和过期状态。连续失败使用有界退避。
- CLI 从 5,196 行入口拆为 38 行入口、类型化参数注册表和各命令模块；底层分为 base、protocol、domain、runtime。
- 日期统一真实日历校验，显式日期支持八位和带横线拼写；百分数字段的模型与输出单位一致。
- 缓存先解码和校验身份，再通过独占临时文件原子替换；区分缺失与 I/O 失败。
- 生成器、35 份最小离线输入、来源及哈希随仓库保存，可复现 21 个生成产物。

## 一致性复核

此次归档检查覆盖公开头文件、CLI 参数、README、工程说明、CMake 目标、测试注册、生成输入、Git 属性和 CI 配置。
保留历史实验日期及计数，不把旧样本数字改写为当前全量事实。现行入口明确 C 工程与历史 C++ 研究结论各自的范围。

SSE 的 `wait_timeout_ms` 是等待事件后发送保活注释的间隔，并非空闲断开时限；`polled` 是裁剪后的候选证券数，含尚未到期的冷档。
新订阅的缓存记录先进入队列，但未发送的初始快照可以被更新的完整状态合并；接口不保证无损逐笔或每次订阅都观察到独立的 snapshot 事件。

另修正 `serve --interval-ms` 的参数下限为 1，与 Hub 的校验一致，避免先创建资源再拒绝 0；`watch` 保留 0 表示连续轮询。
服务器写超时、队列容量和销毁前排空使用者的公开注释也与实现对齐。

输出兼容边界包括：snapshot/status 增加新鲜度与恢复计数，JSN 冲突列改名，非法日期或非法数字输入被拒绝，非有限输出按领域契约为 null 或原始文本。
缓冲和文档输出参数必须先初始化；`tdxl1core` 保留为 CMake 聚合 target，独立库的链接关系见工程说明。

## 已执行的验证与证据

本轮涉及共享解析器、缓存和构建边界，已按仓库规则执行全套 CTest。

| 环境/检查 | 结果 |
|---|---|
| Windows MinGW GCC 15.1.0 | 51/51，5.44 秒 |
| Windows MSVC 19.50 | 51/51，6.68 秒 |
| Linux Clang 22.1.3，ASan/UBSan，启用泄漏检测 | 51/51，2.85 秒，无 sanitizer 报告 |
| Clang libFuzzer 的 JSON/JSN/frame 入口 | 20,000 次运行完成 |
| 一次 7709 真实行情抽样 | SZ000001，1/1 记录，买卖各五档，独立 JSON 解析通过 |
| 离线生成一致性 | 21 个产物、35 个输入，漂移 0 |

CTests 包含独立 Python JSON 检查、五项离线 CLI 契约、真实回环 HTTP/SSE、Hub 生命周期、缓存失败注入和连接池复用。
Linux 验证使用独立构建目录，随后强制刷新同步源码时间并重编，避免 tar 保留旧时间造成旧二进制被误认为当前结果；收尾 renderer 与 CLI 测试源码哈希也已核对一致。

原始执行日志保留在忽略目录 `output/`。为便于核对，归档以下文件字节数与 SHA-256；上表只登记统计与哈希，不上传本地执行日志或认证材料。
随测试入库的最小业务输入来自既有夹具和保留样本，来源与哈希见 [输入清单](../../c/tests/fixtures/sources.json)，不视为新的独立抓包。

| 本地证据文件 | 字节数 | SHA-256 |
|---|---:|---|
| `c-final-mingw-tests.txt` | 5857 | `57c915e7c9f6fa8bd09ffd93dcdb5c560e4e1f96a34398d4ed4dd966ae8bb6bc` |
| `c-final-msvc-tests.txt` | 5873 | `e840b814aa2c4dbb820139d6d47f00c6b9de7ddb745f16aababce788b887b5fc` |
| `c-final-linux-evidence.txt` | 6359 | `c0ff6740fd829a51eaae9fe2c3bf91ddfdd7e6c03e6151961d7f3f7cc0cc417d` |
| `c-engineering-live.json` | 806 | `c8aec99229c7bb79f537e362fddd351ce56a015f31347fc2fd3a4d74fdb83092` |

## 重现与遗留边界

构建依赖 CMake、C11 编译器、zlib；预设使用 Ninja。Python 用于生成和独立验证，生产程序不依赖 Python。
在 `c/` 下运行：

```sh
cmake --preset linux-clang-sanitize
cmake --build --preset linux-clang-sanitize
ctest --preset linux-clang-sanitize
python tools/generate.py --check
```

Windows 替换为 `windows-mingw` 或 `windows-msvc`，MSVC 的 zlib 配置见 [构建说明](../../c/tools/README.md)。
GitHub CI 已配置，本次归档中的测试是本地 Windows 与 SSH Linux 的实测，不能据此宣称 GitHub workflow 已执行。
真实样本缺失仍只跳过对应样本，无法取代覆盖所有市场和交易时段的长期运行验证；SSE 保持最新状态交付契约。

上表的全量结果来自工程修复收尾；本次归档新增的参数下限修正另做定向验证，不重复已通过的全量测试：

- MinGW、MSVC：增量构建 CLI 后，`cli / cli-contracts / hub / hub_lifecycle / serve` 各 5/5 通过。
- Linux Clang ASan/UBSan：增量构建 CLI 后，`cli / cli-contracts` 2/2 通过，无 sanitizer 报告。
- 从 Git 暂存区单独导出 342 个 C/workflow 文件，再次校验 21 个产物及 35 个输入，漂移 0；目录不含本机终端、构建产物或 C++ 源码。
- 新增与修改文档的本地链接、CLI 示例和测试注册一致；暂存变更的 whitespace 检查通过。

冻结的 fixture 输入使用 binary 属性保留精确字节；只清理源码中的末尾空行和生成器的多余空白，不改写输入哈希。

## PR 阶段的 Windows CI 差异

归档提交后的 [首轮 GitHub CI](https://github.com/cloudy064/re-tdx/actions/runs/35699022161) 中，Linux GCC 和 Clang sanitizer 均通过；
Windows MinGW/MSVC 的前 50 项 CTest 通过，最后一项生成一致性检查失败，差异仅在财务 ZIP 夹具。

对 `e2d08b3` 的原生成器进行交叉运行，已精确复现：本机 Python 3.9 使用 zlib，而官方 Windows Python 3.14.4 使用 `1.3.1.zlib-ng`。
相同的 1642 字节财务内容、固定时间和 ZIP 元数据，stored ZIP 均为 1772 字节且逐字节相同；deflate ZIP 从 812 字节变为 943 字节。
内容解压后相同，漂移来自压缩库输出，并非业务解码错误。这也说明同一 runner 上前置 `python` 与 CMake 发现的解释器可能产生不同生成结果。

后续修复已将 method 8 的压缩位流改为确定性的固定 Huffman/LZ77 编码，并继续用 Python zipfile/zlib 独立读回验证。
保留真实压缩、CRC 损坏和缺失 EOCD 的测试覆盖。Python 3.9/zlib 与 Windows Python 3.14.4/zlib-ng 的生成文件逐字节一致，
deflate ZIP 均为 1179 字节；21 个产物与 35 个输入的生成检查无漂移，MinGW 财务包、生成和 CLI 契约 3/3 通过。
PR 的远端检查状态以 [PR #2](https://github.com/cloudy064/re-tdx/pull/2) 为准。
