#include "Imports.h"

#include "PeFile.h"
#include "detail/ParseUtils.h"

const QString& ImportModule::dllName() const { return m_name; }

const std::vector<ImportEntry>& ImportModule::entries() const {
    return m_entries;
}

using namespace PeParseDetail;
#define PE_READ_VALUE(target, expression) target = (expression)

#define PE_READ(type, object, field, base)                          \
    PE_READ_VALUE(object.field, readScalar<decltype(object.field)>( \
                                    source, (base) + offsetof(type, field)))

namespace {
using Code = ParseError::Code;
quint64 readThunk(const PeDataSource& source, const PeFile& file, quint32 rva,
                  bool is64) {
    if (is64) return readAtRva<quint64>(source, file, rva);
    const auto value = readAtRva<quint32>(source, file, rva);
    return value;
}
}  // namespace

void Imports::parse(const PeDataSource& source, const PeFile& file) {
    quint64 stringBytes = 0;
    parse(source, file, stringBytes);
}
void Imports::parse(const PeDataSource& source, const PeFile& file,
                    quint64& stringBytes) {
    std::vector<ImportModule> result;

    const auto dir = directory(file, IMAGE_DIRECTORY_ENTRY_IMPORT);
    if (!dir) {
        m_modules.clear();
        return;
    }
    const bool is64 = file.isPe32Plus();
    const quint64 thunkSize = is64 ? 8 : 4;
    quint64 descriptorOffset = 0;
    quint32 totalEntries = 0;
    for (;;) {
        if (dir->Size - descriptorOffset < sizeof(IMAGE_IMPORT_DESCRIPTOR))
            fail(Code::InvalidDirectory,
                 QStringLiteral("Import directory has no null descriptor "
                                "within its size"),
                 file.rvaToFileOffset(dir->VirtualAddress));
        const auto descriptor = addRva(dir->VirtualAddress, descriptorOffset);
        ImportModule module;
        PE_READ_VALUE(module.m_originalFirstThunk,
                      (readAtRva<quint32>(source, file, descriptor)));
        PE_READ_VALUE(
            module.m_timeDateStamp,
            (readAtRva<quint32>(source, file, addRva(descriptor, 4))));
        PE_READ_VALUE(
            module.m_forwarderChain,
            (readAtRva<quint32>(source, file, addRva(descriptor, 8))));
        PE_READ_VALUE(
            module.m_nameRva,
            (readAtRva<quint32>(source, file, addRva(descriptor, 12))));
        PE_READ_VALUE(
            module.m_firstThunk,
            (readAtRva<quint32>(source, file, addRva(descriptor, 16))));
        descriptorOffset += sizeof(IMAGE_IMPORT_DESCRIPTOR);
        if (!(module.m_originalFirstThunk | module.m_timeDateStamp |
              module.m_forwarderChain | module.m_nameRva | module.m_firstThunk))
            break;
        if (result.size() >= MaxEntries)
            fail(Code::LimitExceeded,
                 QStringLiteral("Import module count exceeds parser limit"));
        if (module.nameRva() == 0 || module.firstThunk() == 0)
            fail(Code::InvalidDirectory,
                 QStringLiteral("Import descriptor has no DLL name or IAT"),
                 file.rvaToFileOffset(descriptor));
        PE_READ_VALUE(
            module.m_name,
            (readCString(source, file, module.nameRva(), stringBytes)));
        const quint32 lookupRva = module.originalFirstThunk()
                                      ? module.originalFirstThunk()
                                      : module.firstThunk();
        for (quint64 index = 0;; ++index) {
            const quint64 delta = index * thunkSize;
            const auto value =
                readThunk(source, file, addRva(lookupRva, delta), is64);
            const auto iatRva = addRva(module.firstThunk(), delta);
            const auto iatValue = readThunk(source, file, iatRva, is64);
            if (value == 0) {
                if (iatValue != 0)
                    fail(Code::InvalidDirectory,
                         QStringLiteral(
                             "Import INT and IAT terminators disagree"),
                         file.rvaToFileOffset(iatRva));
                break;
            }
            if (totalEntries >= MaxEntries)
                fail(Code::LimitExceeded,
                     QStringLiteral("Import entry count exceeds parser limit"));
            ++totalEntries;
            ImportEntry entry;
            entry.m_iatRva = iatRva;
            entry.m_lookupValue = value;
            entry.m_iatValue = iatValue;
            const quint64 ordinalFlag =
                is64 ? Q_UINT64_C(0x8000000000000000) : Q_UINT64_C(0x80000000);
            if (value & ordinalFlag) {
                if (value & ~(ordinalFlag | Q_UINT64_C(0xffff)))
                    fail(
                        Code::InvalidDirectory,
                        QStringLiteral("Ordinal import has reserved bits set"));
                entry.m_kind = ImportEntry::Kind::Ordinal;
                entry.m_ordinal = static_cast<quint16>(value & 0xffff);
            } else {
                if (value > Q_UINT64_C(0xffffffff))
                    fail(Code::InvalidDirectory,
                         QStringLiteral("Import name RVA exceeds 32 bits"));
                const quint32 nameRva = static_cast<quint32>(value);
                PE_READ_VALUE(entry.m_hint,
                              (readAtRva<quint16>(source, file, nameRva)));
                PE_READ_VALUE(entry.m_name,
                              (readCString(source, file, addRva(nameRva, 2),
                                           stringBytes)));
            }
            module.m_entries.push_back(std::move(entry));
        }
        result.push_back(std::move(module));
    }
    m_modules.swap(result);
}

#undef PE_READ
#undef PE_READ_VALUE
