# PeParse

只读解析文件布局的 PE32 / PE32+ 镜像。支持 DOS 头、NT 签名、COFF 文件头、
可选头、数据目录、节表、普通导入表和导出表。

输入为已有的 `PeDataSource`，不负责打开文件、读取进程内存、搜索 DLL、解析 API Set、
建立依赖关系、加载模块或展示界面。`isFileAligned() == false` 的数据源、
ROM、NE、COFF 对象等不支持的格式均解析失败，抛出 `PeParseError`。
其他目录（例如资源、重定位、TLS、延迟导入、证书）的地址和大小保留在可选头中，
本期不解释其内容。节数据不整体加载或缓存。

`PeFile::parse()` 组织解析流程并返回结果。DOS 头、COFF 文件头、可选头、NT 头分别由
`PeDosHeader::parse()`、`PeFileHeader::parse()`、`PeOptionalHeader::parse()`、
`PeNtHeaders::parse()` 实现；节和节表由 `PeSection::parse()`、`PeSections::parse()`
实现；导入表由 `PeImportModule::parseAll()` 实现，导出表由 `PeExports::parse()` 实现。
各部分的解析代码分别位于 `Headers.cpp`、`Sections.cpp`、`Imports.cpp`、
`Exports.cpp`。内部共享函数只处理小端读取、边界检查、RVA 定位和有界字符串读取，
不保存数据源或镜像状态。

## 使用

使用 C++17，链接 CMake 目标 `PeParse`，公共依赖为 `Qt::Core`、`PeDataSource`、`PeStructs`。

```cpp
#include <PeFileDataSource.h>
#include <PeFile.h>

PeFile* pe = nullptr;
try {
    PeFileDataSource source(path);
    pe = PeFile::parse(source);
    const auto entryRva = pe->entryPointRva();
    const auto entryOffset = pe->rvaToFileOffset(entryRva);
    for (const auto& module : pe->imports()) {
        const auto& dll = module.dllName();
        const auto& functions = module.entries();
        // 由调用方使用解析结果。
    }
} catch (const PeParseError& error) {
    // code()：错误类型；what()：诊断文本。
    // offset()：可确定时提供文件偏移；directoryIndex()：相关数据目录索引。
} catch (const PeDataSourceError& error) {
    // 文件打开失败等发生在进入解析入口之前的数据源错误。
} catch (...) {
    delete pe;
    throw;
}
delete pe;
```

各类的 `parse()` 和 `PeImportModule::parseAll()` 成功时返回裸指针，失败时抛出
`PeParseError`。调用方持有并最终 `delete PeFile`，其余解析对象由 `PeFile` 统一管理生命周期：

- `PeFile` 直接持有 DOS 头、NT 头、节表、导入集合、导出对象的指针，在析构时释放。
- NT 头持有并释放 COFF 文件头、可选头；节表持有并释放各节对象。
- 解析过程中发生异常时，已创建的对象会被释放，再继续抛出异常。
- 通过裸指针管理的 `PeFile`、各头部、节和节表、导出对象均用 Qt 的
  `Q_DISABLE_COPY_MOVE` 禁止复制及移动。导入/导出表项按值保存在容器中，保留值类型语义。

单独调用某个子解析入口时，返回对象由该调用方负责 `delete`。
`parseAll()` 返回 `std::vector<PeImportModule>*`，导入目录不存在时返回空集合的指针。
`PeExports::parse()` 返回 `PeExports*`，导出目录不存在时返回 `nullptr`。
导出目录不存在时 `exports()` 为空；已声明但损坏的导出目录会使整个文件解析失败。

`PeFileDataSource` 构造时打开文件，打开失败抛出 `PeDataSourceError`。
`PeDataSource::readExact()` 和标量、结构、字节及字符串读取失败也抛出数据源异常。
进入解析入口后的数据源异常统一包装为 `PeParseError::Code::DataSource`；
导入/导出解析还会补充对应的目录索引。
解析过程中调用方应保持输入内容不变。
解析器无共享可变状态，可以独立解析不同数据源；不需要 QObject、事件循环或 GUI。

