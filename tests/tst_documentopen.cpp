#include "../mainwindow.h"
#include "documentloadresult.h"
#include "../symbolpreviewwidget.h"
#include "symbolsvg.h"
#include "../propertiespanel.h"
#include "../schematicview.h"
#include "../schematicitems.h"
#include <QApplication>
#include <QClipboard>
#include <QMimeData>
#include <QMenu>
#include <QMenuBar>
#include <QComboBox>
#include <QToolButton>
#include <QColorDialog>
#include <QDialog>
#include <QDoubleSpinBox>
#include "../preferencesdialog.h"
#include "../schematicfont.h"
#include <QFontComboBox>
#include <QSpinBox>
#include "../schematicitems.h"
#include <QDialogButtonBox>
#include <QTabWidget>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QFileDialog>
#include <QToolBar>
#include <QScopeGuard>
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
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QCheckBox>
#include <QPushButton>
#include <QTableWidget>
#include <QDockWidget>
#include <QInputDialog>
#include <QLabel>
#include <QAccessible>
#include <QtTest>

class DocumentOpenTests : public QObject
{
    Q_OBJECT
private slots:
    void globalFontPreferences();
    void drawingOptionsControls();
    void drawingStyleControlUndoAndLoad();
    void textNotesPlacementPropertiesClipboard();
    void recentFilesReopenPersistAndClear();
    void recentFilesLimitAndSuccessfulSaves();
    void svgExportActionShortcutAndState();
    void preferencesDialogLifecycle();
    void propertyJustificationInspector();
    void noteAppearanceControls();
    void clipboardMenuShortcutsAndUndo();
    void clipboardMenuRespectsTextEditing();
    void propertyBlockDockDefaultAndReset();
    void propertyDockLayoutAndGroundVisibility();
    void propertyInspectorMixedEditsAndCustomProperties();
    void propertyInspectorContextAndKeyboard();
    void otaLayoutInspectorUndoAndContext();
    void signalCategoriesAndDefaultLabels();
    void paletteFollowsCategoryTreeOrder();
    void importPreviewPreservesBlackFill();
    void transformToolbarActionsAreUndoable();
    void groupToolbarActionsAndSelectionRestoration();
    void paletteReflowsWithAvailableWidth();
    void categoryCollapseReleasesPaletteObjects();
    void categoryAccessibilityMatchesCollapsedRows();
    void reviewChoicesAndAtomicOpen();
    void missingAndIncompatibleChoices();
};

void DocumentOpenTests::recentFilesReopenPersistAndClear()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QStandardPaths::setTestModeEnabled(true);
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,directory.path());
    const auto bytes = Document().toJsonBytes();
    const auto a = directory.filePath("a.schematic");
    const auto b = directory.filePath("b.schematic");
    for (const auto &path : {a,b}) {
        QFile file(path); QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(bytes), qint64(bytes.size()));
    }
    const auto canonicalA = QFileInfo(a).canonicalFilePath();
    const auto canonicalB = QFileInfo(b).canonicalFilePath();
    MainWindow window;
    window.show(); QVERIFY(QTest::qWaitForWindowActive(&window));
    auto *menu = window.findChild<QMenu *>("recentFilesMenu");
    auto *clear = window.findChild<QAction *>("clearRecentFilesAction");
    QVERIFY(menu); QVERIFY(clear);
    QVERIFY(!menu->menuAction()->isEnabled());
    QVERIFY(window.openDocumentFrom(a));
    QVERIFY(window.openDocumentFrom(b));
    QVERIFY(window.openDocumentFrom(directory.path() + "/./a.schematic"));
    QCOMPARE(QSettings().value("files/recentFiles").toStringList(), QStringList({canonicalA,canonicalB}));
    QVERIFY(menu->menuAction()->isEnabled());
    auto *first = window.findChild<QAction *>("recentFileAction1");
    auto *second = window.findChild<QAction *>("recentFileAction2");
    QVERIFY(first); QVERIFY(second);
    QCOMPARE(first->data().toString(), canonicalA);
    QCOMPARE(second->data().toString(), canonicalB);
    QCOMPARE(first->toolTip(), QDir::toNativeSeparators(canonicalA));
    {
        MainWindow restored;
        QCOMPARE(restored.findChild<QAction *>("recentFileAction1")->data().toString(), canonicalA);
    }
    QVERIFY(window.m_editController->commit("Unsaved component", [&] {
        return window.m_document.addComponent(SymbolIds::Resistor, {}) != kInvalidComponentId;
    }));
    const auto before = window.m_document.toJsonBytes();
    const auto originalPath = window.m_currentFilePath;
    bool prompted = false;
    QTimer::singleShot(0, &window, [&] {
        if (auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) {
            prompted = true; box->button(QMessageBox::Cancel)->click();
        }
    });
    second->trigger();
    QVERIFY(prompted);
    QCOMPARE(window.m_document.toJsonBytes(), before);
    QCOMPARE(window.m_currentFilePath, originalPath);
    QCOMPARE(first->data().toString(), canonicalA);
    QTimer::singleShot(0, &window, [&] {
        if (auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget()))
            box->button(QMessageBox::Discard)->click();
    });
    second->trigger();
    QCOMPARE(window.m_currentFilePath, canonicalB);
    QCOMPARE(window.m_document.toJsonBytes(), bytes);
    QCOMPARE(first->data().toString(), canonicalB);
    QCOMPARE(second->data().toString(), canonicalA);
    const auto history = window.m_editController->undoStack()->index();
    clear->trigger();
    QVERIFY(QSettings().value("files/recentFiles").toStringList().isEmpty());
    QVERIFY(!menu->menuAction()->isEnabled());
    QVERIFY(!first->isVisible());
    QCOMPARE(window.m_currentFilePath, canonicalB);
    QCOMPARE(window.m_document.toJsonBytes(), bytes);
    QCOMPARE(window.m_editController->undoStack()->index(), history);
    QVERIFY(QFile::exists(a)); QVERIFY(QFile::exists(b));
}

void DocumentOpenTests::recentFilesLimitAndSuccessfulSaves()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QStandardPaths::setTestModeEnabled(true);
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,directory.path());
    const bool previous = QApplication::testAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    const auto restore = qScopeGuard([previous] { QApplication::setAttribute(Qt::AA_DontUseNativeDialogs, previous); });
    MainWindow window;
    window.show(); QVERIFY(QTest::qWaitForWindowActive(&window));
    const auto bytes = Document().toJsonBytes();
    QString last;
    for (int i = 0; i < 12; ++i) {
        last = directory.filePath(QString("file%1.schematic").arg(i));
        QFile file(last); QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(bytes), qint64(bytes.size())); file.close();
        QVERIFY(window.openDocumentFrom(last));
    }
    auto paths = QSettings().value("files/recentFiles").toStringList();
    QCOMPARE(paths.size(), 10);
    QCOMPARE(paths.first(), QFileInfo(last).canonicalFilePath());
    QCOMPARE(paths.last(), QFileInfo(directory.filePath("file2.schematic")).canonicalFilePath());
    // An unavailable recent file uses normal Open feedback and is retained
    // (for example, a temporarily disconnected drive may return later).
    QVERIFY(QFile::remove(last));
    QTimer::singleShot(0, &window, [] {
        if (auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) box->accept();
    });
    window.findChild<QAction *>("recentFileAction1")->trigger();
    QCOMPARE(QSettings().value("files/recentFiles").toStringList(), paths);
    QCOMPARE(window.m_document.toJsonBytes(), bytes);
    window.m_currentFilePath = directory.filePath("saved.schematic");
    QVERIFY(window.onSave());
    QCOMPARE(QSettings().value("files/recentFiles").toStringList().first(), QFileInfo(window.m_currentFilePath).canonicalFilePath());
    const auto saveAs = directory.filePath("saved & named.schematic");
    QTimer::singleShot(0, &window, [&] {
        if (auto *dialog = qobject_cast<QFileDialog *>(QApplication::activeModalWidget())) {
            dialog->selectFile(saveAs);
            QMetaObject::invokeMethod(dialog, "accept", Qt::QueuedConnection);
        }
    });
    QVERIFY(window.onSaveAs());
    paths = QSettings().value("files/recentFiles").toStringList();
    QCOMPARE(paths.size(), 10);
    QCOMPARE(paths.first(), QFileInfo(saveAs).canonicalFilePath());
    QVERIFY(window.findChild<QAction *>("recentFileAction1")->text().contains("&&"));
    window.m_currentFilePath = directory.filePath("missing/cannot-save.schematic");
    QTimer::singleShot(0, &window, [] {
        if (auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) box->accept();
    });
    QVERIFY(!window.onSave());
    QCOMPARE(QSettings().value("files/recentFiles").toStringList(), paths);
}

