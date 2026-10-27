# PE 结构定义

`PeStructs.h` 的来源是 Microsoft Windows SDK **10.0.28000.0** 的 `um/winnt.h`。
完整保留 `Image Format` 至 `End Image Format` 段，包括 DOS/NE/VXD、PE32/PE32+、
COFF/大对象、符号与辅助符号、全部架构重定位、导入导出、TLS、资源、加载配置、
动态重定位、热补丁/热交换、异常表、enclave、调试信息、归档及 CLR 头。
同时提取段外的 scope/unwind 表、镜像签名等级与类型、`.tPolicy` 元数据及宏、
`IMAGE_FILE_MACHINES` 和 DLL 通知常量。

原来的结构标签、typedef、成员、枚举、宏名称及数值均保留。
没有添加不在该 `winnt.h` 中的结构，例如其他 SDK 头文件定义的 `WIN_CERTIFICATE`，
也不引入依赖 Windows 内核的 `Rtl*` API。

## 平台和使用方式

直接包含：

```cpp
#include <PeStructs.h>
```

Windows 默认沿用 `<Windows.h>`，兼容项目中其他 Windows SDK 头文件。
Linux/macOS 使用头文件中的独立定义，不需要 Windows SDK 或 Qt。
Windows 也可以选择独立定义（这时不要再包含 Windows SDK 头文件）：

```cpp
#define PESTRUCTS_NO_WINDOWS_SDK
#include <PeStructs.h>
```

需要 C++17。独立定义使用固定宽度的基础类型，`LONG` 为 32 位，`WCHAR`
为 16 位 UTF-16 单元；保留 SDK 的分段 packing，并恢复调用方的 packing。
`IMAGE_NT_HEADERS` 等无位数后缀的别名按宿主指针位数选择，解析任意 PE 文件时
应根据文件的 `Magic` 使用明确的 `32`/`64` 类型。

保留匿名结构/联合及 flexible array 等 SDK 扩展，支持 MSVC、GCC 和 Clang；
`NONAMELESSUNION` 与 SDK 一样使用 `u`/`s` 等具名成员。
`.tPolicy` 的导出宏在 GCC/Clang 下适配 section 属性和聚合存储，
导出符号仍为 `__ImagePolicyMetadata`，其声明类型仍为 `IMAGE_POLICY_METADATA`。

PE 文件字段遵循[微软 PE/COFF 规范](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format)的小端编码。
本项目仅支持小端平台，文件字节直接按对应结构布局读取，不执行字节序转换。

## 校验

`Tests/PeStructsLayout.inc` 记录从原 SDK 使用 MSVC x64 编译取得的
110 个类型大小/对齐和 757 个字段偏移。包含原生指针的 unwind history 表
另有 32 位期望值。测试还检查位域、packing 恢复、32/64 位别名、
`IMAGE_FIRST_SECTION`、具名联合以及策略导出宏。

无需 Qt，单独构建测试：

```sh
cmake -S Tests/PeStructs -B out/build/pe-structs
cmake --build out/build/pe-structs
ctest --test-dir out/build/pe-structs --output-on-failure
```

安装原版 SDK 的机器还可以核对提取完整性：

```powershell
python Tests/verify_pe_extraction.py "C:/Program Files (x86)/Windows Kits/10/Include/10.0.28000.0/um/winnt.h"
```

该脚本逐 token 核对完整 `Image Format` 段，只允许 packing 和架构判断的适配，
同时核对 11 个段外声明以及 651 条相关宏定义。
