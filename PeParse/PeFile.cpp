#include "PeFile.h"

#include <PeDataSource.h>

#include <algorithm>

PeFile::~PeFile() {
    delete m_exports;
    delete m_imports;
    delete m_sections;
    delete m_nt;
    delete m_dos;
}

PeFile* PeFile::parse(const PeDataSource& source) {
    PeFile* file = nullptr;
    try {
        if (!source.isFileAligned())
            throw PeParseError(
                PeParseError::Code::UnsupportedLayout,
                QStringLiteral("PeFile::parse requires file-layout PE data"));
        file = new PeFile;
        file->m_fileSize = source.length();
        file->m_dos = PeDosHeader::parse(source);
        file->m_nt = PeNtHeaders::parse(source, file->m_dos->ntHeadersOffset());
        file->m_sections = PeSections::parse(
            source, file->m_dos->ntHeadersOffset() + file->m_nt->totalSize(),
            file->m_nt->fileHeader().numberOfSections(),
            file->m_nt->optionalHeader());
        quint64 stringBytes = 0;
        file->m_imports = PeImportModule::parseAll(source, *file, stringBytes);
        file->m_exports = PeExports::parse(source, *file, stringBytes);
        return file;
    } catch (const PeDataSourceError& error) {
        delete file;
        throw PeParseError(PeParseError::Code::DataSource,
                           QString::fromUtf8(error.what()), error.offset());
    } catch (...) {
        delete file;
        throw;
    }
}

std::optional<PeFile::MappedRange> PeFile::mappedRange(quint32 rva) const {
    const quint64 headers = m_nt->optionalHeader().sizeOfHeaders();
    if (rva < headers && rva < m_fileSize)
        return MappedRange{rva, (std::min)(headers, m_fileSize) - rva};
    for (const auto* section : m_sections->items()) {
        if (rva < section->virtualAddress()) continue;
        const quint64 delta = quint64(rva) - section->virtualAddress();
        if (delta >= section->rawSize()) continue;
        const quint64 offset = quint64(section->rawOffset()) + delta;
        if (offset >= m_fileSize) return std::nullopt;
        return MappedRange{offset,
                           (std::min)(quint64(section->rawSize()) - delta,
                                      m_fileSize - offset)};
    }
    return std::nullopt;
}
std::optional<quint64> PeFile::rvaToFileOffset(quint32 rva) const {
    const auto range = mappedRange(rva);
    return range ? std::optional<quint64>(range->offset) : std::nullopt;
}