void DocumentOpenTests::svgExportActionShortcutAndState()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QStandardPaths::setTestModeEnabled(true);
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,directory.path());
    const bool previous = QApplication::testAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    const auto restoreDialogs = qScopeGuard([previous] { QApplication::setAttribute(Qt::AA_DontUseNativeDialogs, previous); });
    MainWindow window;
    window.show(); QVERIFY(QTest::qWaitForWindowActive(&window));
    auto *action = window.findChild<QAction *>("svgExportAction");
    QVERIFY(action);
    QCOMPARE(action->shortcut(), QKeySequence(Qt::CTRL | Qt::Key_E));
    QVERIFY(!action->icon().isNull());
    QVERIFY(!action->isEnabled());
    auto *toolbar = window.findChild<QToolBar *>("fileToolBar");
    QVERIFY(toolbar);
    QVERIFY(toolbar->actions().contains(action));
    QVERIFY(window.menuBar()->actions().first()->menu()->actions().contains(action));
    ComponentId id = kInvalidComponentId;
    QVERIFY(window.m_editController->commit("Place resistor", [&] {
        id = window.m_document.addComponent(SymbolIds::Resistor, {});
        return id != kInvalidComponentId;
    }));
    window.m_scene->restoreAfterDocumentChange({{id},{}});
    QVERIFY(action->isEnabled());
    window.m_currentFilePath = directory.filePath("original.schematic");
    const auto before = window.m_document.toJsonBytes();
    const int history = window.m_editController->undoStack()->index();
    const bool clean = window.m_editController->undoStack()->isClean();
    const auto selection = window.m_scene->selection();
    bool cancelled = false;
    QTimer::singleShot(0, &window, [&] {
        if (auto *dialog = qobject_cast<QFileDialog *>(QApplication::activeModalWidget())) {
            cancelled = true; dialog->reject();
        }
    });
    action->trigger();
    QVERIFY(cancelled);
    QVERIFY(!QFile::exists(directory.filePath("original.svg")));
    window.activateWindow();
    QVERIFY(QTest::qWaitForWindowActive(&window));
    window.m_view->setFocus();
    QTRY_VERIFY(window.m_view->hasFocus());
    bool opened = false;
    QTimer::singleShot(0, &window, [&] {
        if (auto *dialog = qobject_cast<QFileDialog *>(QApplication::activeModalWidget())) {
            opened = true;
            dialog->selectFile(directory.filePath("exported"));
            QMetaObject::invokeMethod(dialog, "accept", Qt::QueuedConnection);
        }
    });
    QTest::keyClick(window.m_view, Qt::Key_E, Qt::ControlModifier);
    QVERIFY(opened);
    QFile output(directory.filePath("exported.svg"));
    QVERIFY(output.open(QIODevice::ReadOnly));
    const auto bytes = output.readAll();
    QVERIFY(bytes.contains("<svg"));
    QVERIFY(!bytes.contains("#ff0000"));
    QCOMPARE(window.m_currentFilePath, directory.filePath("original.schematic"));
    QCOMPARE(window.m_document.toJsonBytes(), before);
    QCOMPARE(window.m_editController->undoStack()->index(), history);
    QCOMPARE(window.m_editController->undoStack()->isClean(), clean);
    QVERIFY(window.m_scene->selection() == selection);
}

namespace {
class SavedClipboard {
    QMimeData *saved = new QMimeData;
public:
    SavedClipboard() { if (const auto *mime=QApplication::clipboard()->mimeData())
        for(const auto &format:mime->formats()) saved->setData(format,mime->data(format)); }
    ~SavedClipboard() { QApplication::clipboard()->setMimeData(saved); }
};
}

void DocumentOpenTests::otaLayoutInspectorUndoAndContext()
{
    QTemporaryDir temporary; QStandardPaths::setTestModeEnabled(true);
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,temporary.path());
    MainWindow window; window.show(); QVERIFY(QTest::qWaitForWindowActive(&window));
    const auto a = window.m_document.addComponent(SymbolIds::OTAWithSuppliesVerticalBias, {});
    const auto b = window.m_document.addComponent(SymbolIds::OTANoSuppliesVerticalBias, {200,0});
    QVERIFY(window.m_document.addWire(makeTerminalEndpoint(a,3), {}, makeTerminalEndpoint(b,3)));
    window.m_scene->restoreAfterDocumentChange({{a},{}});
    auto *panel = window.findChild<PropertiesPanel *>(); QVERIFY(panel);
    QTRY_VERIFY(panel->findChild<QComboBox *>("symbolLayout_bias_position"));
    auto *combo = panel->findChild<QComboBox *>("symbolLayout_bias_position");
    QCOMPARE(combo->currentData().toString(), QString("below"));
    QCOMPARE(combo->count(), 2);
    const auto below = window.m_document.toJsonBytes();
    const int history = window.m_editController->undoStack()->count();
    combo->setCurrentIndex(1);
    QVERIFY(QMetaObject::invokeMethod(combo,"activated", Q_ARG(int,1)));
    QCOMPARE(window.m_document.component(a)->symbolId(), SymbolIds::OTAWithSuppliesBiasAbove);
    QVERIFY(!window.m_symbolActions.contains(SymbolIds::OTAWithSuppliesBiasAbove));
    QVERIFY(!window.m_symbolActions.contains(SymbolIds::OTANoSuppliesBiasAbove));
    QVERIFY(window.m_symbolActions.contains(SymbolIds::OTAWithSuppliesVerticalBias));
    QVERIFY(window.m_symbolActions.contains(SymbolIds::OTANoSuppliesVerticalBias));
    QCOMPARE(window.m_editController->undoStack()->count(), history+1);
    QCOMPARE(window.m_scene->selection().components, QVector<ComponentId>{a});
    for (auto *item : window.m_scene->items())
        if (auto *label = qgraphicsitem_cast<ComponentLabelItem *>(item))
            if (label->componentId() == a) QCOMPARE(label->brush().color(), QColor(Qt::red));
    const auto above = window.m_document.toJsonBytes();
    window.m_editController->undoStack()->undo(); QCOMPARE(window.m_document.toJsonBytes(), below);
    window.m_editController->undoStack()->redo(); QCOMPARE(window.m_document.toJsonBytes(), above);
    QTRY_COMPARE(panel->findChild<QComboBox *>("symbolLayout_bias_position")->currentData().toString(), QString("above"));
    // No-op selection is not another undo step.
    combo = panel->findChild<QComboBox *>("symbolLayout_bias_position");
    const int count = window.m_editController->undoStack()->count();
    const QPointer<QComboBox> beforeRefresh(combo);
    QVERIFY(QMetaObject::invokeMethod(combo,"activated", Q_ARG(int,1)));
    QCOMPARE(window.m_editController->undoStack()->count(), count);
    QTRY_VERIFY(beforeRefresh.isNull());
    // Commit pending text first, without losing it during the layout change.
    auto *value = panel->findChild<QLineEdit *>("propertyValue_value");
    QVERIFY(value);
    value->setFocus(); value->selectAll(); QTest::keyClicks(value,"LM13700");
    combo = panel->findChild<QComboBox *>("symbolLayout_bias_position");
    combo->setCurrentIndex(0);
    QVERIFY(QMetaObject::invokeMethod(combo,"activated", Q_ARG(int,0)));
    QCOMPARE(window.m_document.component(a)->value(), QString("LM13700"));
    QCOMPARE(window.m_document.component(a)->symbolId(), SymbolIds::OTAWithSuppliesVerticalBias);
    // A queued activation from the previous selection must not edit a new one.
    window.m_scene->restoreSelection({{b},{}});
    QVERIFY(QMetaObject::invokeMethod(combo,"activated", Q_ARG(int,1)));
    QCOMPARE(window.m_document.component(b)->symbolId(), SymbolIds::OTANoSuppliesVerticalBias);
    window.m_scene->restoreSelection({{a,b},{}});
    QTRY_VERIFY(!panel->findChild<QComboBox *>("symbolLayout_bias_position"));
    window.m_scene->restoreSelection({{a},{}});
    QTRY_VERIFY(panel->findChild<QComboBox *>("symbolLayout_bias_position"));
    QApplication::processEvents();
    combo = panel->findChild<QComboBox *>("symbolLayout_bias_position");
    QVERIFY(combo->isVisible());
    QVERIFY(combo->width() > 0);
    auto *table = panel->findChild<QTableWidget *>("propertiesTable"); QVERIFY(table);
    QVERIFY(combo->mapTo(panel, QPoint()).y() < table->mapTo(panel, QPoint()).y());
    QVERIFY(window.grab().save(QCoreApplication::applicationDirPath()+"/ota-bias-inspector.png"));
    const auto anchor = window.m_document.addComponent(SymbolIds::Resistor, {50,-20});
    QVERIFY(window.m_document.addWire(makeTerminalEndpoint(a,3), {}, makeTerminalEndpoint(anchor,0)));
    const QPointer<QComboBox> beforeRebuild(combo);
    window.m_scene->restoreAfterDocumentChange({{a},{}});
    QTRY_VERIFY(beforeRebuild.isNull());
    QTRY_VERIFY(panel->findChild<QComboBox *>("symbolLayout_bias_position"));
    combo = panel->findChild<QComboBox *>("symbolLayout_bias_position");
    const auto beforeFailure = window.m_document.toJsonBytes();
    const auto beforeFailureHistory = window.m_editController->undoStack()->count();
    combo->setCurrentIndex(1);
    QVERIFY(QMetaObject::invokeMethod(combo,"activated", Q_ARG(int,1)));
    QCOMPARE(window.m_document.toJsonBytes(), beforeFailure);
    QCOMPARE(window.m_editController->undoStack()->count(), beforeFailureHistory);
    QCOMPARE(combo->currentData().toString(), QString("below"));
    QVERIFY(!panel->findChild<QLabel *>("propertyError")->text().isEmpty());
}

