#include "ParseUtils.h"

#include <algorithm>
#include <limits>

#include "PeFile.h"

namespace PeParseDetail {
namespace {
using Code = ParseError::Code;
quint64 mappedBytes(const PeFile& file, quint32 rva) {
    const auto offset = file.rvaToFileOffset(rva);
    if (!offset)
        fail(Code::InvalidRva,
             QStringLiteral("RVA %1 has no file data").arg(rva));
    const quint64 headers = file.ntHeaders().optionalHeader().sizeOfHeaders();
    // Header and section extents were checked against the file size during
    // parsing.
    if (rva < headers) return headers - rva;
    const auto* section = file.sections().findByRva(rva);
    if (!section)
        fail(Code::InvalidRva,
             QStringLiteral("RVA %1 has no section").arg(rva));
    return quint64(section->rawSize()) -
           (quint64(rva) - section->virtualAddress());
}
}  // namespace

void fail(Code code, const QString& message, std::optional<quint64> offset,
          std::optional<int> directoryIndex) {
    throw ParseError(code, message, offset, directoryIndex);
}

void checkRange(const PeDataSource& source, quint64 offset, quint64 size) {
    const quint64 length = source.length();
    if (offset > length || size > length - offset)
        fail(Code::TruncatedData,
             QStringLiteral("Read outside PE file at offset %1 (%2 bytes)")
                 .arg(offset)
                 .arg(size),
             offset);
}

quint32 addRva(quint32 rva, quint64 delta) {
    if (delta > Q_UINT64_C(0xffffffff) - rva)
        fail(Code::InvalidRva,
             QStringLiteral("RVA arithmetic overflow at %1").arg(rva));
    return static_cast<quint32>(rva + delta);
}

void checkRvaRange(const PeFile& file, quint32 rva, quint64 size) {
    if (size > Q_UINT64_C(0x100000000) - rva)
        fail(Code::InvalidRva,
             QStringLiteral("Invalid RVA range at %1").arg(rva));
    quint64 done = 0;
    while (done < size) {
        const quint32 current = addRva(rva, done);
        const quint64 available = mappedBytes(file, current);
        if (!available)
            fail(Code::InvalidRva,
                 QStringLiteral("Empty mapping at RVA %1").arg(current));
        done += (std::min)(available, size - done);
    }
}

QByteArray readBytes(const PeDataSource& source, quint64 offset,
                     std::size_t size) {
    if (size > static_cast<std::size_t>((std::numeric_limits<int>::max)()))
        fail(Code::LimitExceeded,
             QStringLiteral("Read exceeds byte-array capacity"), offset);
    checkRange(source, offset, size);
    return source.readBytes(offset, size);
}

QByteArray readBytesAtRva(const PeDataSource& source, const PeFile& file,
                          quint32 rva, std::size_t size) {
    checkRvaRange(file, rva, size);
    if (size > static_cast<std::size_t>((std::numeric_limits<int>::max)()))
        fail(Code::LimitExceeded,
             QStringLiteral("RVA read exceeds byte-array capacity"));
    QByteArray bytes;
    bytes.reserve(static_cast<int>(size));
    quint64 done = 0;
    while (done < size) {
        const quint32 current = addRva(rva, done);
        const quint64 available = mappedBytes(file, current);
        if (!available)
            fail(Code::InvalidRva,
                 QStringLiteral("Empty mapping at RVA %1").arg(current));
        const auto count = static_cast<std::size_t>(
            (std::min)(available, quint64(size) - done));
        bytes.append(readBytes(source, *file.rvaToFileOffset(current), count));
        done += count;
    }
    return bytes;
}

QString readCString(const PeDataSource& source, const PeFile& file, quint32 rva,
                    quint64& stringBytes, quint64 maxLength) {
    const quint64 limit = (std::min)(maxLength, MaxStringLength);
    QByteArray bytes;
    quint64 done = 0;
    while (done < limit) {
        const quint32 current = addRva(rva, done);
        const quint64 available = mappedBytes(file, current);
        if (!available)
            fail(Code::InvalidRva,
                 QStringLiteral("Empty mapping at RVA %1").arg(current));
        const quint64 count =
            (std::min)({available, limit - done, quint64(128)});
        const auto chunk = readBytes(source, *file.rvaToFileOffset(current),
                                     static_cast<std::size_t>(count));
        const int terminator = chunk.indexOf('\0');
        if (terminator >= 0) {
            bytes.append(chunk.constData(), terminator);
            const quint64 consumed = quint64(bytes.size()) + 1;
            if (stringBytes > MaxStringBytes ||
                consumed > MaxStringBytes - stringBytes)
                fail(Code::LimitExceeded,
                     QStringLiteral("PE string budget exceeded"),
                     file.rvaToFileOffset(rva));
            stringBytes += consumed;
            return QString::fromLatin1(bytes.constData(), bytes.size());
        }
        bytes.append(chunk);
        done += count;
    }
    fail(Code::UnterminatedString,
         QStringLiteral("Unterminated PE string at RVA %1 (limit %2)")
             .arg(rva)
             .arg(limit),
         file.rvaToFileOffset(rva));
}

std::optional<IMAGE_DATA_DIRECTORY> directory(const PeFile& file, int index) {
    const auto result = file.ntHeaders().optionalHeader().dataDirectory(
        static_cast<std::size_t>(index));
    if (!result || (result->VirtualAddress == 0 && result->Size == 0))
        return std::nullopt;
    if (result->VirtualAddress == 0 || result->Size == 0)
        fail(Code::InvalidDirectory,
             QStringLiteral("Directory must have both an RVA and a size"),
             std::nullopt, index);
    checkRvaRange(file, result->VirtualAddress, result->Size);
    return result;
}
}  // namespace PeParseDetail
