# document

`Document` 表示一次打开的会话：持有 `PeFileDataSource`、根 `PeFile`、
`PeFileStore`、由它们构建的 `DependencyGraph`，以及该文档的 `Editor`。
构造时完成打开、解析、依赖构建和编辑器创建，失败通过异常报告，不会留下半成品。

```cpp
#include <document.h>

// 只建数据（测试 / 非 UI 用法），editor() 为 nullptr
Document document(path);

// 由 DocumentManager 打开时注入工厂，editor() 一定非空
Document* opened = DocumentManager::instance()->open(path);
Editor* editor = opened->editor();
```

`Editor` 的创建由外部注入的 `Document::EditorFactory` 完成，视图则由
`Editor::attachRegisteredViews()` 从 `EditorViewRegistry` 实例化；
`Editor` 只持有视图，不持有 Ribbon 控件。

`DocumentManager` 是单例，通过 `DocumentManager::instance()` 获取唯一实例，
持有所有已打开的 `Document`，并记录当前激活的会话。实例首次访问时创建，程序退出时销毁，
不能直接构造、复制或移动。
`open()` 会先完整构建新 `Document`，成功后加入列表；失败时列表和当前会话保持不变。
`OpenMode::OpenOnly` 仅打开，保持当前会话不变（即使原本没有当前会话）；
默认的 `OpenMode::OpenAndActivate` 打开并激活。重复打开同一路径会复用已有会话，
按指定模式决定是否激活，不会重复构建。
`close()` 会先询问 `Editor::canClose()`，返回 false 时取消关闭。

```cpp
auto* manager = DocumentManager::instance();
Document* document = manager->open(path);  // 新建并激活，或激活已有会话
manager->open(otherPath, DocumentManager::OpenMode::OpenOnly);  // 仅打开
manager->current();                          // 当前文档，可能为空
manager->documents();                        // 所有已打开的文档
manager->find(path);                         // 按路径查找
manager->activate(other);                    // 切换当前文档
manager->close(document);                    // 关闭指定文档
manager->closeAll();                         // 关闭全部文档
```

信号：

- `documentOpened(Document*)`：新文档成功加入列表后发出，随后按模式激活。重复打开和打开失败不发出。
- `documentActivated(Document*)`：当前文档改变时发出，重复激活不发出。关闭当前文档时，
  自动激活最后打开的剩余文档；没有剩余文档则传入 `nullptr`。
- `documentClosed(Document*)`：文档移出列表后、销毁前发出；如当前文档发生变化，先发出激活信号。
  该指针仅在同步槽执行期间有效，接收方应使用直接连接，且不能保存指针供后续访问。

`closeAll()` 先清空列表及当前文档，必要时发出一次 `documentActivated(nullptr)`，
再为每个移除的文档发出 `documentClosed`，不会逐个激活中间文档。

生命周期：`Document` 内部按 `graph → PeFileStore → 根 PeFile → 数据源` 的顺序析构。
调用方在会话期间可安全借用 `graph()`、`peFile()` 和 `files()` 的引用，但不能在
`close()`/`closeAll()` 之后继续持有 `Document*`。

`Document` 之后会持有该文档的 `Editor`（视图集合），`DocumentManager` 仍然是唯一门面，
详见 [编辑器架构设计](../../docs/editor-architecture.md)。
