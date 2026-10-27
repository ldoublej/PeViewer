#pragma once

#include <PeStructs.h>

#include <QtGlobal>
#include <array>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

#include "ParseError.h"

class PeDataSource;

// parse() updates this object on success and propagates exceptions on failure.

class DosHeader {
public:
    DosHeader() = default;
    void parse(const PeDataSource& source);
    quint16 magic() const { return m_header.e_magic; }
    quint32 ntHeadersOffset() const { return static_cast<quint32>(m_header.e_lfanew); }
    quint16 bytesOnLastPage() const { return m_header.e_cblp; }
    quint16 pagesInFile() const { return m_header.e_cp; }
    quint16 relocations() const { return m_header.e_crlc; }
    quint16 headerParagraphs() const { return m_header.e_cparhdr; }
    quint16 minimumExtraParagraphs() const { return m_header.e_minalloc; }
    quint16 maximumExtraParagraphs() const { return m_header.e_maxalloc; }
    quint16 initialSs() const { return m_header.e_ss; }
    quint16 initialSp() const { return m_header.e_sp; }
    quint16 checksum() const { return m_header.e_csum; }
    quint16 initialIp() const { return m_header.e_ip; }
    quint16 initialCs() const { return m_header.e_cs; }
    quint16 relocationTableOffset() const { return m_header.e_lfarlc; }
    quint16 overlayNumber() const { return m_header.e_ovno; }
    quint16 oemId() const { return m_header.e_oemid; }
    quint16 oemInfo() const { return m_header.e_oeminfo; }
    std::array<quint16, 4> reserved() const;
    std::array<quint16, 10> reserved2() const;

private:
    Q_DISABLE_COPY_MOVE(DosHeader)
    IMAGE_DOS_HEADER m_header{};
};

class FileHeader {
public:
    FileHeader() = default;
    void parse(const PeDataSource& source, quint64 offset);
    quint16 machine() const { return m_header.Machine; }
    quint16 numberOfSections() const { return m_header.NumberOfSections; }
    quint32 timeDateStamp() const { return m_header.TimeDateStamp; }
    quint32 pointerToSymbolTable() const { return m_header.PointerToSymbolTable; }
    quint32 numberOfSymbols() const { return m_header.NumberOfSymbols; }
    quint16 sizeOfOptionalHeader() const { return m_header.SizeOfOptionalHeader; }
    quint16 characteristics() const { return m_header.Characteristics; }

private:
    Q_DISABLE_COPY_MOVE(FileHeader)
    IMAGE_FILE_HEADER m_header{};
};

class OptionalHeader {
public:
    OptionalHeader() = default;
    void parse(const PeDataSource& source, quint64 offset, quint16 declaredSize);
    bool isPe32Plus() const;
    quint16 magic() const;
    std::pair<quint8, quint8> linkerVersion() const;
    quint32 sizeOfCode() const;
    quint32 sizeOfInitializedData() const;
    quint32 sizeOfUninitializedData() const;
    quint32 entryPointRva() const;
    quint32 baseOfCode() const;
    std::optional<quint32> baseOfData() const;
    quint64 imageBase() const;
    quint32 sectionAlignment() const;
    quint32 fileAlignment() const;
    std::pair<quint16, quint16> operatingSystemVersion() const;
    std::pair<quint16, quint16> imageVersion() const;
    std::pair<quint16, quint16> subsystemVersion() const;
    quint32 win32VersionValue() const;
    quint32 sizeOfImage() const;
    quint32 sizeOfHeaders() const;
    quint32 checksum() const;
    quint16 subsystem() const;
    quint16 dllCharacteristics() const;
    quint64 sizeOfStackReserve() const;
    quint64 sizeOfStackCommit() const;
    quint64 sizeOfHeapReserve() const;
    quint64 sizeOfHeapCommit() const;
    quint32 loaderFlags() const;
    quint32 numberOfRvaAndSizes() const;
    std::optional<IMAGE_DATA_DIRECTORY> dataDirectory(std::size_t index) const;
    const std::vector<IMAGE_DATA_DIRECTORY>& dataDirectories() const { return m_directories; }

private:
    Q_DISABLE_COPY_MOVE(OptionalHeader)
    std::variant<IMAGE_OPTIONAL_HEADER32, IMAGE_OPTIONAL_HEADER64> m_header{
        IMAGE_OPTIONAL_HEADER32{}};
    std::vector<IMAGE_DATA_DIRECTORY> m_directories;
};

class NtHeaders {
public:
    // Owns the parsed COFF and optional headers.
    NtHeaders() = default;
    void parse(const PeDataSource& source, quint64 offset);
    ~NtHeaders();
    quint32 signature() const { return m_signature; }
    quint64 totalSize() const { return 4 + 20 + quint64(m_file->sizeOfOptionalHeader()); }
    const FileHeader& fileHeader() const { return *m_file; }
    const OptionalHeader& optionalHeader() const { return *m_optional; }

private:
    Q_DISABLE_COPY_MOVE(NtHeaders)
    quint32 m_signature = 0;
    FileHeader* m_file = nullptr;
    OptionalHeader* m_optional = nullptr;
};
