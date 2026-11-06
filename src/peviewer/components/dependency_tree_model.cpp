#include "dependency_tree_model.h"

#include <QFileInfo>
#include <utility>
#include <vector>

#include "dependency_graph.h"

struct DependencyTreeModel::Item {
    const ModuleNode* module = nullptr;
    QString name;
    QString detail;
    Status status = Status::Resolved;
    bool cycle = false;
    bool populated = false;
    int row = 0;
    Item* parent = nullptr;
    std::vector<std::unique_ptr<Item>> children;
};

namespace {
DependencyTreeModel::Status statusForError(DependencyError::Code code) {
    using Status = DependencyTreeModel::Status;
    switch (code) {
        case DependencyError::Code::NotFound:
            return Status::NotFound;
        case DependencyError::Code::ReadError:
            return Status::ReadError;
        case DependencyError::Code::ParseError:
            return Status::ParseError;
        case DependencyError::Code::ArchitectureMismatch:
            return Status::ArchitectureMismatch;
    }
    return Status::NotFound;
}
}  // namespace

DependencyTreeModel::DependencyTreeModel(QObject* parent) : QAbstractItemModel(parent) {}

DependencyTreeModel::~DependencyTreeModel() = default;

void DependencyTreeModel::setGraph(const DependencyGraph* graph) {
    beginResetModel();
    m_graph = graph;
    m_root.reset();
    if (m_graph) {
        const ModuleNode& root = m_graph->root();
        m_root = std::make_unique<Item>();
        m_root->module = &root;
        m_root->name = QFileInfo(root.identity()).fileName();
        if (m_root->name.isEmpty()) {
            m_root->name = root.identity();
        }
        m_root->detail = root.identity();
        m_root->status = Status::Root;
    }
    endResetModel();
}

void DependencyTreeModel::clear() { setGraph(nullptr); }

void DependencyTreeModel::retranslate() {
    emit headerDataChanged(Qt::Horizontal, ModuleColumn, DetailColumn);
    if (m_root) {
        emitSubtreeChanged(QModelIndex());
    }
}

QModelIndex DependencyTreeModel::index(int row, int column, const QModelIndex& parent) const {
    if (row < 0 || column < 0 || column >= ColumnCount) {
        return {};
    }
    if (!parent.isValid()) {
        if (!m_root || row != 0) {
            return {};
        }
        return createIndex(row, column, m_root.get());
    }
    if (parent.column() != ModuleColumn) {
        return {};
    }
    Item* parentItem = itemFromIndex(parent);
    if (!parentItem || row >= static_cast<int>(parentItem->children.size())) {
        return {};
    }
    return createIndex(row, column, parentItem->children[static_cast<std::size_t>(row)].get());
}

QModelIndex DependencyTreeModel::parent(const QModelIndex& index) const {
    if (!index.isValid()) {
        return {};
    }
    const Item* item = itemFromIndex(index);
    if (!item || !item->parent) {
        return {};
    }
    return createIndex(item->parent->row, ModuleColumn, item->parent);
}

int DependencyTreeModel::rowCount(const QModelIndex& parent) const {
    if (!m_graph || parent.column() > ModuleColumn) {
        return 0;
    }
    if (!parent.isValid()) {
        return m_root ? 1 : 0;
    }
    const Item* item = itemFromIndex(parent);
    return item ? static_cast<int>(item->children.size()) : 0;
}

int DependencyTreeModel::columnCount(const QModelIndex&) const { return ColumnCount; }

bool DependencyTreeModel::hasChildren(const QModelIndex& parent) const {
    if (!m_graph || parent.column() > ModuleColumn) {
        return false;
    }
    if (!parent.isValid()) {
        return m_root != nullptr;
    }
    const Item* item = itemFromIndex(parent);
    return item && hasDependencies(*item);
}

bool DependencyTreeModel::canFetchMore(const QModelIndex& parent) const {
    if (!m_graph || !parent.isValid() || parent.column() > ModuleColumn) {
        return false;
    }
    const Item* item = itemFromIndex(parent);
    return item && !item->populated && hasDependencies(*item);
}

