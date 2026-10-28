#include <api_set.h>

#include <QTemporaryDir>
#include <limits>
#include <vector>

#include "dependencies_test_support.h"

using namespace DependencyTests;

namespace {
using Mapping = std::pair<QString, QStringList>;

QByteArray schema(const std::vector<Mapping>& mappings) {
    int count = 0;
    for (const auto& mapping : mappings) {
        count += mapping.second.size();
    }
    QByteArray bytes(28 + static_cast<int>(mappings.size()) * 24 + count * 20, '\0');
    const auto appendString = [&](const QString& text) {
        const quint32 offset = bytes.size();
        for (const auto character : text) {
            const quint16 unit = character.unicode();
            bytes.append(reinterpret_cast<const char*>(&unit), sizeof(unit));
        }
        return offset;
    };
    put(bytes, 0, quint32(6));
    put(bytes, 12, quint32(mappings.size()));
    put(bytes, 16, quint32(28));
    int value = 28 + static_cast<int>(mappings.size()) * 24;
    for (std::size_t index = 0; index < mappings.size(); ++index) {
        const auto& mapping = mappings[index];
        const int entry = 28 + static_cast<int>(index) * 24;
        put(bytes, entry + 4, appendString(mapping.first));
        put(bytes, entry + 8, quint32(mapping.first.size() * 2));
        put(bytes, entry + 12, quint32(mapping.first.size() * 2));
        put(bytes, entry + 16, quint32(value));
        put(bytes, entry + 20, quint32(mapping.second.size()));
        for (const auto& host : mapping.second) {
            put(bytes, value + 12, appendString(host));
            put(bytes, value + 16, quint32(host.size() * 2));
            value += 20;
        }
    }
    put(bytes, 4, quint32(bytes.size()));
    return bytes;
}

void tests() {
    auto bytes = schema({{QStringLiteral("api-ms-win-core-file-l1-1-1"),
                          {QStringLiteral("kernelbase.dll"), QStringLiteral("kernel32.dll")}},
                         {QStringLiteral("ext-ms-empty-l1-1-0"), {}}});
    const auto parsed = ApiSetMap::parse(bytes);
    require(parsed.has_value(), "Valid API Set schema was rejected");
    require(parsed->resolveHost(QStringLiteral("API-MS-WIN-CORE-FILE-L1-1-9.DLL")) ==
                QStringLiteral("kernelbase.dll"),
            "Minor version/case normalization failed");
    require(parsed->resolveHost(QStringLiteral("api-ms-win-core-file-l1-2-0.dll")).isEmpty(),
            "Major version must match");
    require(parsed->resolveHost(QStringLiteral("kernelbase.dll")).isEmpty(),
            "Ordinary DLL is not a contract");
    require(parsed->resolveHost(QStringLiteral("ext-ms-empty-l1-1-0.dll")).isEmpty(),
            "Empty host list resolved");

    ApiSetMap chain;
    chain.addMapping(QStringLiteral("api-first-l1-1-0"), QStringLiteral("ext-second-l1-1-0"));
    chain.addMapping(QStringLiteral("ext-second-l1-1-0"), QStringLiteral("host.dll"));
    require(chain.resolveHost(QStringLiteral("api-first-l1-1-2.dll")) == QStringLiteral("host.dll"),
            "API Set alias chain failed");
    ApiSetMap cycle;
    cycle.addMapping(QStringLiteral("api-a"), QStringLiteral("api-b"));
    cycle.addMapping(QStringLiteral("api-b"), QStringLiteral("api-a"));
    cycle.addMapping(QStringLiteral("api-self"), QStringLiteral("api-self"));
    require(cycle.resolveHost(QStringLiteral("api-a")).isEmpty(), "Alias cycle was accepted");
    require(cycle.resolveHost(QStringLiteral("api-self")).isEmpty(), "Self alias was accepted");

    // Every truncated prefix must be rejected; a declared namespace cannot run
    // past its supplied data, regardless of which field was cut off.
    for (int length = 0; length < bytes.size(); ++length) {
        require(!ApiSetMap::parse(bytes.left(length)), "Truncated schema was accepted");
    }
    const auto rejectField = [&](int offset, quint32 value) {
        auto invalid = bytes;
        put(invalid, offset, value);
        require(!ApiSetMap::parse(invalid), "Malformed API Set field was accepted");
    };
    rejectField(0, 4);
    rejectField(0, 7);
    rejectField(4, 12);
    rejectField(12, (std::numeric_limits<quint32>::max)());
    rejectField(16, 29);
    rejectField(28 + 4, (std::numeric_limits<quint32>::max)());
    rejectField(28 + 8, 3);
    rejectField(28 + 20, (std::numeric_limits<quint32>::max)());
    auto invalidUtf16 = schema({{QStringLiteral("api-bad"), {QString(QChar(0xd800))}}});
    require(!ApiSetMap::parse(invalidUtf16), "Unpaired UTF-16 surrogate was accepted");
    require(!ApiSetMap::parse(schema({{QStringLiteral("kernel32"), {QStringLiteral("x.dll")}}})),
            "Non-contract schema entry was accepted");

    // Exercise the on-disk PE section reader, not just the namespace decoder.
    QTemporaryDir temporary;
    require(temporary.isValid(), "Temporary directory failed");
    auto image = makePe();
    image.resize(0x400 + ((bytes.size() + 0x1ff) & ~0x1ff));
    const int section = 0x98 + sizeof(IMAGE_OPTIONAL_HEADER32);
    std::memcpy(image.data() + section, ".apiset\0", 8);
    put(image, section + 8, quint32(bytes.size()));
    put(image, section + 16, quint32(image.size() - 0x200));
    std::memcpy(image.data() + 0x200, bytes.constData(), bytes.size());
    const auto path = temporary.filePath(QStringLiteral("schema.dll"));
    writeFile(path, image);
    const auto loaded = ApiSetMap::load(path);
    require(loaded && loaded->resolveHost(QStringLiteral("api-ms-win-core-file-l1-1-0.dll")) ==
                          QStringLiteral("kernelbase.dll"),
            "Cannot load .apiset from PE file");
    require(!ApiSetMap::load(temporary.filePath(QStringLiteral("missing.dll"))),
            "Missing schema was loaded");
}
}  // namespace

int main() { return run(tests); }
