#pragma once

#include <PeStructs.h>

#include <QString>
#include <QStringList>
#include <QtGlobal>
#include <optional>
#include <utility>
#include <vector>

#include "ParseError.h"

class PeDataSource;
class PeFile;

class ExportEntry {
   public:
    enum class Kind { Empty, Function, Forwarder };
    Kind kind() const { return m_kind; }
    quint32 ordinal() const { return m_ordinal; }
    const QStringList& names() const { return m_names; }
    // Always the original EAT RVA, never a file offset or a resolved VA.
    quint32 rva() const { return m_rva; }
    const QString& forwarder() const { return m_forwarder; }

   private:
    friend class Exports;
    Kind m_kind = Kind::Empty;
    quint32 m_ordinal = 0;
    QStringList m_names;
    quint32 m_rva = 0;
    QString m_forwarder;
};

class Exports {
   public:
    Exports() = default;
    // Missing exports clear this object; malformed input propagates an
    // exception.
    void parse(const PeDataSource& source, const PeFile& file);
    bool isPresent() const { return m_present; }
    const QString& dllName() const { return m_name; }
    quint32 ordinalBase() const { return m_header.Base; }
    const std::vector<ExportEntry>& entries() const { return m_entries; }
    // Empty EAT slots are retained in entries(), but are not found as exports.
    const ExportEntry* findByName(const QString& name) const;
    const ExportEntry* findByOrdinal(quint32 ordinal) const;
    quint32 characteristics() const { return m_header.Characteristics; }
    quint32 timeDateStamp() const { return m_header.TimeDateStamp; }
    std::pair<quint16, quint16> version() const {
        return {m_header.MajorVersion, m_header.MinorVersion};
    }
    quint32 nameRva() const { return m_header.Name; }
    quint32 numberOfFunctions() const { return m_header.NumberOfFunctions; }
    quint32 numberOfNames() const { return m_header.NumberOfNames; }
    quint32 addressOfFunctions() const { return m_header.AddressOfFunctions; }
    quint32 addressOfNames() const { return m_header.AddressOfNames; }
    quint32 addressOfNameOrdinals() const {
        return m_header.AddressOfNameOrdinals;
    }

   private:
    friend class PeFile;
    Q_DISABLE_COPY_MOVE(Exports)
    void parse(const PeDataSource& source, const PeFile& file,
               quint64& stringBytes);
    IMAGE_EXPORT_DIRECTORY m_header{};
    QString m_name;
    std::vector<ExportEntry> m_entries;
    bool m_present = false;
};
