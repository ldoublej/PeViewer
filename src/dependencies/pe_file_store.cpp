#include "pe_file_store.h"

#include <pe_data_source.h>
#include <pe_file.h>

#include <stdexcept>
#include <utility>

struct PeFileStore::Entry {
    QString identity;
    // Declaration order matters: file is destroyed first, then the source it borrows.
    std::unique_ptr<PeDataSource> source;
    std::unique_ptr<PeFile> file;
};

PeFileStore::PeFileStore() = default;

PeFileStore::~PeFileStore() = default;

const PeFile* PeFileStore::load(const QString& identity, std::unique_ptr<PeDataSource> source) {
    if (identity.isEmpty() || !source) {
        throw std::invalid_argument("PeFileStore requires an identity and a data source");
    }
    if (const Entry* existing = m_index.value(identity, nullptr)) {
        return existing->file.get();
    }

    auto entry = std::make_unique<Entry>();
    entry->identity = identity;
    entry->source = std::move(source);
    entry->file = std::make_unique<PeFile>(*entry->source);
    entry->file->parse();  // Throws here and the entry is discarded.

    const PeFile* file = entry->file.get();
    m_index.insert(identity, entry.get());
    m_storage.push_back(std::move(entry));
    return file;
}

const PeFile* PeFileStore::find(const QString& identity) const {
    const Entry* entry = m_index.value(identity, nullptr);
    return entry ? entry->file.get() : nullptr;
}

bool PeFileStore::contains(const QString& identity) const {
    return m_index.contains(identity);
}

std::size_t PeFileStore::size() const { return m_storage.size(); }

void PeFileStore::clear() {
    m_index.clear();
    m_storage.clear();
}
