#include "pe_editor.h"

#include "document.h"

PeEditor::PeEditor(Document& document) : Editor(document.path()), m_document(document) {}
