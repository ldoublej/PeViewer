#pragma once

#include <QSet>
#include <QString>
#include <QStringList>

#include "api_set.h"
#include "module_locator.h"

// Empty options provide an isolated search environment. Use systemDefaults() to
// snapshot the local Windows directories, KnownDLLs, API Set schema and PATH.
struct FileLocatorOptions {
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

    static FileLocatorOptions systemDefaults();
};

// Static approximation of the default safe DLL search order. Does not load DLLs
// or emulate SxS, .local, package dependencies or a process's loaded-module list.
class FileModuleLocator final : public ModuleLocator {
public:
    explicit FileModuleLocator(FileLocatorOptions options = FileLocatorOptions::systemDefaults());
    std::optional<LocatedModule> findModule(const QString& dllName) const override;
    std::optional<LocatedModule> findModule(const QString& dllName,
                                           const QString& importer) const override;
    QString findModulePath(const QString& dllName, const QString& importer = {}) const;

    // Canonical absolute path; Windows identity comparisons are case-insensitive.
    // Missing paths throw PeDataSourceError.
    static QString fileIdentity(const QString& path);

private:
    QString systemDirectory() const;
    QStringList searchDirectories() const;
    FileLocatorOptions m_options;
};
