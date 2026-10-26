#include "PeFileDataSource.h"

#include <QMutexLocker>
#include <algorithm>
#include <limits>

PeFileDataSource::PeFileDataSource(const QString& path) : m_file(path) {
    if (!m_file.open(QIODevice::ReadOnly))
        throw PeDataSourceError(PeDataSourceError::Code::Io,
                                QStringLiteral("Cannot open %1: %2")
                                    .arg(path, m_file.errorString()));
}

quint64 PeFileDataSource::length() const {
    QMutexLocker lock(&m_mutex);
    const qint64 size = m_file.size();
    return static_cast<quint64>(size);
}

void PeFileDataSource::readExact(quint64 offset, char* buffer,
                                 std::size_t count) const {
    if (count == 0) return;
    if (!buffer)
        throw std::invalid_argument(
            "readExact requires a buffer for nonempty reads");

    QMutexLocker lock(&m_mutex);
    const qint64 fileSize = m_file.size();
    const std::optional<quint64> knownSize =
        fileSize < 0 ? std::nullopt
                     : std::optional<quint64>(static_cast<quint64>(fileSize));
    const auto outOfBounds = [&](quint64 position, std::size_t remaining) {
        return PeDataSourceError(
            PeDataSourceError::Code::OutOfBounds,
            QStringLiteral("Read outside %1 at offset %2 (%3 bytes)")
                .arg(m_file.fileName())
                .arg(position)
                .arg(static_cast<qulonglong>(remaining)),
            position, remaining);
    };

    if (offset > static_cast<quint64>(std::numeric_limits<qint64>::max()) ||
        (knownSize && (offset > *knownSize || count > *knownSize - offset)))
        throw outOfBounds(offset, count);

    if (!m_file.seek(static_cast<qint64>(offset)))
        throw PeDataSourceError(
            PeDataSourceError::Code::Io,
            QStringLiteral("Cannot seek %1: %2")
                .arg(m_file.fileName(), m_file.errorString()));

    std::size_t done = 0;
    while (done < count) {
        const auto chunk = static_cast<qint64>(std::min<std::size_t>(
            count - done,
            static_cast<std::size_t>(std::numeric_limits<int>::max())));
        const qint64 received = m_file.read(buffer + done, chunk);
        if (received < 0)
            throw PeDataSourceError(
                PeDataSourceError::Code::Io,
                QStringLiteral("Cannot read %1: %2")
                    .arg(m_file.fileName(), m_file.errorString()));
        if (received == 0) throw outOfBounds(offset + done, count - done);
        done += static_cast<std::size_t>(received);
    }
}
