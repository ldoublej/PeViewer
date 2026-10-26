#include "Headers.h"

#include <algorithm>
#include <cstddef>
#include <type_traits>

#include "detail/ParseUtils.h"

std::array<quint16, 4> PeDosHeader::reserved() const {
    std::array<quint16, 4> result{};
    std::copy(std::begin(m_header.e_res), std::end(m_header.e_res),
              result.begin());
    return result;
}
std::array<quint16, 10> PeDosHeader::reserved2() const {
    std::array<quint16, 10> result{};
    std::copy(std::begin(m_header.e_res2), std::end(m_header.e_res2),
              result.begin());
    return result;
}
bool PeOptionalHeader::isPe32Plus() const {
    return std::holds_alternative<IMAGE_OPTIONAL_HEADER64>(m_header);
}

#define PE_OPTIONAL_GETTER(type, name, field)                            \
    type PeOptionalHeader::name() const {                                \
        return std::visit([](const auto& h) -> type { return h.field; }, \
                          m_header);                                     \
    }
PE_OPTIONAL_GETTER(quint16, magic, Magic)
PE_OPTIONAL_GETTER(quint32, sizeOfCode, SizeOfCode)
PE_OPTIONAL_GETTER(quint32, sizeOfInitializedData, SizeOfInitializedData)
PE_OPTIONAL_GETTER(quint32, sizeOfUninitializedData, SizeOfUninitializedData)
PE_OPTIONAL_GETTER(quint32, entryPointRva, AddressOfEntryPoint)
PE_OPTIONAL_GETTER(quint32, baseOfCode, BaseOfCode)
PE_OPTIONAL_GETTER(quint64, imageBase, ImageBase)
PE_OPTIONAL_GETTER(quint32, sectionAlignment, SectionAlignment)
PE_OPTIONAL_GETTER(quint32, fileAlignment, FileAlignment)
PE_OPTIONAL_GETTER(quint32, win32VersionValue, Win32VersionValue)
PE_OPTIONAL_GETTER(quint32, sizeOfImage, SizeOfImage)
PE_OPTIONAL_GETTER(quint32, sizeOfHeaders, SizeOfHeaders)
PE_OPTIONAL_GETTER(quint32, checksum, CheckSum)
PE_OPTIONAL_GETTER(quint16, subsystem, Subsystem)
PE_OPTIONAL_GETTER(quint16, dllCharacteristics, DllCharacteristics)
PE_OPTIONAL_GETTER(quint64, sizeOfStackReserve, SizeOfStackReserve)
PE_OPTIONAL_GETTER(quint64, sizeOfStackCommit, SizeOfStackCommit)
PE_OPTIONAL_GETTER(quint64, sizeOfHeapReserve, SizeOfHeapReserve)
PE_OPTIONAL_GETTER(quint64, sizeOfHeapCommit, SizeOfHeapCommit)
PE_OPTIONAL_GETTER(quint32, loaderFlags, LoaderFlags)
PE_OPTIONAL_GETTER(quint32, numberOfRvaAndSizes, NumberOfRvaAndSizes)
#undef PE_OPTIONAL_GETTER

#define PE_VERSION_GETTER(type, name, major, minor)             \
    std::pair<type, type> PeOptionalHeader::name() const {      \
        return std::visit(                                      \
            [](const auto& h) {                                 \
                return std::pair<type, type>{h.major, h.minor}; \
            },                                                  \
            m_header);                                          \
    }
PE_VERSION_GETTER(quint8, linkerVersion, MajorLinkerVersion, MinorLinkerVersion)
PE_VERSION_GETTER(quint16, operatingSystemVersion, MajorOperatingSystemVersion,
                  MinorOperatingSystemVersion)
PE_VERSION_GETTER(quint16, imageVersion, MajorImageVersion, MinorImageVersion)
PE_VERSION_GETTER(quint16, subsystemVersion, MajorSubsystemVersion,
                  MinorSubsystemVersion)
