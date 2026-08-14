# PBRPC protobuf 纯 C++ 迁移与网页接入

> 日期：2026-08-03

## 目标

将 `reqformat=22` 从历史 Python 验证工具迁入统一 `tdx-tool.exe`，覆盖配置
发现、protobuf 外层、RpcID/StartPos 分段传输、业务 JSON 解码和网页查看。

## 协议实现

新增 `native/src/pbrpc.cpp`，只实现协议实际使用的 protobuf wire type 0/2：

```text
pb_rpc_req: 1 Head, 2 RpcID, 3 StartPos, 4 Moduledll, 5 ReqByte
pb_rpc_ans: 1 Head, 2 RpcID, 3 StartPos, 4 TotalLen, 5 RetByteNum, 6 RetByte
```

首轮省略零值 RpcID/StartPos；服务端返回正 RpcID 后，后续请求带回同一 ID
和累计 StartPos。实现会拒绝负/变化的 RpcID、错位 StartPos、长度不一致、
连续空页、超过轮次以及超过 128 MiB 的聚合响应。HTTP 目标在网页 API 中
固定为官方 TQLEX 地址，不接受任意 URL。

配置层复用了与普通 TQLEX 相同的 XML 编码、注释移除和属性解析边界，解析
`pb_rpc_req:` 中的 `Moduledll/ReqByte`，支持 `$$NAME$$`、重复 ReqId 的
来源/正文选择，以及大小写不敏感的 ReqByte 字段覆盖。

## 测试与真实结果

- `200404` 的完整请求正文与既有十六进制测试向量逐字节一致；
- 单元测试覆盖负数 int32 RpcID、两轮握手、RetByte 拼接、描述符参数替换和
  尾随 NUL JSON；完整原生测试通过；
- 当前安装扫描得到 58 个启用模板、6 个 Entry、6 个模块和 29 个唯一 ReqId；
- `200404` 竞价爆量：2 轮、9 行、6 列；
- `200340` 市场分时段主力资金：2 轮、39 行、39 列；
- `200451` 九转序列：2 轮、118 行、8 列。

行数是 2026-08-03 验证时的业务快照，不作为固定接口契约。

## 产品入口

- CLI：`tdx-tool cloud pbrpc`；
- API：`/api/v1/pbrpc/configs`、`/api/v1/pbrpc/query`；
- Svelte：“策略云查询”页面，可选择全部模板、查看模块/占位符/原始描述符、
  填写 JSON 参数，并按返回 `ColDes/Content` 生成表格。

下一阶段不再是单协议复现，而是把 TQLEX/PBRPC 的主表结果自动映射为明细
参数，迁移基金、指数估值、龙虎榜等 master-detail 工作流。