void DependencyTreeModel::fetchMore(const QModelIndex& parent) {
    if (!m_graph || !parent.isValid() || parent.column() > ModuleColumn) {
        return;
    }
    Item* item = itemFromIndex(parent);
    if (!item || item->populated || !hasDependencies(*item)) {
        return;
    }

    const auto& dependencies = item->module->dependencies();
    beginInsertRows(parent, 0, static_cast<int>(dependencies.size()) - 1);
    item->children.reserve(dependencies.size());
    for (std::size_t edgeIndex = 0; edgeIndex < dependencies.size(); ++edgeIndex) {
        const DependencyEdge& edge = dependencies[edgeIndex];
        auto child = std::make_unique<Item>();
        child->parent = item;
        child->row = static_cast<int>(edgeIndex);
        child->name = edge.dllName;
        if (edge.module) {
            child->module = edge.module;
            child->detail = edge.module->identity();
            child->cycle = isAncestorModule(item, edge.module);
            const bool apiSet = !edge.apiSetHost.isEmpty();
            // A contract can legitimately bind to its importing host itself.
            // Stop expanding that reference, while retaining real multi-module cycles.
            if (child->cycle && (!apiSet || edge.module != item->module)) {
                child->status = Status::Cycle;
            } else {
                child->status = apiSet ? Status::ApiSet : Status::Resolved;
            }
        } else {
            child->status = edge.error ? statusForError(edge.error->code) : Status::NotFound;
            if (edge.error) {
                child->detail = edge.error->message;
            }
        }
        item->children.push_back(std::move(child));
    }
    item->populated = true;
    endInsertRows();
}

QVariant DependencyTreeModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid()) {
        return {};
    }
    const Item* item = itemFromIndex(index);
    if (!item) {
        return {};
    }
    if (role == StatusRole) {
        return static_cast<int>(item->status);
    }
    switch (role) {
        case Qt::DisplayRole:
            switch (index.column()) {
                case ModuleColumn:
                    return item->name;
                case StatusColumn:
                    return statusText(item->status);
                case DetailColumn:
                    if (item->cycle && item->status == Status::ApiSet) {
                        return tr("API Set host is the current module: %1").arg(item->detail);
                    }
                    return item->cycle ? tr("Circular dependency") : item->detail;
                default:
                    break;
            }
            break;
        case Qt::ToolTipRole:
            if (!item->detail.isEmpty() && item->detail != item->name) {
                return item->name + QLatin1Char('\n') + item->detail;
            }
            return item->name;
        default:
            break;
    }
    return {};
}

QVariant DependencyTreeModel::headerData(int section, Qt::Orientation orientation,
                                         int role) const {
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
        return {};
    }
    switch (section) {
        case ModuleColumn:
            return tr("Module");
        case StatusColumn:
            return tr("Status");
        case DetailColumn:
            return tr("Details");
        default:
            return {};
    }
}

Qt::ItemFlags DependencyTreeModel::flags(const QModelIndex& index) const {
    if (!index.isValid()) {
        return Qt::NoItemFlags;
    }
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

DependencyTreeModel::Item* DependencyTreeModel::itemFromIndex(const QModelIndex& index) const {
    return static_cast<Item*>(index.internalPointer());
}

bool DependencyTreeModel::hasDependencies(const Item& item) {
    return item.module && !item.cycle && !item.module->dependencies().empty();
}

bool DependencyTreeModel::isAncestorModule(const Item* item, const ModuleNode* module) {
    for (const Item* current = item; current; current = current->parent) {
        if (current->module == module) {
            return true;
        }
    }
    return false;
}

QString DependencyTreeModel::statusText(Status status) const {
    switch (status) {
        case Status::Root:
            return tr("Root module");
        case Status::Resolved:
            return tr("Resolved");
        case Status::NotFound:
            return tr("Not found");
        case Status::ReadError:
            return tr("Read error");
        case Status::ParseError:
            return tr("Parse error");
        case Status::ArchitectureMismatch:
            return tr("Architecture mismatch");
        case Status::Cycle:
            return tr("Circular");
        case Status::ApiSet:
            return tr("API Set");
    }
    return {};
}

void DependencyTreeModel::emitSubtreeChanged(const QModelIndex& parent) {
    const int rows = rowCount(parent);
    for (int row = 0; row < rows; ++row) {
        const QModelIndex status = index(row, StatusColumn, parent);
        const QModelIndex detail = index(row, DetailColumn, parent);
        emit dataChanged(status, detail);
        emitSubtreeChanged(index(row, ModuleColumn, parent));
    }
}
