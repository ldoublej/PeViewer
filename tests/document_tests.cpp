#include <document.h>
#include <document_manager.h>
#include <editor.h>
#include <pe_data_source.h>
#include <pe_file.h>

#include <QDir>
#include <QFileInfo>
#include <QTemporaryDir>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <vector>

#include "dependencies_test_support.h"
#include "dependency_graph.h"

using namespace DependencyTests;

namespace {
static_assert(!std::is_default_constructible_v<DocumentManager>);
static_assert(!std::is_copy_constructible_v<DocumentManager>);
static_assert(!std::is_move_constructible_v<DocumentManager>);

// Minimal editor used to observe creation, close veto and destruction order.
class TrackingEditor : public Editor {
public:
    TrackingEditor(Document& document, bool& allowClose, bool& destroyed)
        : Editor(document.path()), m_document(document), m_allowClose(allowClose), m_destroyed(destroyed) {}

    ~TrackingEditor() override { m_destroyed = true; }

    bool canClose() const override { return m_allowClose; }
    const Document& document() const { return m_document; }

private:
    Document& m_document;
    bool& m_allowClose;
    bool& m_destroyed;
};

QString writeFixture(const QDir& dir, const QString& name,
                     const QStringList& imports = QStringList{}) {
    writePe(dir, name, imports);
    return dir.filePath(name);
}

void opensSession() {
    QTemporaryDir temporary;
    require(temporary.isValid(), "Temporary directory failed");
    const QDir dir(temporary.path());
    const QString path =
        writeFixture(dir, QStringLiteral("app.exe"), {QStringLiteral("a.dll")});
    writeFixture(dir, QStringLiteral("a.dll"));

    Document document(path, FileLocatorOptions{});
    require(document.path() == QFileInfo(path).absoluteFilePath(),
            "Document did not record the absolute path");
    require(document.peFile().isParsed(), "Document did not parse the root file");
    require(&document.graph().root().pe_file() == &document.peFile(),
            "Graph does not borrow the document root");
    require(document.graph().moduleCount() == 2, "Dependency graph was not built");
    require(document.files().size() == 1 && document.graph().files().size() == 1,
            "Document does not expose its dependency store");
    require(document.editor() == nullptr,
            "Document without an editor factory created an editor");
}

void managerTracksList() {
    QTemporaryDir temporary;
    require(temporary.isValid(), "Temporary directory failed");
    const QDir dir(temporary.path());
    const QString firstPath =
        writeFixture(dir, QStringLiteral("first.exe"), {QStringLiteral("a.dll")});
    writeFixture(dir, QStringLiteral("a.dll"));
    const QString secondPath =
        writeFixture(dir, QStringLiteral("second.exe"), {QStringLiteral("b.dll")});
    writeFixture(dir, QStringLiteral("b.dll"));

    auto* manager = DocumentManager::instance();
    require(manager == DocumentManager::instance(), "Manager is not a singleton");
    require(manager->current() == nullptr && manager->empty() && manager->size() == 0,
            "Manager is not empty");

    Document* first = manager->open(firstPath);
    require(first != nullptr && manager->current() == first && manager->size() == 1,
            "Manager did not open a document");
    require(DocumentManager::instance()->find(firstPath) == first,
            "Singleton access did not share the document list");
    require(first->graph().moduleCount() == 2, "First document graph is incomplete");

    Document* second = manager->open(secondPath);
    require(second != nullptr && second != first && manager->current() == second,
            "Manager did not activate the new document");
    require(manager->size() == 2 && manager->documents()[0].get() == first,
            "Manager did not keep the previous document");

    // Reopening an already-open path activates it instead of duplicating it.
    require(manager->open(firstPath) == first && manager->current() == first &&
                manager->size() == 2,
            "Reopening a path duplicated the document");
    require(manager->find(secondPath) == second, "find() did not locate the document");

    // A failed open must keep the list and the current session intact.
    expectException<PeDataSourceError>(
        [&] { manager->open(dir.filePath(QStringLiteral("missing.exe"))); });
    require(manager->current() == first && manager->size() == 2,
            "Failed open changed the document list");

    writeFile(dir.filePath(QStringLiteral("bad.exe")), QByteArray("invalid PE"));
    expectException<ParseError>(
        [&] { manager->open(dir.filePath(QStringLiteral("bad.exe"))); });
    require(manager->current() == first && manager->size() == 2,
            "Failed parse changed the document list");

    require(manager->activate(second) && manager->current() == second,
            "activate() did not change the current document");
    require(!manager->activate(nullptr), "activate() accepted an unknown document");

    require(manager->close(second) && manager->size() == 1 && manager->current() == first,
            "Closing the current document did not activate another one");
    require(!manager->close(second), "close() accepted a removed document");

    manager->closeAll();
    require(manager->empty() && manager->current() == nullptr, "closeAll() kept documents");
}

void managerSignalsAndOpenModes() {
    QTemporaryDir temporary;
    require(temporary.isValid(), "Temporary directory failed");
    const QDir dir(temporary.path());
    const QString firstPath = writeFixture(dir, QStringLiteral("first.exe"));
    const QString secondPath = writeFixture(dir, QStringLiteral("second.exe"));

    auto* manager = DocumentManager::instance();
    using OpenMode = DocumentManager::OpenMode;
    std::vector<QString> events;
    QObject context;
    QObject::connect(manager, &DocumentManager::documentOpened, &context,
                     [&](Document* document) {
                         require(manager->find(document->path()) == document,
                                 "Opened signal preceded insertion");
                         events.push_back(QStringLiteral("opened:") +
                                          QFileInfo(document->path()).fileName());
                     });
    QObject::connect(manager, &DocumentManager::documentActivated, &context,
                     [&](Document* document) {
                         require(manager->current() == document,
                                 "Activated signal preceded current change");
                         events.push_back(QStringLiteral("activated:") +
                                          (document ? QFileInfo(document->path()).fileName()
                                                     : QStringLiteral("none")));
                     });
    QObject::connect(manager, &DocumentManager::documentClosed, &context,
                     [&](Document* document) {
                         require(manager->find(document->path()) == nullptr &&
                                     manager->current() != document,
                                 "Closed signal preceded removal");
                         require(document->peFile().isParsed(),
                                 "Closed document is not readable during signal delivery");
                         events.push_back(QStringLiteral("closed:") +
                                          QFileInfo(document->path()).fileName());
                     });
    const auto expectEvents = [&](const std::vector<QString>& expected) {
        require(events == expected, "Unexpected document signals or signal order");
        events.clear();
    };

    Document* first = manager->open(firstPath, OpenMode::OpenOnly);
    require(manager->current() == nullptr, "OpenOnly activated the first document");
    expectEvents({QStringLiteral("opened:first.exe")});
    require(manager->open(firstPath, OpenMode::OpenOnly) == first,
            "OpenOnly duplicated an existing document");
    expectEvents({});

    Document* second = manager->open(secondPath, OpenMode::OpenOnly);
    require(manager->current() == nullptr, "OpenOnly changed the current document");
    expectEvents({QStringLiteral("opened:second.exe")});
    require(manager->close(first) && manager->current() == nullptr,
            "Closing an inactive document activated another one");
    expectEvents({QStringLiteral("closed:first.exe")});

    require(manager->activate(second), "Could not activate an opened document");
    expectEvents({QStringLiteral("activated:second.exe")});
    require(manager->activate(second), "Repeated activation failed");
    require(!manager->activate(nullptr), "Null activation succeeded");
    expectEvents({});

    first = manager->open(firstPath, OpenMode::OpenAndActivate);
    require(manager->current() == first, "OpenAndActivate did not activate the document");
    expectEvents({QStringLiteral("opened:first.exe"), QStringLiteral("activated:first.exe")});
    require(manager->open(secondPath, OpenMode::OpenOnly) == second &&
                manager->current() == first,
            "Reopening with OpenOnly changed the current document");
    expectEvents({});
    require(manager->open(secondPath) == second && manager->current() == second,
            "Default open did not activate an existing document");
    expectEvents({QStringLiteral("activated:second.exe")});
    manager->open(secondPath);
    expectEvents({});

    expectException<PeDataSourceError>(
        [&] { manager->open(dir.filePath(QStringLiteral("missing.exe"))); });
    writeFile(dir.filePath(QStringLiteral("bad.exe")), QByteArray("invalid PE"));
    expectException<ParseError>(
        [&] { manager->open(dir.filePath(QStringLiteral("bad.exe")), OpenMode::OpenOnly); });
    require(manager->current() == second && manager->size() == 2,
            "Failed open changed the manager");
    expectEvents({});

    require(manager->close(first), "Could not close an inactive document");
    expectEvents({QStringLiteral("closed:first.exe")});
    require(manager->close(second) && manager->current() == nullptr,
            "Closing the last document kept it active");
    expectEvents({QStringLiteral("activated:none"), QStringLiteral("closed:second.exe")});
    require(!manager->close(nullptr), "Closing an unknown document succeeded");
    expectEvents({});

    first = manager->open(firstPath);
    second = manager->open(secondPath);
    expectEvents({QStringLiteral("opened:first.exe"), QStringLiteral("activated:first.exe"),
                  QStringLiteral("opened:second.exe"), QStringLiteral("activated:second.exe")});
    require(manager->close(second) && manager->current() == first,
            "Closing the current document did not activate the remaining one");
    expectEvents({QStringLiteral("activated:first.exe"), QStringLiteral("closed:second.exe")});
    manager->open(secondPath, OpenMode::OpenOnly);
    expectEvents({QStringLiteral("opened:second.exe")});
    manager->closeAll();
    require(manager->empty() && manager->current() == nullptr, "closeAll() kept documents");
    expectEvents({QStringLiteral("activated:none"), QStringLiteral("closed:first.exe"),
                  QStringLiteral("closed:second.exe")});
    manager->closeAll();
    expectEvents({});

    manager->open(firstPath, OpenMode::OpenOnly);
    manager->open(secondPath, OpenMode::OpenOnly);
    manager->closeAll();
    require(manager->current() == nullptr, "Closing inactive documents activated one");
    expectEvents({QStringLiteral("opened:first.exe"), QStringLiteral("opened:second.exe"),
                  QStringLiteral("closed:first.exe"), QStringLiteral("closed:second.exe")});
}

void editorLifecycle() {
    QTemporaryDir temporary;
    require(temporary.isValid(), "Temporary directory failed");
    const QDir dir(temporary.path());
    const QString path = writeFixture(dir, QStringLiteral("editor.exe"));

    bool allowClose = true;
    bool destroyed = false;
    auto* manager = DocumentManager::instance();
    manager->setEditorFactory([&](Document& document) {
        return std::make_unique<TrackingEditor>(document, allowClose, destroyed);
    });

    Document* document = manager->open(path);
    require(document->editor() != nullptr, "Editor factory was not used");
    require(manager->currentEditor() == document->editor(),
            "currentEditor() did not return the active document's editor");

    allowClose = false;
    require(!manager->close(document), "canClose() == false did not cancel the close");
    require(manager->size() == 1 && !destroyed, "Vetoed close removed the document");

    allowClose = true;
    require(manager->close(document), "Closing the document failed");
    require(destroyed, "Editor was not destroyed together with its document");
    require(manager->empty() && manager->currentEditor() == nullptr,
            "Manager kept state after the document was closed");

    manager->setEditorFactory({});
}

void tests() {
    opensSession();
    managerTracksList();
    managerSignalsAndOpenModes();
    editorLifecycle();
}
}  // namespace

int main() { return run(tests); }
