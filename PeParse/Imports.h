#pragma once

#include <QString>
#include <QtGlobal>
#include <optional>
#include <vector>

#include "ParseError.h"

class PeDataSource;
class PeFile;

class PeImportEntry {
   public:
    enum class Kind { Name, Ordinal };
    Kind kind() const { return m_kind; }
    const QString& name() const { return m_name; }
    quint16 hint() const { return m_hint; }
    quint16 ordinal() const { return m_ordinal; }
    quint32 iatRva() const { return m_iatRva; }
    quint64 lookupValue() const { return m_lookupValue; }
    quint64 iatValue() const { return m_iatValue; }

   private:
    friend class PeImportModule;
    Kind m_kind = Kind::Name;
    QString m_name;
    quint16 m_hint = 0;
    quint16 m_ordinal = 0;
    quint32 m_iatRva = 0;
    quint64 m_lookupValue = 0;
    quint64 m_iatValue = 0;
};

class PeImportModule {
   public:
    // Returns an owning collection pointer (empty if no import directory).
    // The caller deletes it; malformed input throws PeParseError.
    static std::vector<PeImportModule>* parseAll(const PeDataSource& source,
                                                 const PeFile& file);
    const QString& dllName() const;
    const std::vector<PeImportEntry>& entries() const;
    quint32 originalFirstThunk() const { return m_originalFirstThunk; }
    quint32 firstThunk() const { return m_firstThunk; }
    quint32 timeDateStamp() const { return m_timeDateStamp; }
    quint32 forwarderChain() const { return m_forwarderChain; }
    quint32 nameRva() const { return m_nameRva; }

   private:
    friend class PeFile;
    static std::vector<PeImportModule>* parseAll(const PeDataSource& source,
                                                 const PeFile& file,
                                                 quint64& stringBytes);
    QString m_name;
    std::vector<PeImportEntry> m_entries;
    quint32 m_originalFirstThunk = 0;
    quint32 m_firstThunk = 0;
    quint32 m_timeDateStamp = 0;
    quint32 m_forwarderChain = 0;
    quint32 m_nameRva = 0;
};
