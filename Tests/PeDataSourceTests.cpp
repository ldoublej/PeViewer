#include <QFile>
#include <QTemporaryDir>
#include <limits>
#include <stdexcept>
#include <thread>

#include "PeFileDataSource.h"

namespace {
void require(bool condition) {
    if (!condition) {
        throw std::runtime_error("PeDataSource test failed");
    }
}

template <typename F>
void expectError(PeDataSourceError::Code code, F operation) {
    try {
        operation();
    } catch (const PeDataSourceError& error) {
        require(error.code() == code);
        return;
    }
    require(false);
}
}  // namespace

int main() {
    QTemporaryDir directory;
    require(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("sample.pe"));
    QFile file(path);
    require(file.open(QIODevice::WriteOnly));
    const QByteArray contents(
        "MZ\x12\x34\x56\x78\x9a\xbc"
        "Hi\0"
        "A\0\0\0",
        15);
    require(file.write(contents) == contents.size());
    file.close();

    PeFileDataSource source(path);
    require(source.length() == 15);
    require(!source.isEmpty());
    require(source.isFileAligned());
    require(source.imageBase() == 0);
    require(source.url() == path);
    require(source.readU16(0) == 0x5a4d);
    require(source.readU32(2) == 0x78563412);
    require(source.readU64(0) == Q_UINT64_C(0xbc9a785634125a4d));
    require(source.readBytes(8, 2) == QByteArray("Hi", 2));
    require(source.readAscii(8, 3) == QStringLiteral("Hi"));
    require(source.readUtf16(11, 2) == QStringLiteral("A"));
    require(source.readBytes(0, 2) == QByteArray("MZ", 2));
    require(source.readBytes(6, 6) == contents.mid(6, 6));
    require(source.readBytes(100, 0).isEmpty());

    source.readExact(100, nullptr, 0);
    expectError(PeDataSourceError::Code::OutOfBounds, [&] { source.readBytes(14, 2); });
    expectError(PeDataSourceError::Code::OutOfBounds,
                [&] { source.readU16(std::numeric_limits<quint64>::max()); });
    expectError(PeDataSourceError::Code::Io, [&] {
        PeFileDataSource missing(directory.filePath(QStringLiteral("missing.pe")));
    });

    const QString unicodePath = directory.filePath(QStringLiteral("unicode.bin"));
    QFile unicodeFile(unicodePath);
    require(unicodeFile.open(QIODevice::WriteOnly));
    const QByteArray unicodeBytes("\x3d\xd8\x00\xde\x00\xd8\x00\x00", 8);
    require(unicodeFile.write(unicodeBytes) == unicodeBytes.size());
    unicodeFile.close();
    PeFileDataSource unicodeSource(unicodePath);
    require(unicodeSource.readUtf16(0, 4) == QString::fromUtf8("\xf0\x9f\x98\x80\xef\xbf\xbd"));

    std::thread first([&] {
        for (int i = 0; i < 100; ++i) {
            require(source.readU16(0) == 0x5a4d);
        }
    });
    for (int i = 0; i < 100; ++i) {
        require(source.readU32(2) == 0x78563412);
    }
    first.join();
}
