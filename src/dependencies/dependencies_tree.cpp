#include "dependencies_tree.h"

#include <QFileInfo>
#include <algorithm>
#include <stdexcept>
#include <utility>

namespace {
std::unique_ptr<ModuleSearcher> fileSearcher(const PeFile& root, FileSearchOptions options) {
    if (!root.isParsed()) {
        throw std::invalid_argument("Dependency root must already be parsed");
    }
    const auto path = FileModuleSearcher::fileIdentity(root.dataSource().url());
    options.appDirectory = QFileInfo(path).absolutePath();
    options.isPe32Plus = root.isPe32Plus();
    return std::make_unique<FileModuleSearcher>(std::move(options));
}

DependencyIssue currentIssue(DependencyIssue::Code code, const std::exception& error) {
    return {code, QString::fromUtf8(error.what()), std::current_exception(), false, false};
}
}  // namespace

DependencyModule::DependencyModule(QString identity, const PeFile& borrowed)
    : m_identity(std::move(identity)), m_file(&borrowed) {}

DependencyModule::DependencyModule(ResolvedModule resolved)
    : m_identity(std::move(resolved.identity)),
      m_source(std::move(resolved.source)),
      m_ownedFile(std::make_unique<PeFile>(*m_source)),
      m_file(m_ownedFile.get()) {
    m_ownedFile->parse();
}

DependenciesTree::DependenciesTree(const PeFile& root, FileSearchOptions options)
    : DependenciesTree(root, FileModuleSearcher::fileIdentity(root.dataSource().url()),
                       fileSearcher(root, std::move(options))) {}

DependenciesTree::DependenciesTree(const PeFile& root, QString rootIdentity,
                                   std::unique_ptr<ModuleSearcher> searcher)
    : m_searcher(std::move(searcher)) {
    if (!root.isParsed() || rootIdentity.isEmpty() || !m_searcher) {
        throw std::invalid_argument(
            "DependenciesTree requires a parsed root, identity and searcher");
    }
    m_modules.push_back(
        std::unique_ptr<DependencyModule>(new DependencyModule(std::move(rootIdentity), root)));
}

DependenciesTree::Resolution DependenciesTree::resolve(const QString& name) {
    std::optional<ResolvedModule> found;
    try {
        found = m_searcher->findModule(name);
    } catch (const PeDataSourceError& error) {
        return {nullptr, currentIssue(DependencyIssue::Code::ReadError, error)};
    }
    if (!found) {
        return {nullptr, DependencyIssue{DependencyIssue::Code::NotFound,
                                         QStringLiteral("Module not found: %1").arg(name),
                                         {},
                                         false,
                                         false}};
    }
    if (found->identity.isEmpty() || !found->source) {
        throw std::invalid_argument("ModuleSearcher returned an empty identity or data source");
    }
    const auto cached = m_resolutions.constFind(found->identity);
    if (cached != m_resolutions.cend()) {
        return *cached;
    }

    const auto identity = found->identity;
    Resolution resolution;
    try {
        std::unique_ptr<DependencyModule> module(new DependencyModule(std::move(*found)));
        const bool expected = root().file().isPe32Plus();
        const bool actual = module->file().isPe32Plus();
        if (expected != actual) {
            resolution.issue = DependencyIssue{
                DependencyIssue::Code::ArchitectureMismatch,
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
        resolution.issue = currentIssue(DependencyIssue::Code::ParseError, error);
    } catch (const PeDataSourceError& error) {
        resolution.issue = currentIssue(DependencyIssue::Code::ReadError, error);
    }
    m_resolutions.insert(identity, resolution);
    return resolution;
}

void DependenciesTree::build() {
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
        const auto& imports = parent->file().imports();
        parent->m_dependencies.reserve(imports.size());
        for (const auto& imported : imports) {
            const auto result = resolve(imported.dllName());
            parent->m_dependencies.push_back({imported.dllName(), result.module, result.issue});
            if (result.module) {
                auto& dependents = result.module->m_dependents;
                if (std::find(dependents.begin(), dependents.end(), parent) == dependents.end()) {
                    dependents.push_back(parent);
                }
            }
        }
    }
}
