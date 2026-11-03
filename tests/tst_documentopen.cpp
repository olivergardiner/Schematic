#include "../mainwindow.h"
#include "documentloadresult.h"
#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMessageBox>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTimer>
#include <QVBoxLayout>
#include <QTreeWidget>
#include <QScrollArea>
#include <QPointer>
#include <QAction>
#include <QAccessible>
#include <QtTest>

class DocumentOpenTests : public QObject
{
    Q_OBJECT
private slots:
    void transformToolbarActionsAreUndoable();
    void paletteReflowsWithAvailableWidth();
    void categoryCollapseReleasesPaletteObjects();
    void categoryAccessibilityMatchesCollapsedRows();
    void reviewChoicesAndAtomicOpen();
    void missingAndIncompatibleChoices();
};

void DocumentOpenTests::transformToolbarActionsAreUndoable()
{
    QTemporaryDir temporary;
    QStandardPaths::setTestModeEnabled(true);
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, temporary.path());
    MainWindow window;
    const QList<QAction *> actions{window.m_rotateLeftAction, window.m_rotateAction,
                                  window.m_mirrorAction, window.m_flipVerticalAction};
    for (auto *action : actions) {
        QVERIFY(action);
        QVERIFY(!action->icon().isNull());
        QVERIFY(!action->isEnabled());
    }
    QVERIFY(window.m_scene->placeComponentAt(SymbolIds::OpAmp, {100, 100}));
    const auto id = *window.m_scene->singleSelectedComponent();
    auto *stack = window.m_editController->undoStack();
    window.m_rotateAction->trigger();
    QCOMPARE(window.m_document.component(id)->rotation(), Rotation::Deg90);
    window.m_rotateLeftAction->trigger();
    QCOMPARE(window.m_document.component(id)->rotation(), Rotation::Deg0);
    // Start the reflection checks from a rotated pose to distinguish canvas axes.
    window.m_rotateAction->trigger();
    for (auto *action : actions) {
        QVERIFY(action->isEnabled());
        const auto before = window.m_document.toJsonBytes();
        const QPointF terminal = window.m_document.terminalPosition(*window.m_document.component(id), 0);
        const int index = stack->index();
        action->trigger();
        QCOMPARE(stack->index(), index + 1);
        const auto after = window.m_document.toJsonBytes();
        QVERIFY(before != after);
        const QPointF offset = terminal - QPointF(100, 100);
        const QPointF expectedOffset = action == window.m_rotateLeftAction ? QPointF(offset.y(), -offset.x())
            : action == window.m_rotateAction ? QPointF(-offset.y(), offset.x())
            : action == window.m_mirrorAction ? QPointF(-offset.x(), offset.y())
            : QPointF(offset.x(), -offset.y());
        QCOMPARE(window.m_document.terminalPosition(*window.m_document.component(id), 0),
                 QPointF(100, 100) + expectedOffset);
        const auto loaded = Document::fromJsonBytes(after);
        QVERIFY(loaded.document);
        QCOMPARE(loaded.document->toJsonBytes(), after);
        stack->undo();
        QCOMPARE(window.m_document.toJsonBytes(), before);
        stack->redo();
        QCOMPARE(window.m_document.toJsonBytes(), after);
    }
}

void DocumentOpenTests::paletteReflowsWithAvailableWidth()
{
    QTemporaryDir temporary;
    QStandardPaths::setTestModeEnabled(true);
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, temporary.path());
    MainWindow window;
    window.populateSymbolGrid(QStringLiteral("Passives"));
    auto *flow = window.m_symbolSectionsLayout->itemAt(0)->layout();
    QVERIFY(flow);
    QCOMPARE(flow->count(), 3);
    QVERIFY(flow->hasHeightForWidth());
    QVERIFY(flow->heightForWidth(150) > flow->heightForWidth(240));
    flow->setGeometry(QRect(0, 0, 150, 200));
    QCOMPARE(flow->itemAt(0)->geometry().y(), flow->itemAt(1)->geometry().y());
    QVERIFY(flow->itemAt(2)->geometry().y() > flow->itemAt(0)->geometry().y());
    auto *firstTile = flow->itemAt(0)->widget();
    flow->setGeometry(QRect(0, 0, 240, 200));
    QCOMPARE(flow->itemAt(2)->geometry().y(), flow->itemAt(0)->geometry().y());
    QCOMPARE(flow->itemAt(0)->widget(), firstTile);
    flow->setGeometry(QRect(0, 0, 80, 200));
    QVERIFY(flow->itemAt(1)->geometry().y() > flow->itemAt(0)->geometry().y());
}

