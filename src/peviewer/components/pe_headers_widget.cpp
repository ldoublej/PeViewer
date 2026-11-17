#include "pe_headers_widget.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QEvent>
#include <QGridLayout>
#include <QLocale>
#include <QStringList>
#include <QTableWidget>
#include <array>
#include <cstddef>
#include <initializer_list>
#include <optional>
#include <type_traits>
#include <utility>

#include "header_table_widget.h"
#include "headers.h"
#include "pe_file.h"

namespace {
QString hexValue(quint64 value, int digits) {
    return QStringLiteral("0x") +
           QString::number(value, 16).toUpper().rightJustified(digits, QLatin1Char('0'));
}

QString formatOffset(std::size_t offset) { return hexValue(static_cast<quint64>(offset), 4); }

QString timestampText(quint32 value) {
    if (value == 0) {
        return QCoreApplication::translate("PeHeadersWidget", "Not set");
    }
    return QDateTime::fromSecsSinceEpoch(static_cast<qint64>(value), Qt::UTC)
        .toString(QStringLiteral("yyyy-MM-dd HH:mm:ss 'UTC'"));
}

QString byteCount(quint64 value) {
    return QCoreApplication::translate("PeHeadersWidget", "%1 bytes")
        .arg(QLocale(QLocale::English).toString(static_cast<qulonglong>(value)));
}

QString joinFlags(quint32 value, std::initializer_list<std::pair<quint32, const char*>> flags) {
    QStringList names;
    for (const auto& flag : flags) {
        if ((value & flag.first) != 0) {
            names.append(QCoreApplication::translate("PeHeadersWidget", flag.second));
        }
    }
    return names.isEmpty() ? QCoreApplication::translate("PeHeadersWidget", "None")
                           : names.join(QStringLiteral(" · "));
}

template <typename Container>
QString wordList(const Container& values) {
    QStringList words;
    for (const quint16 value : values) {
        words.append(hexValue(value, 4));
    }
    return words.join(QLatin1Char(' '));
}

QString machineDescription(quint16 machine) {
    switch (machine) {
        case IMAGE_FILE_MACHINE_I386:
            return QCoreApplication::translate("PeHeadersWidget", "Intel 386");
        case IMAGE_FILE_MACHINE_R4000:
            return QCoreApplication::translate("PeHeadersWidget", "MIPS");
        case IMAGE_FILE_MACHINE_ARM:
            return QCoreApplication::translate("PeHeadersWidget", "ARM");
        case IMAGE_FILE_MACHINE_THUMB:
            return QCoreApplication::translate("PeHeadersWidget", "ARM Thumb");
        case IMAGE_FILE_MACHINE_ARMNT:
            return QCoreApplication::translate("PeHeadersWidget", "ARM Thumb-2");
        case IMAGE_FILE_MACHINE_IA64:
            return QCoreApplication::translate("PeHeadersWidget", "Intel Itanium");
        case IMAGE_FILE_MACHINE_AMD64:
            return QCoreApplication::translate("PeHeadersWidget", "x64");
        case IMAGE_FILE_MACHINE_ARM64:
            return QCoreApplication::translate("PeHeadersWidget", "ARM64");
        default:
            return QCoreApplication::translate("PeHeadersWidget", "Unknown machine");
    }
}

QString subsystemDescription(quint16 subsystem) {
    switch (subsystem) {
        case IMAGE_SUBSYSTEM_NATIVE:
            return QCoreApplication::translate("PeHeadersWidget", "Native");
        case IMAGE_SUBSYSTEM_WINDOWS_GUI:
            return QCoreApplication::translate("PeHeadersWidget", "Windows GUI");
        case IMAGE_SUBSYSTEM_WINDOWS_CUI:
            return QCoreApplication::translate("PeHeadersWidget", "Windows CUI");
        case IMAGE_SUBSYSTEM_OS2_CUI:
            return QCoreApplication::translate("PeHeadersWidget", "OS/2 CUI");
        case IMAGE_SUBSYSTEM_POSIX_CUI:
            return QCoreApplication::translate("PeHeadersWidget", "POSIX CUI");
        case IMAGE_SUBSYSTEM_WINDOWS_CE_GUI:
            return QCoreApplication::translate("PeHeadersWidget", "Windows CE GUI");
        case IMAGE_SUBSYSTEM_EFI_APPLICATION:
            return QCoreApplication::translate("PeHeadersWidget", "EFI application");
        case IMAGE_SUBSYSTEM_EFI_BOOT_SERVICE_DRIVER:
            return QCoreApplication::translate("PeHeadersWidget", "EFI boot service driver");
        case IMAGE_SUBSYSTEM_EFI_RUNTIME_DRIVER:
            return QCoreApplication::translate("PeHeadersWidget", "EFI runtime driver");
        case IMAGE_SUBSYSTEM_EFI_ROM:
            return QCoreApplication::translate("PeHeadersWidget", "EFI ROM");
        case IMAGE_SUBSYSTEM_XBOX:
            return QCoreApplication::translate("PeHeadersWidget", "Xbox");
        case IMAGE_SUBSYSTEM_WINDOWS_BOOT_APPLICATION:
            return QCoreApplication::translate("PeHeadersWidget", "Windows boot application");
        default:
            return QCoreApplication::translate("PeHeadersWidget", "Unknown subsystem");
    }
}

QString magicDescription(quint16 magic) {
    switch (magic) {
        case IMAGE_NT_OPTIONAL_HDR32_MAGIC:
            return QCoreApplication::translate("PeHeadersWidget", "PE32");
        case IMAGE_NT_OPTIONAL_HDR64_MAGIC:
            return QCoreApplication::translate("PeHeadersWidget", "PE32+");
        default:
            return QCoreApplication::translate("PeHeadersWidget", "Unknown optional header");
    }
}

void addRow(QVector<HeaderField>& rows, std::size_t offset, const char* name, const QString& value,
            const QString& description) {
    rows.append({formatOffset(offset), QString::fromLatin1(name), value, description});
}

QVector<HeaderField> dosRows(const DosHeader& header) {
    QVector<HeaderField> rows;
    addRow(rows, offsetof(IMAGE_DOS_HEADER, e_magic), "e_magic", hexValue(header.magic(), 4),
           QCoreApplication::translate("PeHeadersWidget", "MZ signature"));
    addRow(rows, offsetof(IMAGE_DOS_HEADER, e_cblp), "e_cblp",
           hexValue(header.bytesOnLastPage(), 4),
           QCoreApplication::translate("PeHeadersWidget", "Bytes on last page of file"));
    addRow(rows, offsetof(IMAGE_DOS_HEADER, e_cp), "e_cp", hexValue(header.pagesInFile(), 4),
           QCoreApplication::translate("PeHeadersWidget", "Pages in file"));
    addRow(rows, offsetof(IMAGE_DOS_HEADER, e_crlc), "e_crlc", hexValue(header.relocations(), 4),
           QCoreApplication::translate("PeHeadersWidget", "Relocations"));
    addRow(rows, offsetof(IMAGE_DOS_HEADER, e_cparhdr), "e_cparhdr",
           hexValue(header.headerParagraphs(), 4),
           QCoreApplication::translate("PeHeadersWidget", "Size of header in paragraphs"));
    addRow(rows, offsetof(IMAGE_DOS_HEADER, e_minalloc), "e_minalloc",
           hexValue(header.minimumExtraParagraphs(), 4),
           QCoreApplication::translate("PeHeadersWidget", "Minimum extra paragraphs needed"));
    addRow(rows, offsetof(IMAGE_DOS_HEADER, e_maxalloc), "e_maxalloc",
           hexValue(header.maximumExtraParagraphs(), 4),
           QCoreApplication::translate("PeHeadersWidget", "Maximum extra paragraphs needed"));
    addRow(rows, offsetof(IMAGE_DOS_HEADER, e_ss), "e_ss", hexValue(header.initialSs(), 4),
           QCoreApplication::translate("PeHeadersWidget", "Initial relative SS value"));
    addRow(rows, offsetof(IMAGE_DOS_HEADER, e_sp), "e_sp", hexValue(header.initialSp(), 4),
           QCoreApplication::translate("PeHeadersWidget", "Initial SP value"));
    addRow(rows, offsetof(IMAGE_DOS_HEADER, e_csum), "e_csum", hexValue(header.checksum(), 4),
           QCoreApplication::translate("PeHeadersWidget", "Checksum"));
    addRow(rows, offsetof(IMAGE_DOS_HEADER, e_ip), "e_ip", hexValue(header.initialIp(), 4),
           QCoreApplication::translate("PeHeadersWidget", "Initial IP value"));
    addRow(rows, offsetof(IMAGE_DOS_HEADER, e_cs), "e_cs", hexValue(header.initialCs(), 4),
           QCoreApplication::translate("PeHeadersWidget", "Initial relative CS value"));
    addRow(rows, offsetof(IMAGE_DOS_HEADER, e_lfarlc), "e_lfarlc",
           hexValue(header.relocationTableOffset(), 4),
           QCoreApplication::translate("PeHeadersWidget", "Address of relocation table"));
    addRow(rows, offsetof(IMAGE_DOS_HEADER, e_ovno), "e_ovno", hexValue(header.overlayNumber(), 4),
           QCoreApplication::translate("PeHeadersWidget", "Overlay number"));
    addRow(rows, offsetof(IMAGE_DOS_HEADER, e_res), "e_res[4]", wordList(header.reserved()),
           QCoreApplication::translate("PeHeadersWidget", "Reserved"));
    addRow(rows, offsetof(IMAGE_DOS_HEADER, e_oemid), "e_oemid", hexValue(header.oemId(), 4),
           QCoreApplication::translate("PeHeadersWidget", "OEM identifier"));
    addRow(rows, offsetof(IMAGE_DOS_HEADER, e_oeminfo), "e_oeminfo", hexValue(header.oemInfo(), 4),
           QCoreApplication::translate("PeHeadersWidget", "OEM information"));
    addRow(rows, offsetof(IMAGE_DOS_HEADER, e_res2), "e_res2[10]", wordList(header.reserved2()),
           QCoreApplication::translate("PeHeadersWidget", "Reserved"));
    addRow(rows, offsetof(IMAGE_DOS_HEADER, e_lfanew), "e_lfanew",
           hexValue(header.ntHeadersOffset(), 8),
           QCoreApplication::translate("PeHeadersWidget", "File offset of the NT headers"));
    return rows;
}

QVector<HeaderField> fileRows(const FileHeader& header) {
    QVector<HeaderField> rows;
    addRow(rows, offsetof(IMAGE_FILE_HEADER, Machine), "Machine", hexValue(header.machine(), 4),
           machineDescription(header.machine()));
    addRow(rows, offsetof(IMAGE_FILE_HEADER, NumberOfSections), "NumberOfSections",
           hexValue(header.numberOfSections(), 4),
           QCoreApplication::translate("PeHeadersWidget", "%1 sections")
               .arg(static_cast<uint>(header.numberOfSections())));
    addRow(rows, offsetof(IMAGE_FILE_HEADER, TimeDateStamp), "TimeDateStamp",
           hexValue(header.timeDateStamp(), 8), timestampText(header.timeDateStamp()));
    addRow(rows, offsetof(IMAGE_FILE_HEADER, PointerToSymbolTable), "PointerToSymbolTable",
           hexValue(header.pointerToSymbolTable(), 8),
           QCoreApplication::translate("PeHeadersWidget", "File offset of the COFF symbol table"));
    addRow(rows, offsetof(IMAGE_FILE_HEADER, NumberOfSymbols), "NumberOfSymbols",
           hexValue(header.numberOfSymbols(), 8),
           QCoreApplication::translate("PeHeadersWidget", "Number of COFF symbols"));
    addRow(rows, offsetof(IMAGE_FILE_HEADER, SizeOfOptionalHeader), "SizeOfOptionalHeader",
           hexValue(header.sizeOfOptionalHeader(), 4), byteCount(header.sizeOfOptionalHeader()));
    addRow(
        rows, offsetof(IMAGE_FILE_HEADER, Characteristics), "Characteristics",
        hexValue(header.characteristics(), 4),
        joinFlags(
            header.characteristics(),
            {{IMAGE_FILE_RELOCS_STRIPPED,
              QT_TRANSLATE_NOOP("PeHeadersWidget", "Relocations stripped")},
             {IMAGE_FILE_EXECUTABLE_IMAGE, QT_TRANSLATE_NOOP("PeHeadersWidget", "Executable")},
             {IMAGE_FILE_LINE_NUMS_STRIPPED,
              QT_TRANSLATE_NOOP("PeHeadersWidget", "Line numbers stripped")},
             {IMAGE_FILE_LOCAL_SYMS_STRIPPED,
              QT_TRANSLATE_NOOP("PeHeadersWidget", "Local symbols stripped")},
             {IMAGE_FILE_AGGRESIVE_WS_TRIM,
              QT_TRANSLATE_NOOP("PeHeadersWidget", "Aggressively trim working set")},
             {IMAGE_FILE_LARGE_ADDRESS_AWARE,
              QT_TRANSLATE_NOOP("PeHeadersWidget", "Large address aware")},
             {IMAGE_FILE_BYTES_REVERSED_LO,
              QT_TRANSLATE_NOOP("PeHeadersWidget", "Bytes reversed low")},
             {IMAGE_FILE_32BIT_MACHINE, QT_TRANSLATE_NOOP("PeHeadersWidget", "32-bit machine")},
             {IMAGE_FILE_DEBUG_STRIPPED,
              QT_TRANSLATE_NOOP("PeHeadersWidget", "Debug information stripped")},
             {IMAGE_FILE_REMOVABLE_RUN_FROM_SWAP,
              QT_TRANSLATE_NOOP("PeHeadersWidget", "Removable run from swap")},
             {IMAGE_FILE_NET_RUN_FROM_SWAP,
              QT_TRANSLATE_NOOP("PeHeadersWidget", "Net run from swap")},
             {IMAGE_FILE_SYSTEM, QT_TRANSLATE_NOOP("PeHeadersWidget", "System file")},
             {IMAGE_FILE_DLL, QT_TRANSLATE_NOOP("PeHeadersWidget", "DLL")},
             {IMAGE_FILE_UP_SYSTEM_ONLY, QT_TRANSLATE_NOOP("PeHeadersWidget", "Uniprocessor only")},
             {IMAGE_FILE_BYTES_REVERSED_HI,
              QT_TRANSLATE_NOOP("PeHeadersWidget", "Bytes reversed high")}}));
    return rows;
}

template <typename T>
QVector<HeaderField> optionalRows(const OptionalHeader& header) {
    constexpr bool is64 = std::is_same<T, IMAGE_OPTIONAL_HEADER64>::value;
    const int pointerDigits = is64 ? 16 : 8;
    QVector<HeaderField> rows;
    addRow(rows, offsetof(T, Magic), "Magic", hexValue(header.magic(), 4),
           magicDescription(header.magic()));
    addRow(rows, offsetof(T, MajorLinkerVersion), "MajorLinkerVersion",
           hexValue(header.linkerVersion().first, 2),
           QCoreApplication::translate("PeHeadersWidget", "Linker major version"));
    addRow(rows, offsetof(T, MinorLinkerVersion), "MinorLinkerVersion",
           hexValue(header.linkerVersion().second, 2),
           QCoreApplication::translate("PeHeadersWidget", "Linker minor version"));
    addRow(rows, offsetof(T, SizeOfCode), "SizeOfCode", hexValue(header.sizeOfCode(), 8),
           byteCount(header.sizeOfCode()));
    addRow(rows, offsetof(T, SizeOfInitializedData), "SizeOfInitializedData",
           hexValue(header.sizeOfInitializedData(), 8), byteCount(header.sizeOfInitializedData()));
    addRow(rows, offsetof(T, SizeOfUninitializedData), "SizeOfUninitializedData",
           hexValue(header.sizeOfUninitializedData(), 8),
           byteCount(header.sizeOfUninitializedData()));
    addRow(rows, offsetof(T, AddressOfEntryPoint), "AddressOfEntryPoint",
           hexValue(header.entryPointRva(), 8),
           QCoreApplication::translate("PeHeadersWidget", "RVA of the entry point"));
    addRow(rows, offsetof(T, BaseOfCode), "BaseOfCode", hexValue(header.baseOfCode(), 8),
           QCoreApplication::translate("PeHeadersWidget", "RVA of the beginning of code"));
    if constexpr (std::is_same<T, IMAGE_OPTIONAL_HEADER32>::value) {
        const std::optional<quint32> baseOfData = header.baseOfData();
        addRow(rows, offsetof(T, BaseOfData), "BaseOfData", hexValue(*baseOfData, 8),
               QCoreApplication::translate("PeHeadersWidget", "RVA of the beginning of data"));
    }
    addRow(rows, offsetof(T, ImageBase), "ImageBase", hexValue(header.imageBase(), pointerDigits),
           QCoreApplication::translate("PeHeadersWidget", "Preferred load address"));
    addRow(rows, offsetof(T, SectionAlignment), "SectionAlignment",
           hexValue(header.sectionAlignment(), 8),
           QCoreApplication::translate("PeHeadersWidget", "Alignment of sections in memory"));
    addRow(rows, offsetof(T, FileAlignment), "FileAlignment", hexValue(header.fileAlignment(), 8),
           QCoreApplication::translate("PeHeadersWidget", "Alignment of sections in the file"));
    addRow(rows, offsetof(T, MajorOperatingSystemVersion), "MajorOperatingSystemVersion",
           hexValue(header.operatingSystemVersion().first, 4),
           QCoreApplication::translate("PeHeadersWidget", "Required OS major version"));
    addRow(rows, offsetof(T, MinorOperatingSystemVersion), "MinorOperatingSystemVersion",
           hexValue(header.operatingSystemVersion().second, 4),
           QCoreApplication::translate("PeHeadersWidget", "Required OS minor version"));
    addRow(rows, offsetof(T, MajorImageVersion), "MajorImageVersion",
           hexValue(header.imageVersion().first, 4),
           QCoreApplication::translate("PeHeadersWidget", "Image major version"));
    addRow(rows, offsetof(T, MinorImageVersion), "MinorImageVersion",
           hexValue(header.imageVersion().second, 4),
           QCoreApplication::translate("PeHeadersWidget", "Image minor version"));
    addRow(rows, offsetof(T, MajorSubsystemVersion), "MajorSubsystemVersion",
           hexValue(header.subsystemVersion().first, 4),
           QCoreApplication::translate("PeHeadersWidget", "Required subsystem major version"));
    addRow(rows, offsetof(T, MinorSubsystemVersion), "MinorSubsystemVersion",
           hexValue(header.subsystemVersion().second, 4),
           QCoreApplication::translate("PeHeadersWidget", "Required subsystem minor version"));
    addRow(rows, offsetof(T, Win32VersionValue), "Win32VersionValue",
           hexValue(header.win32VersionValue(), 8),
           QCoreApplication::translate("PeHeadersWidget", "Reserved"));
    addRow(rows, offsetof(T, SizeOfImage), "SizeOfImage", hexValue(header.sizeOfImage(), 8),
           byteCount(header.sizeOfImage()));
    addRow(rows, offsetof(T, SizeOfHeaders), "SizeOfHeaders", hexValue(header.sizeOfHeaders(), 8),
           byteCount(header.sizeOfHeaders()));
    addRow(rows, offsetof(T, CheckSum), "CheckSum", hexValue(header.checksum(), 8),
           QCoreApplication::translate("PeHeadersWidget", "Image checksum"));
    addRow(rows, offsetof(T, Subsystem), "Subsystem", hexValue(header.subsystem(), 4),
           subsystemDescription(header.subsystem()));
    addRow(rows, offsetof(T, DllCharacteristics), "DllCharacteristics",
           hexValue(header.dllCharacteristics(), 4),
           joinFlags(
               header.dllCharacteristics(),
               {{IMAGE_DLLCHARACTERISTICS_HIGH_ENTROPY_VA,
                 QT_TRANSLATE_NOOP("PeHeadersWidget", "High entropy VA")},
                {IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE,
                 QT_TRANSLATE_NOOP("PeHeadersWidget", "Dynamic base")},
                {IMAGE_DLLCHARACTERISTICS_FORCE_INTEGRITY,
                 QT_TRANSLATE_NOOP("PeHeadersWidget", "Force integrity")},
                {IMAGE_DLLCHARACTERISTICS_NX_COMPAT,
                 QT_TRANSLATE_NOOP("PeHeadersWidget", "NX compatible")},
                {IMAGE_DLLCHARACTERISTICS_NO_ISOLATION,
                 QT_TRANSLATE_NOOP("PeHeadersWidget", "No isolation")},
                {IMAGE_DLLCHARACTERISTICS_NO_SEH, QT_TRANSLATE_NOOP("PeHeadersWidget", "No SEH")},
                {IMAGE_DLLCHARACTERISTICS_NO_BIND, QT_TRANSLATE_NOOP("PeHeadersWidget", "No bind")},
                {IMAGE_DLLCHARACTERISTICS_APPCONTAINER,
                 QT_TRANSLATE_NOOP("PeHeadersWidget", "AppContainer")},
                {IMAGE_DLLCHARACTERISTICS_WDM_DRIVER,
                 QT_TRANSLATE_NOOP("PeHeadersWidget", "WDM driver")},
                {IMAGE_DLLCHARACTERISTICS_GUARD_CF,
                 QT_TRANSLATE_NOOP("PeHeadersWidget", "Control Flow Guard")},
                {IMAGE_DLLCHARACTERISTICS_TERMINAL_SERVER_AWARE,
                 QT_TRANSLATE_NOOP("PeHeadersWidget", "Terminal Server aware")}}));
    addRow(rows, offsetof(T, SizeOfStackReserve), "SizeOfStackReserve",
           hexValue(header.sizeOfStackReserve(), pointerDigits),
           QCoreApplication::translate("PeHeadersWidget", "Stack reserve size"));
    addRow(rows, offsetof(T, SizeOfStackCommit), "SizeOfStackCommit",
           hexValue(header.sizeOfStackCommit(), pointerDigits),
           QCoreApplication::translate("PeHeadersWidget", "Stack commit size"));
    addRow(rows, offsetof(T, SizeOfHeapReserve), "SizeOfHeapReserve",
           hexValue(header.sizeOfHeapReserve(), pointerDigits),
           QCoreApplication::translate("PeHeadersWidget", "Heap reserve size"));
    addRow(rows, offsetof(T, SizeOfHeapCommit), "SizeOfHeapCommit",
           hexValue(header.sizeOfHeapCommit(), pointerDigits),
           QCoreApplication::translate("PeHeadersWidget", "Heap commit size"));
    addRow(rows, offsetof(T, LoaderFlags), "LoaderFlags", hexValue(header.loaderFlags(), 8),
           QCoreApplication::translate("PeHeadersWidget", "Reserved"));
    addRow(rows, offsetof(T, NumberOfRvaAndSizes), "NumberOfRvaAndSizes",
           hexValue(header.numberOfRvaAndSizes(), 8),
           QCoreApplication::translate("PeHeadersWidget", "Number of data directories"));
    return rows;
}
}  // namespace

