#pragma once

#include <pe_file.h>

#include <QHash>
#include <QString>
#include <cstdint>
#include <exception>
#include <memory>
#include <optional>
#include <vector>

#include "file_module_locator.h"
#include "pe_file_store.h"

class ModuleNode;

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
    const ModuleNode* module = nullptr;
    // Present exactly when module is null. A bad edge does not stop other modules.
    std::optional<DependencyError> error;
    bool isResolved() const { return module != nullptr; }
};

// The graph owns nodes; every node borrows its PeFile from the graph's store, or
// from the caller in the root's case. Rebuilding invalidates all child nodes and
// edges, while the root node remains at the same address.
class ModuleNode {
public:
    const QString& identity() const { return m_identity; }
    const PeFile& pe_file() const { return *m_file; }
    const std::vector<DependencyEdge>& dependencies() const { return m_dependencies; }
    const std::vector<const ModuleNode*>& dependents() const { return m_dependents; }

private:
    friend class DependencyGraph;
    ModuleNode(QString identity, const PeFile& borrowed);
    QString m_identity;
    const PeFile* m_file = nullptr;
    std::vector<DependencyEdge> m_dependencies;
    std::vector<const ModuleNode*> m_dependents;
};

class DependencyGraph {
public:
    // root must already be parsed. It and its data source must outlive the graph.
    // Application directory and target bitness are derived from root.
    explicit DependencyGraph(const PeFile& root,
                             FileLocatorOptions options = FileLocatorOptions::systemDefaults());
    // A custom locator can supply any file-layout source, including in-memory test data.
    DependencyGraph(const PeFile& root, QString rootIdentity,
                    std::unique_ptr<ModuleLocator> locator);
    void build();
    const ModuleNode& root() const { return *m_modules.front(); }
    std::size_t moduleCount() const { return m_modules.size(); }
    // Parsed dependency files created during build(); the root file is not stored.
    const PeFileStore& files() const { return m_files; }

private:
    Q_DISABLE_COPY_MOVE(DependencyGraph)
    struct Resolution {
        ModuleNode* module = nullptr;
        std::optional<DependencyError> error;
    };
    Resolution resolve(const QString& name);
    // Declared before the nodes so stored files outlive every ModuleNode.
    PeFileStore m_files;
    std::unique_ptr<ModuleLocator> m_locator;
    std::vector<std::unique_ptr<ModuleNode>> m_modules;
    QHash<QString, Resolution> m_resolutions;
};