#undef PE_VERSION_GETTER

std::optional<quint32> PeOptionalHeader::baseOfData() const {
    if (const auto* h = std::get_if<IMAGE_OPTIONAL_HEADER32>(&m_header))
        return h->BaseOfData;
    return std::nullopt;
}
std::optional<IMAGE_DATA_DIRECTORY> PeOptionalHeader::dataDirectory(
    std::size_t index) const {
    if (index >= m_directories.size()) return std::nullopt;
    return m_directories[index];
}

using namespace PeParseDetail;
namespace {
using Code = PeParseError::Code;
bool isPowerOfTwo(quint32 value) {
    return value != 0 && (value & (value - 1)) == 0;
}
// Decode PE fields explicitly, independent of the host byte order.
#define PE_READ_VALUE(target, expression) target = (expression)

#define PE_READ(type, object, field, base)                          \
    PE_READ_VALUE(object.field, readScalar<decltype(object.field)>( \
                                    source, (base) + offsetof(type, field)))
template <typename T>
T readOptionalHeader(const PeDataSource& source, quint64 offset,
                     quint16 declaredSize) {
    constexpr std::size_t fixedSize = offsetof(T, DataDirectory);
    if (declaredSize < fixedSize)
        fail(Code::InvalidHeader,
             QStringLiteral("Optional header is smaller than its fixed fields"),
             offset);
    T h{};
    PE_READ(T, h, Magic, offset);
    PE_READ(T, h, MajorLinkerVersion, offset);
    PE_READ(T, h, MinorLinkerVersion, offset);
    PE_READ(T, h, SizeOfCode, offset);
    PE_READ(T, h, SizeOfInitializedData, offset);
    PE_READ(T, h, SizeOfUninitializedData, offset);
    PE_READ(T, h, AddressOfEntryPoint, offset);
    PE_READ(T, h, BaseOfCode, offset);
    if constexpr (std::is_same<T, IMAGE_OPTIONAL_HEADER32>::value) {
        PE_READ(T, h, BaseOfData, offset);
    }
    PE_READ(T, h, ImageBase, offset);
    PE_READ(T, h, SectionAlignment, offset);
    PE_READ(T, h, FileAlignment, offset);
    PE_READ(T, h, MajorOperatingSystemVersion, offset);
    PE_READ(T, h, MinorOperatingSystemVersion, offset);
    PE_READ(T, h, MajorImageVersion, offset);
    PE_READ(T, h, MinorImageVersion, offset);
    PE_READ(T, h, MajorSubsystemVersion, offset);
    PE_READ(T, h, MinorSubsystemVersion, offset);
    PE_READ(T, h, Win32VersionValue, offset);
    PE_READ(T, h, SizeOfImage, offset);
    PE_READ(T, h, SizeOfHeaders, offset);
    PE_READ(T, h, CheckSum, offset);
    PE_READ(T, h, Subsystem, offset);
    PE_READ(T, h, DllCharacteristics, offset);
    PE_READ(T, h, SizeOfStackReserve, offset);
    PE_READ(T, h, SizeOfStackCommit, offset);
    PE_READ(T, h, SizeOfHeapReserve, offset);
    PE_READ(T, h, SizeOfHeapCommit, offset);
    PE_READ(T, h, LoaderFlags, offset);
    PE_READ(T, h, NumberOfRvaAndSizes, offset);
    const quint32 count = (std::min)(quint32(h.NumberOfRvaAndSizes),
                                     quint32(IMAGE_NUMBEROF_DIRECTORY_ENTRIES));
    if (quint64(count) * 8 > declaredSize - fixedSize)
        fail(Code::InvalidHeader,
             QStringLiteral(
                 "Declared data directories exceed optional header size"),
             offset);
    for (quint32 i = 0; i < count; ++i) {
        PE_READ_VALUE(
            h.DataDirectory[i].VirtualAddress,
            (readScalar<quint32>(source, offset + fixedSize + quint64(i) * 8)));
        PE_READ_VALUE(h.DataDirectory[i].Size,
                      (readScalar<quint32>(
                          source, offset + fixedSize + quint64(i) * 8 + 4)));
    }
    return h;
}

}  // namespace

