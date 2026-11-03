#include "dependency_graph.h"

#include <QFileInfo>
#include <algorithm>
#include <stdexcept>
#include <utility>

namespace {
std::unique_ptr<ModuleLocator> fileLocator(const PeFile& root, FileLocatorOptions options) {
    if (!root.isParsed()) {
        throw std::invalid_argument("Dependency root must already be parsed");
    }
    const auto path = FileModuleLocator::fileIdentity(root.dataSource().url());
    options.appDirectory = QFileInfo(path).absolutePath();
    options.isPe32Plus = root.isPe32Plus();
    return std::make_unique<FileModuleLocator>(std::move(options));
}

DependencyError currentError(DependencyError::Code code, const std::exception& error) {
    return {code, QString::fromUtf8(error.what()), std::current_exception(), false, false};
}
}  // namespace

ModuleNode::ModuleNode(QString identity, const PeFile& borrowed)
    : m_identity(std::move(identity)), m_file(&borrowed) {}

ModuleNode::ModuleNode(LocatedModule resolved)
    : m_identity(std::move(resolved.identity)),
      m_source(std::move(resolved.source)),
      m_ownedFile(std::make_unique<PeFile>(*m_source)),
      m_file(m_ownedFile.get()) {
    m_ownedFile->parse();
}

DependencyGraph::DependencyGraph(const PeFile& root, FileLocatorOptions options)
    : DependencyGraph(root, FileModuleLocator::fileIdentity(root.dataSource().url()),
                      fileLocator(root, std::move(options))) {}

DependencyGraph::DependencyGraph(const PeFile& root, QString rootIdentity,
                                 std::unique_ptr<ModuleLocator> locator)
    : m_locator(std::move(locator)) {
    if (!root.isParsed() || rootIdentity.isEmpty() || !m_locator) {
        throw std::invalid_argument(
            "DependencyGraph requires a parsed root, identity and locator");
    }
    m_modules.push_back(
        std::unique_ptr<ModuleNode>(new ModuleNode(std::move(rootIdentity), root)));
}

DependencyGraph::Resolution DependencyGraph::resolve(const QString& name) {
    std::optional<LocatedModule> found;
    try {
        found = m_locator->findModule(name);
    } catch (const PeDataSourceError& error) {
        return {nullptr, currentError(DependencyError::Code::ReadError, error)};
    }
    if (!found) {
        return {nullptr, DependencyError{DependencyError::Code::NotFound,
                                         QStringLiteral("Module not found: %1").arg(name),
                                         {},
                                         false,
                                         false}};
    }
    if (found->identity.isEmpty() || !found->source) {
        throw std::invalid_argument("ModuleLocator returned an empty identity or data source");
    }
    const auto cached = m_resolutions.constFind(found->identity);
    if (cached != m_resolutions.cend()) {
        return *cached;
    }

    const auto identity = found->identity;
    Resolution resolution;
    try {
        std::unique_ptr<ModuleNode> module(new ModuleNode(std::move(*found)));
        const bool expected = root().pe_file().isPe32Plus();
        const bool actual = module->pe_file().isPe32Plus();
        if (expected != actual) {
            resolution.error = DependencyError{
                DependencyError::Code::ArchitectureMismatch,
                QStringLiteral("Module bitness mismatch: expected %1-bit, found %2-bit")
                    .arg(expected ? 64 : 32)
                    .arg(actual ? 64 : 32),
                {},
                expected,
                actual};
        } else {
            resolution.module = module.get();
            m_modules.push_back(std::move(module));
        }
    } catch (const ParseError& error) {
        resolution.error = currentError(DependencyError::Code::ParseError, error);
    } catch (const PeDataSourceError& error) {
        resolution.error = currentError(DependencyError::Code::ReadError, error);
    }
    m_resolutions.insert(identity, resolution);
    return resolution;
}

void DependencyGraph::build() {
    m_modules.resize(1);
    auto* rootModule = m_modules.front().get();
    rootModule->m_dependencies.clear();
    rootModule->m_dependents.clear();
    m_resolutions.clear();
    m_resolutions.insert(rootModule->identity(), Resolution{rootModule, std::nullopt});

    // New nodes are appended once and visited in breadth-first order. Identity
    // caching breaks cycles and naturally merges shared dependencies and aliases.
    for (std::size_t index = 0; index < m_modules.size(); ++index) {
        auto* parent = m_modules[index].get();
        const auto& imports = parent->pe_file().imports();
        parent->m_dependencies.reserve(imports.size());
        for (const auto& imported : imports) {
            const auto result = resolve(imported.dllName());
            parent->m_dependencies.push_back({imported.dllName(), result.module, result.error});
            if (result.module) {
                auto& dependents = result.module->m_dependents;
                if (std::find(dependents.begin(), dependents.end(), parent) == dependents.end()) {
                    dependents.push_back(parent);
                }
            }
        }
    }
}
