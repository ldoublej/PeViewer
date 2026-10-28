#pragma once

#include <pe_data_source.h>

#include <QString>
#include <memory>
#include <optional>

// identity denotes the actual module, rather than the name used to import it.
// Custom searchers may use any nonempty stable key, shared with the tree's root identity.
struct ResolvedModule {
    QString identity;
    std::unique_ptr<PeDataSource> source;
};

class ModuleSearcher {
public:
    virtual ~ModuleSearcher() = default;
    virtual std::optional<ResolvedModule> findModule(const QString& dllName) const = 0;
};
