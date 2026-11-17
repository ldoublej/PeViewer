#include "header_table_widget.h"

#include <QAbstractItemView>
#include <QCoreApplication>
#include <QFont>
#include <QFontDatabase>
#include <QHeaderView>
#include <QLabel>
#include <QScrollBar>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace {
enum Column : quint8 { OffsetColumn, FieldColumn, ValueColumn, DescriptionColumn, ColumnCount };

QFont monospaceFont() { return QFontDatabase::systemFont(QFontDatabase::FixedFont); }

QTableWidgetItem* makeItem(const QString& text, const QFont& font = QFont()) {
    auto* item = new QTableWidgetItem(text);
    item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    item->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    item->setToolTip(text);
    if (!font.family().isEmpty()) {
        item->setFont(font);
    }
    return item;
}
}  // namespace

HeaderTableWidget::HeaderTableWidget(QWidget* parent)
    : QWidget(parent), m_title(new QLabel(this)), m_table(new QTableWidget(0, ColumnCount, this)) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);
    layout->addWidget(m_title);
    layout->addWidget(m_table, 1);

    QFont titleFont = m_title->font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 2);
    m_title->setFont(titleFont);
    m_title->setObjectName(QStringLiteral("headerTableTitle"));
    m_title->setTextInteractionFlags(Qt::TextSelectableByMouse);

    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    m_table->setWordWrap(false);
    m_table->setTextElideMode(Qt::ElideRight);
    m_table->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_table->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_table->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_table->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_table->verticalHeader()->setVisible(false);
    m_table->verticalHeader()->setDefaultSectionSize(24);

    QHeaderView* header = m_table->horizontalHeader();
    header->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    header->setHighlightSections(false);
    header->setSectionsMovable(false);
    header->setStretchLastSection(false);
    header->setMinimumSectionSize(48);
    header->setSectionResizeMode(OffsetColumn, QHeaderView::Fixed);
    header->setSectionResizeMode(FieldColumn, QHeaderView::Stretch);
    header->setSectionResizeMode(ValueColumn, QHeaderView::Interactive);
    header->setSectionResizeMode(DescriptionColumn, QHeaderView::Stretch);
    m_table->setColumnWidth(OffsetColumn, 64);
    m_table->setColumnWidth(ValueColumn, 150);

    header->setStyleSheet(
        QStringLiteral("QHeaderView::section {"
                       " border: none;"
                       " border-bottom: 1px solid palette(mid);"
                       " border-right: 1px solid palette(mid);"
                       " padding: 2px 6px;"
                       "}"
                       "QHeaderView::section:last {"
                       " border-right: none;"
                       "}"));

    retranslateUi();
}

void HeaderTableWidget::setTitle(const QString& title) { m_title->setText(title); }

QString HeaderTableWidget::title() const { return m_title->text(); }

void HeaderTableWidget::setRows(const QVector<HeaderField>& rows) {
    const int scrollValue = m_table->verticalScrollBar()->value();
    const int selectedRow = m_table->currentRow();
    const QFont mono = monospaceFont();

    m_table->setUpdatesEnabled(false);
    m_table->clearContents();
    m_table->setRowCount(rows.size());
    for (int row = 0; row < rows.size(); ++row) {
        const HeaderField& field = rows.at(row);
        m_table->setItem(row, OffsetColumn, makeItem(field.offset, mono));
        m_table->setItem(row, FieldColumn, makeItem(field.name, mono));
        m_table->setItem(row, ValueColumn, makeItem(field.value, mono));
        m_table->setItem(row, DescriptionColumn, makeItem(field.description));
    }
    m_table->setUpdatesEnabled(true);

    if (selectedRow >= 0 && selectedRow < rows.size()) {
        m_table->selectRow(selectedRow);
    }
    // Restore the offset after the new row count has updated the scroll range.
    m_table->verticalScrollBar()->setValue(
        qMin(scrollValue, m_table->verticalScrollBar()->maximum()));
}

void HeaderTableWidget::clearRows() { setRows({}); }

void HeaderTableWidget::retranslateUi() {
    m_table->setHorizontalHeaderLabels(
        {QCoreApplication::translate("HeaderTableWidget", "Offset"),
         QCoreApplication::translate("HeaderTableWidget", "Field"),
         QCoreApplication::translate("HeaderTableWidget", "Value"),
         QCoreApplication::translate("HeaderTableWidget", "Description")});
}