## 结果和地址

- `PeFile` 自己持有全部解析结果，返回后即可销毁数据源。各访问器提供借用的只读视图，
  指针和引用在 `PeFile` 被删除前有效，外部不得单独释放这些子对象。
- `PeSections::items()` 返回节指针集合的只读引用，节对象由节表持有。
  `PeFile::exports()` 返回借用的 `const PeExports*`，不存在时为 `nullptr`。
- `imageBase()` 为可选头中的首选镜像基址，宽度统一为 `quint64`；
  `entryPointRva()`、导出项 `rva()`、导入项 `iatRva()` 都是原始 RVA。
- `PeFile::rvaToFileOffset()` 支持头部和节原始数据。节的零填充尾部、BSS 和未映射地址
  返回空值。`PeSections::rvaToFileOffset()` 仅处理节原始数据。
- `findByRva()` 查找节时包含虚拟尾部，因此找到节并不代表存在对应文件字节。
- 导入项区分按名和按序号导入，保留 Hint、INT 原始值、IAT 原始值及 IAT 槽位 RVA。
  `OriginalFirstThunk == 0` 时使用 `FirstThunk` 作为查找表。绑定 IAT 的值保留为原始值，
  不将其当作未绑定的名称信息；若缺少 INT 且绑定 IAT 已无法恢复名称，则解析失败。
- 导出项区分普通函数、转发项和空 EAT 槽位，支持别名和仅按序号导出。
  转发项保留字符串（例如 `OTHER.Target` 或 `OTHER.#123`），不解析目标模块。
  空槽保留在 `entries()` 中，但名称/序号查询不会返回空槽。
- PE32+ 的 `baseOfData()` 返回空值。所有数值按小端字段逐项解码，
  镜像类型取决于文件的 Magic，与宿主指针位数无关。
- 标准数据目录最多暴露 16 项，同时受 `NumberOfRvaAndSizes` 和可选头声明长度约束。
  声明数量超过 16 时保留原始数量，仅解释前 16 项。目录字段按原始 PE 语义保留，
  例如证书目录地址是文件偏移，不能一概作为 RVA 使用。

## 校验和限制

校验签名、头部长度、非负且有效的 `e_lfanew`、对齐值、节表、节的文件边界和镜像边界，
拒绝重叠的节 RVA 范围。此模块用于结构解析，不模拟 Windows 加载器的全部接纳规则，
也不保证所有非标准或加壳镜像都能通过上述校验。

导入描述符终止项必须位于导入目录范围内；名称表、IAT、EAT 和导出名称表按各自 RVA
检查读取范围，支持跨相邻 RVA 节且文件位置不连续的读取。导出函数 RVA 可以位于
镜像的零填充区域，无需存在对应文件偏移。所有地址及范围计算检查溢出。

当前固定资源上限：4096 个节、100000 个导入模块、所有模块合计 100000 个导入项、
100000 个导出地址及 100000 个导出名称。单个字符串最多扫描 4096 字节（包含 NUL），
所有解析字符串累计最多 16 MiB（每次解析的重复字符串也计入）。超过限制、
字符串缺少 NUL、地址无对应文件数据或读取失败，都会使 `PeFile::parse()` 抛出 `PeParseError`。
字符串按 Latin-1 解码，保留名称字节值。

## 测试

```powershell
cmake -S . -B out/build/debug -DBUILD_TESTING=ON
cmake --build out/build/debug
ctest --test-dir out/build/debug --output-on-failure
```

运行时确保 Qt DLL 所在目录在 PATH 中。`PeParseTests` 使用构造的 PE32 / PE32+
数据检查正常结构、错误目录、截断、地址溢出、短字符串及资源限制，也验证真实文件数据源。
所有权测试覆盖正常销毁和各解析阶段异常退出；MSVC Debug 构建还检查调试堆的分配差异。
还可以向该测试程序传入额外的 EXE/DLL 路径进行解析检查。
