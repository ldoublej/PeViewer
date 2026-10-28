# PeViewer

仅支持小端平台，整数和 UTF-16 代码单元直接按本机布局读取。

界面支持简体中文和英文。在“语言 / Language”菜单中切换后立即生效，
并自动保存选择；首次启动时，中文系统使用简体中文，其他系统使用英文。

翻译文件位于 `src/peviewer/translations/`。构建时，Qt Linguist 的 `lrelease`
会将 `.ts` 编译为 `.qm` 并嵌入程序，开发运行和安装后均无需额外复制应用翻译文件。
Qt 安装需要包含 LinguistTools。

新增或修改界面文本后，先更新翻译条目：

```powershell
cmake --build out/build/debug --target peviewer_lupdate
```

使用 Qt Linguist 编辑 `.ts` 并完成翻译，再重新构建：

```powershell
cmake --build out/build/debug
```

`.ui` 中的界面文本默认可翻译；C++ 中的界面文本使用 `tr("Text")`。
需要随语言切换刷新的 C++ 文本放在 `PeViewer::retranslateUi()` 中。
新增语言时，在 `src/peviewer/CMakeLists.txt` 的 `TS_FILES`、`LanguageManager`
及语言菜单中注册对应语言。

## clang-tidy

根目录 `.clang-tidy` 配置了静态分析、常见错误、性能及部分 C++17 检查。
检查不自动修改代码，已有诊断暂不作为错误。Qt SDK、构建输出和 `src/pestructs`
中的系统结构定义不输出头文件诊断。

clang-tidy 通过 VS Code 或命令行单独运行，不接入 CMake 构建流程。
CMake 导出 `compile_commands.json`，供工具获取头文件路径、宏定义和编译参数。
先配置并构建一次，生成编译参数及 Qt 自动生成的头文件：

```powershell
cmake --preset Debug-x64
cmake --build out/build/debug
```

Windows 下在 Visual Studio x64 开发者终端中执行，使 clang-tidy 能找到 MSVC 和
Windows SDK 头文件。将 clang-tidy 所在目录加入 PATH，或使用其完整路径，单独检查源文件：

```powershell
clang-tidy src/parse/pe_file.cpp -p out/build/debug --extra-arg-before=--driver-mode=cl --extra-arg=/EHsc
```

VS Code 可在 `.vscode/settings.json` 中启用 C/C++ 扩展的 clang-tidy，使用
CMake Tools 提供编译参数：

```json
{
    "C_Cpp.default.configurationProvider": "ms-vscode.cmake-tools",
    "C_Cpp.codeAnalysis.clangTidy.enabled": true
}
```

执行 `C/C++: Run Code Analysis on All Files` 可检查所有项目源文件。
`.vscode` 目录按仓库现有规则忽略，不随代码提交。

规则选项参考 [clang-tidy 官方文档](https://clang.llvm.org/extra/clang-tidy/)。
