#include <PeDataSource.h>
#include <PeFile.h>
#include <PeFileDataSource.h>

#include <QFile>
#include <QTemporaryDir>
#include <cstdio>
#include <cstring>
#include <functional>
#include <limits>
#include <stdexcept>
#include <type_traits>

#if defined(_MSC_VER) && defined(_DEBUG)
#include <crtdbg.h>
#endif

namespace {
template <typename T>
constexpr bool copyMoveDisabled =
    !std::is_copy_constructible<T>::value && !std::is_copy_assignable<T>::value &&
    !std::is_move_constructible<T>::value && !std::is_move_assignable<T>::value;

static_assert(copyMoveDisabled<PeFile>);
static_assert(copyMoveDisabled<DosHeader>);
static_assert(copyMoveDisabled<FileHeader>);
static_assert(copyMoveDisabled<OptionalHeader>);
static_assert(copyMoveDisabled<NtHeaders>);
static_assert(copyMoveDisabled<Section>);
static_assert(copyMoveDisabled<Sections>);
static_assert(copyMoveDisabled<Exports>);
static_assert(copyMoveDisabled<Imports>);

static_assert(std::is_member_function_pointer<decltype(&PeFile::parse)>::value);
static_assert(std::is_member_function_pointer<decltype(&DosHeader::parse)>::value);
static_assert(std::is_member_function_pointer<decltype(&FileHeader::parse)>::value);
static_assert(std::is_member_function_pointer<decltype(&OptionalHeader::parse)>::value);
static_assert(std::is_member_function_pointer<decltype(&NtHeaders::parse)>::value);
static_assert(std::is_member_function_pointer<decltype(&Section::parse)>::value);
static_assert(std::is_member_function_pointer<decltype(&Sections::parse)>::value);
static_assert(std::is_member_function_pointer<
              decltype(static_cast<void (Imports::*)(const PeDataSource&, const PeFile&)>(
                  &Imports::parse))>::value);
static_assert(std::is_member_function_pointer<
              decltype(static_cast<void (Exports::*)(const PeDataSource&, const PeFile&)>(
                  &Exports::parse))>::value);

void require(bool condition, const char* message = "PeParse assertion failed") {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

class ArraySource final : public PeDataSource {
public:
    explicit ArraySource(QByteArray bytes, bool fileLayout = true, bool failReads = false,
                         std::optional<quint64> failAt = std::nullopt)
        : m_bytes(std::move(bytes)),
          m_fileLayout(fileLayout),
          m_failReads(failReads),
          m_failAt(failAt) {}
    quint64 length() const override { return static_cast<quint64>(m_bytes.size()); }
    bool isFileAligned() const override { return m_fileLayout; }
    quint64 imageBase() const override { return 0; }
    QString url() const override { return QStringLiteral("test://pe"); }
    void readExact(quint64 offset, char* buffer, std::size_t count) const override {
        if (count == 0) {
            return;
        }
        if (!buffer || offset > length() || count > length() - offset) {
            throw PeDataSourceError(PeDataSourceError::Code::OutOfBounds,
                                    QStringLiteral("test source bounds"), offset, count);
        }
        if (m_failReads || (m_failAt && offset <= *m_failAt && count > *m_failAt - offset)) {
            throw PeDataSourceError(PeDataSourceError::Code::Io,
                                    QStringLiteral("injected read failure"),
                                    m_failAt.value_or(offset), count);
        }
        std::memcpy(buffer, m_bytes.constData() + static_cast<int>(offset), count);
    }

private:
    QByteArray m_bytes;
    bool m_fileLayout;
    bool m_failReads;
    std::optional<quint64> m_failAt;
};

struct Fixture {
    static constexpr int Nt = 0x80;
    static constexpr int Coff = Nt + 4;
    static constexpr int Optional = Coff + 20;
    QByteArray bytes = QByteArray(0x1200, '\0');
    bool is64;
    mutable std::optional<ArraySource> parsedSource;
    mutable PeFile* parsed = nullptr;

    ~Fixture() { delete parsed; }
    Fixture(const Fixture& other) : bytes(other.bytes), is64(other.is64) {}
    Fixture& operator=(const Fixture& other) {
        if (this != &other) {
            delete parsed;
            parsed = nullptr;
            bytes = other.bytes;
            is64 = other.is64;
        }
        return *this;
    }

    explicit Fixture(bool pe64 = false) : is64(pe64) {
        put(0, 0x5a4d, 2);
        put(0x3c, Nt);
        put(Nt, 0x4550);
        put(Coff, is64 ? 0x8664 : 0x14c, 2);
        put(Coff + 2, 1, 2);
        put(Coff + 4, 0x12345678);
        put(Coff + 16, is64 ? 240 : 224, 2);
        put(Coff + 18, 0x2102, 2);
        put(Optional, is64 ? 0x20b : 0x10b, 2);
        put(Optional + 2, 14, 1);
        put(Optional + 3, 7, 1);
        put(Optional + 4, 0x200);
        put(Optional + 8, 0x400);
        put(Optional + 12, 0x100);
        put(Optional + 16, 0x10f0);
        put(Optional + 20, 0x1000);
        if (is64) {
            put(Optional + 24, Q_UINT64_C(0x140000000), 8);
        } else {
            put(Optional + 24, 0x2000);
            put(Optional + 28, 0x400000);
        }
        put(Optional + 32, 0x1000);
        put(Optional + 36, 0x200);
        put(Optional + 40, 6, 2);
        put(Optional + 44, 1, 2);
        put(Optional + 48, 6, 2);
        put(Optional + 56, 0x3000);
        put(Optional + 60, 0x200);
        put(Optional + 68, 3, 2);
        put(Optional + 70, 0x140, 2);
        const int stack = Optional + 72;
        const int width = is64 ? 8 : 4;
        put(stack, is64 ? Q_UINT64_C(0x100000000) : 0x100000, width);
        put(stack + width, 0x1000, width);
        put(stack + width * 2, 0x200000, width);
        put(stack + width * 3, 0x2000, width);
        put(directoryCountOffset(), 16);
        text(sectionOffset(), ".test");
        put(sectionOffset() + 8, 0x1200);
        put(sectionOffset() + 12, 0x1000);
        put(sectionOffset() + 16, 0x1000);
        put(sectionOffset() + 20, 0x200);
        put(sectionOffset() + 36, 0x60000020);
    }
    int sectionOffset() const { return Optional + (is64 ? 240 : 224); }
    int directoryCountOffset() const { return Optional + (is64 ? 108 : 92); }
    int directoriesOffset() const { return directoryCountOffset() + 4; }
    int foa(quint32 rva) const {
        return rva < 0x200 ? static_cast<int>(rva) : static_cast<int>(rva - 0x1000 + 0x200);
    }
    void put(int offset, quint64 value, int width = 4) {
        require(width >= 0 && width <= static_cast<int>(sizeof(value)) && offset >= 0 &&
                    offset + width <= bytes.size(),
                "fixture write out of range");
        std::memcpy(bytes.data() + offset, &value, static_cast<std::size_t>(width));
    }
    void putRva(quint32 rva, quint64 value, int width = 4) { put(foa(rva), value, width); }
    void text(int offset, const char* value) {
        const auto size = std::strlen(value) + 1;
        require(offset >= 0 && quint64(offset) + size <= quint64(bytes.size()),
                "fixture text out of range");
        std::memcpy(bytes.data() + offset, value, size);
    }
    void setDirectory(int index, quint32 rva, quint32 size) {
        put(directoriesOffset() + index * 8, rva);
        put(directoriesOffset() + index * 8 + 4, size);
    }
    void imports(bool fallback = false) {
        setDirectory(1, 0x1100, 40);
        putRva(0x1100, fallback ? 0 : 0x1200);
        putRva(0x1104, 123);
        putRva(0x1108, 0xffffffff);
        putRva(0x110c, 0x11c0);
        putRva(0x1110, 0x1240);
        text(foa(0x11c0), "KERNEL32.dll");
        const int width = is64 ? 8 : 4;
        const quint64 ordinal =
            (is64 ? Q_UINT64_C(0x8000000000000000) : Q_UINT64_C(0x80000000)) | 0x42;
        putRva(0x1200, 0x1280, width);
        putRva(0x1200 + width, ordinal, width);
        putRva(0x1240, 0x1280, width);
        putRva(0x1240 + width, ordinal, width);
        putRva(0x1280, 7, 2);
        text(foa(0x1282), "ExitProcess");
    }
    void exports() {
        setDirectory(0, 0x1400, 0x100);
        putRva(0x1404, 456);
        putRva(0x1408, 2, 2);
        putRva(0x140a, 3, 2);
        putRva(0x140c, 0x14a0);
        putRva(0x1410, 0x10000);
        putRva(0x1414, 4);
        putRva(0x1418, 3);
        putRva(0x141c, 0x1600);
        putRva(0x1420, 0x1620);
        putRva(0x1424, 0x1640);
        text(foa(0x14a0), "sample.dll");
        text(foa(0x14c0), "OTHER.Target");
        putRva(0x1600, 0x10f0);
        putRva(0x1604, 0);
        putRva(0x1608, 0x14c0);
        putRva(0x160c, 0x2100);  // Zero-filled section tail: valid RVA, no FOA.
        putRva(0x1620, 0x1660);
        putRva(0x1624, 0x1670);
        putRva(0x1628, 0x1680);
        putRva(0x1640, 2, 2);
        putRva(0x1642, 0, 2);
        putRva(0x1644, 0, 2);
        text(foa(0x1660), "Alpha");
        text(foa(0x1670), "Zeta");
        text(foa(0x1680), "Alias");
    }
    const PeFile& parse() const {
        delete parsed;
        parsed = nullptr;
        parsedSource.emplace(bytes);
        parsed = new PeFile(*parsedSource);
        parsed->parse();
        return *parsed;
    }
};

template <typename Error = ParseError, typename F>
Error captureError(F operation) {
    try {
        if constexpr (std::is_pointer<decltype(operation())>::value) {
            delete operation();
        } else {
            operation();
        }
    } catch (const Error& error) {
        return error;
    }
    throw std::runtime_error("Expected exception was not thrown");
}

template <typename Error = ParseError, typename F>
void expectFailure(F operation) {
    const auto error = captureError<Error>(operation);
    require(std::strlen(error.what()) != 0, "Missing parsing diagnostic");
}

void headersAndMapping() {
    for (bool is64 : {false, true}) {
        Fixture f(is64);
        f.put(0x1c, 0x1234, 2);
        f.put(0x28, 0x5678, 2);
        const auto& pe = f.parse();
        const auto& nt = pe.ntHeaders();
        const auto& optional = nt.optionalHeader();
        require(pe.isPe32Plus() == is64);
        require(pe.dosHeader().magic() == 0x5a4d && pe.dosHeader().ntHeadersOffset() == 0x80);
        require(pe.dosHeader().reserved()[0] == 0x1234 && pe.dosHeader().reserved2()[0] == 0x5678);
        require(nt.signature() == 0x4550 && nt.totalSize() == quint64(is64 ? 264 : 248));
        require(nt.fileHeader().machine() == (is64 ? 0x8664 : 0x14c));
        require(nt.fileHeader().timeDateStamp() == 0x12345678 &&
                nt.fileHeader().characteristics() == 0x2102);
        require(pe.imageBase() == (is64 ? Q_UINT64_C(0x140000000) : 0x400000));
        require(pe.entryPointRva() == 0x10f0);
        require(optional.baseOfData() == (is64 ? std::nullopt : std::optional<quint32>(0x2000)));
        require(optional.sizeOfStackReserve() == (is64 ? Q_UINT64_C(0x100000000) : 0x100000));
        require(optional.sizeOfStackCommit() == 0x1000 && optional.sizeOfHeapReserve() == 0x200000);
        require(optional.linkerVersion() == std::make_pair(quint8(14), quint8(7)));
        require(optional.dataDirectories().size() == 16 && !optional.dataDirectory(16));
        require(pe.imports().empty() && !pe.exports());
        require(pe.sections().items().size() == 1);
        const auto* section = pe.sections().findByName(QStringLiteral(".test"));
        require(section && section->characteristics() == 0x60000020);
        require(pe.rvaToFileOffset(0x80) == 0x80 && !pe.sections().rvaToFileOffset(0x80));
        require(pe.rvaToFileOffset(0x1000) == 0x200 && pe.rvaToFileOffset(0x1fff) == 0x11ff);
        require(pe.sections().findByRva(0x2100) == section && !pe.rvaToFileOffset(0x2100));
        require(!pe.rvaToFileOffset(0x200) && !pe.rvaToFileOffset(0xffffffff));
        require(!pe.sections().findByRva(0x2200) &&
                !pe.sections().findByName(QStringLiteral("missing")));
    }
}

void optionalHeaderBounds() {
    for (bool is64 : {false, true}) {
        Fixture f(is64);
        const int oldSections = f.sectionOffset();
        const QByteArray section = f.bytes.mid(oldSections, 40);
        const int fixed = is64 ? 112 : 96;
        f.put(f.directoryCountOffset(), 0);
        f.put(Fixture::Coff + 16, fixed, 2);
        std::memcpy(f.bytes.data() + Fixture::Optional + fixed, section.constData(), 40);
        const auto& pe = f.parse();
        require(pe.ntHeaders().optionalHeader().dataDirectories().empty());
        require(pe.sections().findByName(QStringLiteral(".test")) != nullptr);
        f.put(f.directoryCountOffset(), 1);
        expectFailure([&] { return PeFile{ArraySource(f.bytes)}.parse(); });
        f.put(Fixture::Coff + 16, fixed - 1, 2);
        expectFailure([&] { return PeFile{ArraySource(f.bytes)}.parse(); });
    }
    Fixture f;
    f.put(f.directoryCountOffset(), 0xffffffff);
    require(f.parse().ntHeaders().optionalHeader().dataDirectories().size() == 16);
    f.put(f.directoryCountOffset(), 1);
    f.setDirectory(1, 0xffffffff, 40);  // Undeclared directory is ignored.
    require(f.parse().imports().empty());
}

void importTables() {
    for (bool is64 : {false, true}) {
        for (bool fallback : {false, true}) {
            Fixture f(is64);
            f.imports(fallback);
            if (!fallback) {
                f.putRva(0x1240, is64 ? Q_UINT64_C(0x7fff12345678) : 0x76543210, is64 ? 8 : 4);
            }
            const auto& pe = f.parse();
            require(pe.imports().size() == 1);
            const auto& module = pe.imports()[0];
            require(module.dllName() == QStringLiteral("KERNEL32.dll"));
            require(module.originalFirstThunk() == quint32(fallback ? 0 : 0x1200));
            require(module.firstThunk() == 0x1240 && module.timeDateStamp() == 123 &&
                    module.forwarderChain() == 0xffffffff);
            require(module.entries().size() == 2);
            const auto& byName = module.entries()[0];
            const auto& byOrdinal = module.entries()[1];
            require(byName.kind() == ImportEntry::Kind::Name &&
                    byName.name() == QStringLiteral("ExitProcess"));
            require(byName.hint() == 7 && byName.iatRva() == 0x1240 &&
                    byName.lookupValue() == 0x1280);
            require(byName.iatValue() ==
                    (fallback ? 0x1280 : (is64 ? Q_UINT64_C(0x7fff12345678) : 0x76543210)));
            require(byOrdinal.kind() == ImportEntry::Kind::Ordinal && byOrdinal.ordinal() == 0x42);
            require(byOrdinal.iatRva() == quint32(is64 ? 0x1248 : 0x1244));
        }
    }
}

void shortStringsAtEof() {
    Fixture f;
    f.imports();
    f.putRva(0x110c, 0x1ffc);
    f.text(f.foa(0x1ffc), "a.b");
    require(f.parse().imports()[0].dllName() == QStringLiteral("a.b"));
    f.putRva(0x1280, 9, 2);
    f.putRva(0x1200, 0x1ffa);
    f.putRva(0x1240, 0x1ffa);
    f.putRva(0x1ffa, 42, 2);
    f.text(f.foa(0x1ffc), "End");
    const auto& pe = f.parse();
    require(pe.imports()[0].entries()[0].name() == QStringLiteral("End"));
    require(pe.imports()[0].entries()[0].hint() == 42);
}

void exportTables() {
    for (bool is64 : {false, true}) {
        Fixture f(is64);
        f.exports();
        const auto& pe = f.parse();
        require(pe.exports() != nullptr);
        const auto& exports = *pe.exports();
        require(exports.dllName() == QStringLiteral("sample.dll") &&
                exports.ordinalBase() == 0x10000);
        require(exports.timeDateStamp() == 456 &&
                exports.version() == std::make_pair(quint16(2), quint16(3)));
        require(exports.entries().size() == 4 && exports.numberOfNames() == 3);
        const auto* normal = exports.findByName(QStringLiteral("Zeta"));
        const auto* alias = exports.findByName(QStringLiteral("Alias"));
        const auto* forward = exports.findByName(QStringLiteral("Alpha"));
        require(normal && normal == alias && normal->ordinal() == 0x10000);
        require(normal->kind() == ExportEntry::Kind::Function && normal->rva() == 0x10f0);
        require(forward && forward->ordinal() == 0x10002 &&
                forward->kind() == ExportEntry::Kind::Forwarder);
        require(forward->forwarder() == QStringLiteral("OTHER.Target"));
        require(exports.findByOrdinal(0x10003)->rva() == 0x2100 && !pe.rvaToFileOffset(0x2100));
        require(!exports.findByOrdinal(0xffff) && !exports.findByOrdinal(0x10001) &&
                !exports.findByOrdinal(0x10004));
        require(!exports.findByName(QStringLiteral("missing")));
        require(exports.entries()[1].kind() == ExportEntry::Kind::Empty);
    }
    Fixture empty;
    empty.setDirectory(0, 0x1400, 40);
    require(empty.parse().exports()->entries().empty());
    Fixture aliases;
    aliases.exports();
    aliases.putRva(0x1414, 1);
    aliases.putRva(0x1640, 0, 2);
    require(aliases.parse().exports()->entries()[0].names().size() == 3);
}

void bssAndSectionNames() {
    Fixture f;
    f.put(Fixture::Coff + 2, 2, 2);
    f.put(Fixture::Optional + 56, 0x4000);
    const int second = f.sectionOffset() + 40;
    std::memcpy(f.bytes.data() + second, "ABCDEFGH", 8);
    f.put(second + 8, 0x800);
    f.put(second + 12, 0x3000);
    const auto& pe = f.parse();
    require(pe.sections().findByName(QStringLiteral("ABCDEFGH")) != nullptr);
    require(pe.sections().findByRva(0x3000) && !pe.rvaToFileOffset(0x3000));
    f.put(second + 12, 0x2000);
    expectFailure([&] { return PeFile{ArraySource(f.bytes)}.parse(); });
    f.put(second + 12, 0xfffff000);
    f.put(second + 8, 0x2000);
    expectFailure([&] { return PeFile{ArraySource(f.bytes)}.parse(); });
}

void headersRejectMalformedData() {
    expectFailure([] { return PeFile{ArraySource(QByteArray())}.parse(); });
    Fixture f;
    expectFailure([&] { return PeFile{ArraySource(f.bytes, false)}.parse(); });
    expectFailure<PeDataSourceError>(
        [&] { return PeFile{ArraySource(f.bytes, true, true)}.parse(); });
    auto bad = f;
    bad.put(0, 0, 2);
    expectFailure([&] { return PeFile{ArraySource(bad.bytes)}.parse(); });
    for (quint32 offset : {quint32(0xffffffff), quint32(0x20)}) {
        bad = f;
        bad.put(0x3c, offset);
        expectFailure([&] { return PeFile{ArraySource(bad.bytes)}.parse(); });
    }
    bad = f;
    bad.put(0x3c, 0x100000);
    expectFailure([&] { return PeFile{ArraySource(bad.bytes)}.parse(); });
    bad = f;
    bad.put(Fixture::Nt, 0);
    expectFailure([&] { return PeFile{ArraySource(bad.bytes)}.parse(); });
    bad = f;
    bad.put(Fixture::Optional, 0x107, 2);
    expectFailure([&] { return PeFile{ArraySource(bad.bytes)}.parse(); });
    bad = f;
    bad.put(Fixture::Coff + 16, 1, 2);
    expectFailure([&] { return PeFile{ArraySource(bad.bytes)}.parse(); });
    bad = f;
    bad.put(Fixture::Optional + 36, 0);
    expectFailure([&] { return PeFile{ArraySource(bad.bytes)}.parse(); });
    bad = f;
    bad.put(Fixture::Optional + 60, 0x100);
    expectFailure([&] { return PeFile{ArraySource(bad.bytes)}.parse(); });
    bad = f;
    bad.bytes.chop(1);
    expectFailure([&] { return PeFile{ArraySource(bad.bytes)}.parse(); });
    bad = f;
    bad.put(bad.sectionOffset() + 20, 0x100);
    expectFailure([&] { return PeFile{ArraySource(bad.bytes)}.parse(); });
    bad = f;
    bad.put(bad.sectionOffset() + 12, 0xfffffff0);
    expectFailure([&] { return PeFile{ArraySource(bad.bytes)}.parse(); });
}

void directoriesRejectMalformedData() {
    Fixture f;
    f.imports();
    auto bad = f;
    bad.setDirectory(1, 0x1100, 20);
    expectFailure([&] { return PeFile{ArraySource(bad.bytes)}.parse(); });
    bad = f;
    bad.setDirectory(1, 0, 40);
    expectFailure([&] { return PeFile{ArraySource(bad.bytes)}.parse(); });
    bad = f;
    bad.setDirectory(1, 0xfffffff0, 40);
    expectFailure([&] { return PeFile{ArraySource(bad.bytes)}.parse(); });
    bad = f;
    bad.putRva(0x110c, 0x2100);
    expectFailure([&] { return PeFile{ArraySource(bad.bytes)}.parse(); });
    bad = f;
    bad.putRva(0x1100, 0x1ffc);
    bad.putRva(0x1ffc, 0x80000042);
    // Missing thunk terminator.
    expectFailure([&] { return PeFile{ArraySource(bad.bytes)}.parse(); });
    bad = f;
    bad.putRva(0x1200, 0x80010042);
    expectFailure([&] { return PeFile{ArraySource(bad.bytes)}.parse(); });
    bad = f;
    bad.putRva(0x1248, 1);
    expectFailure([&] { return PeFile{ArraySource(bad.bytes)}.parse(); });
    Fixture wide(true);
    wide.imports();
    wide.putRva(0x1200, Q_UINT64_C(0x100001280), 8);
    expectFailure([&] { return PeFile{ArraySource(wide.bytes)}.parse(); });

    f = Fixture();
    f.exports();
    bad = f;
    bad.setDirectory(0, 0x1400, 39);
    expectFailure([&] { return PeFile{ArraySource(bad.bytes)}.parse(); });
    bad = f;
    bad.putRva(0x1640, 4, 2);
    expectFailure([&] { return PeFile{ArraySource(bad.bytes)}.parse(); });
    bad = f;
    bad.putRva(0x1414, 0xffffffff);
    expectFailure([&] { return PeFile{ArraySource(bad.bytes)}.parse(); });
    bad = f;
    bad.putRva(0x1410, 0xffffffff);
    expectFailure([&] { return PeFile{ArraySource(bad.bytes)}.parse(); });
    bad = f;
    bad.putRva(0x141c, 0);
    expectFailure([&] { return PeFile{ArraySource(bad.bytes)}.parse(); });
    bad = f;
    bad.putRva(0x1608, 0x14fe);
    bad.bytes[bad.foa(0x14fe)] = 'A';
    bad.bytes[bad.foa(0x14ff)] = 'B';
    expectFailure([&] { return PeFile{ArraySource(bad.bytes)}.parse(); });
    bad = f;
    bad.putRva(0x1600, 0x3000);
    expectFailure([&] { return PeFile{ArraySource(bad.bytes)}.parse(); });
}

void stringsAndLimits() {
    Fixture f;
    f.bytes.resize(0x2200);
    f.bytes.fill('\0', 0x2200);
    // Rebuild with a larger raw section and image while preserving headers.
    Fixture original;
    std::memcpy(f.bytes.data(), original.bytes.constData(),
                static_cast<std::size_t>(original.bytes.size()));
    f.put(f.sectionOffset() + 8, 0x2000);
    f.put(f.sectionOffset() + 16, 0x2000);
    f.put(Fixture::Optional + 56, 0x4000);
    f.imports();
    f.putRva(0x110c, 0x2000);
    std::memset(f.bytes.data() + f.foa(0x2000), 'A', 4096);
    expectFailure([&] { return PeFile{ArraySource(f.bytes)}.parse(); });
}

void crossSectionReads() {
    Fixture f;
    f.imports();
    // Split the existing RVA range at 0x1800 and move the second half in the
    // file. Leave an unmapped file gap, so incrementing FOA instead of RVA is
    // observable.
    const QByteArray secondHalf = f.bytes.mid(f.foa(0x1800), 0x800);
    f.bytes.resize(0x1a00);
    std::memset(f.bytes.data() + 0xa00, 0x7f, 0x1000);
    std::memcpy(f.bytes.data() + 0x1200, secondHalf.constData(), 0x800);
    f.put(Fixture::Coff + 2, 2, 2);
    f.put(f.sectionOffset() + 8, 0x800);
    f.put(f.sectionOffset() + 16, 0x800);
    const int second = f.sectionOffset() + 40;
    f.text(second, ".second");
    f.put(second + 8, 0xa00);
    f.put(second + 12, 0x1800);
    f.put(second + 16, 0x800);
    f.put(second + 20, 0x1200);
    // A four-byte lookup entry spans two sections with discontiguous FOAs.
    f.putRva(0x1100, 0x17fe);
    f.put(f.foa(0x17fe), 0x1280, 2);
    f.put(0x1200, 0, 2);
    f.put(0x1202, 0);
    // A DLL name also crosses the same boundary (separately from the lookup).
    auto stringFixture = f;
    stringFixture.putRva(0x1100, 0x1200);
    stringFixture.putRva(0x110c, 0x17fe);
    std::memcpy(stringFixture.bytes.data() + stringFixture.foa(0x17fe), "AB", 2);
    stringFixture.text(0x1200, "CD.dll");
    require(stringFixture.parse().imports()[0].dllName() == QStringLiteral("ABCD.dll"));
    // The one-entry lookup requires a one-entry IAT as well.
    f.putRva(0x1244, 0);
    const auto& pe = f.parse();
    require(pe.imports()[0].entries().size() == 1);
    require(pe.imports()[0].entries()[0].name() == QStringLiteral("ExitProcess"));
    require(pe.rvaToFileOffset(0x1800) == 0x1200);
    // Place descriptors across the boundary, too: the null descriptor follows
    // in the second section, and is still bounded by the directory's RVA size.
    auto descriptors = stringFixture;
    const auto table = descriptors.bytes.mid(descriptors.foa(0x1100), 40);
    descriptors.setDirectory(1, 0x17f6, 40);
    std::memcpy(descriptors.bytes.data() + descriptors.foa(0x17f6), table.constData(), 10);
    std::memcpy(descriptors.bytes.data() + 0x1200, table.constData() + 10, 30);
    descriptors.put(descriptors.foa(0x17f6) + 0, 0x1200);
    // Restore a DLL name outside the descriptor storage.
    descriptors.put(0x1200 + 2, 0x11c0);
    require(descriptors.parse().imports()[0].dllName() == QStringLiteral("KERNEL32.dll"));
}

void countLimits() {
    Fixture sections;
    sections.bytes.resize(0x30000);
    sections.put(Fixture::Coff + 2, 4097, 2);
    sections.put(Fixture::Optional + 60, 0x30000);
    sections.put(Fixture::Optional + 56, 0x40000);
    expectFailure([&] { return PeFile{ArraySource(sections.bytes)}.parse(); });

    Fixture imports;
    imports.bytes.resize(0x70000);
    // Fill the expansion deterministically; QByteArray::resize does not zero
    // it.
    std::memset(imports.bytes.data() + 0x1200, 0, 0x70000 - 0x1200);
    imports.put(imports.sectionOffset() + 8, 0x6fe00);
    imports.put(imports.sectionOffset() + 16, 0x6fe00);
    imports.put(Fixture::Optional + 56, 0x71000);
    imports.imports(true);
    for (int i = 0; i <= 100000; ++i) {
        imports.put(imports.foa(0x1240) + i * 4, 0x80000001);
    }
    expectFailure([&] { return PeFile{ArraySource(imports.bytes)}.parse(); });
}

void fileSourceIntegration() {
    QTemporaryDir directory;
    require(directory.isValid());
    Fixture f(true);
    f.imports();
    f.exports();
    const QString path = directory.filePath(QStringLiteral("sample.exe"));
    QFile output(path);
    require(output.open(QIODevice::WriteOnly));
    require(output.write(f.bytes) == f.bytes.size());
    output.close();
    std::optional<PeFile> pe;
    {
        PeFileDataSource source(path);
        pe.emplace(source);
        pe->parse();
    }
    // Reading parsed results and destruction do not access the borrowed source.
    require(pe->isPe32Plus() && pe->imports().size() == 1 && pe->exports()->entries().size() == 4);
}

void readFailuresThrow() {
    for (bool is64 : {false, true}) {
        Fixture f(is64);
        f.imports();
        f.exports();
        // Include unused metadata and terminators: read failure must never be
        // mistaken for a decoded zero or an absent directory.
        const int offsets[] = {0,
                               2,
                               0x3c,
                               Fixture::Nt,
                               Fixture::Coff + 4,
                               Fixture::Optional + 32,
                               Fixture::Optional + 72,
                               f.directoryCountOffset(),
                               f.directoriesOffset(),
                               f.sectionOffset() + 36,
                               f.foa(0x1104),
                               f.foa(0x1114),
                               f.foa(0x1200),
                               f.foa(0x1240),
                               f.foa(0x1280),
                               f.foa(0x1282),
                               f.foa(0x11c0),
                               f.foa(0x1404),
                               f.foa(0x1600),
                               f.foa(0x1620),
                               f.foa(0x1640),
                               f.foa(0x1660),
                               f.foa(0x14c0)};
        for (int offset : offsets) {
            expectFailure<PeDataSourceError>([&] {
                return PeFile{ArraySource(f.bytes, true, false, static_cast<quint64>(offset))}
                    .parse();
            });
        }
    }
}

void componentParsers() {
    for (bool is64 : {false, true}) {
        Fixture f(is64);
        f.imports();
        f.exports();
        ArraySource source(f.bytes);
        ArraySource unreadable(f.bytes, true, true);
        const auto& file = f.parse();
        DosHeader dos;
        dos.parse(source);
        require(dos.ntHeadersOffset() == Fixture::Nt);
        FileHeader coff;
        coff.parse(source, Fixture::Coff);
        require(coff.numberOfSections() == 1);
        const quint16 size = file.ntHeaders().fileHeader().sizeOfOptionalHeader();
        OptionalHeader optional;
        optional.parse(source, Fixture::Optional, size);
        require(optional.isPe32Plus() == is64);
        NtHeaders nt;
        nt.parse(source, Fixture::Nt);
        require(nt.optionalHeader().isPe32Plus() == is64);
        Section section;
        section.parse(source, f.sectionOffset());
        require(section.name() == QStringLiteral(".test"));
        Sections sections;
        sections.parse(source, f.sectionOffset(), 1, file.ntHeaders().optionalHeader());
        require(sections.items().size() == 1 &&
                sections.items()[0] == sections.findByName(QStringLiteral(".test")));
        Imports imports;
        imports.parse(source, file);
        require(imports.modules().size() == 1 && imports.modules().front().entries().size() == 2);
        Exports exports;
        exports.parse(source, file);
        require(exports.isPresent() && exports.entries().size() == 4);
        expectFailure<PeDataSourceError>([&] { dos.parse(unreadable); });
        expectFailure<PeDataSourceError>([&] { coff.parse(unreadable, Fixture::Coff); });
        expectFailure<PeDataSourceError>(
            [&] { optional.parse(unreadable, Fixture::Optional, size); });
        expectFailure<PeDataSourceError>([&] { nt.parse(unreadable, Fixture::Nt); });
        expectFailure<PeDataSourceError>([&] { section.parse(unreadable, f.sectionOffset()); });
        expectFailure<PeDataSourceError>([&] {
            sections.parse(unreadable, f.sectionOffset(), 1, file.ntHeaders().optionalHeader());
        });
        expectFailure<PeDataSourceError>([&] { imports.parse(unreadable, file); });
        expectFailure<PeDataSourceError>([&] { exports.parse(unreadable, file); });
        require(dos.magic() == IMAGE_DOS_SIGNATURE && coff.numberOfSections() == 1 &&
                optional.isPe32Plus() == is64 && nt.optionalHeader().isPe32Plus() == is64 &&
                section.name() == QStringLiteral(".test") && sections.items().size() == 1 &&
                imports.modules().size() == 1 && exports.isPresent() &&
                exports.entries().size() == 4);

        // Reusing objects for a file with absent directories clears old data.
        Fixture absent(is64);
        ArraySource absentSource(absent.bytes);
        const auto& absentFile = absent.parse();
        imports.parse(absentSource, absentFile);
        exports.parse(absentSource, absentFile);
        require(imports.modules().empty() && !exports.isPresent() && exports.dllName().isEmpty() &&
                exports.entries().empty() && exports.findByOrdinal(0x10000) == nullptr);
    }
}

void exceptionDetails() {
    using Code = ParseError::Code;
    Fixture f;
    auto error = captureError([&] { PeFile{ArraySource(f.bytes, false)}.parse(); });
    require(error.code() == Code::UnsupportedLayout && !error.directoryIndex());
    auto bad = f;
    bad.put(0, 0, 2);
    error = captureError([&] { bad.parse(); });
    require(error.code() == Code::InvalidSignature && error.offset() == 0);
    bad = f;
    bad.put(Fixture::Nt, 0);
    error = captureError([&] { bad.parse(); });
    require(error.code() == Code::InvalidSignature && error.offset() == Fixture::Nt);
    bad = f;
    bad.put(Fixture::Optional, 0x107, 2);
    error = captureError([&] { bad.parse(); });
    require(error.code() == Code::UnsupportedFormat && error.offset() == Fixture::Optional);
    error = captureError([] { PeFile{ArraySource(QByteArray())}.parse(); });
    require(error.code() == Code::TruncatedData && error.offset() == 0);

    f.imports();
    f.exports();
    bad = f;
    bad.setDirectory(1, 0x1100, 20);
    error = captureError([&] { bad.parse(); });
    require(error.code() == Code::InvalidDirectory && error.offset() == quint64(f.foa(0x1100)) &&
            !error.directoryIndex());
    bad = f;
    bad.putRva(0x1640, 4, 2);
    error = captureError([&] { bad.parse(); });
    require(error.code() == Code::InvalidDirectory && error.offset() == quint64(f.foa(0x1640)) &&
            !error.directoryIndex());
    bad = f;
    bad.setDirectory(1, 0x1100, 0);
    error = captureError([&] { bad.parse(); });
    require(error.code() == Code::InvalidDirectory && error.directoryIndex() == 1);
    for (int offset :
         {0, Fixture::Optional + 72, f.sectionOffset(), f.foa(0x1200), f.foa(0x1600)}) {
        const auto sourceError = captureError<PeDataSourceError>(
            [&] { return PeFile{ArraySource(f.bytes, true, false, quint64(offset))}.parse(); });
        require(sourceError.code() == PeDataSourceError::Code::Io);
        require(sourceError.offset() == quint64(offset));
        require(sourceError.length() > 0 && std::strlen(sourceError.what()) > 0);
    }
}

void ownershipCleanup() {
    Fixture f(true);
    f.imports();
    f.exports();
    ArraySource source(f.bytes);
    ArraySource headerFailure(f.bytes, true, false, Fixture::Optional + 72);
    ArraySource importFailure(f.bytes, true, false, quint64(f.foa(0x1282)));
    ArraySource exportFailure(f.bytes, true, false, quint64(f.foa(0x1640)));
    Fixture invalidSection = f;
    invalidSection.put(invalidSection.sectionOffset() + 12, 0x2000);
    ArraySource sectionFailure(invalidSection.bytes);

    const auto cycle = [&] {
        source = ArraySource(f.bytes);
        PeFile* file = new PeFile(source);
        file->parse();
        require(file->dosHeader().magic() == 0x5a4d);
        require(file->ntHeaders().fileHeader().numberOfSections() == 1);
        require(file->sections().items()[0] == file->sections().findByRva(0x1000));
        require(file->imports().size() == 1 && file->exports()->entries().size() == 4);
        const auto* oldNt = &file->ntHeaders();
        const auto* oldSections = &file->sections();
        const auto* oldExports = file->exports();
        expectFailure<PeDataSourceError>([&] {
            source = headerFailure;
            file->parse();
        });
        expectFailure([&] {
            source = sectionFailure;
            file->parse();
        });
        expectFailure<PeDataSourceError>([&] {
            source = importFailure;
            file->parse();
        });
        expectFailure<PeDataSourceError>([&] {
            source = exportFailure;
            file->parse();
        });
        require(&file->ntHeaders() == oldNt && &file->sections() == oldSections &&
                file->exports() == oldExports && file->isPe32Plus() &&
                file->imports().size() == 1 && file->exports()->entries().size() == 4);
        source = ArraySource(f.bytes);
        file->parse();
        Fixture absent;
        source = ArraySource(absent.bytes);
        file->parse();
        require(!file->isPe32Plus() && file->imports().empty() && file->exports() == nullptr);
        delete file;
        expectFailure<PeDataSourceError>([&] { return PeFile{headerFailure}.parse(); });
        expectFailure([&] { return PeFile{sectionFailure}.parse(); });
        expectFailure<PeDataSourceError>([&] { return PeFile{importFailure}.parse(); });
        expectFailure<PeDataSourceError>([&] { return PeFile{exportFailure}.parse(); });
    };
    cycle();  // Initialize any runtime caches before checking allocations.
#if defined(_MSC_VER) && defined(_DEBUG)
    _CrtMemState before{}, after{}, difference{};
    _CrtMemCheckpoint(&before);
#endif
    for (int i = 0; i < 20; ++i) {
        cycle();
    }
#if defined(_MSC_VER) && defined(_DEBUG)
    _CrtMemCheckpoint(&after);
    _CrtMemDifference(&difference, &before, &after);
    require(difference.lCounts[_NORMAL_BLOCK] == 0 && difference.lSizes[_NORMAL_BLOCK] == 0,
            "Parsed objects leaked after deletion or an exception");
#endif
}
}  // namespace

int main(int argc, char** argv) {
    const std::pair<const char*, std::function<void()>> tests[] = {
        {"headers and RVA mapping", headersAndMapping},
        {"optional header bounds", optionalHeaderBounds},
        {"import tables", importTables},
        {"short strings at EOF", shortStringsAtEof},
        {"export tables", exportTables},
        {"BSS and section names", bssAndSectionNames},
        {"malformed headers", headersRejectMalformedData},
        {"malformed directories", directoriesRejectMalformedData},
        {"string limits", stringsAndLimits},
        {"cross-section reads", crossSectionReads},
        {"entry count limits", countLimits},
        {"read failures throw", readFailuresThrow},
        {"component parsers", componentParsers},
        {"exception details", exceptionDetails},
        {"ownership and exception cleanup", ownershipCleanup},
        {"file data-source integration", fileSourceIntegration}};
    for (const auto& test : tests) {
        try {
            test.second();
            std::printf("PASS: %s\n", test.first);
        } catch (const std::exception& error) {
            std::fprintf(stderr, "FAIL: %s: %s\n", test.first, error.what());
            return 1;
        }
    }
    for (int i = 1; i < argc; ++i) {
        PeFile* pe = nullptr;
        try {
            PeFileDataSource source(QString::fromLocal8Bit(argv[i]));
            pe = new PeFile(source);
            pe->parse();
            std::printf(
                "PASS: %s: PE%s, %zu sections, %zu import modules, %zu "
                "exports\n",
                argv[i], pe->isPe32Plus() ? "32+" : "32", pe->sections().items().size(),
                pe->imports().size(), pe->exports() ? pe->exports()->entries().size() : 0);
            delete pe;
        } catch (const std::exception& error) {
            delete pe;
            std::fprintf(stderr, "FAIL: %s: %s\n", argv[i], error.what());
            return 1;
        }
    }
}
