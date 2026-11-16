# editor

编辑器框架：把一个已打开文档的展示拆成若干 `EditorView`，每个视图对应一个 Ribbon 页签
和一块中央页面。框架本身不认识 PE，只依赖 Qt Widgets。

```cpp
#include <editor_view.h>

class MyView : public EditorView {
public:
    QString id() const override;                 // 页签唯一标识
    QString title() const override;              // 页签标题（随语言变化）
    QWidget* contentWidget() override;           // 中央页面，可懒创建
    void populateRibbon(SARibbonCategory*) override;   // 往页签里加 panel / action
};
```

- `Editor`：文档的呈现容器，持有一到多个视图，由 `Document` 在创建时通过注入的工厂创建；
  构造完成后调用 `attachRegisteredViews()` 按注册表实例化视图。
- `EditorViewRegistry`：启动时注册视图类型（`id`、顺序、适用性 `supports` 与工厂），
  之后所有文档按同一份注册表生成页签。

```cpp
EditorViewRegistry::instance()->add(
    {QStringLiteral("my-view"), 100,
     [](const Editor& editor) { return dynamic_cast<const MyEditor*>(&editor) != nullptr; },
     [](Editor& editor) { return std::make_unique<MyView>(static_cast<MyEditor&>(editor)); }});
```

约定：

- 视图构造函数接收 `Editor&`（具体类型强转），这是视图与 Editor 的关联点；
- `contentWidget()` 幂等，可能因窗口销毁而重建，需要能重新同步数据；
- `populateRibbon()` 在每次文档激活时被调用，只能添加视图自己持有的 `QAction`；
- 视图对象随 Editor 销毁，页签由 `EditorHost` 在切换文档时重建。

完整设计见 [编辑器架构设计](../../docs/editor-architecture.md)。
