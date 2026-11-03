#pragma once

#include <pe_file.h>

#include <QHash>
#include <QString>
#include <cstdint>
#include <exception>
#include <memory>
#include <optional>
#include <vector>

#include "file_module_searcher.h"

class DependencyModule;

struct DependencyError {
    enum class Code : std::int8_t { NotFound, ReadError, ParseError, ArchitectureMismatch };
    Code code = Code::NotFound;
    QString message;
    // Preserves the original ParseError/PeDataSourceError and its structured details.
    std::exception_ptr exception;
    bool expectedPe32Plus = false;
    bool actualPe32Plus = false;
};

struct DependencyEdge {
    QString dllName;
    const DependencyModule* module = nullptr;
    // Present exactly when module is null. A bad edge does not stop other modules.
    std::optional<DependencyError> error;
    bool isResolved() const { return module != nullptr; }
};

// The graph owns nodes. References/pointers are borrowed; rebuilding invalidates
// all child nodes and edges, while the root node remains at the same address.
class DependencyModule {
public:
    const QString& identity() const { return m_identity; }
    const PeFile& file() const { return *m_file; }
    const std::vector<DependencyEdge>& dependencies() const { return m_dependencies; }
    const std::vector<const DependencyModule*>& dependents() const { return m_dependents; }

private:
    friend class DependencyGraph;
    DependencyModule(QString identity, const PeFile& borrowed);
    explicit DependencyModule(ResolvedModule resolved);
    QString m_identity;
    // Destruction order keeps the source alive until after its PeFile is destroyed.
    std::unique_ptr<PeDataSource> m_source;
    std::unique_ptr<PeFile> m_ownedFile;
    const PeFile* m_file = nullptr;
    std::vector<DependencyEdge> m_dependencies;
    std::vector<const DependencyModule*> m_dependents;
};

class DependencyGraph {
public:
    // root must already be parsed. It and its data source must outlive the graph.
    // Application directory and target bitness are derived from root.
    explicit DependencyGraph(const PeFile& root,
                             FileSearchOptions options = FileSearchOptions::systemDefaults());
    // A custom searcher can supply any file-layout source, including in-memory test data.
    DependencyGraph(const PeFile& root, QString rootIdentity,
                    std::unique_ptr<ModuleSearcher> searcher);
    void build();
    const DependencyModule& root() const { return *m_modules.front(); }
    std::size_t moduleCount() const { return m_modules.size(); }

private:
    Q_DISABLE_COPY_MOVE(DependencyGraph)
    struct Resolution {
        DependencyModule* module = nullptr;
        std::optional<DependencyError> error;
    };
    Resolution resolve(const QString& name);
    std::unique_ptr<ModuleSearcher> m_searcher;
    std::vector<std::unique_ptr<DependencyModule>> m_modules;
    QHash<QString, Resolution> m_resolutions;
};
