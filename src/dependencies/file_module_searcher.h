#pragma once

#include <QSet>
#include <QString>
#include <QStringList>

#include "api_set.h"
#include "module_searcher.h"

// Empty options provide an isolated search environment. Use systemDefaults() to
// snapshot the local Windows directories, KnownDLLs, API Set schema and PATH.
struct FileSearchOptions {
    QString appDirectory;
    QString workDirectory;
    QString system32Directory;
    QString system64Directory;
    QString windowsDirectory;
    QStringList environmentPaths;
    QStringList fallbackDirectories;
    QSet<QString> knownDlls;
    ApiSetMap apiSet;
    bool isPe32Plus = sizeof(void*) == 8;

    static FileSearchOptions systemDefaults();
};

// Static approximation of the default safe DLL search order. Does not load DLLs
// or emulate SxS, .local, package dependencies or a process's loaded-module list.
class FileModuleSearcher final : public ModuleSearcher {
public:
    explicit FileModuleSearcher(FileSearchOptions options = FileSearchOptions::systemDefaults());
    std::optional<ResolvedModule> findModule(const QString& dllName) const override;
    QString findModulePath(const QString& dllName) const;

    // Canonical absolute path; Windows identity comparisons are case-insensitive.
    // Missing paths throw PeDataSourceError.
    static QString fileIdentity(const QString& path);

private:
    QString systemDirectory() const;
    QStringList searchDirectories() const;
    FileSearchOptions m_options;
};