PeDosHeader* PeDosHeader::parse(const PeDataSource& source) {
    checkRange(source, 0, sizeof(IMAGE_DOS_HEADER));
    PeDosHeader result;
    auto& dos = result.m_header;
    PE_READ(IMAGE_DOS_HEADER, dos, e_magic, 0);
    if (dos.e_magic != IMAGE_DOS_SIGNATURE)
        fail(Code::InvalidSignature,
             QStringLiteral("Invalid DOS signature; expected MZ"), 0);
    PE_READ(IMAGE_DOS_HEADER, dos, e_cblp, 0);
    PE_READ(IMAGE_DOS_HEADER, dos, e_cp, 0);
    PE_READ(IMAGE_DOS_HEADER, dos, e_crlc, 0);
    PE_READ(IMAGE_DOS_HEADER, dos, e_cparhdr, 0);
    PE_READ(IMAGE_DOS_HEADER, dos, e_minalloc, 0);
    PE_READ(IMAGE_DOS_HEADER, dos, e_maxalloc, 0);
    PE_READ(IMAGE_DOS_HEADER, dos, e_ss, 0);
    PE_READ(IMAGE_DOS_HEADER, dos, e_sp, 0);
    PE_READ(IMAGE_DOS_HEADER, dos, e_csum, 0);
    PE_READ(IMAGE_DOS_HEADER, dos, e_ip, 0);
    PE_READ(IMAGE_DOS_HEADER, dos, e_cs, 0);
    PE_READ(IMAGE_DOS_HEADER, dos, e_lfarlc, 0);
    PE_READ(IMAGE_DOS_HEADER, dos, e_ovno, 0);
    PE_READ(IMAGE_DOS_HEADER, dos, e_oemid, 0);
    PE_READ(IMAGE_DOS_HEADER, dos, e_oeminfo, 0);
    PE_READ(IMAGE_DOS_HEADER, dos, e_lfanew, 0);
    for (std::size_t i = 0; i < 4; ++i)
        PE_READ_VALUE(dos.e_res[i],
                      (readScalar<quint16>(
                          source, offsetof(IMAGE_DOS_HEADER, e_res) + i * 2)));
    for (std::size_t i = 0; i < 10; ++i)
        PE_READ_VALUE(dos.e_res2[i],
                      (readScalar<quint16>(
                          source, offsetof(IMAGE_DOS_HEADER, e_res2) + i * 2)));
    if (dos.e_lfanew < static_cast<LONG>(sizeof(IMAGE_DOS_HEADER)))
        fail(Code::InvalidHeader, QStringLiteral("Invalid e_lfanew"),
             offsetof(IMAGE_DOS_HEADER, e_lfanew));

    auto* parsed = new PeDosHeader;
    parsed->m_header = result.m_header;
    return parsed;
}

PeFileHeader* PeFileHeader::parse(const PeDataSource& source, quint64 offset) {
    checkRange(source, offset, sizeof(IMAGE_FILE_HEADER));
    PeFileHeader result;
    auto& coff = result.m_header;
    PE_READ(IMAGE_FILE_HEADER, coff, Machine, offset);
    PE_READ(IMAGE_FILE_HEADER, coff, NumberOfSections, offset);
    PE_READ(IMAGE_FILE_HEADER, coff, TimeDateStamp, offset);
    PE_READ(IMAGE_FILE_HEADER, coff, PointerToSymbolTable, offset);
    PE_READ(IMAGE_FILE_HEADER, coff, NumberOfSymbols, offset);
    PE_READ(IMAGE_FILE_HEADER, coff, SizeOfOptionalHeader, offset);
    PE_READ(IMAGE_FILE_HEADER, coff, Characteristics, offset);
    auto* parsed = new PeFileHeader;
    parsed->m_header = result.m_header;
    return parsed;
}