void DocumentOpenTests::signalCategoriesAndDefaultLabels()
{
    QTemporaryDir temporary; QStandardPaths::setTestModeEnabled(true);
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,temporary.path());
    MainWindow window; window.show(); QVERIFY(QTest::qWaitForWindowActive(&window));
    auto *signalsCategory = window.m_categoryItems.value(QStringLiteral("Signals")); QVERIFY(signalsCategory);
    QCOMPARE(signalsCategory->childCount(),3);
    QVERIFY(window.m_symbolActions.contains(SymbolIds::RailVCC));
    QVERIFY(window.m_symbolActions.contains(SymbolIds::GroundSignal));
    QVERIFY(window.m_symbolActions.contains(SymbolIds::Ground));
    QVERIFY(window.m_symbolActions.contains(SymbolIds::GroundChassis));
    const QStringList categories{"Power Rails","Grounds","Ports"};
    for (int i = 0; i < categories.size(); ++i) {
        QCOMPARE(signalsCategory->child(i)->text(0),categories[i]);
        window.populateSymbolGrid("Signals/" + categories[i]);
        const auto *tiles = window.m_symbolSectionsLayout->itemAt(0)->layout(); QVERIFY(tiles);
        QCOMPARE(tiles->count(),i == 0 ? 1 : 3);
        QVERIFY(window.grab().save(QCoreApplication::applicationDirPath()+QString("/signals-palette-%1.png").arg(i)));
    }
    const QVector<SymbolId> ids{SymbolIds::RailVCC, SymbolIds::Ground, SymbolIds::GroundChassis, SymbolIds::GroundSignal,
        SymbolIds::PortInput, SymbolIds::PortOutput,
        SymbolIds::PortBidirectional};
    ComponentId analog = kInvalidComponentId;
    for (int i = 0; i < ids.size(); ++i) {
        const auto id = window.m_document.addComponent(ids[i], {-200.0 + 100*(i%5), -100.0 + 100*(i/5)});
        QVERIFY(id);
        if (ids[i] == SymbolIds::GroundSignal) analog = id;
    }
    window.m_scene->restoreAfterDocumentChange({{}, {}});
    QApplication::processEvents();
    window.m_view->centerOn(0,0);
    QVERIFY(window.grab().save(QCoreApplication::applicationDirPath()+"/signals-canvas.png"));
    window.m_scene->restoreSelection({{analog},{}});
    auto *panel = window.findChild<PropertiesPanel *>(); QVERIFY(panel);
    QTRY_VERIFY(panel->findChild<QLineEdit *>("propertyValue_value"));
    auto *value = panel->findChild<QLineEdit *>("propertyValue_value");
    QCOMPARE(value->text(),QString());
    QCOMPARE(panel->findChild<QCheckBox *>("propertyVisible_reference")->checkState(),Qt::Unchecked);
    const auto nets = window.m_document.computeNets().size();
    value->setFocus(); value->selectAll(); QTest::keyClicks(value,"Audio Ground");
    QTest::keyClick(value,Qt::Key_Return);
    QTRY_COMPARE(window.m_document.component(analog)->value(),QString("Audio Ground"));
    QCOMPARE(window.m_document.computeNets().size(),nets);
    window.m_editController->undoStack()->undo();
    QVERIFY(window.m_document.component(analog)->value().isEmpty());
}

void DocumentOpenTests::paletteFollowsCategoryTreeOrder()
{
    struct Restore { SymbolLibrary saved; ~Restore() { applicationSymbolLibrary() = saved; } } restore{applicationSymbolLibrary()};
    SymbolLibrary library;
    library.declareCategories({"PaletteOrderTest/First","PaletteOrderTest/Empty","PaletteOrderTest/Second"});
    auto symbol = *builtinSymbolLibrary().find(SymbolIds::Resistor);
    QStringList errors;
    symbol.id = SymbolId("OrderSecond"); symbol.displayName = "Second symbol"; symbol.category = "PaletteOrderTest/Second";
    QVERIFY(library.add(symbol,&errors));
    symbol.id = SymbolId("OrderFirst"); symbol.displayName = "First symbol"; symbol.category = "PaletteOrderTest/First";
    QVERIFY(library.add(symbol,&errors));
    symbol.id = SymbolId("OrderParent"); symbol.displayName = "Parent symbol"; symbol.category = "PaletteOrderTest";
    QVERIFY(library.add(symbol,&errors));
    applicationSymbolLibrary() = library;
    QTemporaryDir temporary; QStandardPaths::setTestModeEnabled(true);
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,temporary.path());
    MainWindow window;
    auto *parent = window.m_categoryItems.value("PaletteOrderTest"); QVERIFY(parent);
    QCOMPARE(parent->childCount(),3);
    QCOMPARE(parent->child(0)->text(0),QString("First"));
    QCOMPARE(parent->child(1)->text(0),QString("Empty"));
    QCOMPARE(parent->child(2)->text(0),QString("Second"));
    window.populateSymbolGrid("PaletteOrderTest");
    auto *sections = window.m_symbolSectionsLayout;
    QCOMPARE(sections->itemAt(0)->layout()->itemAt(0)->widget()->accessibleName(),QString("Parent symbol"));
    QCOMPARE(qobject_cast<QLabel *>(sections->itemAt(1)->widget())->text(),QString("First"));
    QCOMPARE(sections->itemAt(2)->layout()->itemAt(0)->widget()->accessibleName(),QString("First symbol"));
    QCOMPARE(qobject_cast<QLabel *>(sections->itemAt(3)->widget())->text(),QString("Second"));
    QCOMPARE(sections->itemAt(4)->layout()->itemAt(0)->widget()->accessibleName(),QString("Second symbol"));
    window.populateSymbolGrid("PaletteOrderTest/Empty");
    QCOMPARE(window.m_symbolSectionsLayout->count(),1);
    auto moved = *applicationSymbolLibrary().find(SymbolId("OrderSecond"));
    moved.category = "PaletteOrderTest/First";
    QVERIFY(applicationSymbolLibrary().replace(moved,&errors));
    window.rebuildSymbolDockContents(); window.populateSymbolGrid("PaletteOrderTest/First");
    auto *tiles = window.m_symbolSectionsLayout->itemAt(0)->layout(); QVERIFY(tiles);
    QCOMPARE(tiles->count(),2);
    QCOMPARE(tiles->itemAt(0)->widget()->accessibleName(),QString("First symbol"));
    QCOMPARE(tiles->itemAt(1)->widget()->accessibleName(),QString("Second symbol"));
}

void DocumentOpenTests::propertyJustificationInspector()
{
    QTemporaryDir temporary; QStandardPaths::setTestModeEnabled(true);
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,temporary.path());
    MainWindow window; window.show(); QVERIFY(QTest::qWaitForWindowActive(&window));
    const auto a=window.m_document.addComponent(SymbolIds::Resistor,{});
    const auto b=window.m_document.addComponent(SymbolIds::Ground,{100,0});
    QVERIFY(window.m_document.setPropertyBlockOffset({a},QPointF(13,17)));
    window.m_scene->restoreAfterDocumentChange({{a,b},{}});
    auto *panel=window.findChild<PropertiesPanel *>(); QVERIFY(panel);
    QTRY_VERIFY(panel->findChild<QToolButton *>("propertyBlockJustificationRight"));
    auto *button=panel->findChild<QToolButton *>("propertyBlockJustificationRight");
    for(auto *item : button->parentWidget()->findChildren<QToolButton *>()) QVERIFY(!item->isChecked());
    const auto before=window.m_document.toJsonBytes();
    button->click();
    QCOMPARE(window.m_document.component(a)->propertyBlockJustification(),PropertyJustification::Right);
    QCOMPARE(window.m_document.component(b)->propertyBlockJustification(),PropertyJustification::Right);
    QCOMPARE(window.m_document.component(a)->propertyBlockOffset(),std::optional<QPointF>(QPointF(13,17)));
    QCOMPARE(window.m_editController->undoStack()->count(),1);
    window.m_editController->undoStack()->undo(); QCOMPARE(window.m_document.toJsonBytes(),before);
    window.m_editController->undoStack()->redo();
    QTRY_VERIFY(panel->findChild<QToolButton *>("propertyBlockJustificationRight")->isChecked());
    QVERIFY(window.m_scene->resetPropertyPositions({a,b}));
    QCOMPARE(window.m_document.component(a)->propertyBlockJustification(),PropertyJustification::Right);
}

void DocumentOpenTests::noteAppearanceControls()
{
    QTemporaryDir temporary; QStandardPaths::setTestModeEnabled(true);
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,temporary.path());
    MainWindow window; window.show(); QVERIFY(QTest::qWaitForWindowActive(&window));
    const auto id=window.m_document.addNote({},"A note",120);
    window.m_scene->restoreAfterDocumentChange({{}, {}, {id}});
    auto *panel=window.findChild<PropertiesPanel *>(); QVERIFY(panel);
    QTRY_VERIFY(panel->findChild<QToolButton *>("noteJustificationRight"));
    panel->findChild<QToolButton *>("noteJustificationRight")->click();
    QCOMPARE(window.m_document.note(id)->justification,PropertyJustification::Right);
    window.m_editController->undoStack()->undo();
    QCOMPARE(window.m_document.note(id)->justification,PropertyJustification::Left);
    window.m_editController->undoStack()->redo();
    QTRY_VERIFY(panel->findChild<QToolButton *>("noteJustificationRight")->isChecked());
    QApplication::setAttribute(Qt::AA_DontUseNativeDialogs,true);
    const auto restore=qScopeGuard([] { QApplication::setAttribute(Qt::AA_DontUseNativeDialogs,false); });
    for(int target=1;target<=2;++target) {
        bool opened=false;
        QTimer::singleShot(0,&window,[&] {
            if(auto *dialog=qobject_cast<QColorDialog *>(QApplication::activeModalWidget())) {
                opened=true; dialog->setCurrentColor(QColor("#ff80a0c0")); dialog->accept();
            }
        });
        panel->findChild<QToolButton *>(target==1 ? "noteFillColour" : "noteOutlineColour")->click();
        QVERIFY(opened);
        QCOMPARE(target==1 ? window.m_document.note(id)->fillColor : window.m_document.note(id)->outlineColor,QString("#ff80a0c0"));
        QApplication::processEvents();
    }
    const auto before=window.m_document.toJsonBytes();
    QTimer::singleShot(0,&window,[] {
        if(auto *dialog=qobject_cast<QColorDialog *>(QApplication::activeModalWidget())) dialog->reject();
    });
    panel->findChild<QToolButton *>("noteFillColour")->click();
    QCOMPARE(window.m_document.toJsonBytes(),before);
    panel->findChild<QAction *>("clearNoteFill")->trigger();
    QCOMPARE(window.m_document.note(id)->fillColor,QString("none"));
    window.m_editController->undoStack()->undo();
    QCOMPARE(window.m_document.toJsonBytes(),before);
}

