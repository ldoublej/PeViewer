#pragma once

#include <pe_data_source.h>
#include <pe_structs.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStringList>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <utility>

namespace DependencyTests {
inline void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

template <typename Exception, typename Operation>
void expectException(Operation operation) {
    try {
        operation();
    } catch (const Exception&) {
        return;
    }
    throw std::runtime_error("Expected exception was not thrown");
}

template <typename T>
void put(QByteArray& bytes, int offset, const T& value) {
    require(offset >= 0 && quint64(offset) + sizeof(T) <= quint64(bytes.size()),
            "Fixture write is out of range");
    std::memcpy(bytes.data() + offset, &value, sizeof(T));
}

inline QByteArray makePe(const QStringList& imports = {}, bool is64 = false) {
    // One .idata section. Descriptor, string and thunk offsets intentionally
    // differ from their RVAs, so the tests exercise real file-layout parsing.
    const int descriptorBytes = (imports.size() + 1) * sizeof(IMAGE_IMPORT_DESCRIPTOR);
    int rawSize = descriptorBytes;
    for (const auto& name : imports) {
        rawSize += name.toLatin1().size() + 1 + 8;
    }
    rawSize = (rawSize + 0x1ff) & ~0x1ff;
    QByteArray bytes(0x200 + rawSize, '\0');
    IMAGE_DOS_HEADER dos{};
    dos.e_magic = IMAGE_DOS_SIGNATURE;
    dos.e_lfanew = 0x80;
    put(bytes, 0, dos);
    put(bytes, 0x80, quint32(IMAGE_NT_SIGNATURE));
    IMAGE_FILE_HEADER coff{};
    coff.Machine = is64 ? IMAGE_FILE_MACHINE_AMD64 : IMAGE_FILE_MACHINE_I386;
    coff.NumberOfSections = 1;
    coff.SizeOfOptionalHeader =
        is64 ? sizeof(IMAGE_OPTIONAL_HEADER64) : sizeof(IMAGE_OPTIONAL_HEADER32);
    put(bytes, 0x84, coff);
    const auto optional = [&](auto header) {
        header.Magic = is64 ? IMAGE_NT_OPTIONAL_HDR64_MAGIC : IMAGE_NT_OPTIONAL_HDR32_MAGIC;
        header.ImageBase = 0x400000;
        header.SectionAlignment = 0x1000;
        header.FileAlignment = 0x200;
        header.SizeOfHeaders = 0x200;
        header.SizeOfImage = 0x1000 + ((rawSize + 0xfff) & ~0xfff);
        header.NumberOfRvaAndSizes = IMAGE_NUMBEROF_DIRECTORY_ENTRIES;
        if (!imports.isEmpty()) {
            header.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress = 0x1000;
            header.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size = descriptorBytes;
        }
        put(bytes, 0x98, header);
    };
    if (is64) {
        optional(IMAGE_OPTIONAL_HEADER64{});
    } else {
        optional(IMAGE_OPTIONAL_HEADER32{});
    }
    IMAGE_SECTION_HEADER section{};
    std::memcpy(section.Name, ".idata", 6);
    section.Misc.VirtualSize = rawSize;
    section.VirtualAddress = 0x1000;
    section.SizeOfRawData = rawSize;
    section.PointerToRawData = 0x200;
    put(bytes, 0x98 + coff.SizeOfOptionalHeader, section);
    int cursor = descriptorBytes;
    for (int index = 0; index < imports.size(); ++index) {
        const auto name = imports[index].toLatin1();
        IMAGE_IMPORT_DESCRIPTOR descriptor{};
        descriptor.Name = 0x1000 + cursor;
        std::memcpy(bytes.data() + 0x200 + cursor, name.constData(), name.size());
        cursor += name.size() + 1;
        descriptor.FirstThunk = 0x1000 + cursor;
        cursor += 8;
        put(bytes, 0x200 + index * sizeof(descriptor), descriptor);
    }
    return bytes;
}

inline void writeFile(const QString& path, const QByteArray& bytes = {}) {
    require(QDir().mkpath(QFileInfo(path).absolutePath()), "Cannot create fixture directory");
    QFile file(path);
    require(file.open(QIODevice::WriteOnly), "Cannot open fixture file");
    require(file.write(bytes) == bytes.size(), "Cannot write fixture file");
}

inline void writePe(const QDir& dir, const QString& name, const QStringList& imports = {},
                    bool is64 = false) {
    writeFile(dir.filePath(name), makePe(imports, is64));
}

inline int run(void (*tests)()) {
    try {
        tests();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
}  // namespace DependencyTests
