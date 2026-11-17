# 编辑器架构设计：Document / Editor / EditorView

> 状态：已实现（P1–P4），实现细节以代码为准
> 基线：`dev` @ `c95a55a`（SARibbon v2.9.6，Qt 5.15.2）
> 适用：PeViewer 主窗口的 Ribbon 页签组织与文档打开/关闭

## 目录

1. [背景与目标](#1-背景与目标)
2. [术语与职责](#2-术语与职责)
3. [与现状的对照](#3-与现状的对照)
4. [分层](#4-分层)
5. [目录与构建目标](#5-目录与构建目标)
6. [接口定义](#6-接口定义)
7. [生命周期与所有权](#7-生命周期与所有权)
8. [打开流程](#8-打开流程)
9. [Ribbon 与中央区联动](#9-ribbon-与中央区联动)
10. [关键实现要点与 SARibbon 陷阱](#10-关键实现要点与-saribbon-陷阱)
11. [错误处理与边界](#11-错误处理与边界)
12. [迁移步骤](#12-迁移步骤)
13. [测试计划](#13-测试计划)
14. [决策记录](#14-决策记录)
15. [后续扩展](#15-后续扩展)
16. [风险与注意事项](#16-风险与注意事项)
17. [验收清单](#17-验收清单)

## 1. 背景与目标

现状里 `PeViewer` 直接连着 `WorkspaceManager`，中央区固定是一个 `DependencyTreeWidget`，
Ribbon 只有「主页」一个页签（里面只有语言面板），「文件」是左上角应用按钮的下拉菜单。
要新增一种展示方式，就得改主窗口。

目标是把「文档」和「文档怎么显示」拆开，做成可注册、可扩展的结构：

| 编号 | 目标 |
|---|---|
| G1 | 未打开文件时，页签只有「主页」；「文件」是应用按钮菜单，不是页签 |
| G2 | 打开文件后，出现该文档的 N 个页签，**一个页签 = 一个 `EditorView`** |
| G3 | `EditorView` 负责自己那个页签的面板和对应的中央页面 |
| G4 | `EditorView` 由外部实现并在启动时注册，框架和主窗口不认识具体视图 |
| G5 | `DocumentManager` 是唯一对外门面，提供打开、切换、关闭，打开结果是一个 `Document` |
| G6 | `Document : Editor = 1 : 1`，`Editor : EditorView = 1 : N` |
| G7 | 页签与中央内容严格对应，**点「主页」也切换中央内容** |

非目标（本次不做）：多窗口、同一文档挂多个 Editor、页签拖拽/停靠、异步打开、插件 DLL 动态加载。

## 2. 术语与职责

| 类型 | 职责 | 数量 |
|---|---|---|
| `Document` | 一次打开的会话：数据源、根 `PeFile`、`PeFileStore`、依赖图，以及它的 `Editor` | 1 文件 : 1 文档 |
| `DocumentManager` | 单例。`open/find/activate/close/closeAll`，维护文档列表与当前文档 | 1 |
| `Editor` | 文档的呈现容器：显示名、能否关闭、视图集合 | 1 文档 : 1 Editor |
| `EditorView` | 一个 Ribbon 页签 + 一个中央页面；负责面板/动作与页面 UI | 1 Editor : N 视图 |
| `EditorViewRegistry` | 视图类型注册表：id、顺序、适用性、工厂 | 1 |
| `EditorHost`（主窗口侧） | 页签 ↔ `QStackedWidget` 的联动，页签重建、语言转发、当前视图跟踪 | 1 |

一句话边界：

- **Document** 管数据什么时候来、什么时候走；
- **Editor** 管这个文档怎么显示（对外身份是"一个可关闭的文档"）；
- **EditorView** 管一个页签和它的页面；
- **EditorHost** 管页签与页面之间的搬运。

约定：不引入 `EditorManager`。文档列表、当前文档、关闭顺序只在 `DocumentManager` 一处记账；
`Document` 持有 `Editor`，因此不存在"第二个 current"。

## 3. 与现状的对照

| 现在 | 改造后 |
|---|---|
| `Workspace`（src/workspace） | `Document`（src/document），新增 `Editor*` 成员 |
| `WorkspaceManager`（单例） | `DocumentManager`（单例，语义平移 + `currentEditor()`） |
| `WorkspaceManager::open() -> Workspace*` | `DocumentManager::open() -> Document*` |
| `workspaceCombo` | `documentCombo`（objectName / 测试同步） |
| `ui.dependencyTree`（.ui 中央控件） | `DependencyTreeEditorView` 的内容控件，放进 `viewStack` |
| `PeViewer` 直连 manager、直接 `setGraph` | `PeViewer` + `EditorHost`，`PeViewer` 不再碰依赖树 |
| 「主页」= 唯一页签 | 「主页」固定页签 + 每个视图一个页签 |
| `src/workspace/readme.md` | `src/document/readme.md`，补"文档与编辑器"关系 |

## 4. 分层

```
peviewer ──> document ──> editor ──> Qt::Widgets + SARibbonBar
   │                          ▲
   └──────────────────────────┘
```

- `editor`：框架层，只有 `Editor` / `EditorView` / `EditorViewRegistry`，不认识 PE，也不认识 `Document`；
- `document`：会话层，`Document` 持有数据并从外部注入的工厂创建 `Editor`；
- `peviewer`：应用层，`PeEditor`、具体视图、`EditorHost`、注册代码、主窗口。

依赖方向单向，没有循环：`document` 在 `.h` 里只前向声明 `class Editor;`，`editor` 里不出现 `Document`。

## 5. 目录与构建目标

```
src/editor/                      ← 新库 editor（Qt::Widgets + SARibbonBar）
  editor.h / editor.cpp
  editor_view.h
  editor_view_registry.h / editor_view_registry.cpp
  CMakeLists.txt
src/document/                    ← 由 src/workspace 改名
  document.h / document.cpp          （原 workspace.*）
  document_manager.h / document_manager.cpp
  readme.md
  CMakeLists.txt                     （target: document）
src/peviewer/
  editors/pe_editor.h / pe_editor.cpp
  editors/dependency_tree_editor_view.h / .cpp
  editor_host.h / editor_host.cpp
  editor_registration.h / editor_registration.cpp
  pe_viewer.h / pe_viewer.cpp / pe_viewer.ui
```

CMake：

- `src/editor/CMakeLists.txt`：`add_library(editor STATIC ...)`，`PUBLIC` 链接 `Qt::Widgets`、`SARibbonBar::SARibbonBar`；
- `src/document/CMakeLists.txt`：`project(document)`，`PUBLIC` 链接 `Qt::Core`、`parse`、`datasource`、`dependencies`、`editor`；
- `src/CMakeLists.txt`：`add_subdirectory("editor")`、`add_subdirectory("document")`（顺序在 `peviewer` 之前）；
- `tests/CMakeLists.txt`：新增 `EditorTests`、`DocumentTests`（原 `WorkspaceTests` 改名），`PeViewerUiTests` 链接 `document`。

## 6. 接口定义

以下为头文件草案，命名遵循仓库约定（snake_case 文件、`Q_DISABLE_COPY_MOVE`、`Q_SIGNALS`、英文单行注释）。

### 6.1 EditorView（外部扩展点）

```cpp
// src/editor/editor_view.h
#pragma once

#include <QString>

class QWidget;
class SARibbonCategory;

// 一个视图 = 一个 Ribbon 页签 + 对应的中央页面。
// 生命周期与所属 Editor 相同；Ribbon 页面由 EditorHost 在每次激活文档时重建。
class EditorView {
public:
    virtual ~EditorView() = default;
    EditorView(const EditorView&) = delete;
    EditorView& operator=(const EditorView&) = delete;

    // Stable identifier, unique inside one Editor, e.g. "dependency-tree".
    virtual QString id() const = 0;
    // Localized Ribbon tab title; re-read on every language change.
    virtual QString title() const = 0;
    // Central widget. Created once, owned by the view, reused across activations.
    virtual QWidget* contentWidget() = 0;

    // Fills the empty Ribbon page created by the host. Called again on every
    // activation, so it may only add QActions owned by the view.
    virtual void populateRibbon(SARibbonCategory* page) = 0;

    // Refresh translated texts of actions and widgets.
    virtual void retranslateUi() {}
    // Called when this view becomes / stops being the displayed page.
    virtual void activated() {}
    virtual void deactivated() {}
};
```

### 6.2 Editor

```cpp
// src/editor/editor.h
#pragma once

#include <QString>
#include <memory>
#include <vector>

class EditorView;

// One opened document as seen by the UI: it owns the views and nothing else.
// Created by the factory installed on DocumentManager; views are attached right
// after construction through attachRegisteredViews().
class Editor {
public:
    virtual ~Editor();
    Editor(const Editor&) = delete;
    Editor& operator=(const Editor&) = delete;

    const QString& path() const { return m_path; }
    // Display name in the document selector, defaults to the file name.
    virtual QString displayName() const;
    // Asked before closing; false cancels the close (unsaved changes).
    virtual bool canClose() const { return true; }
    virtual void retranslateUi() {}

    const std::vector<std::unique_ptr<EditorView>>& views() const { return m_views; }

    // Instantiates every registered view that matches this editor and attaches it.
    // Called by Document once the concrete editor is fully constructed.
    void attachRegisteredViews();

protected:
    explicit Editor(QString path);

private:
    void addView(std::unique_ptr<EditorView> view);

    QString m_path;
    std::vector<std::unique_ptr<EditorView>> m_views;
};
```

### 6.3 EditorViewRegistry

```cpp
// src/editor/editor_view_registry.h
#pragma once

#include <QString>
#include <functional>
#include <memory>
#include <vector>

class Editor;
class EditorView;

// 一个视图类型的注册信息。启动时注册一次，Document 创建 Editor 后逐条实例化。
struct EditorViewDescriptor {
    QString id;                                            // 唯一，如 "dependency-tree"
    int order = 100;                                       // 页签顺序，升序
    std::function<bool(const Editor&)> supports;           // 空 = 对所有 Editor 适用
    std::function<std::unique_ptr<EditorView>(Editor&)> create;
};

class EditorViewRegistry {
public:
    static EditorViewRegistry* instance();
    EditorViewRegistry(const EditorViewRegistry&) = delete;
    EditorViewRegistry& operator=(const EditorViewRegistry&) = delete;

    // Returns false for an empty id or a duplicate id.
    bool add(EditorViewDescriptor descriptor);
    bool remove(const QString& id);
    bool contains(const QString& id) const;
    // Test-only: drop every descriptor.
    void clear();

    // Descriptors whose supports() passes, sorted by (order, id).
    std::vector<EditorViewDescriptor> viewsFor(const Editor& editor) const;

private:
    EditorViewRegistry() = default;
    std::vector<EditorViewDescriptor> m_descriptors;
};
```

`add()` 规则：`id` 非空、`create` 非空、`id` 不重复。排序保证同样的注册内容产生同样的页签顺序（`order` 相同按 `id` 字典序），便于测试与用户预期稳定。

### 6.4 Document

```cpp
// src/document/document.h
#pragma once

#include <QString>
#include <functional>
#include <memory>

#include "file_module_locator.h"
#include "pe_file_store.h"

class DependencyGraph;
class Editor;
class PeFile;
class PeFileDataSource;

// 一次打开的会话：持有根文件、数据源、依赖文件、依赖图，以及该文档的 Editor。
// 构造时完成打开、解析、依赖构建和编辑器创建；任何一步失败都抛异常，不留半成品。
class Document {
public:
    using EditorFactory = std::function<std::unique_ptr<Editor>(Document&)>;

    // editorFactory 为空时只建数据（测试 / 非 UI 用法），此时 editor() 为 nullptr。
    explicit Document(const QString& path,
                      FileLocatorOptions options = FileLocatorOptions::systemDefaults(),
                      EditorFactory editorFactory = {});
    ~Document();

    Document(const Document&) = delete;
    Document& operator=(const Document&) = delete;

    const QString& path() const { return m_path; }
    const PeFile& peFile() const;
    const DependencyGraph& graph() const;
    const PeFileStore& files() const { return m_files; }
    // nullptr only when the document was created without an editor factory.
    Editor* editor() const { return m_editor.get(); }

private:
    QString m_path;
    // 析构顺序：editor → graph → store → 根 PeFile → 数据源。
    // m_editor 最后声明，因此最先析构，视图不会碰到已释放的图。
    std::unique_ptr<PeFileDataSource> m_dataSource;
    std::unique_ptr<PeFile> m_peFile;
    PeFileStore m_files;
    std::unique_ptr<DependencyGraph> m_graph;
    std::unique_ptr<Editor> m_editor;
};
```

构造函数末尾：

```cpp
if (editorFactory) {
    m_editor = editorFactory(*this);        // 失败直接抛，Document 不成立
    m_editor->attachRegisteredViews();      // 统一在这里挂视图，具体 Editor 不用记
}
```

### 6.5 DocumentManager

```cpp
// src/document/document_manager.h
#pragma once

#include <QObject>
#include <QString>
#include <cstddef>
#include <functional>
#include <memory>
#include <vector>

class Document;
class Editor;

// 单例：持有全部已打开文档和当前文档。文档持有其 Editor，因此这里也是
// "当前编辑器" 的唯一来源。
class DocumentManager : public QObject {
    Q_OBJECT

public:
    enum class OpenMode { OpenOnly, OpenAndActivate };
    Q_ENUM(OpenMode)

    // 由应用在创建窗口之前安装，负责 PeEditor 之类的具体编辑器。
    using EditorFactory = std::function<std::unique_ptr<Editor>(Document&)>;

    static DocumentManager* instance();

    DocumentManager(const DocumentManager&) = delete;
    DocumentManager& operator=(const DocumentManager&) = delete;

    void setEditorFactory(EditorFactory factory);

    // 新建并（按 mode）激活；同一路径已打开时复用。失败抛异常，列表与当前不变。
    Document* open(const QString& path, OpenMode mode = OpenMode::OpenAndActivate);

    Document* find(const QString& path) const;
    bool activate(Document* document);
    // canClose() 为 false 时取消并返回 false。
    bool close(Document* document);
    void closeAll();

    Document* current() const { return m_current; }
    // 便捷函数：current() ? current()->editor() : nullptr
    Editor* currentEditor() const;

    std::size_t size() const { return m_documents.size(); }
    bool empty() const { return m_documents.empty(); }
    const std::vector<std::unique_ptr<Document>>& documents() const { return m_documents; }

Q_SIGNALS:
    void documentOpened(Document* document);
    // 移出列表后、销毁前发出；指针只在同步槽内有效。
    void documentClosed(Document* document);
    // 当前文档变化时发出，nullptr 表示没有当前文档。
    void documentActivated(Document* document);

private:
    DocumentManager();
    ~DocumentManager() override;

    EditorFactory m_editorFactory;
    std::vector<std::unique_ptr<Document>> m_documents;
    Document* m_current = nullptr;
};
```

`open()` 语义与现有 `WorkspaceManager` 保持一致：

```cpp
Document* DocumentManager::open(const QString& path, OpenMode mode) {
    if (Document* existing = find(path)) {
        if (mode == OpenMode::OpenAndActivate) activate(existing);
        return existing;
    }
    auto document = std::make_unique<Document>(path, FileLocatorOptions::systemDefaults(), m_editorFactory);
    Document* raw = document.get();
    m_documents.push_back(std::move(document));
    Q_EMIT documentOpened(raw);
    if (mode == OpenMode::OpenAndActivate) activate(raw);
    return raw;
}
```

关闭顺序：先决定下一个当前文档，发 `documentActivated(下一个或 nullptr)`，再把文档移出列表并发 `documentClosed`，最后销毁（沿用现有 `WorkspaceManager` 的语义，UI 测试已覆盖）。

### 6.6 PeEditor

```cpp
// src/peviewer/editors/pe_editor.h
#pragma once

#include <document.h>
#include <editor.h>

// PE 文档的编辑器：为视图提供类型化的数据入口，显示细节归主窗口。
class PeEditor : public Editor {
public:
    explicit PeEditor(Document& document);

    QString displayName() const override;   // 文件名
    const Document& document() const { return m_document; }

private:
    Document& m_document;
};
```

> 说明：`Editor::path()` 已经由构造传入，`PeEditor` 只需要数据引用。
> 状态栏文案、窗口标题等由 `PeViewer` 生成（见 D10），`Editor` 不暴露显示文案接口。

```cpp
PeEditor::PeEditor(Document& document) : Editor(document.path()), m_document(document) {
}
```

### 6.7 DependencyTreeEditorView

```cpp
// src/peviewer/editors/dependency_tree_editor_view.h
#pragma once

#include <editor_view.h>

class DependencyTreeWidget;
class PeEditor;
class QAction;

// 依赖树视图：一个页签（刷新 / 展开 / 折叠）+ 一个中央依赖树页面。
class DependencyTreeEditorView : public EditorView {
public:
    explicit DependencyTreeEditorView(PeEditor& editor);

    QString id() const override;                               // "dependency-tree"
    QString title() const override;                            // tr("Dependency")
    QWidget* contentWidget() override;                         // 懒创建 DependencyTreeWidget
    void populateRibbon(SARibbonCategory* page) override;      // 建 panel，只挂自己的 action
    void retranslateUi() override;
    void activated() override;                                 // 首次显示时 setGraph

private:
    PeEditor& m_editor;
    DependencyTreeWidget* m_widget = nullptr;
    QAction* m_refresh = nullptr;                              // 成员持有，可重复 populate
};
```

`activated()` 里才 `setGraph(&m_editor.document().graph())`，避免文档不可见时做重活；
`contentWidget()` 幂等，多次调用返回同一个实例。

### 6.8 EditorHost

```cpp
// src/peviewer/editor_host.h
#pragma once

#include <QHash>
#include <QObject>

class Document;
class EditorView;
class QStackedWidget;
class QWidget;
class SARibbonBar;
class SARibbonCategory;

// 把「当前文档的视图」映射成 Ribbon 页签 + viewStack 页面。
class EditorHost : public QObject {
    Q_OBJECT

public:
    EditorHost(SARibbonBar* ribbon, QStackedWidget* stack, QWidget* homePage, QObject* parent = nullptr);

    EditorView* currentView() const { return m_currentView; }

public Q_SLOTS:
    void retranslateUi();

private Q_SLOTS:
    void onDocumentActivated(Document* document);
    void onDocumentClosed(Document* document);
    void onRibbonTabChanged(int index);

private:
    void clearPages();                     // 删掉当前文档的所有页签
    void buildPages(Document* document);   // 为每个视图建页签并挂页面
    void showView(EditorView* view);
    void showHome();
    void selectViewTab(EditorView* view);
    void selectHomeTab();
    EditorView* targetView(Document* document) const;

    SARibbonBar* m_ribbon = nullptr;
    QStackedWidget* m_stack = nullptr;
    QWidget* m_homePage = nullptr;
    QHash<SARibbonCategory*, EditorView*> m_pageViews;   // 只含当前文档
    QHash<const Document*, EditorView*> m_lastView;      // 每个文档最后显示的视图
    EditorView* m_currentView = nullptr;                 // nullptr 表示正停在主页
    bool m_rebuilding = false;
};
```

## 7. 生命周期与所有权

| 对象 | 创建者 | 拥有者 | 释放时机 |
|---|---|---|---|
| `Document` | `DocumentManager::open` | `DocumentManager`（`vector<unique_ptr>`） | `close` / `closeAll` / 退出 |
| `Editor` | 注册的 `EditorFactory` | `Document`（`unique_ptr` 成员） | 随文档销毁 |
| `EditorView` | `Editor::attachRegisteredViews` | `Editor`（`vector<unique_ptr>`） | 随 Editor 销毁 |
| 视图中央控件 | 视图自己 | 视图持有 `QPointer`，加入 `viewStack` 后 `QObject` 父对象是 `viewStack`；窗口销毁时 `EditorHost::detachViews()` 先摘出，视图之后按需重建 | 随窗口或视图销毁；视图在下次 `contentWidget()` 时重建并重新同步数据 |
| Ribbon 页签 `SARibbonCategory` | `EditorHost` | `EditorHost` | 切文档/关文档/退出前 `removeCategory` 后删除 |
| 视图的 `QAction` | 视图自己 | 视图 | 随视图销毁，页签重建不影响 |

关键点：

1. `Document` 的成员声明顺序保证 **Editor（视图）先于依赖图销毁**；
2. `SARibbonBar::removeCategory()` **不会 delete** 页签，必须由 `EditorHost` 删除，否则泄漏；
3. `documentClosed` 是"已移出列表、尚未销毁"，`EditorHost` 在里面把视图控件从 stack 摘除，随后文档析构带走视图；
4. 页签只是 `QAction` 的壳，删除页签不会删除视图持有的 action。

## 8. 打开流程

```
PeViewer::on_actionOpen_triggered
 ├─ QFileDialog::getOpenFileName
 └─ DocumentManager::open(path)                  // 抛异常 → QMessageBox 提示
      ├─ 已打开？find() → activate → return
      ├─ new Document(path, options, editorFactory)
      │    ├─ PeFileDataSource / PeFile / PeFileStore / DependencyGraph   // 失败抛异常
      │    ├─ EditorFactory → PeEditor(document)
      │    └─ Editor::attachRegisteredViews() → 按 order 建 DependencyTreeEditorView
      ├─ push_back / emit documentOpened
      └─ activate → emit documentActivated(document)
            └─ EditorHost::onDocumentActivated
                 ├─ 拆掉上一个文档的页签
                 ├─ 为每个视图建 SARibbonCategory + populateRibbon + addCategoryPage
                 ├─ 视图 contentWidget 加入 viewStack（已在则复用）
                 ├─ 选中该文档上次的视图（首次为首个）
                 └─ 状态栏 ← 当前文档的路径与模块数（PeViewer 拼装）
```

`Document` 构造失败时不会进入列表，因此不存在"半个文档"。这与现有 `Workspace` 的原子性一致。

### 8.1 启动注册顺序

```cpp
int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    Q_INIT_RESOURCE(SARibbonResource);
    // 1. 编辑器工厂：决定 Document 里装哪个 Editor
    DocumentManager::instance()->setEditorFactory([](Document& document) {
        return std::make_unique<PeEditor>(document);
    });
    // 2. 视图注册：决定每个 Editor 有哪些页签
    registerPeEditorViews();                       // 内部向 EditorViewRegistry 注册
    // 3. 再创建窗口
    LanguageManager languageManager;
    PeViewer window(languageManager);
    window.show();
    return app.exec();
}
```

顺序不能反：窗口创建后随时可能触发 `open()`，而 `Document` 构造时会立刻调用工厂与注册表。
注册函数放在 `src/peviewer/editor_registration.cpp`，只依赖 `editor` 与具体视图头文件。

## 9. Ribbon 与中央区联动

### 9.1 页签构成

| 页签 | 来源 | 内容页 |
|---|---|---|
| 「文件」 | 应用按钮（不是页签） | 下拉菜单：打开 / 关闭 |
| 「主页」 | `PeViewer` 固定创建，`objectName = "ribbonHome"` | `.ui` 中的 `homePage`（欢迎 / 最近打开 / 打开文件…） |
| 视图页签 × N | `EditorHost` 按当前文档的视图创建，`objectName = "editorView/" + view->id()` | `view->contentWidget()` |

### 9.2 页签 = 页面

选中的页签决定 `viewStack` 显示哪一页，**主页也不例外**。主页固定是 stack 的第 0 页。

### 9.3 状态表

| 当前状态 | 动作 | 页签 | 中央区 |
|---|---|---|---|
| 无文档 | 启动 | 主页 | `homePage` |
| 无文档 | 打开文档 | 主页 + 视图，自动选中首个/上次视图 | 该视图页面 |
| 文档A，视图页 | 点主页 | 主页 | `homePage`（文档内容被收起） |
| 文档A，主页 | 点视图页签 | 该视图 | 该视图页面 |
| 文档A、B | 组合框切换 | 重建为 B 的页签并选中 B 的上次视图 | B 的上次视图 |
| 关闭非当前文档 | 关闭 | 不变 | 不变 |
| 关闭当前文档（还有其它） | 关闭 | 换成下一个当前文档的页签 | 下一个文档的上次视图 |
| 最后一个文档 | 关闭 | 只剩主页 | `homePage`，关闭动作置灰 |
| 文档无注册视图 | 打开 | 只有主页 | `homePage` + 状态栏提示"该文档没有可用视图" |

### 9.4 建页与拆页

- 每次文档激活**重建**页签，视图对象与中央控件**常驻**（状态不丢）。理由：多文档时若页签常驻，需要 `hideCategory` + `moveCategory` 维护顺序，SARibbon 在隐藏页签组合上有历史缺陷（2.9.6 才修 #160）；而页签本身只是 action 的壳，重建成本可忽略。
- 建页顺序 = 注册表顺序 = 页签顺序 = 视图下标顺序。
- 拆页顺序：`removeCategory(page)` → `delete page`（`removeCategory` 只摘除、不删除）。
- 例外：窗口销毁时中央控件随 `viewStack` 一起销毁，视图用 `QPointer` 感知并在下次 `contentWidget()` 时重建；`PeViewer` 析构会先调用 `EditorHost::detachViews()`，避免窗口带走在其它窗口/文档仍在使用的页面。

### 9.5 代码要点

```cpp
void EditorHost::onRibbonTabChanged(int index) {
    if (m_rebuilding) return;
    SARibbonCategory* page = m_ribbon->categoryByIndex(index);
    if (!page) return;
    if (EditorView* view = m_pageViews.value(page, nullptr)) {
        showView(view);
    } else {
        showHome();                    // 主页等固定页签
    }
}

void EditorHost::showHome() {
    if (m_currentView) {
        m_currentView->deactivated();
        m_currentView = nullptr;
    }
    m_stack->setCurrentWidget(m_homePage);
}

void EditorHost::showView(EditorView* view) {
    if (!view || view == m_currentView) return;
    if (m_currentView) m_currentView->deactivated();
    m_currentView = view;
    if (Document* doc = DocumentManager::instance()->current()) {
        m_lastView.insert(doc, view);
    }
    m_stack->setCurrentWidget(view->contentWidget());
    view->activated();
}
```

### 9.6 语言切换

`PeViewer::changeEvent(QEvent::LanguageChange)`：

1. `ui.retranslateUi(this)`（主页、组合框、状态栏文本）；
2. 主页 category 名称、应用按钮、文件菜单标题；
3. `EditorHost::retranslateUi()` → 遍历 `m_pageViews`：`view->retranslateUi()` 并 `page->setCategoryName(view->title())`。

不重建页签、不重建视图，避免丢状态。

### 9.7 PeViewer 与 EditorHost 的分工

| 关注点 | `PeViewer` | `EditorHost` |
|---|---|---|
| 应用按钮 / 文件菜单 | ✅ 创建与连接 | — |
| 主页页签与 `homePage` | ✅ 创建、翻译 | ✅ 选中主页时显示 `homePage` |
| `viewStack` | ✅ `.ui` 里创建（含 `homePage`） | ✅ 增删视图页面、切当前页 |
| 视图页签 | — | ✅ 建 / 删 / 选 |
| `DocumentManager` 信号 | ✅ 连接 | ✅ 连接 `documentActivated/Closed` |
| 文档组合框 | ✅ 增删、激活 | — |
| 状态栏 | ✅ 由 `current()->path()` 与 `graph().moduleCount()` 拼装 | — |
| 「关闭」动作 enable | ✅ `current() != nullptr` | — |
| 语言切换 | ✅ `ui`、主页、菜单 | ✅ 页签标题与视图文本 |
| 打开失败提示 | ✅ `QMessageBox` | — |

## 10. 关键实现要点与 SARibbon 陷阱

| # | 现象 | 处理 |
|---|---|---|
| 1 | `setCurrentIndex()` 索引未变时**不发** `currentRibbonTabChanged` | 激活文档时 `selectViewTab()` 之后显式 `showView()` |
| 2 | 建页/拆页期间页签栏会自动改选中，触发中间态 | `m_rebuilding` 屏蔽 `onRibbonTabChanged` |
| 3 | `removeCategory()` 只摘除、不删除 | `EditorHost` 负责 `deleteLater()` |
| 4 | 用 `index - 固定页数` 推算视图会错位（以后加固定页/上下文页签） | 用 `categoryByIndex()` + `QHash<SARibbonCategory*, EditorView*>` 查 |
| 5 | 主题 QSS 给应用按钮写了负 padding，导致按钮被拉伸到约 140px | 保留现有 `updateFileButtonWidth()`：按文字宽度 `setFixedWidth` |
| 6 | 页签 `objectName` 重复会导致以后的自定义/持久化冲突 | `"editorView/" + view->id()`，视图 id 在注册时唯一 |

另外：`addCategoryPage()` 会把页签 reparent 进 ribbon 的 stacked container，页签的生命周期从此归 ribbon 管；
`Document`/`Editor` 只持有数据与视图，不持有任何 Ribbon 控件。

## 11. 错误处理与边界

| 场景 | 行为 |
|---|---|
| 打开失败（解析/依赖构建/Editor 工厂抛异常） | 异常透传给 `PeViewer`，弹 `QMessageBox`；列表与当前不变 |
| 重复打开同一路径 | 复用并激活，不重建 |
| 未安装 `EditorFactory` | 文档仍能打开，但 `editor()` 为 nullptr、没有视图页签（仅出现在分阶段实施的中间态和纯数据测试里；生产环境由 `main()` 保证安装） |
| 注册表为空 | 文档没有视图，停在主页 + 状态栏提示 |
| 关闭时 `canClose() == false` | 取消关闭，保持现状 |
| 关闭最后一个文档 | `documentActivated(nullptr)` → 主页 + 关闭动作置灰 |
| 语言切换时页签不重建 | 只改文本 |
| 大文件 | v1 同步打开；后续可加 `openAsync` + 进度信号，`open()` 语义不变 |

## 12. 迁移步骤

### P1 纯重命名（行为零变化，单独提交）— 已完成

1. `git mv src/workspace src/document`；`workspace.h/.cpp` → `document.h/.cpp`；`workspace_manager.*` → `document_manager.*`；
2. 类名 `Workspace` → `Document`、`WorkspaceManager` → `DocumentManager`；信号 `workspaceOpened` → `documentOpened` 等；`workspaces()` → `documents()`；
3. `src/CMakeLists.txt`、`src/document/CMakeLists.txt`（target `document`）、`tests/CMakeLists.txt`（`WorkspaceTests` → `DocumentTests`）、`src/peviewer/CMakeLists.txt` 同步；
4. `tests/workspace_tests.cpp` → `tests/document_tests.cpp`，`tests/pe_viewer_ui_tests.cpp` 里 `workspaceCombo` → `documentCombo`；
5. UI 文案同步：辅助名 `Workspace` → `Document`、占位符 `Search workspaces...` → `Search documents...`，`.ts` 更新为“文档 / 搜索文档...”；
6. 重写 `src/document/readme.md`，根 `readme.md` 的“工作区”改为“文档”；
7. 验收：全量 `ctest` 通过，界面布局与行为不变（仅组合框文案变为“文档”）。

提交信息：`refactor: 重命名 Workspace 为 Document`。

### P2 框架层 — 已完成

1. 新增 `src/editor`（`Editor`、`EditorView`、`EditorViewRegistry`）；
2. `Document` 增加 `m_editor` 与 `EditorFactory` 参数，末尾创建 Editor 并 `attachRegisteredViews()`；
3. `DocumentManager` 增加 `setEditorFactory` / `currentEditor`；
4. 新增 `tests/editor_tests.cpp`（假 Editor + 假视图）；
5. 验收：`document`/`editor` 测试通过，UI 行为不变（还没人设置工厂，`editor()` 为 nullptr）。

提交信息：`feat: 引入 Editor/EditorView 框架`。

### P3 主窗口接入 — 已完成

1. `PeEditor`、`DependencyTreeEditorView`、`registerPeEditorViews()`；`main()` 里先注册再建窗口；
2. `.ui` 中央区换成 `QStackedWidget`（`viewStack`），第 0 页 `homePage`；
3. `EditorHost`：页签重建、页签→页面映射、`retranslateUi`；
4. `PeViewer` 改为连接 `DocumentManager`，组合框、状态栏、关闭动作、错误提示；
5. 删除 `ui.dependencyTree` 及相关代码（由视图接管）；
6. 更新翻译：主页与视图页签标题；状态栏文案仍留在 `PeViewer` 上下文，无需新增条目；
7. UI 测试按第 13 节补齐。

提交信息：`feat: 文档视图以 Ribbon 页签呈现`。

### P4 收尾 — 已完成

1. 清理 `readme.md`、`src/document/readme.md`、`src/peviewer/` 的说明；
2. 全量构建（`out/build/debug`、`out/build/uitest`）+ `ctest`；
3. 手工冒烟：无文档、单文档多视图、多文档切换、关最后一个、切语言、重复打开。

## 13. 测试计划

### 13.1 `tests/editor_tests.cpp`（框架）

| 用例 | 断言 |
|---|---|
| 注册表 | 空 id / 重复 id 返回 false；`remove` 生效；`clear` 清空 |
| 排序与过滤 | 按 `order`、`id` 排序；`supports` 为假的视图不创建；空 `supports` 全部创建 |
| 挂接 | `attachRegisteredViews` 后 `views()` 数量、顺序、`id()` 正确 |
| 空注册表 | 不挂任何视图，不崩溃 |

### 13.2 `tests/document_tests.cpp`（会话，原 workspace_tests 迁移）

| 用例 | 断言 |
|---|---|
| 单例 / 不可拷贝 | 同现有 static_assert |
| 打开并激活 / OpenOnly | 同现有 |
| 重复打开 | 返回同一指针 |
| 打开失败 | 抛异常且列表、当前不变 |
| Editor 注入 | 设置工厂后 `currentEditor()` 非空；不设置时为 nullptr |
| 关闭顺序 | `documentActivated` 先于 `documentClosed`；关最后一个发 `nullptr` |
| 销毁顺序 | 关闭后视图已析构（用假视图计数） |

### 13.3 `tests/pe_viewer_ui_tests.cpp`（界面）

| 用例 | 断言 |
|---|---|
| 无文档 | 页签只有主页；`viewStack` 当前页是 `homePage`；关闭动作置灰 |
| 打开文档 | 页签 = 主页 + 视图；当前页是首个视图控件；状态栏为「已加载 …：N 个模块」 |
| 点主页 | 页签切到主页，`viewStack` 当前页变 `homePage` |
| 点回视图 | `viewStack` 回到该视图控件 |
| 切文档 | 页签换新文档视图；显示其上次视图 |
| 关文档 | 页签只剩主页；回到 `homePage`；`currentEditor() == nullptr` |
| 无视图文档 | 只有主页，状态栏提示，不崩溃 |
| 语言切换 | 主页/页签标题/视图内动作文本同步更新，页签数量不变 |

测试运行仍用 `QT_QPA_PLATFORM=offscreen`；`EditorTests` 为控制台测试，不需要 `QApplication`（除非构造 `QWidget`）。

## 14. 决策记录

| # | 决策 | 理由 | 备选与代价 |
|---|---|---|---|
| D1 | 只保留一个门面 `DocumentManager`，不引入 `EditorManager` | 文档列表/当前/关闭只记一处账 | 双管理器需要同步两份 `current`，易漂移 |
| D2 | `Document` 持有 `Editor`（而非 `Editor` 包 `Document`） | 打开语义、组合框、关闭顺序都与现有代码一致，改动最小 | 反向持有则 `src/document` 不依赖 UI，但要改 `open()` 返回值与大量测试 |
| D3 | `EditorView` 是控制器对象，不是 `QWidget` 子类 | 可组合现有 `DependencyTreeWidget`；页签与页面解耦 | 直接继承 QWidget 会多一层包装或被迫重构控件 |
| D4 | 页签每次激活重建，视图与页面常驻 | 顺序天然正确，避开 `hideCategory`/`moveCategory` 的坑 | 常驻页签需处理隐藏页签的索引与布局问题 |
| D5 | 点「主页」也切换中央内容 | 页签 = 页面，规则无例外；符合本次需求 | 切语言时文档被收起，需要额外的返回操作 |
| D6 | 主页是全局唯一页面，不做 per-document 主页 | 规则简单，文档间无需重复实现 | 将来可加 `Editor::homeWidget()` 扩展点 |
| D7 | 无视图文档停在主页 + 状态栏提示 | 不引入第三种页面 | 也可以在 `homePage` 里显示提示 |
| D8 | 视图注册在编译期（`main()` 注册） | 简单、可测；插件化不是当前需求 | 动态插件留到后续 `QPluginLoader` |
| D9 | 每个文档记住上次视图（`m_lastView`） | 切回文档时体验一致，成本极低 | 不记忆则总是回到第一个视图 |
| D10 | `Editor` 不提供 `statusText()`，状态栏由 `PeViewer` 生成 | 显示文案属于外壳职责；文档对象保持最小接口（身份 + 视图 + 关闭确认） | 放进 `Editor` 会让文档对象依赖 `QCoreApplication::translate`，并把 UI 文案耦合进数据侧 |

## 15. 后续扩展

- **多视图**：`src/parse` 已有 headers/sections/imports/exports，各自做成 `EditorView` 即可验证 1:N，无需改框架；
- **per-document 主页**：`Editor::homeWidget()` 返回非空时，「主页」页签显示它；
- **插件化**：`QPluginLoader` 加载后向 `EditorViewRegistry` 注册，接口不变；
- **异步打开**：`DocumentManager::openAsync()` + `documentOpening` 进度信号，`open()` 保留同步语义；
- **最近文件**：`homePage` 的列表来自 `QSettings`，与 `DocumentManager` 解耦；
- **拖放打开**：`PeViewer::dragEnterEvent/dropEvent` 直接调 `DocumentManager::open()`。

## 16. 风险与注意事项

1. **`src/document` 变成 UI 依赖**：它现在链接 `editor`（进而 `Qt::Widgets`、SARibbon）。纯数据测试不要传 `EditorFactory`，就不会实例化任何 widget；`DocumentTests` 因此不需要 `QApplication`。
2. **多窗口不是目标**：一个文档只有一个视图控件，第二个窗口激活文档时会把页面从第一个窗口“搬走”；`detachViews()` 保证窗口销毁时不连带删除，但两个窗口同时显示同一文档不支持。
2. **页签重建的中间态**：任何批量增删页签的路径都必须走 `m_rebuilding`，否则会出现内容闪跳。
3. **`m_currentView == nullptr` 合法**：正停在主页时为空，所有使用者都要判空（状态栏、菜单 enable、`retranslateUi`）。
4. **视图契约**：`contentWidget()` 幂等；`populateRibbon()` 可重复调用且只挂自身 action；`title()` 每次取 `tr()`。
5. **语言切换**：只重译、不重建；视图若在 `retranslateUi` 里重建控件会丢状态。
6. **大文件**：同步打开会阻塞 UI，属于已知限制，后续用异步方案解决。

## 17. 验收清单

- [ ] **G1** 无文档启动：页签只有「主页」，「文件」仍是应用按钮菜单，中央区是 `homePage`
- [ ] **G2** 打开文档：出现该文档的 N 个视图页签，自动选中首个（或上次的）视图
- [ ] **G3** 每个视图页签的面板由该视图自己的 `populateRibbon()` 决定，主窗口不写业务面板
- [ ] **G4** 删除 `DependencyTreeEditorView` 的注册后重新构建，程序仍能打开文档（只是没有视图页签），说明框架不依赖具体视图
- [ ] **G5** `DocumentManager` 是唯一对外门面，`EditorManager` 不存在
- [ ] **G6** 一个 `Document` 对应一个 `Editor`，新增一个视图实现注册后自动多出一个页签
- [ ] **G7** 点「主页」中央区切到 `homePage`，点任意视图页签切回该视图
- [ ] 全量 `ctest` 通过（`editor` / `document` / `peviewer` 三组测试）
- [ ] 手工冒烟：无文档、单文档多视图、多文档切换、关最后一个、切语言、重复打开同一文件
