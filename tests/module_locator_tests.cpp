#include <file_module_locator.h>

#include <QTemporaryDir>

#include "dependencies_test_support.h"

using namespace DependencyTests;

namespace {
void tests() {
    QTemporaryDir temporary;
    require(temporary.isValid(), "Temporary directory failed");
    const QDir dir(temporary.path());
    FileLocatorOptions options;
    options.isPe32Plus = true;
    options.appDirectory = dir.filePath(QStringLiteral("app"));
    options.workDirectory = dir.filePath(QStringLiteral("work"));
    options.system32Directory = dir.filePath(QStringLiteral("system32"));
    options.system64Directory = dir.filePath(QStringLiteral("system64"));
    options.windowsDirectory = dir.filePath(QStringLiteral("windows"));
    options.environmentPaths =
        QStringList{dir.filePath(QStringLiteral("path1")), dir.filePath(QStringLiteral("path2"))};
    options.fallbackDirectories = QStringList{dir.filePath(QStringLiteral("extra"))};
    const QStringList order = {options.appDirectory,
                               options.system64Directory,
                               QDir(options.windowsDirectory).filePath(QStringLiteral("System")),
                               options.windowsDirectory,
                               options.workDirectory,
                               options.environmentPaths[0],
                               options.environmentPaths[1],
                               options.fallbackDirectories[0]};
    for (const auto& path : order) {
        writeFile(QDir(path).filePath(QStringLiteral("dependency.dll")), QByteArray("MZ"));
    }
    FileModuleLocator locator(options);
    for (const auto& path : order) {
        const auto file = QDir(path).filePath(QStringLiteral("dependency.dll"));
        require(FileModuleLocator::fileIdentity(locator.findModulePath(
                    QStringLiteral("DEPENDENCY.DLL"))) == FileModuleLocator::fileIdentity(file),
                "Safe DLL search order is incorrect");
        auto found = locator.findModule(QStringLiteral("dependency.dll"));
        require(found && found->source->readBytes(0, 2) == QByteArray("MZ"),
                "Source cannot be read");
        require(found->identity == FileModuleLocator::fileIdentity(file),
                "Module identity is not canonical");
        found.reset();
        require(QFile::remove(file), "Cannot remove search-order fixture");
    }
    require(!locator.findModule(QStringLiteral("dependency.dll")), "Missing module was resolved");
    require(locator.findModulePath(QString{}).isEmpty(), "Empty module name was accepted");

    // KnownDLLs never falls back to an application-local shadow copy.
    options.knownDlls = {QStringLiteral("KNOWN.DLL")};
    writeFile(QDir(options.appDirectory).filePath(QStringLiteral("known.dll")));
    require(FileModuleLocator(options).findModulePath(QStringLiteral("known.dll")).isEmpty(),
            "KnownDLL fell back to an application-local copy");
    const auto known64 = QDir(options.system64Directory).filePath(QStringLiteral("known.dll"));
    const auto known32 = QDir(options.system32Directory).filePath(QStringLiteral("known.dll"));
    writeFile(known64);
    writeFile(known32);
    require(FileModuleLocator(options).findModulePath(QStringLiteral("known.dll")) ==
                QFileInfo(known64).canonicalFilePath(),
            "64-bit KnownDLL search failed");
    options.isPe32Plus = false;
    require(FileModuleLocator(options).findModulePath(QStringLiteral("known.dll")) ==
                QFileInfo(known32).canonicalFilePath(),
            "32-bit KnownDLL search failed");

    // A contract and its host have one identity; explicit paths bypass the map.
    options.apiSet.addMapping(QStringLiteral("api-test-l1-1-1"), QStringLiteral("known.dll"));
    FileModuleLocator apiLocator(options);
    auto contract = apiLocator.findModule(QStringLiteral("API-TEST-L1-1-0.DLL"));
    auto host = apiLocator.findModule(QStringLiteral("known.dll"));
    require(contract && host && contract->identity == host->identity,
            "API Set host identity differs");
    const auto explicitFile =
        QDir(options.appDirectory).filePath(QStringLiteral("api-test-l1-1-0.dll"));
    writeFile(explicitFile);
    require(apiLocator.findModulePath(explicitFile) == QFileInfo(explicitFile).canonicalFilePath(),
            "Explicit path was redirected through API Set");
    require(apiLocator.findModulePath(dir.filePath(QStringLiteral("absent/known.dll"))).isEmpty(),
            "Missing explicit path fell back to a DLL name search");
    require(apiLocator.findModulePath(options.appDirectory).isEmpty(),
            "Directory was accepted as a module");
    const auto unmappedFile =
        QDir(options.appDirectory).filePath(QStringLiteral("api-local-l1-1-0.dll"));
    writeFile(unmappedFile);
    require(apiLocator.findModulePath(QStringLiteral("api-local-l1-1-0.dll")) ==
                QFileInfo(unmappedFile).canonicalFilePath(),
            "Unmapped API Set name did not fall back to ordinary file search");

    const auto relativeFile =
        QDir(options.workDirectory).filePath(QStringLiteral("relative/other.dll"));
    writeFile(relativeFile);
    options.environmentPaths = QStringList{QStringLiteral("relative")};
    require(FileModuleLocator(options).findModulePath(QStringLiteral("other.dll")) ==
                QFileInfo(relativeFile).canonicalFilePath(),
            "Relative PATH entry did not use target work directory");
    require(FileModuleLocator(options).findModulePath(QStringLiteral("relative\\other.dll")) ==
                QFileInfo(relativeFile).canonicalFilePath(),
            "Explicit relative file did not use target work directory");
    options.workDirectory.clear();
    require(
        FileModuleLocator(options).findModulePath(QStringLiteral("relative/other.dll")).isEmpty(),
        "Relative path used the viewer working directory");
    require(FileModuleLocator(options).findModulePath(QStringLiteral("other.dll")).isEmpty(),
            "Relative PATH entry used the viewer working directory");

    auto contextualOptions = options;
    contextualOptions.apiSet.addMapping(QStringLiteral("api-test-l1-1-0"),
                                        QStringLiteral("other.dll"), QStringLiteral("known.dll"));
    const auto other = QDir(options.system32Directory).filePath(QStringLiteral("other.dll"));
    writeFile(other);
    FileModuleLocator contextualLocator(contextualOptions);
    const auto overridden = contextualLocator.findModule(
        QStringLiteral("api-test-l1-1-0.dll"), QStringLiteral("C:\\target\\KNOWN.DLL"));
    require(overridden && overridden->identity == FileModuleLocator::fileIdentity(other) &&
                overridden->apiSetHost == QStringLiteral("other.dll"),
            "Module locator lost the importing module's API Set override");
    const auto direct = contextualLocator.findModule(QStringLiteral("known.dll"));
    require(direct && direct->apiSetHost.isEmpty(), "Ordinary import gained API Set metadata");
    const auto explicitContract = contextualLocator.findModule(explicitFile,
                                                              QStringLiteral("known.dll"));
    require(explicitContract && explicitContract->apiSetHost.isEmpty(),
            "Explicit path was classified as an API Set redirection");

#ifdef Q_OS_WIN
    auto local = FileLocatorOptions::systemDefaults();
    const FileModuleLocator system(local);
    require(!system.findModulePath(QStringLiteral("kernel32.dll")).isEmpty(),
            "Native system DLL not found");
    // This checks the real local schema in addition to the deterministic fixtures.
    const auto realHost =
        local.apiSet.resolveHost(QStringLiteral("api-ms-win-core-rtlsupport-l1-1-0.dll"));
    require(!realHost.isEmpty(), "Native Windows API Set schema could not be read");
    require(system.findModulePath(realHost) ==
                system.findModulePath(QStringLiteral("api-ms-win-core-rtlsupport-l1-1-0.dll")),
            "Native API Set contract does not resolve to its host");
#endif
}
}  // namespace

int main() { return run(tests); }