void DocumentOpenTests::categoryCollapseReleasesPaletteObjects()
{
    QTemporaryDir temporary;
    QStandardPaths::setTestModeEnabled(true);
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, temporary.path());
    MainWindow window;
    auto *parent = window.m_categoryItems.value(QStringLiteral("Semiconductors"));
    auto *child = window.m_categoryItems.value(QStringLiteral("Semiconductors/Diodes"));
    QVERIFY(parent);
    QVERIFY(child);
    window.show();
    window.m_symbolActions.value(SymbolIds::Diode)->trigger();
    window.m_categoryTree->setCurrentItem(parent);
    QApplication::processEvents();
    const auto widgetCount = window.findChildren<QWidget *>().size();
    const auto layoutCount = window.findChildren<QLayout *>().size();
    const auto actionCount = window.m_symbolActions.size();
    for (int cycle = 0; cycle < 100; ++cycle) {
        window.m_categoryTree->expandItem(parent);
        QPointer<QWidget> oldContent = window.m_symbolScrollArea->widget();
        window.m_categoryTree->setCurrentItem(child);
        QVERIFY(oldContent.isNull());
        window.m_categoryTree->collapseItem(parent);
        window.m_categoryTree->setCurrentItem(parent);
        QApplication::processEvents();
        QCOMPARE(window.findChildren<QWidget *>().size(), widgetCount);
        QCOMPARE(window.findChildren<QLayout *>().size(), layoutCount);
        QCOMPARE(window.m_symbolActions.size(), actionCount);
    }
    // Exercise the actual tree expander hit path, including short/narrow docks
    // where collapsing a category can change scrollbar visibility.
    for (const QSize size : {QSize(400, 200), QSize(700, 350), QSize(1000, 700)}) {
        window.resize(size);
        for (int cycle = 0; cycle < 10; ++cycle) {
            window.m_categoryTree->expandItem(parent);
            window.m_categoryTree->setCurrentItem(child);
            window.m_categoryTree->scrollToItem(parent);
            QApplication::processEvents();
            const auto rect = window.m_categoryTree->visualItemRect(parent);
            const QPoint expander(rect.left() - window.m_categoryTree->indentation() / 2,
                                  rect.center().y());
            QTest::mouseClick(window.m_categoryTree->viewport(), Qt::LeftButton,
                              Qt::NoModifier, expander);
            QVERIFY(!parent->isExpanded());
            window.m_categoryTree->setCurrentItem(parent);
            QApplication::processEvents();
            QCOMPARE(window.findChildren<QWidget *>().size(), widgetCount);
            QCOMPARE(window.findChildren<QLayout *>().size(), layoutCount);
        }
    }
}

void DocumentOpenTests::categoryAccessibilityMatchesCollapsedRows()
{
    QTemporaryDir temporary;
    QStandardPaths::setTestModeEnabled(true);
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, temporary.path());
    MainWindow window;
    window.show();
    QApplication::processEvents();
    auto *tree = window.m_categoryTree;
    auto *parent = window.m_categoryItems.value(QStringLiteral("Semiconductors"));
    QVERIFY(parent);
    auto *accessible = QAccessible::queryAccessibleInterface(tree);
    QVERIFY(accessible);
    // Prime every accessible row, as an external Windows UIA client does.
    for (int i = 0; i < accessible->childCount(); ++i) {
        auto *child = accessible->child(i);
        QVERIFY(child);
        QCOMPARE(accessible->indexOfChild(child), i);
    }
    tree->collapseItem(parent);
    for (int i = 0; i < accessible->childCount(); ++i) {
        auto *child = accessible->child(i);
        QVERIFY(child);
        QCOMPARE(accessible->indexOfChild(child), i);
    }
}