PeOptionalHeader* PeOptionalHeader::parse(const PeDataSource& source,
                                          quint64 offset,
                                          quint16 declaredSize) {
    PeOptionalHeader result;
    checkRange(source, offset, declaredSize);
    if (declaredSize < 2)
        fail(Code::InvalidHeader,
             QStringLiteral("Missing optional header magic"), offset);
    const auto magic = readScalar<quint16>(source, offset);
    if (magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
        result.m_header = readOptionalHeader<IMAGE_OPTIONAL_HEADER32>(
            source, offset, declaredSize);
    } else if (magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
        result.m_header = readOptionalHeader<IMAGE_OPTIONAL_HEADER64>(
            source, offset, declaredSize);
    } else {
        fail(Code::UnsupportedFormat,
             QStringLiteral("Unsupported optional header magic %1").arg(magic),
             offset);
    }
    std::visit(
        [&](const auto& h) {
            const quint32 count =
                (std::min)(quint32(h.NumberOfRvaAndSizes),
                           quint32(IMAGE_NUMBEROF_DIRECTORY_ENTRIES));
            result.m_directories.assign(h.DataDirectory,
                                        h.DataDirectory + count);
        },
        result.m_header);
    const auto& optional = result;
    if (!isPowerOfTwo(optional.fileAlignment()) ||
        !isPowerOfTwo(optional.sectionAlignment()) ||
        optional.fileAlignment() > optional.sectionAlignment())
        fail(Code::InvalidHeader,
             QStringLiteral("Invalid file or section alignment"), offset);
    if (optional.sizeOfImage() == 0 ||
        optional.sizeOfHeaders() > optional.sizeOfImage())
        fail(Code::InvalidHeader,
             QStringLiteral("Invalid SizeOfImage or SizeOfHeaders"), offset);
    auto* parsed = new PeOptionalHeader;
    parsed->m_header = result.m_header;
    parsed->m_directories = std::move(result.m_directories);
    return parsed;
}

PeNtHeaders::~PeNtHeaders() {
    delete m_optional;
    delete m_file;
}

PeNtHeaders* PeNtHeaders::parse(const PeDataSource& source, quint64 offset) {
    checkRange(source, offset, 4 + sizeof(IMAGE_FILE_HEADER));
    auto* result = new PeNtHeaders;
    try {
        result->m_signature = readScalar<quint32>(source, offset);
        if (result->m_signature != IMAGE_NT_SIGNATURE)
            fail(Code::InvalidSignature,
                 QStringLiteral("Invalid NT signature; expected PE\\0\\0"),
                 offset);
        result->m_file = PeFileHeader::parse(source, offset + 4);
        const quint64 optionalOffset = offset + 4 + sizeof(IMAGE_FILE_HEADER);
        result->m_optional = PeOptionalHeader::parse(
            source, optionalOffset, result->m_file->sizeOfOptionalHeader());
        const quint64 sectionOffset = offset + result->totalSize();
        const quint64 sectionBytes =
            quint64(result->m_file->numberOfSections()) *
            sizeof(IMAGE_SECTION_HEADER);
        const quint64 headersSize = result->m_optional->sizeOfHeaders();
        if (sectionOffset > headersSize ||
            sectionBytes > headersSize - sectionOffset)
            fail(Code::InvalidHeader,
                 QStringLiteral(
                     "SizeOfHeaders does not cover the section table"),
                 optionalOffset);
        checkRange(source, 0, headersSize);
        return result;
    } catch (...) {
        delete result;
        throw;
    }
}

#undef PE_READ

#undef PE_READ_VALUE
