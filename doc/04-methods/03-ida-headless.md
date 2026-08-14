# IDA Headless 使用指南

## 基本命令

```powershell
& "C:\Program Files\IDA Professional 9.0\idat.exe" -A "-L<日志>" "-S<脚本.py>" "<目标.i64>"
```

- `-A` 自主模式（不弹框）
- `-S` 跑 IDAPython 脚本
- `-L` 日志输出
- 可直接对 `.i64` 数据库运行，不需要原始 EXE

## TDX 项目专用示例

### 获取所有函数列表
```powershell
& "C:\Program Files\IDA Professional 9.0\idat.exe" -A "-LC:\temp\tdx_funcs.log" "-Slist_funcs.py" "C:\Users\cloudy064\workspace\ida\tdx\ida\TdxW.exe.i64"
```

`list_funcs.py`:
```python
import idaapi, idautils, ida_pro
with open('C:\\temp\\tdx_functions.txt', 'w') as f:
    for ea, name in idautils.Names():
        func = idaapi.get_func(ea)
        if func:
            f.write(f'{ea:08X} {name}\n')
ida_pro.qexit(0)
```

### 获取所有字符串引用
```powershell
& "C:\Program Files\IDA Professional 9.0\idat.exe" -A "-LC:\temp\tdx_str.log" "-Slist_strings.py" "C:\Users\cloudy064\workspace\ida\tdx\ida\TdxW.exe.i64"
```

### 搜索特定函数的交叉引用
```python
import idautils, ida_xref, ida_pro

target = idaapi.get_name_ea(idaapi.BADADDR, 'TDataParse.dll')  # 或用感兴趣的字符串
for xref in idautils.XrefsTo(target):
    print(f'xref from {xref.frm:08X} type={xref.type}')
ida_pro.qexit(0)
```

### 导出伪代码
```python
import ida_hexrays, idautils, ida_pro

# 导出所有函数的伪代码
for ea in idautils.Functions():
    try:
        cfunc = ida_hexrays.decompile(ea)
        print(f'// Function at {ea:08X}')
        print(str(cfunc))
    except:
        pass
ida_pro.qexit(0)
```

## 当前 IDA 数据库状态

- 文件：`C:\Users\cloudy064\workspace\ida\tdx\ida\TdxW.exe.i64`
- 大小：162MB
- 分析状态：已完成自动分析（2026-07-30）
- 架构：PE32（x86）
- 编译器：VC++ 16.0 (VS2010)，MFC 10.0

## 注意事项

- TdxW.exe 是 32 位程序，IDA 和 Hex-Rays 使用 x86 模式
- VS2010 编译的 MFC 程序通常有完整的 RTTI (Run-Time Type Information)，可通过 IDA 的 Class Informer 等插件恢复类结构
- VS2010 的 `std::string` 使用 SSO (Small String Optimization)，布局为 24/28 字节（取决于配置），注意与后续版本区别
- 无壳意味着交叉引用完整，函数边界准确
