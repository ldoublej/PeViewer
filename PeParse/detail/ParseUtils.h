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

[[noreturn]] void fail(ParseError::Code code, const QString& message,
                       std::optional<quint64> offset = std::nullopt,
                       std::optional<int> directoryIndex = std::nullopt);
void checkRange(const PeDataSource& source, quint64 offset, quint64 size);
void checkRvaRange(const PeFile& file, quint32 rva, quint64 size);
quint32 addRva(quint32 rva, quint64 delta);
QByteArray readBytes(const PeDataSource& source, quint64 offset, std::size_t size);
QByteArray readBytesAtRva(const PeDataSource& source, const PeFile& file, quint32 rva,
                          std::size_t size);
QString readCString(const PeDataSource& source, const PeFile& file, quint32 rva,
                    quint64& stringBytes, quint64 maxLength = MaxStringLength);
std::optional<IMAGE_DATA_DIRECTORY> directory(const PeFile& file, int index);

template <typename T>
T readScalar(const PeDataSource& source, quint64 offset) {
    static_assert(std::is_integral<T>::value && sizeof(T) <= 8, "PE scalar required");
    checkRange(source, offset, sizeof(T));
    return source.readStruct<T>(offset);
}

template <typename T>
T readAtRva(const PeDataSource& source, const PeFile& file, quint32 rva) {
    static_assert(std::is_integral<T>::value && sizeof(T) <= 8, "PE scalar required");
    const auto bytes = readBytesAtRva(source, file, rva, sizeof(T));
    T result;
    std::memcpy(&result, bytes.constData(), sizeof(result));
    return result;
}
}  // namespace PeParseDetail
