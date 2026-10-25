#include "PeDataSource.h"

#include <limits>

PeDataSourceError::PeDataSourceError(Code code, const QString& message,
                                     quint64 offset, std::size_t length)
    : std::runtime_error(message.toUtf8().constData()),
      m_code(code),
      m_offset(offset),
      m_length(length) {}

namespace {
template <typename T>
T readLittleEndian(const PeDataSource& source, quint64 offset) {
    const auto bytes = source.readBytes(offset, sizeof(T));
    T value = 0;
    for (int i = 0; i < bytes.size(); ++i)
        value |= static_cast<T>(static_cast<quint8>(bytes[i])) << (i * 8);
    return value;
}
}  // namespace

quint16 PeDataSource::readU16(quint64 offset) const {
    return readLittleEndian<quint16>(*this, offset);
}

quint32 PeDataSource::readU32(quint64 offset) const {
    return readLittleEndian<quint32>(*this, offset);
}

quint64 PeDataSource::readU64(quint64 offset) const {
    return readLittleEndian<quint64>(*this, offset);
}

QByteArray PeDataSource::readBytes(quint64 offset, std::size_t count) const {
    if (count > static_cast<std::size_t>(std::numeric_limits<int>::max()))
        throw std::length_error(
            "Requested byte array exceeds QByteArray capacity");

    QByteArray bytes(static_cast<int>(count), Qt::Uninitialized);
    readExact(offset, bytes.data(), count);
    return bytes;
}

QString PeDataSource::readAscii(quint64 offset, std::size_t maxLength) const {
    const QByteArray bytes = readBytes(offset, maxLength);
    const int terminator = bytes.indexOf('\0');
    const int length = terminator < 0 ? bytes.size() : terminator;
    return QString::fromUtf8(bytes.constData(), length);
}

QString PeDataSource::readUtf16(quint64 offset,
                                std::size_t maxCodeUnits) const {
    if (maxCodeUnits >
        static_cast<std::size_t>(std::numeric_limits<int>::max()) / 2)
        throw std::length_error(
            "Requested UTF-16 string exceeds QByteArray capacity");

    const QByteArray bytes = readBytes(offset, maxCodeUnits * 2);
    QString text;
    text.reserve(static_cast<int>(maxCodeUnits));
    for (int i = 0; i < bytes.size(); i += 2) {
        const auto unit =
            static_cast<quint16>(static_cast<quint8>(bytes[i])) |
            (static_cast<quint16>(static_cast<quint8>(bytes[i + 1])) << 8);
        if (unit == 0) break;
        if (QChar::isHighSurrogate(unit)) {
            if (i + 3 < bytes.size()) {
                const auto next =
                    static_cast<quint16>(static_cast<quint8>(bytes[i + 2])) |
                    (static_cast<quint16>(static_cast<quint8>(bytes[i + 3]))
                     << 8);
                if (QChar::isLowSurrogate(next)) {
                    text.append(QChar(unit));
                    text.append(QChar(next));
                    i += 2;
                    continue;
                }
            }
            text.append(QChar(QChar::ReplacementCharacter));
        } else if (QChar::isLowSurrogate(unit)) {
            text.append(QChar(QChar::ReplacementCharacter));
        } else {
            text.append(QChar(unit));
        }
    }
    return text;
}
