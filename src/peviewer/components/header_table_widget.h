#pragma once

#include <QString>
#include <QVector>
#include <QWidget>

class QLabel;
class QTableWidget;

// One row of a read-only PE header table.
struct HeaderField {
    QString offset;
    QString name;
    QString value;
    QString description;
};

// A titled, read-only table used by PeHeadersWidget: QLabel above a QTableWidget.
class HeaderTableWidget : public QWidget {
    Q_OBJECT

public:
    explicit HeaderTableWidget(QWidget* parent = nullptr);

    void setTitle(const QString& title);
    QString title() const;

    // Replaces every row. The table keeps its scroll position when possible.
    void setRows(const QVector<HeaderField>& rows);
    void clearRows();

    void retranslateUi();

    // Exposed so the owning view can give the table a stable object name.
    QTableWidget* table() const { return m_table; }

private:
    QLabel* m_title = nullptr;
    QTableWidget* m_table = nullptr;
};
