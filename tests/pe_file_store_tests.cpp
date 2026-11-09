#include <pe_data_source.h>
#include <pe_file.h>
#include <pe_file_store.h>

#include <QByteArray>
#include <QString>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <utility>

#include "dependencies_test_support.h"

using namespace DependencyTests;

namespace {
class MemorySource final : public PeDataSource {
public:
    MemorySource(QByteArray bytes, std::shared_ptr<int> alive)
        : m_bytes(std::move(bytes)), m_alive(std::move(alive)) {
        ++*m_alive;
    }
    ~MemorySource() override { --*m_alive; }

    quint64 length() const override { return m_bytes.size(); }
    bool isFileAligned() const override { return true; }
    quint64 imageBase() const override { return 0; }
    QString url() const override { return QStringLiteral("memory://module"); }
    void readExact(quint64 offset, char* buffer, std::size_t count) const override {
        if (offset > length() || count > length() - offset) {
            throw PeDataSourceError(PeDataSourceError::Code::OutOfBounds,
                                    QStringLiteral("Fixture bounds"), offset, count);
        }
        std::memcpy(buffer, m_bytes.constData() + offset, count);
    }

private:
    QByteArray m_bytes;
    std::shared_ptr<int> m_alive;
};

void loadsAndDeduplicates() {
    auto alive = std::make_shared<int>(0);
    const QByteArray bytes = makePe({QStringLiteral("a.dll")});
    const QString identity = QStringLiteral("mod-a");

    PeFileStore store;
    const PeFile* first = store.load(identity, std::make_unique<MemorySource>(bytes, alive));
    require(first != nullptr && first->isParsed(), "load did not parse the module");
    require(store.size() == 1 && store.contains(identity) && store.find(identity) == first,
            "loaded module was not indexed by identity");
    require(*alive == 1, "load did not take ownership of the source");

    const PeFile* duplicate =
        store.load(identity, std::make_unique<MemorySource>(bytes, alive));
    require(duplicate == first, "duplicate identity did not reuse the stored file");
    require(store.size() == 1 && *alive == 1, "duplicate source was retained");

    require(store.find(QStringLiteral("missing")) == nullptr &&
                !store.contains(QStringLiteral("missing")),
            "unknown identity was reported as loaded");

    store.clear();
    require(store.size() == 0 && store.find(identity) == nullptr && *alive == 0,
            "clear did not release stored files");
}

void rejectsInvalidInput() {
    auto alive = std::make_shared<int>(0);
    PeFileStore store;

    expectException<std::invalid_argument>(
        [&] { store.load(QString{}, std::make_unique<MemorySource>(makePe(), alive)); });
    expectException<std::invalid_argument>(
        [&] { store.load(QStringLiteral("mod"), std::unique_ptr<PeDataSource>{nullptr}); });
    require(store.size() == 0 && *alive == 0, "invalid input changed the store");

    expectException<ParseError>([&] {
        store.load(QStringLiteral("bad"),
                   std::make_unique<MemorySource>(QByteArray(64, '\0'), alive));
    });
    require(store.size() == 0 && *alive == 0, "failed parse was retained");
}

void keepsDistinctIdentitiesStable() {
    auto alive = std::make_shared<int>(0);
    PeFileStore store;
    const PeFile* a =
        store.load(QStringLiteral("a"), std::make_unique<MemorySource>(makePe(), alive));
    const PeFile* b =
        store.load(QStringLiteral("b"), std::make_unique<MemorySource>(makePe(), alive));
    require(a != b && store.size() == 2 && *alive == 2, "store merged distinct identities");
    require(store.find(QStringLiteral("a")) == a && store.find(QStringLiteral("b")) == b,
            "stored addresses changed");
}

void tests() {
    loadsAndDeduplicates();
    rejectsInvalidInput();
    keepsDistinctIdentitiesStable();
}
}  // namespace

int main() { return run(tests); }
