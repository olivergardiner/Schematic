#include "../mainwindow.h"
#include "documentloadresult.h"
#include "../symbolpreviewwidget.h"
#include "symbolsvg.h"
#include "../propertiespanel.h"
#include "../schematicview.h"
#include <QApplication>
#include <QClipboard>
#include <QMimeData>
#include <QMenu>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QTabWidget>
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
#include <QLineEdit>
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
    void preferencesDialogLifecycle();
    void clipboardMenuShortcutsAndUndo();
    void clipboardMenuRespectsTextEditing();
    void propertyBlockDockDefaultAndReset();
    void propertyDockLayoutAndGroundVisibility();
    void propertyInspectorMixedEditsAndCustomProperties();
    void propertyInspectorContextAndKeyboard();
    void importPreviewPreservesBlackFill();
    void transformToolbarActionsAreUndoable();
    void groupToolbarActionsAndSelectionRestoration();
    void paletteReflowsWithAvailableWidth();
    void categoryCollapseReleasesPaletteObjects();
    void categoryAccessibilityMatchesCollapsedRows();
    void reviewChoicesAndAtomicOpen();
    void missingAndIncompatibleChoices();
};

namespace {
class SavedClipboard {
    QMimeData *saved = new QMimeData;
public:
    SavedClipboard() { if (const auto *mime=QApplication::clipboard()->mimeData())
        for(const auto &format:mime->formats()) saved->setData(format,mime->data(format)); }
    ~SavedClipboard() { QApplication::clipboard()->setMimeData(saved); }
};
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
    for(int closeMode=0;closeMode<3;++closeMode) {
        bool opened=false, valid=false, captured=true;
        QTimer::singleShot(0,&window,[&] {
            auto *dialog=qobject_cast<QDialog *>(QApplication::activeModalWidget());
            if(!dialog)return;
            opened=dialog->objectName()=="preferencesDialog";
            auto *tabs=dialog->findChild<QTabWidget *>("preferencesTabs");
            auto *buttons=dialog->findChild<QDialogButtonBox *>();
            valid=tabs && tabs->count()==1 && tabs->tabText(0)=="General" && tabs->currentIndex()==0
                && tabs->widget(0)->layout() && tabs->widget(0)->layout()->count()==0
                && buttons && buttons->standardButtons()==QDialogButtonBox::Close;
            dialog->resize(640,400);
            if(closeMode==0) {
                captured=dialog->grab().save(QCoreApplication::applicationDirPath()+"/preferences-dialog.png");
                if(buttons && buttons->button(QDialogButtonBox::Close)) buttons->button(QDialogButtonBox::Close)->click();
                else dialog->reject();
            } else if(closeMode==1) QTest::keyClick(dialog,Qt::Key_Escape);
            else dialog->close();
        });
        if(closeMode==1) QTest::keyClick(window.m_view,Qt::Key_Comma,Qt::ControlModifier);
        else action->trigger();
        QVERIFY2(opened,qPrintable(QString("Close mode %1").arg(closeMode))); QVERIFY(valid); QVERIFY(captured);
        QVERIFY(!QApplication::activeModalWidget());
        QCOMPARE(window.m_document.toJsonBytes(),before);
        QCOMPARE(window.m_scene->selection().components,selection.components);
        QCOMPARE(window.m_editController->undoStack()->index(),history);
        QCOMPARE(window.m_editController->undoStack()->isClean(),clean);
        window.activateWindow(); window.m_view->setFocus();
        QVERIFY(QTest::qWaitForWindowActive(&window));
        QApplication::processEvents();
    }
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
    QTest::keyClick(window.m_view,Qt::Key_V,Qt::ControlModifier);
    QTRY_COMPARE(window.m_document.components().size(),4); QCOMPARE(window.m_document.wires().size(),2);
    QCOMPARE(window.m_editController->undoStack()->count(),1);
    const auto first=window.m_scene->selection(); QCOMPARE(first.components.size(),2); QCOMPARE(first.wires.size(),1);
    QCOMPARE(window.m_document.component(first.components[0])->position(),QPointF(10,10));
    window.m_pasteAction->trigger(); QCOMPARE(window.m_document.components().size(),6);
    const auto second=window.m_scene->selection();
    QCOMPARE(window.m_document.component(second.components[0])->position(),QPointF(20,20));
    window.m_editController->undoStack()->undo(); QCOMPARE(window.m_scene->selection().components,first.components);
    window.m_editController->undoStack()->undo(); QCOMPARE(window.m_document.toJsonBytes(),before);
    QCOMPARE(window.m_scene->selection().components,selected.components);
    window.m_editController->undoStack()->redo(); QCOMPARE(window.m_scene->selection().components,first.components);
    window.m_editController->undoStack()->undo();
    // Cut copies only selected objects; existing incident-route deletion still applies.
    window.m_scene->restoreSelection({{a},{}}); window.m_cutAction->trigger();
    QCOMPARE(window.m_document.components().size(),1); QVERIFY(window.m_document.wires().isEmpty());
    QCOMPARE(window.m_editController->undoStack()->command(0)->text(),QString("Cut selection"));
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
    QVERIFY(window.m_categoryItems.contains(QStringLiteral("Compatibility/Legacy Symbols")));
    window.resize(1100,750);
    QVERIFY(window.grab().save(QCoreApplication::applicationDirPath() + "/packaged-library-palette.png"));
    auto *flow = window.m_symbolSectionsLayout->itemAt(0)->layout();
    QVERIFY(flow);
    QCOMPARE(flow->count(), 4);
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
    window.populateSymbolGrid(QStringLiteral("Semiconductors/ICs"));
    QVERIFY(window.grab().save(QCoreApplication::applicationDirPath() + "/packaged-opamp-palette.png"));
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

QTEST_MAIN(DocumentOpenTests)
#include "tst_documentopen.moc"