void DocumentOpenTests::preferencesDialogLifecycle()
{
    QTemporaryDir temporary; QStandardPaths::setTestModeEnabled(true);
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,temporary.path());
    MainWindow window; window.show(); QVERIFY(QTest::qWaitForWindowActive(&window));
    auto *action=window.findChild<QAction *>("preferencesAction"); QVERIFY(action);
    QCOMPARE(window.m_editMenu->actions().last(),action);
    const auto id=window.m_document.addComponent(SymbolIds::Resistor,{});
    window.m_scene->restoreAfterDocumentChange({{id},{}});
    window.m_view->setFocus(); QTRY_VERIFY(window.m_view->hasFocus());
    const auto before=window.m_document.toJsonBytes();
    const auto selection=window.m_scene->selection();
    const auto history=window.m_editController->undoStack()->index();
    const bool clean=window.m_editController->undoStack()->isClean();
    QCOMPARE(window.m_scene->propertyBlockGridSpacing(), 2.0);
    QVERIFY(window.m_view->cadStyleMouseWheel());
    QVERIFY(PreferencesDialog::savedCadStyleMouseWheel());
    for(int closeMode=0;closeMode<3;++closeMode) {
        bool opened=false, valid=false, captured=true;
        QTimer::singleShot(0,&window,[&] {
            auto *dialog=qobject_cast<QDialog *>(QApplication::activeModalWidget());
            if(!dialog)return;
            opened=dialog->objectName()=="preferencesDialog";
            auto *tabs=dialog->findChild<QTabWidget *>("preferencesTabs");
            auto *buttons=dialog->findChild<QDialogButtonBox *>();
            valid=tabs && tabs->count()==2 && tabs->tabText(1)=="Appearance" && tabs->tabText(0)=="General" && tabs->currentIndex()==0
                && tabs->widget(0)->layout()
                && buttons && buttons->standardButtons()==(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
            auto *grid=dialog->findChild<QDoubleSpinBox *>("propertyBlockGridSpacing");
            valid = valid && grid && grid->value()==2.0;
            auto *wheel = dialog->findChild<QCheckBox *>("cadStyleMouseWheel");
            valid = valid && wheel && wheel->isChecked();
            if(wheel) wheel->setChecked(false);
            dialog->findChild<QCheckBox *>("keepPlacingSymbols")->setChecked(true);
            if(grid) grid->setValue(3.5);
            dialog->resize(640,400);
            if(closeMode==0) {
                tabs->setCurrentIndex(1);
                captured=dialog->grab().save(QCoreApplication::applicationDirPath()+"/preferences-dialog.png");
                if(buttons && buttons->button(QDialogButtonBox::Cancel)) buttons->button(QDialogButtonBox::Cancel)->click();
                else dialog->reject();
            } else if(closeMode==1) QTest::keyClick(dialog,Qt::Key_Escape);
            else dialog->close();
        });
        if(closeMode==1) QTest::keyClick(window.m_view,Qt::Key_Comma,Qt::ControlModifier);
        else action->trigger();
        QVERIFY2(opened,qPrintable(QString("Close mode %1").arg(closeMode))); QVERIFY(valid); QVERIFY(captured);
        QVERIFY(!QApplication::activeModalWidget());
        QCOMPARE(window.m_scene->propertyBlockGridSpacing(), 2.0);
        QVERIFY(window.m_view->cadStyleMouseWheel());
        QVERIFY(PreferencesDialog::savedCadStyleMouseWheel());
        QVERIFY(!PreferencesDialog::savedKeepPlacingSymbols());
        QCOMPARE(window.m_document.toJsonBytes(),before);
        QCOMPARE(window.m_scene->selection().components,selection.components);
        QCOMPARE(window.m_editController->undoStack()->index(),history);
        QCOMPARE(window.m_editController->undoStack()->isClean(),clean);
        window.activateWindow(); window.m_view->setFocus();
        QVERIFY(QTest::qWaitForWindowActive(&window));
        QApplication::processEvents();
    }
    QTimer::singleShot(0,&window,[&] {
        auto *dialog=qobject_cast<PreferencesDialog *>(QApplication::activeModalWidget());
        if(!dialog)return;
        dialog->findChild<QDoubleSpinBox *>("propertyBlockGridSpacing")->setValue(3.5);
        dialog->findChild<QCheckBox *>("cadStyleMouseWheel")->setChecked(false);
        dialog->findChild<QCheckBox *>("keepPlacingSymbols")->setChecked(true);
        dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();
    });
    action->trigger();
    QCOMPARE(window.m_scene->propertyBlockGridSpacing(),3.5);
    QCOMPARE(PreferencesDialog::savedPropertyBlockGridSpacing(),3.5);
    QVERIFY(!window.m_view->cadStyleMouseWheel());
    QVERIFY(!PreferencesDialog::savedCadStyleMouseWheel());
    QVERIFY(PreferencesDialog::savedKeepPlacingSymbols());
    {
        PreferencesDialog reopened;
        QVERIFY(!reopened.cadStyleMouseWheel());
        QVERIFY(reopened.keepPlacingSymbols());
        MainWindow restarted;
        QVERIFY(!restarted.m_view->cadStyleMouseWheel());
    }
    QCOMPARE(window.m_document.toJsonBytes(),before);
    QCOMPARE(window.m_editController->undoStack()->index(),history);
    for (const QVariant &bad : {QVariant("bad"),QVariant(-1),QVariant(1001),QVariant(qInf())}) {
        QSettings().setValue("appearance/propertyBlockGridSpacing",bad);
        QCOMPARE(PreferencesDialog::savedPropertyBlockGridSpacing(),2.0);
    }
    QSettings().remove("appearance/propertyBlockGridSpacing");
}

void DocumentOpenTests::clipboardMenuShortcutsAndUndo()
{
    SavedClipboard saved; QApplication::clipboard()->setText("Clipboard test initial state");
    QTemporaryDir temporary; QStandardPaths::setTestModeEnabled(true);
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,temporary.path());
    MainWindow window; window.resize(1100,850); window.show(); QVERIFY(QTest::qWaitForWindowActive(&window));
    const auto a=window.m_document.addComponent(SymbolIds::Resistor,{});
    const auto b=window.m_document.addComponent(SymbolIds::Capacitor,{100,0});
    const auto wire=window.m_document.addWire(makeTerminalEndpoint(a,1),{},makeTerminalEndpoint(b,0)); QVERIFY(wire);
    const DocumentSelection selected{{a,b},{*wire}}; window.m_scene->restoreAfterDocumentChange(selected);
    window.m_view->setFocus(); QTRY_VERIFY(window.m_view->hasFocus());
    const auto before=window.m_document.toJsonBytes();
    window.m_editMenu->popup(window.mapToGlobal(QPoint(50,70))); QApplication::processEvents();
    QVERIFY(window.m_editMenu->grab().save(QCoreApplication::applicationDirPath()+"/edit-menu.png"));
    window.m_editMenu->hide(); window.m_view->setFocus();
    const auto actions=window.m_editMenu->actions();
    QVERIFY(actions.indexOf(window.m_cutAction)<actions.indexOf(window.m_rotateLeftAction));
    QVERIFY(actions.indexOf(window.m_rotateAction)<actions.indexOf(window.m_mirrorAction));
    QCOMPARE(window.m_rotateAction->shortcut(),QKeySequence(Qt::Key_R));
    QTest::keyClick(window.m_view,Qt::Key_C,Qt::ControlModifier);
    QTRY_VERIFY(QApplication::clipboard()->mimeData()->hasFormat("application/x-schematic-fragment+json"));
    QCOMPARE(window.m_document.toJsonBytes(),before); QVERIFY(window.m_editController->undoStack()->isClean());
    QTest::mouseMove(window.m_view->viewport(), window.m_view->mapFromScene(203,117));
    QApplication::processEvents();
    const QPointF cursor = window.m_view->pastePosition();
    const QPointF snapped(qRound(cursor.x()/10)*10, qRound(cursor.y()/10)*10);
    QTest::keyClick(window.m_view,Qt::Key_V,Qt::ControlModifier);
    QTRY_COMPARE(window.m_document.components().size(),4); QCOMPARE(window.m_document.wires().size(),2);
    QCOMPARE(window.m_editController->undoStack()->count(),1);
    const auto first=window.m_scene->selection(); QCOMPARE(first.components.size(),2); QCOMPARE(first.wires.size(),1);
    QCOMPARE(window.m_document.component(first.components[0])->position(),snapped-QPointF(50,0));
    window.m_pasteAction->trigger(); QCOMPARE(window.m_document.components().size(),6);
    const auto second=window.m_scene->selection();
    QCOMPARE(window.m_document.component(second.components[0])->position(),snapped-QPointF(50,0));
    window.m_editController->undoStack()->undo(); QCOMPARE(window.m_scene->selection().components,first.components);
    window.m_editController->undoStack()->undo(); QCOMPARE(window.m_document.toJsonBytes(),before);
    QCOMPARE(window.m_scene->selection().components,selected.components);
    window.m_editController->undoStack()->redo(); QCOMPARE(window.m_scene->selection().components,first.components);
    window.m_editController->undoStack()->undo();
    // Cut copies only selected objects; unselected incident wires stay behind.
    window.m_scene->restoreSelection({{a},{}}); window.m_cutAction->trigger();
    QCOMPARE(window.m_document.components().size(),1);
    QCOMPARE(window.m_document.wires().size(),1);
    QCOMPARE(window.m_document.nodes().size(),1);
    QVERIFY(isNode(window.m_document.wires().first().start));
    QCOMPARE(window.m_document.wires().first().id,*wire);
    QCOMPARE(window.m_editController->undoStack()->command(0)->text(),QString("Cut selection"));
    window.m_editController->undoStack()->undo(); QCOMPARE(window.m_document.toJsonBytes(),before);
    window.m_editController->undoStack()->redo();
    QCOMPARE(window.m_document.wires().size(),1);
    QCOMPARE(window.m_document.nodes().size(),1);
    window.m_editController->undoStack()->undo(); QCOMPARE(window.m_document.toJsonBytes(),before);
    window.m_scene->restoreSelection(selected);
    QTest::keyClick(window.m_view,Qt::Key_R); QTRY_COMPARE(window.m_document.component(a)->rotation(),Rotation::Deg90);
    QTest::keyClick(window.m_view,Qt::Key_R,Qt::ShiftModifier); QTRY_COMPARE(window.m_document.component(a)->rotation(),Rotation::Deg0);
    QTest::keyClick(window.m_view,Qt::Key_H); QTRY_VERIFY(window.m_document.component(a)->mirrored());
    QTest::keyClick(window.m_view,Qt::Key_V);
    QTRY_COMPARE(window.m_document.component(a)->rotation(),Rotation::Deg180);
    window.m_deleteAction->trigger(); QVERIFY(!window.m_document.component(a));
    window.m_editController->undoStack()->undo(); QVERIFY(window.m_document.component(a));
    auto *bad=new QMimeData; bad->setData("application/x-schematic-fragment+json","bad");
    QApplication::clipboard()->setMimeData(bad); const auto invalidBefore=window.m_document.toJsonBytes();
    const auto count=window.m_editController->undoStack()->count(); window.m_pasteAction->trigger();
    QCOMPARE(window.m_document.toJsonBytes(),invalidBefore); QCOMPARE(window.m_editController->undoStack()->count(),count);
    window.m_scene->setEditMode(SchematicScene::EditMode::DrawWire); QVERIFY(!window.m_pasteAction->isEnabled());
    QVERIFY(!window.m_cutAction->isEnabled()); QVERIFY(!window.m_rotateAction->isEnabled());
}

