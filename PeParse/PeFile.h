#pragma once

#include <QtGlobal>

#include "Exports.h"
#include "Headers.h"
#include "Imports.h"
#include "ParseError.h"
#include "Sections.h"

class PeDataSource;

// Owns parsed results and borrows the data source for each parse() call.
class PeFile {
   public:
    // The source is borrowed and must remain valid whenever parse() is called.
    explicit PeFile(const PeDataSource& source);
    // Parses into this object; failure preserves the previous result.
    void parse();
    ~PeFile();

    // All returned references and pointers are borrowed from this PeFile.
    const DosHeader& dosHeader() const { return *m_dos; }
    const NtHeaders& ntHeaders() const { return *m_nt; }
    const Sections& sections() const { return *m_sections; }
    const std::vector<ImportModule>& imports() const {
        return m_imports->modules();
    }
    const Exports* exports() const { return m_exports; }
    bool isPe32Plus() const { return m_nt->optionalHeader().isPe32Plus(); }
    quint64 imageBase() const { return m_nt->optionalHeader().imageBase(); }
    quint32 entryPointRva() const {
        return m_nt->optionalHeader().entryPointRva();
    }
    std::optional<quint64> rvaToFileOffset(quint32 rva) const;

   private:
    Q_DISABLE_COPY_MOVE(PeFile)
    const PeDataSource& m_source;
    DosHeader* m_dos = nullptr;
    NtHeaders* m_nt = nullptr;
    Sections* m_sections = nullptr;
    Imports* m_imports = nullptr;
    Exports* m_exports = nullptr;
};
