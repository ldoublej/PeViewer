#include "document.h"

#include <QFileInfo>
#include <utility>

#include "dependency_graph.h"
#include "editor.h"
#include "pe_file.h"
#include "pe_file_data_source.h"

Document::Document(const QString& path, FileLocatorOptions options,
                   EditorFactory editorFactory)
    : m_path(QFileInfo(path).absoluteFilePath()),
      m_dataSource(std::make_unique<PeFileDataSource>(path)),
      m_peFile(std::make_unique<PeFile>(*m_dataSource)) {
    m_peFile->parse();
    m_graph = std::make_unique<DependencyGraph>(*m_peFile, m_files, std::move(options));
    m_graph->build();
    if (editorFactory) {
        m_editor = editorFactory(*this);
        m_editor->attachRegisteredViews();
    }
}

Document::~Document() = default;

const PeFile& Document::peFile() const { return *m_peFile; }

const DependencyGraph& Document::graph() const { return *m_graph; }
