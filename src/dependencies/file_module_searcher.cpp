#include "file_module_searcher.h"

#include <pe_file_data_source.h>

#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <utility>

#ifdef Q_OS_WIN
#include <Windows.h>

#include <system_error>
#include <vector>
#endif

namespace {
QString normalizedPath(QString path) { return path.replace(QLatin1Char('\\'), QLatin1Char('/')); }

QString existingFile(const QString& path) {
    const QFileInfo file(path);
    if (file.isFile()) {
        return file.canonicalFilePath();
    }
#ifndef Q_OS_WIN
    // Preserve Windows DLL name matching when inspecting files on another OS.
    for (const auto& candidate : file.dir().entryInfoList(QDir::Files)) {
        if (candidate.fileName().compare(file.fileName(), Qt::CaseInsensitive) == 0) {
            return candidate.canonicalFilePath();
        }
    }
#endif
    return {};
}

#ifdef Q_OS_WIN
QString queryWindowsDirectory(UINT(WINAPI* query)(LPWSTR, UINT), bool optional = false) {
    std::vector<wchar_t> buffer(MAX_PATH);
    for (;;) {
        const auto length = query(buffer.data(), static_cast<UINT>(buffer.size()));
        if (length == 0) {
            if (optional && GetLastError() == ERROR_CALL_NOT_IMPLEMENTED) {
                return {};
            }
            throw std::system_error(static_cast<int>(GetLastError()), std::system_category(),
                                    "Cannot query Windows system directory");
        }
        if (length < buffer.size()) {
            return normalizedPath(QString::fromWCharArray(buffer.data(), static_cast<int>(length)));
        }
        buffer.resize(length);
    }
}
#endif
}  // namespace

FileSearchOptions FileSearchOptions::systemDefaults() {
    FileSearchOptions options;
#ifdef Q_OS_WIN
    options.windowsDirectory = queryWindowsDirectory(GetWindowsDirectoryW);
    options.system64Directory = queryWindowsDirectory(GetSystemDirectoryW);
    options.system32Directory = queryWindowsDirectory(GetSystemWow64DirectoryW, true);
    if (options.system32Directory.isEmpty()) {
        options.system32Directory = options.system64Directory;
    }
    BOOL wow64 = FALSE;
    if (IsWow64Process(GetCurrentProcess(), &wow64) && wow64) {
        options.system64Directory =
            QDir(options.windowsDirectory).filePath(QStringLiteral("Sysnative"));
    }
    QSettings registry(QStringLiteral("HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Control\\"
                                      "Session Manager\\KnownDLLs"),
                       QSettings::NativeFormat);
    for (const auto& key : registry.childKeys()) {
        const auto name = registry.value(key).toString();
        if (name.endsWith(QStringLiteral(".dll"), Qt::CaseInsensitive)) {
            options.knownDlls.insert(name.toLower());
        }
    }
    if (auto map = ApiSetMap::load(
            QDir(options.system64Directory).filePath(QStringLiteral("apisetschema.dll")))) {
        options.apiSet = std::move(*map);
    }
#endif
    options.environmentPaths =
        qEnvironmentVariable("PATH").split(QDir::listSeparator(), Qt::SkipEmptyParts);
    return options;
}

FileModuleSearcher::FileModuleSearcher(FileSearchOptions options) : m_options(std::move(options)) {
    QSet<QString> normalized;
    for (const auto& name : m_options.knownDlls) {
        normalized.insert(name.toLower());
    }
    m_options.knownDlls = std::move(normalized);
}

QString FileModuleSearcher::fileIdentity(const QString& path) {
    auto identity = QFileInfo(path).canonicalFilePath();
    if (identity.isEmpty() || !QFileInfo(identity).isFile()) {
        throw PeDataSourceError(PeDataSourceError::Code::Io,
                                QStringLiteral("Cannot identify module: %1").arg(path));
    }
#ifdef Q_OS_WIN
    identity = identity.toLower();
#endif
    return identity;
}

QString FileModuleSearcher::systemDirectory() const {
    return m_options.isPe32Plus ? m_options.system64Directory : m_options.system32Directory;
}

QStringList FileModuleSearcher::searchDirectories() const {
    QStringList directories;
    const auto append = [&](const QString& directory) {
        if (!directory.isEmpty()) {
            directories.append(normalizedPath(directory));
        }
    };
    append(m_options.appDirectory);
    append(systemDirectory());
    if (!m_options.windowsDirectory.isEmpty()) {
        append(QDir(m_options.windowsDirectory).filePath(QStringLiteral("System")));
        append(m_options.windowsDirectory);
    }
    append(m_options.workDirectory);
    for (auto path : m_options.environmentPaths) {
        if (path.startsWith(QLatin1Char('"')) && path.endsWith(QLatin1Char('"'))) {
            path = path.mid(1, path.size() - 2);
        }
        path = normalizedPath(path);
        if (path.isEmpty()) {
            continue;
        }
        if (QDir::isAbsolutePath(path)) {
            append(path);
        } else if (!m_options.workDirectory.isEmpty()) {
            append(QDir(m_options.workDirectory).filePath(path));
        }
    }
    for (const auto& path : m_options.fallbackDirectories) {
        append(path);
    }
    return directories;
}

QString FileModuleSearcher::findModulePath(const QString& dllName) const {
    if (dllName.isEmpty() || dllName.contains(QChar(0))) {
        return {};
    }
    auto name = normalizedPath(dllName);
    if (QDir::isAbsolutePath(name)) {
        return existingFile(name);
    }
    if (name.contains(QLatin1Char('/')) || name.contains(QLatin1Char(':')) ||
        name == QStringLiteral(".")) {
        return m_options.workDirectory.isEmpty()
                   ? QString{}
                   : existingFile(QDir(m_options.workDirectory).filePath(name));
    }
    const auto host = m_options.apiSet.resolveHost(name);
    if (!host.isEmpty()) {
        name = host;
    }
    if (m_options.knownDlls.contains(name.toLower())) {
        const auto directory = systemDirectory();
        return directory.isEmpty() ? QString{} : existingFile(QDir(directory).filePath(name));
    }
    for (const auto& directory : searchDirectories()) {
        auto path = existingFile(QDir(directory).filePath(name));
        if (!path.isEmpty()) {
            return path;
        }
    }
    return {};
}

std::optional<ResolvedModule> FileModuleSearcher::findModule(const QString& dllName) const {
    const auto path = findModulePath(dllName);
    if (path.isEmpty()) {
        return std::nullopt;
    }
    return ResolvedModule{fileIdentity(path), std::make_unique<PeFileDataSource>(path)};
}
