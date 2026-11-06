#pragma once

#include <QAbstractItemModel>
#include <QString>
#include <memory>

class DependencyGraph;
class ModuleNode;

// Lazily exposes a DependencyGraph as a tree. A module shared by several parents
// appears once per dependency edge, and an edge that closes a cycle is shown as
// a terminal leaf instead of being expanded again.
class DependencyTreeModel : public QAbstractItemModel {
    Q_OBJECT

public:
    enum class Status : int {
        Root,
        Resolved,
        NotFound,
        ReadError,
        ParseError,
        ArchitectureMismatch,
        Cycle,
        ApiSet,
    };
    enum Column : int { ModuleColumn = 0, StatusColumn = 1, DetailColumn = 2, ColumnCount = 3 };
    enum Role : int { StatusRole = Qt::UserRole + 1 };

    explicit DependencyTreeModel(QObject* parent = nullptr);
    ~DependencyTreeModel() override;

    // The graph is borrowed. It must outlive the model or until setGraph(nullptr).
    void setGraph(const DependencyGraph* graph);
    void clear();
    void retranslate();

    QModelIndex index(int row, int column,
                      const QModelIndex& parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex& index) const override;
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    bool hasChildren(const QModelIndex& parent = QModelIndex()) const override;
    bool canFetchMore(const QModelIndex& parent) const override;
    void fetchMore(const QModelIndex& parent) override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

private:
    struct Item;

    Item* itemFromIndex(const QModelIndex& index) const;
    static bool hasDependencies(const Item& item);
    static bool isAncestorModule(const Item* item, const ModuleNode* module);
    QString statusText(Status status) const;
    void emitSubtreeChanged(const QModelIndex& parent);

    const DependencyGraph* m_graph = nullptr;
    std::unique_ptr<Item> m_root;
};
