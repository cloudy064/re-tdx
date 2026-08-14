# UserComm 网络层静态分析

## 目标

利用用户新增的 `TAsioComm.dll.i64`、`TDXAsioComm.dll.i64`，确认网络库
角色并从 `TdxW.exe.i64` 追到真实连接/收发调用。

## 输入

| 数据库 | 原始文件 SHA-256 |
|---|---|
| `ida\TAsioComm.dll.i64` | `d3d4fa6b2281fe5e63dfbe78fa7784b80431a0fe96fc2500b4757fd70ef57cff` |
| `ida\TDXAsioComm.dll.i64` | `1df4d21ba80194f2bb9dd4824ae5f4e9e603afe254cc4b69ba40e82bf669317a` |
| `ida\TdxW.exe.i64` | `f5f2e6025a4d80bb1afbcb2a51c753d3c1909e09aa9d1bc8f7c3b701b081f74c` |

## 方法

- `dumpbin /imports` / `/exports`
- Rizin 自动分析和 socket/IOCP 交叉引用
- IDA headless + Hex-Rays
- 新增 `doc/90-scripts/ida_network_inventory.py`

headless 原始日志写到工作站临时目录，不进入仓库。

## 关键结果

1. 两个库都是 Boost.Asio + IOCP + overlapped Winsock 传输层；
2. 都由 `MakeUserCommModule` 创建 `CUserComm : VUserComm`；
3. 主版本对象为 `0xE0` 字节，插件版本为 `0x75` 字节，并非同一构建；
4. 主版本 `CUserComm` 有 15 个业务虚方法；
5. `TdxW.exe::sub_413B20` 通过虚表槽 8/9 建立同步连接；
6. `TdxW.exe::sub_414240` 通过槽 2/6/3/13/14 完成异步连接、worker、
   发送队列和读取循环；
7. 传输库内没有行情帧/登录/7709 等业务证据，协议边界应在上层回调、
   `TaApi.dll` 或 `TDataParse.dll`。

完整地址和虚表见
[UserComm 网络传输层](../02-engine/01-network-layer.md)。

## 遗留问题

- `sub_414240` 的数据回调目标尚未反查到首个帧边界判断；
- 虚表槽 1、4、5 尚未获得足够上层调用证据命名；
- 插件版本当前未加载，触发条件未知；
- 待用运行时 hook 验证 host/service、读写长度与回调顺序。
