#pragma once

#include <QString>
#include <memory>

#include "file_module_locator.h"
#include "pe_file_store.h"

class DependencyGraph;
class PeFile;
class PeFileDataSource;

// One opened session: owns the root file, its data source, the dependency files
// and the graph built from them. Construction loads and parses the file, then
// builds the dependency graph; failures are reported as exceptions.
class Workspace {
public:
    explicit Workspace(const QString& path,
                       FileLocatorOptions options = FileLocatorOptions::systemDefaults());
    ~Workspace();

    Q_DISABLE_COPY_MOVE(Workspace)

    // Absolute path of the opened file.
    const QString& path() const { return m_path; }
    const PeFile& peFile() const;
    const DependencyGraph& graph() const;
    const PeFileStore& files() const { return m_files; }

private:
    QString m_path;
    // Destruction order matters: the graph borrows the parsed root file, which
    // borrows the data source; the graph is destroyed before the store and root.
    std::unique_ptr<PeFileDataSource> m_dataSource;
    std::unique_ptr<PeFile> m_peFile;
    PeFileStore m_files;
    std::unique_ptr<DependencyGraph> m_graph;
};
