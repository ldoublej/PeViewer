#pragma once

#include <PeDataSource.h>
#include <PeStructs.h>

#include <cstring>
#include <type_traits>

#include "ParseError.h"

class PeFile;

namespace PeParseDetail {
inline constexpr quint32 MaxEntries = 100000;
inline constexpr quint32 MaxSections = 4096;
inline constexpr quint64 MaxStringLength = 4096;
inline constexpr quint64 MaxStringBytes = 16 * 1024 * 1024;

[[noreturn]] void fail(PeParseError::Code code, const QString& message,
                       std::optional<quint64> offset = std::nullopt,
                       std::optional<int> directoryIndex = std::nullopt);
void checkRange(const PeDataSource& source, quint64 offset, quint64 size);
void checkRvaRange(const PeFile& file, quint32 rva, quint64 size);
quint32 addRva(quint32 rva, quint64 delta);
QByteArray readBytes(const PeDataSource& source, quint64 offset,
                     std::size_t size);
QByteArray readBytesAtRva(const PeDataSource& source, const PeFile& file,
                          quint32 rva, std::size_t size);
QString readCString(const PeDataSource& source, const PeFile& file, quint32 rva,
                    quint64& stringBytes, quint64 maxLength = MaxStringLength);
std::optional<IMAGE_DATA_DIRECTORY> directory(const PeFile& file, int index);

template <typename T>
T decodeLittleEndian(const QByteArray& bytes) {
    static_assert(std::is_integral<T>::value && sizeof(T) <= 8,
                  "PE scalar required");
    using U = typename std::make_unsigned<T>::type;
    quint64 value = 0;
    for (std::size_t i = 0; i < sizeof(T); ++i)
        value |= quint64(static_cast<quint8>(bytes[static_cast<int>(i)]))
                 << (i * 8);
    const U bits = static_cast<U>(value);
    T result{};
    std::memcpy(&result, &bits, sizeof(T));
    return result;
}

template <typename T>
T readScalar(const PeDataSource& source, quint64 offset) {
    return decodeLittleEndian<T>(readBytes(source, offset, sizeof(T)));
}

template <typename T>
T readAtRva(const PeDataSource& source, const PeFile& file, quint32 rva) {
    return decodeLittleEndian<T>(readBytesAtRva(source, file, rva, sizeof(T)));
}
}  // namespace PeParseDetail
