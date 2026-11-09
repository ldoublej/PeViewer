#include "workspace.h"

#include <QFileInfo>
#include <utility>

#include "dependency_graph.h"
#include "pe_file.h"
#include "pe_file_data_source.h"

Workspace::Workspace(const QString& path, FileLocatorOptions options)
    : m_path(QFileInfo(path).absoluteFilePath()),
      m_dataSource(std::make_unique<PeFileDataSource>(path)),
      m_peFile(std::make_unique<PeFile>(*m_dataSource)) {
    m_peFile->parse();
    m_graph = std::make_unique<DependencyGraph>(*m_peFile, m_files, std::move(options));
    m_graph->build();
}

Workspace::~Workspace() = default;

const PeFile& Workspace::peFile() const { return *m_peFile; }

const DependencyGraph& Workspace::graph() const { return *m_graph; }
