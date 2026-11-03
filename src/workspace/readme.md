# workspace

`Workspace` 表示一次打开的会话：持有 `PeFileDataSource`、根 `PeFile`、
`PeFileStore` 以及由它们构建的 `DependencyGraph`。构造时完成打开、解析和依赖构建，
失败通过异常报告，不会留下半成品。

```cpp
#include <workspace.h>

Workspace workspace(path);  // 打开并构建依赖图
const DependencyGraph& graph = workspace.graph();
```

`WorkspaceManager` 持有所有已打开的 `Workspace`，并记录当前激活的会话。
`open()` 会先完整构建新 `Workspace`，成功后加入列表；失败时列表和当前会话保持不变。
重复打开同一路径只会激活已有会话，不会重复构建。

```cpp
WorkspaceManager manager;
Workspace* workspace = manager.open(path);  // 新建并激活，或激活已有会话
manager.current();                          // 当前会话，可能为空
manager.workspaces();                       // 所有已打开的会话
manager.find(path);                         // 按路径查找
manager.activate(other);                    // 切换当前会话
manager.close(workspace);                   // 关闭指定会话
manager.closeAll();                         // 关闭全部会话
```

生命周期：`Workspace` 内部按 `graph → PeFileStore → 根 PeFile → 数据源` 的顺序析构。
调用方在会话期间可安全借用 `graph()`、`peFile()` 和 `files()` 的引用，但不能在
`close()`/`closeAll()` 之后继续持有 `Workspace*`。