PeHeadersWidget::PeHeadersWidget(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("peHeadersWidget"));

    m_dosTable = new HeaderTableWidget(this);
    m_dosTable->setObjectName(QStringLiteral("dosHeaderSection"));
    m_dosTable->table()->setObjectName(QStringLiteral("dosHeaderTable"));

    m_fileTable = new HeaderTableWidget(this);
    m_fileTable->setObjectName(QStringLiteral("fileHeaderSection"));
    m_fileTable->table()->setObjectName(QStringLiteral("fileHeaderTable"));

    m_optionalTable = new HeaderTableWidget(this);
    m_optionalTable->setObjectName(QStringLiteral("optionalHeaderSection"));
    m_optionalTable->table()->setObjectName(QStringLiteral("optionalHeaderTable"));

    // DOS header on the left; file and optional headers stacked on the right.
    auto* layout = new QGridLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setHorizontalSpacing(12);
    layout->setVerticalSpacing(12);
    layout->addWidget(m_dosTable, 0, 0, 2, 1);
    layout->addWidget(m_fileTable, 0, 1);
    layout->addWidget(m_optionalTable, 1, 1);
    layout->setColumnStretch(0, 1);
    layout->setColumnStretch(1, 1);
    layout->setRowStretch(0, 1);
    layout->setRowStretch(1, 1);

    retranslateUi();
}