void DocumentOpenTests::clipboardMenuRespectsTextEditing()
{
    SavedClipboard saved; QApplication::clipboard()->setText("Clipboard test initial state");
    QTemporaryDir temporary; QStandardPaths::setTestModeEnabled(true);
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,temporary.path());
    MainWindow window; window.show(); QVERIFY(QTest::qWaitForWindowActive(&window));
    const auto id=window.m_document.addComponent(SymbolIds::Resistor,{});
    QVERIFY(window.m_document.setComponentLabels(id,"R1","10k"));
    QVERIFY(window.m_editController->commit("Move for text isolation", [&] { return window.m_document.moveComponent(id,{30,0}); }));
    window.m_scene->restoreAfterDocumentChange({{id},{}}); window.m_propertiesPanel->focusProperty("value");
    QTRY_VERIFY(window.m_propertiesPanel->findChild<QLineEdit *>("propertyValue_value"));
    QTRY_VERIFY(window.m_propertiesPanel->findChild<QLineEdit *>("propertyValue_value")->hasFocus());
    QPointer<QLineEdit> line=window.m_propertiesPanel->findChild<QLineEdit *>("propertyValue_value");
    line->selectAll(); QTest::keyClicks(line,"RHV"); QCOMPARE(line->text(),QString("RHV"));
    const auto before=window.m_document.toJsonBytes();
    QVERIFY(!window.m_rotateAction->isEnabled());
    line->selectAll();
    window.m_editMenu->popup(window.mapToGlobal(QPoint(50,70))); QApplication::processEvents();
    QVERIFY(line); QCOMPARE(window.m_document.toJsonBytes(),before);
    window.m_copyAction->trigger(); QCOMPARE(QApplication::clipboard()->text(),QString("RHV"));
    window.m_editMenu->hide(); QApplication::processEvents();
    QVERIFY(line); QCOMPARE(window.m_document.toJsonBytes(),before);
    window.m_cutAction->trigger(); QVERIFY(line->text().isEmpty());
    window.m_undoAction->trigger(); QCOMPARE(line->text(),QString("RHV"));
    line->selectAll(); window.m_deleteAction->trigger(); QVERIFY(line->text().isEmpty());
    window.m_pasteAction->trigger(); QCOMPARE(line->text(),QString("RHV"));
    QCOMPARE(window.m_document.toJsonBytes(),before); QCOMPARE(window.m_editController->undoStack()->count(),1);
    QTest::keyClick(line,Qt::Key_Escape); window.m_view->setFocus();
    QTRY_VERIFY(window.m_view->hasFocus()); QVERIFY(window.m_rotateAction->isEnabled());
    QVERIFY(!window.m_pasteAction->isEnabled());
}

void DocumentOpenTests::propertyBlockDockDefaultAndReset()
{
    QTemporaryDir temporary; QStandardPaths::setTestModeEnabled(true);
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,temporary.path());
    {
        MainWindow window; window.resize(1100,850); window.show();
        QVERIFY(QTest::qWaitForWindowActive(&window));
        auto *symbols=window.findChild<QDockWidget *>("symbolDock"); QVERIFY(symbols);
        QCOMPARE(window.dockWidgetArea(window.m_propertiesDock),Qt::LeftDockWidgetArea);
        QVERIFY(window.m_propertiesDock->geometry().top()>symbols->geometry().top());
        const auto a=window.m_document.addComponent(SymbolIds::Resistor,{});
        const auto b=window.m_document.addComponent(SymbolIds::OpAmpGrid10,{100,0},Rotation::Deg90);
        QVERIFY(window.m_document.setComponentLabels(a,"R1","10k"));
        QVERIFY(window.m_document.editProperties({a},{{PropertyEdit::Kind::Add,"tolerance","Tolerance","±5%",true}}));
        QVERIFY(window.m_document.setPropertyBlockOffset({a},QPointF(25,-32)));
        window.m_scene->restoreAfterDocumentChange({{a,b},{}});
        auto *reset=window.m_propertiesPanel->findChild<QPushButton *>("resetPropertyPositionButton");
        QTRY_VERIFY(reset->isEnabled()); QTest::mouseClick(reset,Qt::LeftButton);
        QVERIFY(!window.m_document.component(a)->propertyBlockOffset());
        QTRY_VERIFY(!reset->isEnabled()); window.m_editController->undoStack()->undo();
        QVERIFY(window.m_document.component(a)->propertyBlockOffset());
        QTRY_VERIFY(reset->isEnabled());
        QVERIFY(window.grab().save(QCoreApplication::applicationDirPath()+"/property-block-dock.png"));
        window.addDockWidget(Qt::RightDockWidgetArea,window.m_propertiesDock);
        window.saveWindowLayout();
    }
    MainWindow restored; restored.show();
    QVERIFY(QTest::qWaitForWindowActive(&restored));
    QCOMPARE(restored.dockWidgetArea(restored.m_propertiesDock),Qt::RightDockWidgetArea);
}

void DocumentOpenTests::propertyDockLayoutAndGroundVisibility()
{
    QTemporaryDir temporary;
    QStandardPaths::setTestModeEnabled(true);
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,temporary.path());
    { MainWindow first; first.m_propertiesDock->hide(); first.saveWindowLayout(); }
    MainWindow window; window.show();
    QVERIFY(QTest::qWaitForWindowActive(&window));
    QVERIFY(window.m_propertiesDock->isHidden());
    const auto id = window.m_document.addComponent(SymbolIds::Ground,{});
    window.m_scene->restoreAfterDocumentChange({{id},{}});
    window.onRenameSelectedComponent();
    auto *panel = window.m_propertiesPanel;
    QTRY_VERIFY(panel->findChild<QCheckBox *>("propertyVisible_reference"));
    QVERIFY(!window.m_propertiesDock->isHidden());
    auto *check = panel->findChild<QCheckBox *>("propertyVisible_reference");
    QCOMPARE(check->checkState(),Qt::Unchecked);
    QTest::mouseClick(check,Qt::LeftButton);
    QVERIFY(window.m_document.component(id)->property("reference")->visible);
    const auto other = window.m_document.addComponent(SymbolIds::Resistor,{100,0});
    const auto wire = window.m_document.addWire(makeTerminalEndpoint(id,0),{{0,0}},makeTerminalEndpoint(other,0));
    QVERIFY(wire);
    window.m_scene->restoreAfterDocumentChange({{}, {*wire}});
    QTRY_COMPARE(panel->findChild<QTableWidget *>("propertiesTable")->rowCount(),0);
    QVERIFY(!panel->findChild<QPushButton *>("addPropertyButton")->isEnabled());
    window.m_scene->clearSelection();
    QApplication::processEvents();
    QVERIFY(window.writeDocumentTo(temporary.filePath("properties.schematic")));
    QFile saved(temporary.filePath("properties.schematic")); QVERIFY(saved.open(QIODevice::ReadOnly));
    const auto loaded = Document::fromJsonBytes(saved.readAll()); QVERIFY(loaded.document);
    QVERIFY(loaded.document->component(id)->property("reference")->visible);
}

