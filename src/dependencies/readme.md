# dependencies

C++17 静态库，链接 CMake target `dependencies` 即可使用；公共依赖为 `parse`、
`datasource` 和 Qt Core。文件名沿用项目的小写蛇形命名。

## 文件依赖图

```cpp
#include <dependency_graph.h>
#include <pe_file_data_source.h>
#include <pe_file_store.h>

PeFileDataSource source(path);
PeFile file(source);
file.parse();

PeFileStore files;
DependencyGraph graph(file, files);
graph.build();
for (const auto& edge : graph.root().dependencies()) {
    if (edge.isResolved()) {
        const auto& module = *edge.module;
        const auto& imports = module.pe_file().imports();
        const auto& callers = module.dependents();
        // module.dependencies() 给出下一层依赖。
    } else {
        const auto& error = *edge.error;
        // error.code / error.message 描述失败；exception 保留原始异常及其字段。
    }
}
```

根 `PeFile` 必须已经成功解析；根文件及其数据源由调用方持有，生命周期必须覆盖图。
其余模块的 `PeFile` 和数据源由调用方提供的 `PeFileStore` 统一持有，并按定位器身份
去重；Store 需专属于该图且生命周期覆盖图，图会在 `build()` 时清空它。Store 保证先释放
`PeFile`，再释放其借用的数据源。`graph.files()` 提供只读访问，可用于诊断；根文件
不在 Store 中。位数不匹配的模块虽然不进入依赖图，但解析成功后仍由 Store 管理，
直到重新构建或图析构。
节点地址在一次构建内保持稳定，因此可直接用节点指针表示共享依赖、循环和反向引用，
不需要额外的模块 ID、全局 ID 分配器或单独的遍历器。

构造后可查询根节点，`build()` 才展开依赖。遍历使用迭代方式，不递归调用，
按规范化文件路径去重，缓存解析错误和位数不匹配；DLL 别名不会造成重复解析。
直接依赖保留导入表顺序，同一父模块在反向依赖中只出现一次。
缺失文件、读取错误、无效 PE、32/64 位不匹配均保存在对应边中，继续处理其他依赖。
调用方可通过 `std::rethrow_exception(error.exception)` 取得原始 `ParseError` 或
`PeDataSourceError`；缺失文件和位数不匹配不包含异常。

再次 `build()` 会重新读取子模块并清空旧关系和错误缓存，同时清空 `PeFileStore` 并
重新解析；根节点地址保持不变，根 `PeFile` 不会重新解析。所有旧的子节点、依赖边和
反向依赖引用均失效。
构建期间不要并发修改或查询同一张图，也不要重新解析其根文件。

## 定位配置

`module_locator.h` 声明通用定位接口；文件定位器及其配置声明在
`file_module_locator.h`，实现在 `file_module_locator.cpp`。

`FileLocatorOptions::systemDefaults()` 获取本机系统目录、KnownDLLs、API Set schema
及 PATH 的快照，是默认定位配置。也可以从空的 `FileLocatorOptions{}` 开始，显式配置
离线分析环境；空字段不参与搜索，不会被隐式解释为查看器的当前目录。

```cpp
auto options = FileLocatorOptions::systemDefaults();
options.workDirectory = targetWorkingDirectory;
options.fallbackDirectories.append(extraDirectory);
DependencyGraph graph(file, files, options);
```

图根据根模块设置应用目录和目标位数，覆盖配置中的 `appDirectory` 和 `isPe32Plus`。
调用方应使用绝对路径配置应用、系统、工作和额外目录。
搜索顺序为 API Set 宿主映射、KnownDLLs，然后是应用目录、目标位数的系统目录、
Windows/System、Windows、显式指定的目标工作目录、PATH、额外回退目录。
KnownDLLs 只使用对应系统目录，缺失时不回退到应用目录的同名文件。
相对 PATH 项只在指定了目标工作目录时生效。
绝对文件路径直接检查；带目录的相对文件路径仅相对于目标工作目录解析。
文件路径与导入名的大小写不会让同一 Windows 文件产生多个节点。

