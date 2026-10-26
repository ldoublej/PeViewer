#include "Sections.h"

#include <algorithm>
#include <cstddef>

#include "Headers.h"
#include "detail/ParseUtils.h"

QString PeSection::name() const {
    int length = 0;
    while (length < IMAGE_SIZEOF_SHORT_NAME && m_header.Name[length] != 0)
        ++length;
    return QString::fromLatin1(reinterpret_cast<const char*>(m_header.Name),
                               length);
}
const PeSection* PeSections::findByName(const QString& name) const {
    for (const auto* section : m_items)
        if (section->name() == name) return section;
    return nullptr;
}
const PeSection* PeSections::findByRva(quint32 rva) const {
    for (const auto* section : m_items) {
        const quint64 size =
            (std::max)(section->rawSize(), section->virtualSize());
        if (rva >= section->virtualAddress() &&
            quint64(rva) - section->virtualAddress() < size)
            return section;
    }
    return nullptr;
}
std::optional<quint64> PeSections::rvaToFileOffset(quint32 rva) const {
    for (const auto* section : m_items) {
        if (rva >= section->virtualAddress() &&
            quint64(rva) - section->virtualAddress() < section->rawSize())
            return quint64(section->rawOffset()) + rva -
                   section->virtualAddress();
    }
    return std::nullopt;
}

using namespace PeParseDetail;
namespace {
using Code = PeParseError::Code;
}
// Decode PE fields explicitly, independent of the host byte order.
#define PE_READ_VALUE(target, expression) target = (expression)

#define PE_READ(type, object, field, base)                          \
    PE_READ_VALUE(object.field, readScalar<decltype(object.field)>( \
                                    source, (base) + offsetof(type, field)))

PeSection* PeSection::parse(const PeDataSource& source, quint64 offset) {
    PeSection section;
    checkRange(source, offset, sizeof(IMAGE_SECTION_HEADER));
    auto& h = section.m_header;
    const auto name = readBytes(source, offset, IMAGE_SIZEOF_SHORT_NAME);
    std::memcpy(h.Name, name.constData(), IMAGE_SIZEOF_SHORT_NAME);
    PE_READ_VALUE(h.Misc.VirtualSize,
                  (readScalar<quint32>(
                      source, offset + offsetof(IMAGE_SECTION_HEADER, Misc))));
    PE_READ(IMAGE_SECTION_HEADER, h, VirtualAddress, offset);
    PE_READ(IMAGE_SECTION_HEADER, h, SizeOfRawData, offset);
    PE_READ(IMAGE_SECTION_HEADER, h, PointerToRawData, offset);
    PE_READ(IMAGE_SECTION_HEADER, h, PointerToRelocations, offset);
    PE_READ(IMAGE_SECTION_HEADER, h, PointerToLinenumbers, offset);
    PE_READ(IMAGE_SECTION_HEADER, h, NumberOfRelocations, offset);
    PE_READ(IMAGE_SECTION_HEADER, h, NumberOfLinenumbers, offset);
    PE_READ(IMAGE_SECTION_HEADER, h, Characteristics, offset);

    return new PeSection(std::move(section));
}

PeSections::~PeSections() {
    for (const auto* section : m_items) delete section;
}

PeSections* PeSections::parse(const PeDataSource& source, quint64 offset,
                              quint16 count,
                              const PeOptionalHeader& optionalHeader) {
    if (count > MaxSections)
        fail(Code::LimitExceeded,
             QStringLiteral("Section count exceeds parser limit"), offset);
    checkRange(source, offset, quint64(count) * sizeof(IMAGE_SECTION_HEADER));
    auto* result = new PeSections;
    try {
        result->m_items.reserve(count);
        for (quint32 i = 0; i < count;
             ++i, offset += sizeof(IMAGE_SECTION_HEADER)) {
            auto* section = PeSection::parse(source, offset);
            // Capacity is reserved; the pointer becomes owned before
            // validation.
            result->m_items.push_back(section);
            const quint64 virtualSize =
                (std::max)(section->rawSize(), section->virtualSize());
            const quint64 virtualEnd =
                quint64(section->virtualAddress()) + virtualSize;
            if (virtualEnd > Q_UINT64_C(0x100000000) ||
                virtualEnd > optionalHeader.sizeOfImage())
                fail(Code::InvalidHeader,
                     QStringLiteral("Section extends beyond SizeOfImage"),
                     offset);
            if (virtualSize &&
                section->virtualAddress() < optionalHeader.sizeOfHeaders())
                fail(Code::InvalidHeader,
                     QStringLiteral("Section overlaps PE headers"), offset);
            if (section->rawSize()) {
                if (section->rawOffset() < optionalHeader.sizeOfHeaders())
                    fail(Code::InvalidHeader,
                         QStringLiteral("Section raw data overlaps PE headers"),
                         offset);
                checkRange(source, section->rawOffset(), section->rawSize());
            }
            for (quint32 j = 0; j < i; ++j) {
                const auto* previous = result->m_items[j];
                const quint64 previousSize =
                    (std::max)(previous->rawSize(), previous->virtualSize());
                if (virtualSize && previousSize &&
                    section->virtualAddress() <
                        quint64(previous->virtualAddress()) + previousSize &&
                    previous->virtualAddress() < virtualEnd)
                    fail(Code::InvalidHeader,
                         QStringLiteral("Overlapping section RVA ranges"),
                         offset);
            }
        }
        return result;
    } catch (...) {
        delete result;
        throw;
    }
}

#undef PE_READ

#undef PE_READ_VALUE
