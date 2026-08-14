# 引擎分析

> 行情引擎、协议、公开 API 的逆向分析文档。

## 当前文档

- [UserComm 网络传输层](01-network-layer.md)：两套 Asio DLL、IOCP/socket
  锚点、15 槽虚表和 `TdxW.exe` 连接/收发调用路径。
- [应用消息与数据插件层](02-application-data-layer.md)：`tpbus` →
  `TaApi`、`FastHQ.Subscribe`、`TDataParse`、`TBigData` 和 `TPool`
  的真实调用关系。
- [认证与加密辅助层](03-auth-crypto-layer.md)：Blowfish、Base64、RSA、
  HTTPS POST、TJyaid 设备指纹和主程序调用场景。
- [TCalc 公式与指标引擎](04-formula-engine.md)：四类公开公式、5072 字节
  记录、分类树、来源标志及离线/运行时只读导出。
- [板块目录与证券成员关系](05-block-membership.md)：通达信行业、研究行业、
  概念、风格和指数板块，以及离线导出的板块—证券关系。
- [板块指数 1 分钟 K 线](06-minute-kline.md)：`.lc1` 的 32 字节 OHLCV
  记录、板块市场映射、离线图表和盘后/TQ 刷新路径。
- [可产品化的直连行情功能](07-useful-live-features.md)：批量快照、板块
  广度、分时、竞价、成交明细和多周期 K 线的真实主站验证，以及建议的
  板块雷达推进顺序。
- [客户端功能面与高价值数据入口](08-client-feature-surface.md)：从当前
  安装的启用云配置恢复 49 个服务 Entry 和 63 个 ReqId，验证因子、异动、
  F10、财务、股本变迁等功能，并闭合 PBRPC 竞价、资金、选股模型、
  九转、压力支撑和龙虎榜入口。
- [JSN 静态资源协议](09-jsn-resource-protocol.md)：`reqformat=11` 的
  TdxW 调度、7709 `709/1721` 文件协议，以及行业树、主题和证券关系
  的在线更新工具。
- [TBigData 计算列与债券函数](10-tbigdata-cloud-calculations.md)：
  `calc/calcref` 结构、36 项内建注册表、当前 1370 条计算列审计，以及纯
  C++ 的依赖排序和逐行复算。
- [TDataParse 本地盘口快照结构](../03-memory-layout/01-image-data-snapshot.md)：
  24 字节 zlib 包装、增量控制流、1032 字节快照和原 DLL 差分验证。
- [TTPlugin 批量行情与状态记录](../03-memory-layout/02-ttplugin-batch-quote.md)：
  期货/扩展市场 9221 批量包、zlib 帧和 712 字节状态缓存。

## 待填充

- `FastHQ.Subscribe` 的最终 `LX` 枚举标定；接收 `111/112` 原始字段布局
  已恢复；
- Level2 数据流。

当前安装没有发现此前推断的 `TPyth.dll`，不再把 Python 插件作为既定
分析目标。