该搜索策略是[默认安全 DLL 搜索顺序](https://learn.microsoft.com/en-us/windows/win32/dlls/dynamic-link-library-search-order)
的静态近似，沿用参考实现的范围：不模拟 `.local`、SxS、包依赖关系、进程已加载模块
和运行时搜索 API；不调用 `LoadLibrary`，不会执行被分析 DLL 的代码。
不处理延迟导入，也不跟踪导出转发字符串中的额外依赖。位数检查区分 PE32 / PE32+，
不验证同位数的具体机器架构（例如 AMD64 与 ARM64）。
32 位查看器访问原生 64 位系统目录时使用
[Sysnative](https://learn.microsoft.com/en-us/windows/win32/winprog64/file-system-redirector)。

## API Set 和自定义数据源

`ApiSetMap::load()` 从 `apisetschema.dll` 的 `.apiset` 节读取命名空间；
`ApiSetMap::parse()` 可解析离线字节，只接受已知的 version 6 布局。
解析验证命名空间大小、数组范围和 UTF-16 字符串；不认识或损坏时返回 `std::nullopt`，
文件搜索退回普通 DLL 名称搜索。契约匹配忽略大小写、`.dll` 和末尾 minor 版本，
保留 major 版本；多个宿主按导入者名称选择覆盖项，未命中时使用默认项（名称为空的项），
不会使用其他导入者的专属宿主。导入者路径只取文件名并忽略大小写；别名链最多跟随 8 步，
全程保留原导入者上下文。

依赖图在定位每条导入边时传入父模块身份，并在 `DependencyEdge::apiSetHost` 中保留
API Set 宿主名称；解析缓存仍按实际宿主身份去重，不缓存与导入者有关的契约映射。
树界面对已映射的契约显示 `API Set` 状态。契约直接映射回当前宿主时作为终止引用显示，
保留宿主路径，不继续展开；普通 DLL 自环以及跨模块的真实循环仍显示循环依赖。

实现 `ModuleLocator::findModule()` 并返回 `LocatedModule`，即可注入其他定位策略。
返回值包含稳定身份字符串、独占所有权的数据源及可选的 `apiSetHost`；
同一模块的所有别名必须使用同一身份，
根身份也须与定位器一致。找不到返回 `std::nullopt`，打开失败可抛出 `PeDataSourceError`。
空身份、空数据源等接口误用抛出 `std::invalid_argument`。
定位器也可重写接受 `(dllName, importer)` 的重载来使用导入者上下文；默认实现调用原来的
单参数接口，已有自定义定位器无需修改。

```cpp
DependencyGraph graph(file, files, rootIdentity, std::move(customLocator));
```

当前 C++ 解析器仅支持文件布局，因此自定义源也须提供文件布局的 PE 字节。
测试中的内存数据源存储的是文件字节，不是已经加载的进程镜像。

## 测试

```powershell
cmake -S . -B out/build/debug -DBUILD_TESTING=ON
cmake --build out/build/debug
ctest --test-dir out/build/debug --output-on-failure
```

- `api_set_tests`：契约名、版本、多宿主、别名链及循环，截断、溢出、无效 UTF-16，
  按导入者覆盖与默认宿主选择，以及从 PE 文件读取 `.apiset`。
- `module_locator_tests`：完整搜索顺序、KnownDLLs 优先级、位数、显式路径、目标工作目录、
  API Set 别名身份；Windows 上额外验证本机 schema。
- `dependency_graph_tests`：共享与循环依赖、反向查询、缺失和无效模块、位数不匹配、
  读取失败、别名错误缓存、重新构建和数据源生命周期。
- `pe_file_store_tests`：身份去重、稳定地址、所有权转移、失败回滚和清空行为。
- `DependencyTreeModelTests`：API Set 导入者重定向、宿主自身引用、共享宿主及真实循环的展示。
