#include "Exports.h"

#include <cstddef>

#include "PeFile.h"
#include "detail/ParseUtils.h"

const ExportEntry* Exports::findByName(const QString& name) const {
    for (const auto& entry : m_entries)
        if (entry.kind() != ExportEntry::Kind::Empty &&
            entry.names().contains(name))
            return &entry;
    return nullptr;
}
const ExportEntry* Exports::findByOrdinal(quint32 ordinal) const {
    if (ordinal < ordinalBase()) return nullptr;
    const quint64 index = quint64(ordinal) - ordinalBase();
    if (index >= m_entries.size()) return nullptr;
    const auto& entry = m_entries[static_cast<std::size_t>(index)];
    return entry.kind() == ExportEntry::Kind::Empty ? nullptr : &entry;
}

using namespace PeParseDetail;
namespace {
using Code = ParseError::Code;
}
#define PE_READ_VALUE(target, expression) target = (expression)

#define PE_READ(type, object, field, base)                          \
    PE_READ_VALUE(object.field, readScalar<decltype(object.field)>( \
                                    source, (base) + offsetof(type, field)))

void Exports::parse(const PeDataSource& source, const PeFile& file) {
    quint64 stringBytes = 0;
    parse(source, file, stringBytes);
}
void Exports::parse(const PeDataSource& source, const PeFile& file,
                    quint64& stringBytes) {
    const auto dir = directory(file, IMAGE_DIRECTORY_ENTRY_EXPORT);
    if (!dir) {
        m_header = {};
        m_name.clear();
        m_entries.clear();
        m_present = false;
        return;
    }
    if (dir->Size < sizeof(IMAGE_EXPORT_DIRECTORY))
        fail(Code::InvalidDirectory,
             QStringLiteral("Export directory is smaller than its header"),
             file.rvaToFileOffset(dir->VirtualAddress));
    IMAGE_EXPORT_DIRECTORY h{};
    QString dllName;
    std::vector<ExportEntry> entries;
    const quint32 base = dir->VirtualAddress;
#define PE_EXPORT_READ(field)                    \
    PE_READ_VALUE(h.field,                       \
                  (readAtRva<decltype(h.field)>( \
                      source, file,              \
                      addRva(base, offsetof(IMAGE_EXPORT_DIRECTORY, field)))))
    PE_EXPORT_READ(Characteristics);
    PE_EXPORT_READ(TimeDateStamp);
    PE_EXPORT_READ(MajorVersion);
    PE_EXPORT_READ(MinorVersion);
    PE_EXPORT_READ(Name);
    PE_EXPORT_READ(Base);
    PE_EXPORT_READ(NumberOfFunctions);
    PE_EXPORT_READ(NumberOfNames);
    PE_EXPORT_READ(AddressOfFunctions);
    PE_EXPORT_READ(AddressOfNames);
    PE_EXPORT_READ(AddressOfNameOrdinals);
#undef PE_EXPORT_READ
    if (h.NumberOfFunctions > MaxEntries || h.NumberOfNames > MaxEntries)
        fail(Code::LimitExceeded,
             QStringLiteral("Export count exceeds parser limit"),
             file.rvaToFileOffset(base));
    if (h.NumberOfFunctions &&
        quint64(h.Base) + h.NumberOfFunctions - 1 > Q_UINT64_C(0xffffffff))
        fail(Code::InvalidDirectory,
             QStringLiteral("Export ordinal range overflows"),
             file.rvaToFileOffset(base));
    if (h.NumberOfFunctions && h.AddressOfFunctions == 0)
        fail(Code::InvalidDirectory,
             QStringLiteral("Missing export address table"),
             file.rvaToFileOffset(base));
    if (h.NumberOfNames &&
        (h.AddressOfNames == 0 || h.AddressOfNameOrdinals == 0))
        fail(Code::InvalidDirectory,
             QStringLiteral("Missing export name or ordinal table"),
             file.rvaToFileOffset(base));
    // Tables are located by their own RVAs and may be outside the directory
    // range.
    checkRvaRange(file, h.AddressOfFunctions, quint64(h.NumberOfFunctions) * 4);
    checkRvaRange(file, h.AddressOfNames, quint64(h.NumberOfNames) * 4);
    checkRvaRange(file, h.AddressOfNameOrdinals, quint64(h.NumberOfNames) * 2);
    if (h.Name)
        PE_READ_VALUE(dllName,
                      (readCString(source, file, h.Name, stringBytes)));
    entries.reserve(h.NumberOfFunctions);
    const quint64 directoryEnd = quint64(base) + dir->Size;
    for (quint32 i = 0; i < h.NumberOfFunctions; ++i) {
        ExportEntry entry;
        entry.m_ordinal = h.Base + i;
        PE_READ_VALUE(
            entry.m_rva,
            (readAtRva<quint32>(source, file,
                                addRva(h.AddressOfFunctions, quint64(i) * 4))));
        if (entry.m_rva) {
            if (entry.m_rva >= base && entry.m_rva < directoryEnd) {
                entry.m_kind = ExportEntry::Kind::Forwarder;
                PE_READ_VALUE(
                    entry.m_forwarder,
                    (readCString(source, file, entry.m_rva, stringBytes,
                                 directoryEnd - entry.m_rva)));
            } else {
                if (entry.m_rva >=
                    file.ntHeaders().optionalHeader().sizeOfImage())
                    fail(Code::InvalidDirectory,
                         QStringLiteral(
                             "Export function RVA is outside SizeOfImage"),
                         file.rvaToFileOffset(h.AddressOfFunctions));
                entry.m_kind = ExportEntry::Kind::Function;
            }
        }
        entries.push_back(std::move(entry));
    }
    for (quint32 i = 0; i < h.NumberOfNames; ++i) {
        const auto nameRva = readAtRva<quint32>(
            source, file, addRva(h.AddressOfNames, quint64(i) * 4));
        const auto index = readAtRva<quint16>(
            source, file, addRva(h.AddressOfNameOrdinals, quint64(i) * 2));
        if (index >= entries.size() || nameRva == 0)
            fail(Code::InvalidDirectory,
                 QStringLiteral("Invalid export name or ordinal index"),
                 file.rvaToFileOffset(h.AddressOfNameOrdinals));
        const auto name = readCString(source, file, nameRva, stringBytes);
        entries[index].m_names.append(name);
    }
    m_header = h;
    m_name = std::move(dllName);
    m_entries = std::move(entries);
    m_present = true;
}

#undef PE_READ
#undef PE_READ_VALUE