void DocumentOpenTests::propertyInspectorMixedEditsAndCustomProperties()
{
    QTemporaryDir temporary;
    QStandardPaths::setTestModeEnabled(true);
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,temporary.path());
    MainWindow window; window.resize(1400,850); window.show();
    QVERIFY(QTest::qWaitForWindowActive(&window));
    const auto a = window.m_document.addComponent(SymbolIds::Resistor, {});
    const auto b = window.m_document.addComponent(SymbolIds::Capacitor, {100,0});
    QVERIFY(window.m_document.editProperties({a}, {{PropertyEdit::Kind::SetValue,"value",{},"10k"},
        {PropertyEdit::Kind::Add,"tolerance","Tolerance","1%",true}}));
    QVERIFY(window.m_document.editProperties({b}, {{PropertyEdit::Kind::SetValue,"value",{},"22k"},
        {PropertyEdit::Kind::SetVisible,"value",{},{},false}}));
    window.m_scene->restoreAfterDocumentChange({{a,b},{}});
    auto *panel = window.m_propertiesPanel;
    auto value = [&]() { return panel->findChild<QLineEdit *>("propertyValue_value"); };
    QTRY_VERIFY(value());
    QCOMPARE(value()->placeholderText(),QString("[Multiple values]"));
    QVERIFY(value()->text().isEmpty());
    QVERIFY(panel->findChild<QLineEdit *>("propertyValue_reference")->isReadOnly());
    auto visible = [&]() { return panel->findChild<QCheckBox *>("propertyVisible_value"); };
    QCOMPARE(visible()->checkState(),Qt::PartiallyChecked);
    const auto before = window.m_document.toJsonBytes();
    value()->setFocus(); QTest::keyClicks(value(),"47k"); QTest::keyClick(value(),Qt::Key_Return);
    QApplication::processEvents();
    QTRY_COMPARE(window.m_document.component(a)->value(),QString("47k"));
    QCOMPARE(window.m_document.component(b)->value(),QString("47k"));
    QVERIFY(window.m_document.component(a)->property("value")->visible);
    QVERIFY(!window.m_document.component(b)->property("value")->visible);
    QTRY_COMPARE(value()->text(),QString("47k"));
    QTest::mouseClick(visible(),Qt::LeftButton);
    QTRY_VERIFY(window.m_document.component(b)->property("value")->visible);
    QCOMPARE(window.m_editController->undoStack()->count(),2);
    window.m_editController->undoStack()->undo(); window.m_editController->undoStack()->undo();
    QCOMPARE(window.m_document.toJsonBytes(),before);
    QVERIFY(window.m_editController->undoStack()->isClean());
    window.m_editController->undoStack()->redo(); window.m_editController->undoStack()->redo();
    QApplication::processEvents();
    QTRY_COMPARE(value()->text(),QString("47k"));
    auto *tolerance = panel->findChild<QLineEdit *>("propertyValue_tolerance");
    QCOMPARE(tolerance->placeholderText(),QString("[Not set on all]"));
    QTest::mouseClick(panel->findChild<QCheckBox *>("propertyVisible_tolerance"),Qt::LeftButton);
    QTRY_VERIFY(window.m_document.component(b)->property("tolerance"));
    QCOMPARE(window.m_document.component(b)->property("tolerance")->value,QString{});
    QCOMPARE(window.m_document.component(a)->property("tolerance")->value,QString("1%"));
    QTRY_COMPARE(panel->findChild<QLineEdit *>("propertyValue_tolerance")->placeholderText(),QString("[Multiple values]"));
    QTimer::singleShot(0, [] {
        auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
        QVERIFY(dialog);
        dialog->findChild<QLineEdit *>("newPropertyName")->setText("Voltage rating");
        dialog->findChild<QLineEdit *>("newPropertyValue")->setText("25V");
        dialog->accept();
    });
    QTest::mouseClick(panel->findChild<QPushButton *>("addPropertyButton"),Qt::LeftButton);
    QTRY_VERIFY(window.m_document.component(a)->property("voltage_rating"));
    QVERIFY(window.m_document.component(b)->property("voltage_rating"));
    QVERIFY(!window.m_document.component(a)->property("voltage_rating")->visible);
    window.m_propertiesDock->hide(); window.onEditComponentLabels(a);
    QTRY_VERIFY(panel->findChild<QLineEdit *>("propertyValue_value")->hasFocus());
    QVERIFY(!window.m_propertiesDock->isHidden());
    window.onRenameSelectedComponent();
    QTRY_VERIFY(panel->findChild<QLineEdit *>("propertyValue_reference")->hasFocus());
    const auto referenceBefore = window.m_document.component(a)->reference();
    auto *reference = panel->findChild<QLineEdit *>("propertyValue_reference");
    reference->selectAll(); QTest::keyClicks(reference,window.m_document.component(b)->reference());
    QTest::keyClick(reference,Qt::Key_Return);
    QCOMPARE(window.m_document.component(a)->reference(),referenceBefore);
    QVERIFY(!panel->findChild<QLabel *>("propertyError")->text().isEmpty());
    QTest::keyClick(reference,Qt::Key_Escape);
    QCOMPARE(reference->text(),referenceBefore);
    auto *table = panel->findChild<QTableWidget *>("propertiesTable");
    int row = -1;
    for (int i=0;i<table->rowCount();++i) if (table->item(i,0)->data(Qt::UserRole) == "voltage_rating") row=i;
    QVERIFY(row>=2); table->setCurrentCell(row,0);
    QTimer::singleShot(0, [] {
        auto *dialog = qobject_cast<QInputDialog *>(QApplication::activeModalWidget());
        QVERIFY(dialog); dialog->setTextValue("Working voltage"); dialog->accept();
    });
    QTest::mouseClick(panel->findChild<QPushButton *>("renamePropertyButton"),Qt::LeftButton);
    QTRY_COMPARE(window.m_document.component(a)->property("voltage_rating")->name,QString("Working voltage"));
    QTRY_COMPARE(panel->findChild<QLineEdit *>("propertyValue_voltage_rating")->text(),QString("25V"));
    table->setCurrentCell(row,0);
    QTest::mouseClick(panel->findChild<QPushButton *>("removePropertyButton"),Qt::LeftButton);
    QTRY_VERIFY(!window.m_document.component(a)->property("voltage_rating"));
    QVERIFY(window.m_document.component(b)->property("voltage_rating"));
    window.m_scene->restoreSelection({{a,b},{}});
    QApplication::processEvents();
    QVERIFY(window.grab().save(QCoreApplication::applicationDirPath() + "/instance-properties-inspector.png"));
}

void DocumentOpenTests::propertyInspectorContextAndKeyboard()
{
    QTemporaryDir temporary;
    QStandardPaths::setTestModeEnabled(true);
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,temporary.path());
    MainWindow window; window.show();
    QVERIFY(QTest::qWaitForWindowActive(&window));
    const auto a = window.m_document.addComponent(SymbolIds::Resistor, {});
    const auto b = window.m_document.addComponent(SymbolIds::Resistor, {100,0});
    window.m_scene->restoreAfterDocumentChange({{a},{}});
    auto editor = [&]() { return window.m_propertiesPanel->findChild<QLineEdit *>("propertyValue_value"); };
    QTRY_VERIFY(editor());
    editor()->setFocus(); QTest::keyClicks(editor(),"pending");
    window.m_scene->restoreSelection({{b},{}});
    QApplication::processEvents();
    QVERIFY(window.m_document.component(a)->value().isEmpty());
    QVERIFY(window.m_document.component(b)->value().isEmpty());
    QTRY_VERIFY(editor()); editor()->setFocus(); QTest::keyClicks(editor(),"10k"); QTest::keyClick(editor(),Qt::Key_Return);
    QApplication::processEvents();
    QTRY_COMPARE(window.m_document.component(b)->value(),QString("10k"));
    QTRY_COMPARE(editor()->text(),QString("10k"));
    editor()->setFocus(); editor()->selectAll(); QTest::keyClicks(editor(),"47k");
    const int history = window.m_editController->undoStack()->index();
    QTest::keyClick(editor(),Qt::Key_Z,Qt::ControlModifier);
    QCOMPARE(window.m_editController->undoStack()->index(),history);
    QCOMPARE(window.m_scene->selection().components,QVector<ComponentId>{b});
    QTest::keyClick(editor(),Qt::Key_A,Qt::ControlModifier);
    QTest::keyClick(editor(),Qt::Key_Delete);
    QVERIFY(window.m_document.component(b));
    window.m_editController->undoStack()->undo();
    QTRY_VERIFY(window.m_document.component(b)->value().isEmpty());
    QTRY_VERIFY(editor());
    editor()->setFocus(); QTest::keyClicks(editor(),"do not carry over");
    window.onNew();
    const auto fresh = window.m_document.addComponent(SymbolIds::Resistor, {});
    window.m_scene->restoreAfterDocumentChange({{fresh},{}});
    QApplication::processEvents();
    QVERIFY(window.m_document.component(fresh)->value().isEmpty());
    QTRY_VERIFY(editor()); editor()->setFocus(); QTest::keyClicks(editor(),"discard on canvas selection");
    window.m_view->centerOn(0,0);
    QTest::mouseClick(window.m_view->viewport(),Qt::LeftButton,Qt::NoModifier,
                      window.m_view->mapFromScene(-70,-70));
    QApplication::processEvents();
    QVERIFY(window.m_document.component(fresh)->value().isEmpty());
}

void DocumentOpenTests::importPreviewPreservesBlackFill()
{
    SymbolPrimitive polygon;
    polygon.kind = PrimitiveKind::Polygon;
    polygon.points = {{-10,-10}, {10,-10}, {10,10}, {-10,10}};
    polygon.fillColor = PrimitiveFillColor::Black;
    SymbolPreviewWidget preview;
    preview.resize(400,320);
    preview.setArtwork({polygon});
    const auto black = preview.grab().toImage();
    QCOMPARE(black.pixelColor(220,180), QColor(Qt::black));
    QVERIFY(black.save(QCoreApplication::applicationDirPath() + "/black-fill-import-preview.png"));
    polygon.fillColor = PrimitiveFillColor::White;
    preview.setArtwork({polygon});
    const auto white = preview.grab().toImage();
    QCOMPARE(white.pixelColor(220,180), QColor(Qt::white));
    QFile svg(QFINDTESTDATA("fixtures/monochrome-fill.svg"));
    QFile sidecar(QFINDTESTDATA("fixtures/monochrome-fill.json"));
    QVERIFY(svg.open(QIODevice::ReadOnly));
    QVERIFY(sidecar.open(QIODevice::ReadOnly));
    const auto fixture = SymbolSvgImporter::importSymbol(sidecar.readAll(), svg.readAll(),
                                                        QFileInfo(svg).absolutePath());
    QVERIFY2(fixture.symbol, qPrintable(fixture.errors.join('\n')));
    preview.resize(800,480);
    preview.setArtwork(fixture.symbol->primitives);
    preview.setTerminals(fixture.symbol->terminals);
    QVERIFY(preview.grab().save(QCoreApplication::applicationDirPath() + "/monochrome-fill-fixture.png"));
}

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
    QVERIFY(window.m_scene->placeComponentAt(SymbolIds::OpAmpGrid10, {100, 100}));
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

