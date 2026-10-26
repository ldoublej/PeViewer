#pragma once

#include <PeStructs.h>

#include <QString>
#include <QtGlobal>
#include <optional>
#include <vector>

#include "ParseError.h"

class PeDataSource;
class PeOptionalHeader;

// parse() returns a caller-owned pointer and throws PeParseError on failure.

class PeSection {
   public:
    static PeSection* parse(const PeDataSource& source, quint64 offset);
    QString name() const;
    quint32 virtualAddress() const { return m_header.VirtualAddress; }
    quint32 virtualSize() const { return m_header.Misc.VirtualSize; }
    quint32 rawOffset() const { return m_header.PointerToRawData; }
    quint32 rawSize() const { return m_header.SizeOfRawData; }
    quint32 pointerToRelocations() const {
        return m_header.PointerToRelocations;
    }
    quint32 pointerToLineNumbers() const {
        return m_header.PointerToLinenumbers;
    }
    quint16 numberOfRelocations() const { return m_header.NumberOfRelocations; }
    quint16 numberOfLineNumbers() const { return m_header.NumberOfLinenumbers; }
    quint32 characteristics() const { return m_header.Characteristics; }

   private:
    Q_DISABLE_COPY_MOVE(PeSection)
    PeSection() = default;
    IMAGE_SECTION_HEADER m_header{};
};

class PeSections {
   public:
    static PeSections* parse(const PeDataSource& source, quint64 offset,
                             quint16 count,
                             const PeOptionalHeader& optionalHeader);
    ~PeSections();
    // Borrowed elements, owned and deleted by PeSections.
    const std::vector<const PeSection*>& items() const { return m_items; }
    const PeSection* findByName(const QString& name) const;
    // Includes the virtual zero-filled tail. A match need not have file bytes.
    const PeSection* findByRva(quint32 rva) const;
    // Maps section raw data only; PeFile additionally handles PE headers.
    std::optional<quint64> rvaToFileOffset(quint32 rva) const;

   private:
    Q_DISABLE_COPY_MOVE(PeSections)
    PeSections() = default;
    std::vector<const PeSection*> m_items;
};
