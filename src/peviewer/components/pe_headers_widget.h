#pragma once

#include <QWidget>

class DosHeader;
class FileHeader;
class HeaderTableWidget;
class OptionalHeader;
class PeFile;

// Read-only view of the three PE headers. Each table can be updated on its own.
// Header objects are borrowed and must stay alive until the next update or clear().
class PeHeadersWidget : public QWidget {
    Q_OBJECT

public:
    explicit PeHeadersWidget(QWidget* parent = nullptr);

    // Updates all three tables from one parsed file; nullptr clears them.
    void setPeFile(const PeFile* file);

    // Independent updates: changing one table leaves the other two untouched.
    void setDosHeader(const DosHeader* header);
    void setFileHeader(const FileHeader* header);
    void setOptionalHeader(const OptionalHeader* header);
    void clear();

    void retranslateUi();

protected:
    void changeEvent(QEvent* event) override;

private:
    void updateDosTable();
    void updateFileTable();
    void updateOptionalTable();

    HeaderTableWidget* m_dosTable = nullptr;
    HeaderTableWidget* m_fileTable = nullptr;
    HeaderTableWidget* m_optionalTable = nullptr;

    const DosHeader* m_dosHeader = nullptr;
    const FileHeader* m_fileHeader = nullptr;
    const OptionalHeader* m_optionalHeader = nullptr;
};
