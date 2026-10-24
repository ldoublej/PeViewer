# PeViewer

界面支持简体中文和英文。在“语言 / Language”菜单中切换后立即生效，
并自动保存选择；首次启动时，中文系统使用简体中文，其他系统使用英文。

翻译文件位于 `PeViewer/translations/`。构建时，Qt Linguist 的 `lrelease`
会将 `.ts` 编译为 `.qm` 并嵌入程序，开发运行和安装后均无需额外复制应用翻译文件。
Qt 安装需要包含 LinguistTools。

新增或修改界面文本后，先更新翻译条目：

```powershell
cmake --build out/build/debug --target PeViewer_lupdate
```

使用 Qt Linguist 编辑 `.ts` 并完成翻译，再重新构建：

```powershell
cmake --build out/build/debug
```

`.ui` 中的界面文本默认可翻译；C++ 中的界面文本使用 `tr("Text")`。
需要随语言切换刷新的 C++ 文本放在 `PeViewer::retranslateUi()` 中。
新增语言时，在 `PeViewer/CMakeLists.txt` 的 `TS_FILES`、`LanguageManager`
及语言菜单中注册对应语言。