void PeHeadersWidget::setPeFile(const PeFile* file) {
    setDosHeader(file ? &file->dosHeader() : nullptr);
    setFileHeader(file ? &file->ntHeaders().fileHeader() : nullptr);
    setOptionalHeader(file ? &file->ntHeaders().optionalHeader() : nullptr);
}

void PeHeadersWidget::setDosHeader(const DosHeader* header) {
    m_dosHeader = header;
    updateDosTable();
}

void PeHeadersWidget::setFileHeader(const FileHeader* header) {
    m_fileHeader = header;
    updateFileTable();
}

void PeHeadersWidget::setOptionalHeader(const OptionalHeader* header) {
    m_optionalHeader = header;
    updateOptionalTable();
}

void PeHeadersWidget::clear() {
    m_dosHeader = nullptr;
    m_fileHeader = nullptr;
    m_optionalHeader = nullptr;
    m_dosTable->clearRows();
    m_fileTable->clearRows();
    m_optionalTable->clearRows();
}

void PeHeadersWidget::retranslateUi() {
    m_dosTable->setTitle(QCoreApplication::translate("PeHeadersWidget", "DOS Header"));
    m_fileTable->setTitle(QCoreApplication::translate("PeHeadersWidget", "File Header"));
    m_optionalTable->setTitle(QCoreApplication::translate("PeHeadersWidget", "Optional Header"));
    m_dosTable->retranslateUi();
    m_fileTable->retranslateUi();
    m_optionalTable->retranslateUi();
    updateDosTable();
    updateFileTable();
    updateOptionalTable();
}

void PeHeadersWidget::changeEvent(QEvent* event) {
    QWidget::changeEvent(event);
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
}

void PeHeadersWidget::updateDosTable() {
    if (m_dosHeader) {
        m_dosTable->setRows(dosRows(*m_dosHeader));
    } else {
        m_dosTable->clearRows();
    }
}

void PeHeadersWidget::updateFileTable() {
    if (m_fileHeader) {
        m_fileTable->setRows(fileRows(*m_fileHeader));
    } else {
        m_fileTable->clearRows();
    }
}

void PeHeadersWidget::updateOptionalTable() {
    if (!m_optionalHeader) {
        m_optionalTable->clearRows();
        return;
    }
    if (m_optionalHeader->isPe32Plus()) {
        m_optionalTable->setRows(optionalRows<IMAGE_OPTIONAL_HEADER64>(*m_optionalHeader));
    } else {
        m_optionalTable->setRows(optionalRows<IMAGE_OPTIONAL_HEADER32>(*m_optionalHeader));
    }
}
