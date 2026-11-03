#include "dependency_tree_widget.h"

#include <QAbstractItemView>
#include <QEvent>
#include <QHeaderView>
#include <QTreeView>
#include <QVBoxLayout>

#include "dependency_tree_delegate.h"
#include "dependency_tree_model.h"

DependencyTreeWidget::DependencyTreeWidget(QWidget* parent)
    : QWidget(parent),
      m_model(new DependencyTreeModel(this)),
      m_delegate(new DependencyTreeDelegate(this)),
      m_view(new QTreeView(this)) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_view);

    m_view->setObjectName(QStringLiteral("dependencyTreeView"));
    m_view->setModel(m_model);
    m_view->setItemDelegate(m_delegate);
    m_view->setUniformRowHeights(true);
    m_view->setAlternatingRowColors(true);
    m_view->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_view->setAllColumnsShowFocus(true);
    m_view->setExpandsOnDoubleClick(true);

    QHeaderView* header = m_view->header();
    header->setStretchLastSection(false);
    header->setSectionResizeMode(DependencyTreeModel::ModuleColumn, QHeaderView::Interactive);
    header->setSectionResizeMode(DependencyTreeModel::StatusColumn, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(DependencyTreeModel::DetailColumn, QHeaderView::Stretch);
    m_view->setColumnWidth(DependencyTreeModel::ModuleColumn, 300);

    connect(m_view, &QTreeView::expanded, m_model, [this](const QModelIndex& index) {
        if (m_model->canFetchMore(index)) {
            m_model->fetchMore(index);
        }
    });
}

DependencyTreeWidget::~DependencyTreeWidget() = default;

void DependencyTreeWidget::setGraph(const DependencyGraph* graph) {
    m_model->setGraph(graph);
    if (!graph) {
        return;
    }
    const QModelIndex root = m_model->index(0, DependencyTreeModel::ModuleColumn);
    if (root.isValid()) {
        if (m_model->canFetchMore(root)) {
            m_model->fetchMore(root);
        }
        m_view->expand(root);
    }
    m_view->scrollToTop();
}

void DependencyTreeWidget::clear() { m_model->clear(); }

void DependencyTreeWidget::changeEvent(QEvent* event) {
    QWidget::changeEvent(event);
    if (event->type() == QEvent::LanguageChange) {
        m_model->retranslate();
    }
}
