#pragma once

#include "Exports.h"
#include "Headers.h"
#include "Imports.h"
#include "ParseError.h"
#include "Sections.h"

class PeDataSource;

// An owning, read-only snapshot. No data-source lifetime is required after
// parse.
class PeFile {
   public:
    // The caller deletes PeFile; PeFile owns all parsed components.
    static PeFile* parse(const PeDataSource& source);
    ~PeFile();
    PeFile(const PeFile&) = delete;
    PeFile& operator=(const PeFile&) = delete;
    PeFile(PeFile&&) = delete;
    PeFile& operator=(PeFile&&) = delete;

    // All returned references and pointers are borrowed from this PeFile.
    const PeDosHeader& dosHeader() const { return *m_dos; }
    const PeNtHeaders& ntHeaders() const { return *m_nt; }
    const PeSections& sections() const { return *m_sections; }
    const std::vector<PeImportModule>& imports() const { return *m_imports; }
    const PeExports* exports() const { return m_exports; }
    bool isPe32Plus() const { return m_nt->optionalHeader().isPe32Plus(); }
    quint64 imageBase() const { return m_nt->optionalHeader().imageBase(); }
    quint32 entryPointRva() const {
        return m_nt->optionalHeader().entryPointRva();
    }
    quint64 fileSize() const { return m_fileSize; }
    std::optional<quint64> rvaToFileOffset(quint32 rva) const;

   private:
    PeFile() = default;
    struct MappedRange {
        quint64 offset;
        quint64 size;
    };
    std::optional<MappedRange> mappedRange(quint32 rva) const;
    PeDosHeader* m_dos = nullptr;
    PeNtHeaders* m_nt = nullptr;
    PeSections* m_sections = nullptr;
    std::vector<PeImportModule>* m_imports = nullptr;
    PeExports* m_exports = nullptr;
    quint64 m_fileSize = 0;
};
