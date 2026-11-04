# workspace

`Workspace` 表示一次打开的会话：持有 `PeFileDataSource`、根 `PeFile`、
`PeFileStore` 以及由它们构建的 `DependencyGraph`。构造时完成打开、解析和依赖构建，
失败通过异常报告，不会留下半成品。

```cpp
#include <workspace.h>

Workspace workspace(path);  // 打开并构建依赖图
const DependencyGraph& graph = workspace.graph();
```

`WorkspaceManager` 是单例，通过 `WorkspaceManager::instance()` 获取唯一实例，
持有所有已打开的 `Workspace`，并记录当前激活的会话。实例首次访问时创建，程序退出时销毁，
不能直接构造、复制或移动。
`open()` 会先完整构建新 `Workspace`，成功后加入列表；失败时列表和当前会话保持不变。
`OpenMode::OpenOnly` 仅打开，保持当前会话不变（即使原本没有当前会话）；
默认的 `OpenMode::OpenAndActivate` 打开并激活。重复打开同一路径会复用已有会话，
按指定模式决定是否激活，不会重复构建。

```cpp
auto* manager = WorkspaceManager::instance();
Workspace* workspace = manager->open(path);  // 新建并激活，或激活已有会话
manager->open(otherPath, WorkspaceManager::OpenMode::OpenOnly);  // 仅打开
manager->current();                          // 当前会话，可能为空
manager->workspaces();                       // 所有已打开的会话
manager->find(path);                         // 按路径查找
manager->activate(other);                    // 切换当前会话
manager->close(workspace);                   // 关闭指定会话
manager->closeAll();                         // 关闭全部会话
```

信号：

- `workspaceOpened(Workspace*)`：新会话成功加入列表后发出，随后按模式激活。重复打开和打开失败不发出。
- `workspaceActivated(Workspace*)`：当前会话改变时发出，重复激活不发出。关闭当前会话时，
  自动激活最后打开的剩余会话；没有剩余会话则传入 `nullptr`。
- `workspaceClosed(Workspace*)`：会话移出列表后、销毁前发出；如当前会话发生变化，先发出激活信号。
  该指针仅在同步槽执行期间有效，接收方应使用直接连接，且不能保存指针供后续访问。

`closeAll()` 先清空列表及当前会话，必要时发出一次 `workspaceActivated(nullptr)`，
再为每个移除的会话发出 `workspaceClosed`，不会逐个激活中间会话。

生命周期：`Workspace` 内部按 `graph → PeFileStore → 根 PeFile → 数据源` 的顺序析构。
调用方在会话期间可安全借用 `graph()`、`peFile()` 和 `files()` 的引用，但不能在
`close()`/`closeAll()` 之后继续持有 `Workspace*`。