void DocumentOpenTests::groupToolbarActionsAndSelectionRestoration()
{
    QTemporaryDir temporary;
    QStandardPaths::setTestModeEnabled(true);
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, temporary.path());
    MainWindow window;
    QVERIFY(window.m_scene->placeComponentAt(SymbolIds::Resistor, {0, 0}));
    const auto left = *window.m_scene->singleSelectedComponent();
    QVERIFY(window.m_scene->placeComponentAt(SymbolIds::Resistor, {100, 0}));
    const auto right = *window.m_scene->singleSelectedComponent();
    const auto wire = window.m_document.addWire(makeTerminalEndpoint(left, 1), {}, makeTerminalEndpoint(right, 0));
    QVERIFY(wire);
    const DocumentSelection selected{{left, right}, {*wire}};
    window.m_scene->restoreAfterDocumentChange(selected);
    QVERIFY(!window.m_renameAction->isEnabled());
    auto *stack = window.m_editController->undoStack();
    for (auto *action : {window.m_rotateLeftAction, window.m_rotateAction,
                         window.m_mirrorAction, window.m_flipVerticalAction}) {
        QVERIFY(action->isEnabled());
        const auto before = window.m_document.toJsonBytes();
        const int index = stack->index();
        action->trigger();
        QCOMPARE(stack->index(), index + 1);
        QVERIFY(window.m_scene->selection() == selected);
        QVERIFY(window.m_document.toJsonBytes() != before);
        stack->undo();
        QCOMPARE(window.m_document.toJsonBytes(), before);
        QVERIFY(window.m_scene->selection() == selected);
    }
    window.m_scene->restoreSelection({{}, {*wire}});
    QVERIFY(window.m_rotateAction->isEnabled());
    QVERIFY(!window.m_renameAction->isEnabled());
    window.m_scene->setEditMode(SchematicScene::EditMode::DrawWire);
    QVERIFY(!window.m_rotateAction->isEnabled());
}

void DocumentOpenTests::paletteReflowsWithAvailableWidth()
{
    QTemporaryDir temporary;
    QStandardPaths::setTestModeEnabled(true);
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, temporary.path());
    MainWindow window;
    window.populateSymbolGrid(QStringLiteral("Passives"));
    QVERIFY(window.m_symbolActions.contains(SymbolIds::ElectrolyticCapacitor));
    QVERIFY(window.m_symbolActions.contains(SymbolIds::OpAmpGrid10));
    QVERIFY(!window.m_categoryItems.contains(QStringLiteral("Compatibility")));
    QVERIFY(!window.m_categoryItems.contains(QStringLiteral("Compatibility/Legacy Symbols")));
    window.resize(1100,750);
    QVERIFY(window.grab().save(QCoreApplication::applicationDirPath() + "/packaged-library-palette.png"));
    auto *flow = window.m_symbolSectionsLayout->itemAt(0)->layout();
    QVERIFY(flow);
    QCOMPARE(flow->count(), 5);
    QVERIFY(flow->hasHeightForWidth());
    QVERIFY(flow->heightForWidth(150) > flow->heightForWidth(320));
    flow->setGeometry(QRect(0, 0, 150, 200));
    QCOMPARE(flow->itemAt(0)->geometry().y(), flow->itemAt(1)->geometry().y());
    QVERIFY(flow->itemAt(2)->geometry().y() > flow->itemAt(0)->geometry().y());
    auto *firstTile = flow->itemAt(0)->widget();
    flow->setGeometry(QRect(0, 0, 240, 200));
    QCOMPARE(flow->itemAt(2)->geometry().y(), flow->itemAt(0)->geometry().y());
    QCOMPARE(flow->itemAt(0)->widget(), firstTile);
    flow->setGeometry(QRect(0, 0, 80, 200));
    QVERIFY(flow->itemAt(1)->geometry().y() > flow->itemAt(0)->geometry().y());
    window.populateSymbolGrid(QStringLiteral("Semiconductors/ICs/Op Amps"));
    QCOMPARE(window.m_symbolSectionsLayout->itemAt(0)->layout()->count(), 4);
    QVERIFY(!window.m_categoryItems.contains(QStringLiteral("Semiconductors/ICs/OTAs")));
    QVERIFY(window.grab().save(QCoreApplication::applicationDirPath() + "/packaged-opamp-palette.png"));
    QVERIFY(window.grab().save(QCoreApplication::applicationDirPath() + "/packaged-ota-palette.png"));
    window.populateSymbolGrid(QStringLiteral("Semiconductors/ICs/Buffers"));
    QCOMPARE(window.m_symbolSectionsLayout->itemAt(0)->layout()->count(), 1);
    QVERIFY(window.grab().save(QCoreApplication::applicationDirPath() + "/packaged-buffer-palette.png"));
    auto *semiconductors = window.m_categoryItems.value(QStringLiteral("Semiconductors"));
    QVERIFY(semiconductors);
    QCOMPARE(semiconductors->childCount(), 3);
    QCOMPARE(semiconductors->child(0)->text(0), QStringLiteral("Diodes"));
    QCOMPARE(semiconductors->child(1)->text(0), QStringLiteral("Transistors"));
    QCOMPARE(semiconductors->child(2)->text(0), QStringLiteral("ICs"));
    QVERIFY(window.m_categoryItems.contains(QStringLiteral("Semiconductors/ICs/Op Amps")));
    window.populateSymbolGrid(QStringLiteral("Semiconductors/Transistors"));
    auto *transistors = window.m_symbolSectionsLayout->itemAt(0)->layout();
    QVERIFY(transistors);
    QCOMPARE(transistors->count(), 6);
    QCOMPARE(window.m_categoryItems.value(QStringLiteral("Semiconductors/Transistors"))->childCount(), 0);
    QVERIFY(window.grab().save(QCoreApplication::applicationDirPath() + "/packaged-transistors-palette.png"));
    window.populateSymbolGrid(QStringLiteral("Semiconductors/Diodes"));
    auto *diodes = window.m_symbolSectionsLayout->itemAt(0)->layout();
    QVERIFY(diodes);
    QCOMPARE(diodes->count(), 4);
    QVERIFY(window.grab().save(QCoreApplication::applicationDirPath() + "/packaged-diodes-palette.png"));
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
    embedded.primitives.first().fillColor = PrimitiveFillColor::Black;
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
    QCOMPARE(window.m_document.symbol(embedded.id)->primitives.first().fillColor, PrimitiveFillColor::Black);
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

void DocumentOpenTests::textNotesPlacementPropertiesClipboard()
{
    SavedClipboard clipboard;
    QTemporaryDir directory; QVERIFY(directory.isValid());
    QStandardPaths::setTestModeEnabled(true);
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,directory.path());
    MainWindow window; window.show(); QVERIFY(QTest::qWaitForWindowActive(&window));
    auto *action = window.findChild<QAction *>("addTextNoteAction"); QVERIFY(action);
    QVERIFY(window.findChild<QToolBar *>("toolsToolBar")->actions().contains(action));
    action->trigger(); QCOMPARE(window.m_scene->editMode(),SchematicScene::EditMode::PlaceNote);
    QTest::mouseClick(window.m_view->viewport(),Qt::LeftButton,Qt::NoModifier,window.m_view->mapFromScene(13,17));
    QCOMPARE(window.m_document.notes().size(),1);
    const auto note = window.m_document.notes().first().id;
    QCOMPARE(window.m_document.note(note)->position,QPointF(10,20));
    QCOMPARE(window.m_scene->editMode(),SchematicScene::EditMode::Select);
    QCOMPARE(window.m_scene->selection().notes,QVector<NoteId>{note});
    auto *editor = window.findChild<QPlainTextEdit *>("noteTextEditor"); QVERIFY(editor);
    QTRY_VERIFY(editor->hasFocus());
    QTest::keyClicks(editor,"First paragraph"); QTest::keyClick(editor,Qt::Key_Return);
    QTest::keyClicks(editor,"Second paragraph");
    QCOMPARE(window.m_document.note(note)->text,QString("Note")); // still a staged text edit
    QTest::keyClick(editor,Qt::Key_Return,Qt::ControlModifier);
    QTRY_COMPARE(window.m_document.note(note)->text,QString("First paragraph\nSecond paragraph"));
    QCOMPARE(window.m_editController->undoStack()->count(),2);
    QTRY_VERIFY(editor->toPlainText() == window.m_document.note(note)->text);
    QApplication::processEvents(); // Finish the deferred inspector rebuild after the commit.
    editor->selectAll(); QTest::keyClicks(editor,"discard this");
    QTest::keyClick(editor,Qt::Key_Escape);
    QCOMPARE(editor->toPlainText(),window.m_document.note(note)->text);
    QCOMPARE(window.m_editController->undoStack()->count(),2);
    editor->selectAll();
    window.m_copyAction->trigger();
    QCOMPARE(QApplication::clipboard()->text(),window.m_document.note(note)->text);
    QApplication::clipboard()->setText("Local paste\ninside the note editor");
    window.m_pasteAction->trigger();
    QCOMPARE(editor->toPlainText(),QString("Local paste\ninside the note editor"));
    QCOMPARE(window.m_document.note(note)->text,QString("First paragraph\nSecond paragraph"));
    window.m_undoAction->trigger();
    QCOMPARE(editor->toPlainText(),window.m_document.note(note)->text);
    QCOMPARE(window.m_editController->undoStack()->count(),2);
    auto *width = window.findChild<QDoubleSpinBox *>("noteWrapWidth"); QVERIFY(width);
    width->setValue(90);
    QMetaObject::invokeMethod(width,"editingFinished",Qt::DirectConnection);
    QTRY_COMPARE(window.m_document.note(note)->width,90.0);
    window.m_view->setFocus(); QTRY_VERIFY(window.m_view->hasFocus());
    const auto before = window.m_document.toJsonBytes();
    window.m_copyAction->trigger();
    QVERIFY(QApplication::clipboard()->mimeData()->hasFormat("application/x-schematic-fragment+json"));
    window.m_pasteAction->trigger();
    QCOMPARE(window.m_document.notes().size(),2);
    const auto pasted = window.m_scene->selection().notes;
    QCOMPARE(pasted.size(),1); QVERIFY(pasted.first() != note);
    QCOMPARE(window.m_document.note(pasted.first())->text,window.m_document.note(note)->text);
    QCOMPARE(window.m_document.note(pasted.first())->width,90.0);
    window.m_deleteAction->trigger(); QCOMPARE(window.m_document.notes().size(),1);
    window.m_editController->undoStack()->undo(); QCOMPARE(window.m_document.notes().size(),2);
    window.m_editController->undoStack()->undo(); QCOMPARE(window.m_document.toJsonBytes(),before);
    QVERIFY(window.m_svgExportAction->isEnabled());
    QVERIFY(window.exportSvgTo(directory.filePath("notes.svg")));
    QVERIFY(QFile::exists(directory.filePath("notes.svg")));
    const auto original = *window.m_document.note(note);
    QVERIFY(window.m_editController->commit("Precise note data", [&] {
        return window.m_document.setNote(note,original.position,"Original\r\nline endings",90.125);
    }));
    QApplication::processEvents();
    window.m_propertiesPanel->focusNote();
    QTRY_VERIFY(editor->hasFocus());
    const auto precise = window.m_document.toJsonBytes();
    const auto history = window.m_editController->undoStack()->index();
    window.m_view->setFocus();
    QApplication::processEvents();
    QCOMPARE(window.m_document.toJsonBytes(),precise);
    QCOMPARE(window.m_editController->undoStack()->index(),history);
}

