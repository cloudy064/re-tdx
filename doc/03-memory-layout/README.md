# 内存布局

> 证券主表、订阅列表和历史内存结构证据。

当前已经闭合第一种定长行情结构：

- [TDataParse 本地盘口快照结构](01-image-data-snapshot.md)：缓存包装、控制流、
  1032 字节偏移、状态继承/去重规则和原 DLL 差分验证。
- [TTPlugin 批量行情与状态记录](02-ttplugin-batch-quote.md)：9221 批量请求、
  16 字节 zlib 响应帧、150/140 字节线上字段和 712 字节状态缓存。

仍待继续标定：

- 行情数据结构（类似 THS 的 `CDataType`）
- 普通 A 股 `FastHQ.Subscribe` 的运行时订阅集合与授权会话样本；`tpbus`
  落表对象和 `TTPlugin` 扩展市场状态表已定位
- 订阅代码列表
- 行情记录格式
- 参考值验证
