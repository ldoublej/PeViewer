#include "api_set.h"

#include <pe_data_source.h>
#include <pe_file.h>
#include <pe_file_data_source.h>

#include <cstring>
#include <type_traits>

namespace {
constexpr quint64 MaxSchemaSize = Q_UINT64_C(64) * 1024 * 1024;
struct NamespaceHeader {
    quint32 version, size, flags, count, entryOffset, hashOffset, hashFactor;
};
struct NamespaceEntry {
    quint32 flags, nameOffset, nameLength, hashedLength, valueOffset, valueCount;
};
struct NamespaceValue {
    quint32 flags, nameOffset, nameLength, valueOffset, valueLength;
};
static_assert(sizeof(NamespaceHeader) == 28);
static_assert(sizeof(NamespaceEntry) == 24);
static_assert(sizeof(NamespaceValue) == 20);

struct InvalidSchema {};

// Copying bounded structures avoids alignment and aliasing assumptions about QByteArray.
class SchemaReader {
public:
    explicit SchemaReader(const QByteArray& bytes) : m_bytes(bytes), m_size(bytes.size()) {}

    void limit(quint64 size) {
        if (size < sizeof(NamespaceHeader) || size > m_size) {
            throw InvalidSchema{};
        }
        m_size = size;
    }

    void checkRange(quint64 offset, quint64 count, quint64 stride = 1) const {
        if (offset > m_size || count > (m_size - offset) / stride) {
            throw InvalidSchema{};
        }
    }

    template <typename T>
    T read(quint64 offset) const {
        static_assert(std::is_trivially_copyable<T>::value);
        checkRange(offset, sizeof(T));
        T value{};
        std::memcpy(&value, m_bytes.constData() + offset, sizeof(T));
        return value;
    }

    QString string(quint32 offset, quint32 length) const {
        checkRange(offset, length);
        if (length % 2 != 0) {
            throw InvalidSchema{};
        }
        QString text;
        text.reserve(static_cast<int>(length / 2));
        bool needsLowSurrogate = false;
        for (quint64 position = offset; position < quint64(offset) + length; position += 2) {
            const auto unit = read<quint16>(position);
            if (unit == 0 || (needsLowSurrogate && !QChar::isLowSurrogate(unit)) ||
                (!needsLowSurrogate && QChar::isLowSurrogate(unit))) {
                throw InvalidSchema{};
            }
            needsLowSurrogate = QChar::isHighSurrogate(unit);
            text.append(QChar(unit));
        }
        if (needsLowSurrogate) {
            throw InvalidSchema{};
        }
        return text;
    }

private:
    const QByteArray& m_bytes;
    quint64 m_size;
};
}  // namespace

QString ApiSetMap::lookupKey(QString name) {
    name = name.toLower();
    if (name.endsWith(QStringLiteral(".dll"))) {
        name.chop(4);
    }
    if (!name.startsWith(QStringLiteral("api-")) && !name.startsWith(QStringLiteral("ext-"))) {
        return {};
    }
    const auto dash = name.lastIndexOf(QLatin1Char('-'));
    const auto minor = name.mid(dash + 1);
    bool numeric = !minor.isEmpty();
    for (const auto character : minor) {
        numeric = numeric && character >= QLatin1Char('0') && character <= QLatin1Char('9');
    }
    if (numeric) {
        name.truncate(dash);
    }
    return name;
}

void ApiSetMap::addMapping(const QString& contract, const QString& host) {
    const auto key = lookupKey(contract);
    if (!key.isEmpty() && !host.isEmpty()) {
        m_hosts[key].append(host);
    }
}

QString ApiSetMap::resolveHost(const QString& dllName) const {
    auto key = lookupKey(dllName);
    for (int depth = 0; depth < 8 && !key.isEmpty(); ++depth) {
        const auto found = m_hosts.constFind(key);
        if (found == m_hosts.cend() || found->isEmpty()) {
            return {};
        }
        const auto& host = found->front();
        const auto next = lookupKey(host);
        if (next.isEmpty()) {
            return host;
        }
        // A self-reference, like any other cycle, does not resolve to a host DLL.
        key = next;
    }
    return {};
}

std::optional<ApiSetMap> ApiSetMap::parse(const QByteArray& bytes) {
    if (quint64(bytes.size()) > MaxSchemaSize) {
        return std::nullopt;
    }
    try {
        SchemaReader reader(bytes);
        const auto header = reader.read<NamespaceHeader>(0);
        if (header.version != 6 || header.entryOffset < sizeof(NamespaceHeader) ||
            header.entryOffset % 4 != 0) {
            return std::nullopt;
        }
        reader.limit(header.size);
        reader.checkRange(header.entryOffset, header.count, sizeof(NamespaceEntry));
        ApiSetMap map;
        for (quint32 i = 0; i < header.count; ++i) {
            const auto entry = reader.read<NamespaceEntry>(header.entryOffset +
                                                           quint64(i) * sizeof(NamespaceEntry));
            const auto name = reader.string(entry.nameOffset, entry.nameLength);
            if (lookupKey(name).isEmpty()) {
                throw InvalidSchema{};
            }
            reader.checkRange(entry.valueOffset, entry.valueCount, sizeof(NamespaceValue));
            for (quint32 j = 0; j < entry.valueCount; ++j) {
                const auto value = reader.read<NamespaceValue>(entry.valueOffset +
                                                               quint64(j) * sizeof(NamespaceValue));
                // Validate alias strings too, even though desktop lookup uses the first host.
                reader.string(value.nameOffset, value.nameLength);
                const auto host = reader.string(value.valueOffset, value.valueLength);
                if (host.contains(QLatin1Char('/')) || host.contains(QLatin1Char('\\')) ||
                    host.contains(QLatin1Char(':'))) {
                    throw InvalidSchema{};
                }
                map.addMapping(name, host);
            }
        }
        return map;
    } catch (const InvalidSchema&) {
        return std::nullopt;
    }
}

std::optional<ApiSetMap> ApiSetMap::load(const QString& schemaPath) {
    try {
        PeFileDataSource source(schemaPath);
        PeFile file(source);
        file.parse();
        const auto* section = file.sections().findByName(QStringLiteral(".apiset"));
        if (!section || section->rawSize() > MaxSchemaSize) {
            return std::nullopt;
        }
        return parse(source.readBytes(section->rawOffset(), section->rawSize()));
    } catch (const ParseError&) {
        return std::nullopt;
    } catch (const PeDataSourceError&) {
        return std::nullopt;
    }
}
