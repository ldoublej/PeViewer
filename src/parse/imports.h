#pragma once

#include <QString>
#include <QtGlobal>
#include <cstdint>
#include <optional>
#include <vector>

#include "parse_error.h"

class PeDataSource;
class PeFile;

class ImportEntry {
public:
    enum class Kind : std::int8_t { Name, Ordinal };
    Kind kind() const { return m_kind; }
    const QString& name() const { return m_name; }
    quint16 hint() const { return m_hint; }
    quint16 ordinal() const { return m_ordinal; }
    quint32 iatRva() const { return m_iatRva; }
    quint64 lookupValue() const { return m_lookupValue; }
    quint64 iatValue() const { return m_iatValue; }

private:
    friend class Imports;
    Kind m_kind = Kind::Name;
    QString m_name;
    quint16 m_hint = 0;
    quint16 m_ordinal = 0;
    quint32 m_iatRva = 0;
    quint64 m_lookupValue = 0;
    quint64 m_iatValue = 0;
};

class ImportModule {
public:
    const QString& dllName() const;
    const std::vector<ImportEntry>& entries() const;
    quint32 originalFirstThunk() const { return m_originalFirstThunk; }
    quint32 firstThunk() const { return m_firstThunk; }
    quint32 timeDateStamp() const { return m_timeDateStamp; }
    quint32 forwarderChain() const { return m_forwarderChain; }
    quint32 nameRva() const { return m_nameRva; }

private:
    friend class Imports;
    QString m_name;
    std::vector<ImportEntry> m_entries;
    quint32 m_originalFirstThunk = 0;
    quint32 m_firstThunk = 0;
    quint32 m_timeDateStamp = 0;
    quint32 m_forwarderChain = 0;
    quint32 m_nameRva = 0;
};

class Imports {
public:
    Imports() = default;
    void parse(const PeDataSource& source, const PeFile& file);
    const std::vector<ImportModule>& modules() const { return m_modules; }

private:
    friend class PeFile;
    Q_DISABLE_COPY_MOVE(Imports)
    void parse(const PeDataSource& source, const PeFile& file, quint64& stringBytes);
    std::vector<ImportModule> m_modules;
};
