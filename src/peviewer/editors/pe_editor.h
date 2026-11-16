#pragma once

#include <editor.h>

class Document;

// PE 文档的编辑器：为视图提供类型化的数据入口，显示细节归主窗口。
class PeEditor : public Editor {
public:
    explicit PeEditor(Document& document);

    const Document& document() const { return m_document; }

private:
    Document& m_document;
};
