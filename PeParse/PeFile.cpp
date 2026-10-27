#include "PeFile.h"

#include <PeDataSource.h>

#include <utility>

PeFile::PeFile(const PeDataSource& source) : m_source(source) {}

PeFile::~PeFile() {
    delete m_exports;
    delete m_imports;
    delete m_sections;
    delete m_nt;
    delete m_dos;
}

void PeFile::parse() {
    const auto& source = m_source;
    if (!source.isFileAligned())
        throw ParseError(ParseError::Code::UnsupportedLayout,
                         QStringLiteral("PeFile::parse requires file-layout PE data"));
    PeFile file(source);
    file.m_dos = new DosHeader;
    file.m_dos->parse(source);
    file.m_nt = new NtHeaders;
    file.m_nt->parse(source, file.m_dos->ntHeadersOffset());
    file.m_sections = new Sections;
    file.m_sections->parse(source, file.m_dos->ntHeadersOffset() + file.m_nt->totalSize(),
                           file.m_nt->fileHeader().numberOfSections(), file.m_nt->optionalHeader());
    quint64 stringBytes = 0;
    file.m_imports = new Imports;
    file.m_imports->parse(source, file, stringBytes);
    file.m_exports = new Exports;
    file.m_exports->parse(source, file, stringBytes);
    if (!file.m_exports->isPresent()) {
        delete file.m_exports;
        file.m_exports = nullptr;
    }

    // Commit only after parsing succeeds; file releases the previous result.
    std::swap(m_dos, file.m_dos);
    std::swap(m_nt, file.m_nt);
    std::swap(m_sections, file.m_sections);
    std::swap(m_imports, file.m_imports);
    std::swap(m_exports, file.m_exports);
}

std::optional<quint64> PeFile::rvaToFileOffset(quint32 rva) const {
    // Header and section file ranges are validated during parsing.
    if (rva < m_nt->optionalHeader().sizeOfHeaders()) return rva;
    return m_sections->rvaToFileOffset(rva);
}