void DocumentOpenTests::reviewChoicesAndAtomicOpen()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, temporary.path());
    struct Restore { SymbolLibrary saved; ~Restore() { applicationSymbolLibrary() = saved; } } restore{applicationSymbolLibrary()};
    QStandardPaths::setTestModeEnabled(true);
    MainWindow window;
    window.m_symbolPackageStore = SymbolPackageStore(temporary.filePath("library"));
    QVERIFY(window.m_editController->commit("Place", [&] {
        return window.m_document.addComponent(SymbolIds::Resistor, {}) != kInvalidComponentId;
    }));
    window.m_currentFilePath = "unchanged.schematic";
    const auto before = window.m_document.toJsonBytes();
    const int history = window.m_editController->undoStack()->count();

    Document candidate;
    candidate.addComponent(SymbolIds::Resistor, {100, 0});
    auto embedded = *candidate.symbol(SymbolIds::Resistor);
    embedded.displayName = "Embedded artwork";
    QStringList errors;
    QVERIFY(candidate.useSymbol(embedded, &errors));
    const QString path = temporary.filePath("candidate.schematic");
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(candidate.toJsonBytes());
    file.close();

    QTimer::singleShot(0, [&] {
        auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
        QVERIFY(dialog);
        const auto choices = dialog->findChildren<QComboBox *>();
        QCOMPARE(choices.size(), 1);
        QCOMPARE(choices[0]->count(), 3);
        dialog->reject();
    });
    QVERIFY(!window.openDocumentFrom(path));
    QCOMPARE(window.m_document.toJsonBytes(), before);
    QCOMPARE(window.m_currentFilePath, QString("unchanged.schematic"));
    QCOMPARE(window.m_editController->undoStack()->count(), history);
    QVERIFY(!window.m_editController->undoStack()->isClean());

    // A failed batch must leave dirty history untouched without a save prompt.
    QFile blocked(temporary.filePath("blocked"));
    QVERIFY(blocked.open(QIODevice::WriteOnly));
    blocked.write("keep");
    blocked.close();
    window.m_symbolPackageStore = SymbolPackageStore(blocked.fileName());
    QTimer::singleShot(0, [&] {
        auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
        QVERIFY(dialog);
        dialog->findChildren<QComboBox *>()[0]->setCurrentIndex(1);
        dialog->accept();
        QTimer::singleShot(0, [] {
            auto *warning = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            QVERIFY(warning);
            warning->accept();
        });
    });
    QVERIFY(!window.openDocumentFrom(path));
    QCOMPARE(window.m_document.toJsonBytes(), before);
    QCOMPARE(window.m_currentFilePath, QString("unchanged.schematic"));
    QCOMPARE(window.m_editController->undoStack()->count(), history);
    QCOMPARE(applicationSymbolLibrary().find(embedded.id)->displayName,
             restore.saved.find(embedded.id)->displayName);

    QVERIFY(!window.m_editController->undoStack()->isClean());
    window.m_editController->setClean();
    // Selecting the active version changes only the candidate and commits open.
    QTimer::singleShot(0, [&] {
        auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
        QVERIFY(dialog);
        dialog->findChildren<QComboBox *>()[0]->setCurrentIndex(2);
        dialog->accept();
    });
    QVERIFY(window.openDocumentFrom(path));
    QCOMPARE(window.m_currentFilePath, path);
    QCOMPARE(window.m_editController->undoStack()->count(), 0);
    QCOMPARE(window.m_document.symbol(embedded.id)->displayName,
             applicationSymbolLibrary().find(embedded.id)->displayName);

    // Default embedded choice remains usable even when persistence is unavailable.
    QTimer::singleShot(0, [] {
        auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
        QVERIFY(dialog);
        dialog->accept();
    });
    QVERIFY(window.openDocumentFrom(path));
    QCOMPARE(window.m_document.symbol(embedded.id)->displayName, embedded.displayName);
    const QString savedPath = temporary.filePath("saved.schematic");
    QVERIFY(window.writeDocumentTo(savedPath));
    QFile saved(savedPath);
    QVERIFY(saved.open(QIODevice::ReadOnly));
    auto reopened = Document::fromJsonBytes(saved.readAll());
    QVERIFY(reopened.document);
    QCOMPARE(reopened.document->toJsonBytes(), window.m_document.toJsonBytes());
}

void DocumentOpenTests::missingAndIncompatibleChoices()
{
    QTemporaryDir temporary;
    QStandardPaths::setTestModeEnabled(true);
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, temporary.path());
    struct Restore { SymbolLibrary saved; ~Restore() { applicationSymbolLibrary() = saved; } } restore{applicationSymbolLibrary()};
    MainWindow window;
    window.m_symbolPackageStore = SymbolPackageStore(temporary.filePath("library"));
    Document candidate;
    candidate.addComponent(SymbolIds::Resistor, {});
    auto custom = *candidate.symbol(SymbolIds::Resistor);
    custom.id = SymbolId(QStringLiteral("NewDocumentSymbol"));
    QStringList errors;
    QVERIFY(candidate.useSymbol(custom, &errors));
    candidate.addComponent(custom.id, {100, 0});
    auto root = candidate.toJson();
    auto definitions = root.value("symbols").toArray();
    auto resistor = definitions[1].toObject();
    auto terminals = resistor.value("terminals").toArray();
    auto terminal = terminals[0].toObject();
    terminal.insert("x", -25);
    terminals[0] = terminal;
    resistor.insert("terminals", terminals);
    definitions[1] = resistor;
    root.insert("symbols", definitions);
    const QString path = temporary.filePath("missing.schematic");
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(QJsonDocument(root).toJson());
    file.close();
    QTimer::singleShot(0, [] {
        auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
        QVERIFY(dialog);
        const auto choices = dialog->findChildren<QComboBox *>();
        QCOMPARE(choices.size(), 2);
        QCOMPARE(choices[0]->count(), 2); // missing: embedded or add
        QCOMPARE(choices[1]->count(), 1); // terminals differ: embedded only
        choices[0]->setCurrentIndex(1);
        dialog->accept();
    });
    QVERIFY(window.openDocumentFrom(path));
    QVERIFY(applicationSymbolLibrary().find(custom.id));
    QVERIFY(window.m_symbolActions.contains(custom.id));
    QCOMPARE(window.m_document.terminalPosition(*window.m_document.component(1), 0), QPointF(-25, 0));
    SymbolLibrary restarted = builtinSymbolLibrary();
    errors = window.m_symbolPackageStore.loadInto(&restarted);
    QVERIFY(errors.isEmpty());
    QVERIFY(restarted.find(custom.id));
}

QTEST_MAIN(DocumentOpenTests)
#include "tst_documentopen.moc"
