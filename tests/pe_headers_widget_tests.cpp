#include <pe_file.h>
#include <pe_file_data_source.h>

#include <QApplication>
#include <QDir>
#include <QLabel>
#include <QTableWidget>
#include <QTemporaryDir>
#include <iostream>
#include <stdexcept>

#include "components/pe_headers_widget.h"
#include "dependencies_test_support.h"

namespace {
using DependencyTests::require;

QTableWidget* table(const PeHeadersWidget& widget, const char* name) {
    QTableWidget* result = widget.findChild<QTableWidget*>(QString::fromLatin1(name));
    require(result != nullptr, "PE header table is missing");
    return result;
}

void testIndependentUpdates(PeFile& pe32, PeFile& pe64) {
    PeHeadersWidget widget;
    QTableWidget* dos = table(widget, "dosHeaderTable");
    QTableWidget* file = table(widget, "fileHeaderTable");
    QTableWidget* optional = table(widget, "optionalHeaderTable");

    require(dos->rowCount() == 0 && file->rowCount() == 0 && optional->rowCount() == 0,
            "A fresh headers widget is not empty");

    widget.setDosHeader(&pe32.dosHeader());
    require(dos->rowCount() == 19 && file->rowCount() == 0 && optional->rowCount() == 0,
            "Updating the DOS header changed another table");
    require(dos->item(18, 1)->text() == QStringLiteral("e_lfanew"), "DOS field name is missing");

    widget.setFileHeader(&pe32.ntHeaders().fileHeader());
    require(dos->rowCount() == 19 && file->rowCount() == 7 && optional->rowCount() == 0,
            "Updating the file header changed another table");
    require(file->item(0, 2)->text() == QStringLiteral("0x014C"), "File header value is incorrect");

    widget.setOptionalHeader(&pe32.ntHeaders().optionalHeader());
    require(dos->rowCount() == 19 && file->rowCount() == 7 && optional->rowCount() == 30,
            "Updating the optional header changed another table");
    require(optional->item(0, 2)->text() == QStringLiteral("0x010B"),
            "Optional header value is incorrect");

    // Replacing one header must not reload the other two.
    widget.setDosHeader(&pe64.dosHeader());
    require(dos->rowCount() == 19 && file->rowCount() == 7 && optional->rowCount() == 30,
            "Replacing the DOS header reset another table");

    widget.setPeFile(&pe64);
    require(optional->rowCount() == 29, "PE32+ optional header has the wrong fields");
    require(optional->item(0, 2)->text() == QStringLiteral("0x020B"),
            "PE32+ optional header value is incorrect");
    require(file->item(0, 2)->text() == QStringLiteral("0x8664"),
            "x64 file header value is incorrect");

    widget.setDosHeader(nullptr);
    require(dos->rowCount() == 0 && file->rowCount() == 7 && optional->rowCount() == 29,
            "Clearing the DOS header changed another table");

    widget.setPeFile(nullptr);
    require(dos->rowCount() == 0 && file->rowCount() == 0 && optional->rowCount() == 0,
            "setPeFile(nullptr) did not clear every table");
}

void testLayout() {
    PeHeadersWidget widget;
    widget.resize(1200, 800);
    widget.show();
    QApplication::processEvents();

    auto* dos = widget.findChild<QWidget*>(QStringLiteral("dosHeaderSection"));
    auto* file = widget.findChild<QWidget*>(QStringLiteral("fileHeaderSection"));
    auto* optional = widget.findChild<QWidget*>(QStringLiteral("optionalHeaderSection"));
    require(dos && file && optional, "Header sections are missing");
    require(dos->x() < file->x() && dos->x() < optional->x(),
            "The DOS header is not in the left column");
    require(file->x() == optional->x() && file->y() < optional->y(),
            "The file and optional headers are not stacked in the right column");
    require(dos->height() >= file->height() + optional->height(),
            "The DOS header does not span the full height");
}

void testTitlesAndColumns() {
    PeHeadersWidget widget;
    const struct {
        const char* section;
        const char* title;
    } sections[] = {{"dosHeaderSection", "DOS Header"},
                    {"fileHeaderSection", "File Header"},
                    {"optionalHeaderSection", "Optional Header"}};
    for (const auto& section : sections) {
        QWidget* host = widget.findChild<QWidget*>(QString::fromLatin1(section.section));
        auto* title = host ? host->findChild<QLabel*>(QStringLiteral("headerTableTitle")) : nullptr;
        require(title && title->text() == QString::fromLatin1(section.title),
                "Header table title is incorrect");
    }

    QTableWidget* dos = table(widget, "dosHeaderTable");
    require(dos->horizontalHeaderItem(0)->text() == QStringLiteral("Offset") &&
                dos->horizontalHeaderItem(1)->text() == QStringLiteral("Field") &&
                dos->horizontalHeaderItem(2)->text() == QStringLiteral("Value") &&
                dos->horizontalHeaderItem(3)->text() == QStringLiteral("Description"),
            "Header table columns are incorrect");

    widget.retranslateUi();
    require(dos->horizontalHeaderItem(1)->text() == QStringLiteral("Field"),
            "retranslateUi changed English column names");
}
}  // namespace

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    QTemporaryDir temporary;
    if (!temporary.isValid()) {
        return 1;
    }
    const QDir dir(temporary.path());
    DependencyTests::writePe(dir, QStringLiteral("x86.exe"));
    DependencyTests::writePe(dir, QStringLiteral("x64.exe"), {}, true);

    try {
        PeFileDataSource source32(dir.filePath(QStringLiteral("x86.exe")));
        PeFile pe32(source32);
        pe32.parse();
        PeFileDataSource source64(dir.filePath(QStringLiteral("x64.exe")));
        PeFile pe64(source64);
        pe64.parse();

        testIndependentUpdates(pe32, pe64);
        testLayout();
        testTitlesAndColumns();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
