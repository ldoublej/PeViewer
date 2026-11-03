#pragma once

#include <QHash>
#include <QString>
#include <cstddef>
#include <memory>
#include <vector>

class PeDataSource;
class PeFile;

// Owns the data sources and parsed PeFiles created while resolving dependencies.
// Entries are keyed by locator identity and never move, so every PeFile* returned
// by load() stays valid until clear() or destruction.
class PeFileStore {
public:
    PeFileStore();
    ~PeFileStore();

    Q_DISABLE_COPY_MOVE(PeFileStore)

    // Takes ownership of source, parses it and returns the stored PeFile.
    // - Same identity already present: returns the existing PeFile, discards source.
    // - Parse failure: throws ParseError/PeDataSourceError, nothing is retained.
    // - Empty identity or null source: throws std::invalid_argument.
    const PeFile* load(const QString& identity, std::unique_ptr<PeDataSource> source);

    // Both return nullptr/false for identities that were never loaded successfully.
    const PeFile* find(const QString& identity) const;
    bool contains(const QString& identity) const;
    std::size_t size() const;

    // Invalidates every PeFile* previously returned by this store.
    void clear();

private:
    struct Entry;
    std::vector<std::unique_ptr<Entry>> m_storage;  // Stable PeFile addresses.
    QHash<QString, Entry*> m_index;
};
