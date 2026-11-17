#pragma once

// 注册 PE 文档的 EditorFactory 与全部视图实现。
// 必须在创建主窗口之前调用，否则 Document 打开时拿不到 Editor。
void registerPeEditorViews();