QTEST_MAIN(DocumentOpenTests)
#include "tst_documentopen.moc"

void DocumentOpenTests::drawingStyleControlUndoAndLoad()
{
    MainWindow window;
    auto *combo=window.findChild<QComboBox *>("drawingStyleCombo"); QVERIFY(combo);
    QCOMPARE(combo->currentIndex(),0);
    combo->setCurrentIndex(1);
    QCOMPARE(window.m_document.drawingStyle(),DrawingStyle::HandDrawn);
    window.m_editController->undoStack()->undo(); QCOMPARE(combo->currentIndex(),0);
    window.m_editController->undoStack()->redo(); QCOMPARE(combo->currentIndex(),1);
    window.m_document.setDrawingStyle(DrawingStyle::Standard);
    window.syncGridControls(); QCOMPARE(combo->currentIndex(),0);
    QTemporaryDir directory; QVERIFY(directory.isValid());
    Document handDrawn; handDrawn.setDrawingStyle(DrawingStyle::HandDrawn);
    QFile file(directory.filePath("style.schematic")); QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(handDrawn.toJsonBytes()); file.close();
    window.m_editController->resetHistory();
    QVERIFY(window.openDocumentFrom(file.fileName())); QCOMPARE(combo->currentIndex(),1);
    window.onNew(); QCOMPARE(combo->currentIndex(),0);
    window.m_editController->resetHistory();
}

void DocumentOpenTests::drawingOptionsControls()
{
    MainWindow window;
    QVERIFY(!window.findChild<QComboBox *>("drawingVariationCombo"));
    QVERIFY(!window.findChild<QComboBox *>("drawingStrokesCombo"));
    auto *style=window.findChild<QComboBox *>("drawingStyleCombo"); QVERIFY(style);
    style->setCurrentIndex(1);
    const auto before=window.m_document.toJsonBytes();
    auto changeOptions=[&](bool accept) {
        bool found=false;
        QTimer::singleShot(0,&window,[&] {
            auto *dialog=qobject_cast<PreferencesDialog *>(QApplication::activeModalWidget());
            if(!dialog) return;
            auto *tabs=dialog->findChild<QTabWidget *>(); tabs->setCurrentIndex(1);
            auto *variation=dialog->findChild<QComboBox *>("drawingVariationCombo");
            auto *strokes=dialog->findChild<QComboBox *>("drawingStrokesCombo");
            auto *looseness=dialog->findChild<QDoubleSpinBox *>("drawingLooseness");
            found=variation && strokes && looseness && variation->isEnabled();
            if(found) {
                variation->setCurrentIndex(1); strokes->setCurrentIndex(1); looseness->setValue(200);
                found = dialog->grab().save(QCoreApplication::applicationDirPath()+"/hand-drawn-preferences.png");
            }
            auto *buttons=dialog->findChild<QDialogButtonBox *>();
            buttons->button(accept ? QDialogButtonBox::Ok : QDialogButtonBox::Cancel)->click();
        });
        window.onPreferences(); QVERIFY(found);
    };
    changeOptions(false); QCOMPARE(window.m_document.toJsonBytes(),before);
    changeOptions(true);
    QCOMPARE(window.m_document.drawingVariation(),DrawingVariation::Consistent);
    QCOMPARE(window.m_document.drawingStrokes(),2); QCOMPARE(window.m_document.drawingLooseness(),2.0);
    window.m_editController->undoStack()->undo(); QCOMPARE(window.m_document.toJsonBytes(),before);
    window.m_editController->undoStack()->redo(); QCOMPARE(window.m_document.drawingLooseness(),2.0);
    PreferencesDialog reopened; reopened.setDrawingOptions(window.m_document);
    QCOMPARE(reopened.drawingLooseness(),2.0); QCOMPARE(reopened.drawingStrokes(),2);
    window.m_document.setDrawingLooseness(1.234567); reopened.setDrawingOptions(window.m_document);
    QCOMPARE(reopened.drawingLooseness(),1.234567);
    window.m_document.setDrawingStyle(DrawingStyle::Standard); reopened.setDrawingOptions(window.m_document);
    QVERIFY(!reopened.findChild<QComboBox *>("drawingVariationCombo")->isEnabled());
    window.m_editController->resetHistory();
}

void DocumentOpenTests::globalFontPreferences()
{
    const auto oldOrg=QCoreApplication::organizationName(), oldApp=QCoreApplication::applicationName();
    const auto oldFormat=QSettings::defaultFormat();
    QTemporaryDir settingsDirectory; QVERIFY(settingsDirectory.isValid());
    QCoreApplication::setOrganizationName("SchematicFontTests"); QCoreApplication::setApplicationName("Fonts");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settingsDirectory.path());
    const auto restoreSettingsContext=qScopeGuard([&] {
        QSettings::setDefaultFormat(oldFormat);
        QCoreApplication::setOrganizationName(oldOrg); QCoreApplication::setApplicationName(oldApp);
    });
    QSettings settings;
    const QStringList keys={"appearance/schematicFontFamily","appearance/labelFontSize","appearance/noteFontSize"};
    QVariantList saved; for(const auto &key:keys) { saved.append(settings.value(key)); settings.remove(key); }
    const auto cleanup=qScopeGuard([&] {
        for(int i=0;i<keys.size();++i) { if(saved[i].isValid()) settings.setValue(keys[i],saved[i]); else settings.remove(keys[i]); }
    });
    MainWindow window, other;
    const auto id=window.m_document.addComponent(SymbolIds::Resistor,{});
    window.m_document.addNote({0,40},"A wrapped schematic note",100);
    window.m_scene->restoreAfterDocumentChange({{id},{}});
    other.m_document.addComponent(SymbolIds::Capacitor,{}); other.m_scene->restoreAfterDocumentChange({});
    const auto before=window.m_document.toJsonBytes();
    const int history=window.m_editController->undoStack()->index();
    QString chosen;
    const auto change=[&](bool accept) {
        bool found=false;
        QTimer::singleShot(0,&window,[&] {
            auto *dialog=qobject_cast<PreferencesDialog *>(QApplication::activeModalWidget()); if(!dialog)return;
            dialog->findChild<QTabWidget *>()->setCurrentIndex(1);
            auto *family=dialog->findChild<QFontComboBox *>("schematicFontFamily");
            auto *labels=dialog->findChild<QSpinBox *>("labelFontSize");
            auto *notes=dialog->findChild<QSpinBox *>("noteFontSize");
            found=family && labels && notes;
            if(found) {
                family->setCurrentFont(QFont("Courier New")); chosen=family->currentFont().family();
                labels->setValue(12); notes->setValue(15);
                found=dialog->grab().save(QCoreApplication::applicationDirPath()+"/font-preferences.png");
            }
            dialog->findChild<QDialogButtonBox *>()->button(accept?QDialogButtonBox::Ok:QDialogButtonBox::Cancel)->click();
        });
        window.onPreferences(); QVERIFY(found);
    };
    change(false); QCOMPARE(SchematicFont::labels().pixelSize(),8); QCOMPARE(SchematicFont::notes().pixelSize(),10);
    change(true); QCOMPARE(SchematicFont::labels().family(),chosen);
    QCOMPARE(SchematicFont::labels().pixelSize(),12); QCOMPARE(SchematicFont::notes().pixelSize(),15);
    for(auto *current : {&window,&other}) {
        bool label=false;
        for(auto *item:current->m_scene->items()) {
            if(auto *text=qgraphicsitem_cast<ComponentLabelItem *>(item)) {
                QCOMPARE(text->font(),SchematicFont::labels()); label=true;
            }
            if(auto *note=qgraphicsitem_cast<NoteItem *>(item)) QCOMPARE(note->font(),SchematicFont::notes());
        }
        QVERIFY(label);
    }
    QCOMPARE(window.m_document.toJsonBytes(),before);
    QCOMPARE(window.m_editController->undoStack()->index(),history);
    QCOMPARE(window.m_scene->selection().components,QVector<ComponentId>{id});
    PreferencesDialog reopened;
    QCOMPARE(reopened.findChild<QSpinBox *>("labelFontSize")->value(),12);
    QCOMPARE(reopened.findChild<QSpinBox *>("noteFontSize")->value(),15);
    for(const auto &value : {QVariant("bad"),QVariant(0),QVariant(73)}) {
        settings.setValue("appearance/labelFontSize",value); QCOMPARE(SchematicFont::labels().pixelSize(),8);
    }
    window.m_editController->resetHistory(); other.m_editController->resetHistory();
}
