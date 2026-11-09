#pragma once

#include <pe_data_source.h>

#include <QString>
#include <memory>
#include <optional>

// identity denotes the actual module, rather than the name used to import it.
// Custom locators may use any nonempty stable key, shared with the graph's root identity.
struct LocatedModule {
    QString identity;
    std::unique_ptr<PeDataSource> source;
    // Nonempty only when the import was redirected through an API Set contract.
    QString apiSetHost;
};

class ModuleLocator {
public:
    virtual ~ModuleLocator() = default;
    virtual std::optional<LocatedModule> findModule(const QString& dllName) const = 0;
    // Existing custom locators may ignore importer context.
    virtual std::optional<LocatedModule> findModule(const QString& dllName,
                                                   const QString& importer) const {
        Q_UNUSED(importer);
        return findModule(dllName);
    }
};
