#include "../schematicscene.h"
#include "../schematicview.h"
#include "../schematicitems.h"
#include "../documenteditcontroller.h"

#include <QtTest>
#include <QMouseEvent>
#include <QScrollBar>
#include <QWheelEvent>
#include <QFocusEvent>
#include <QPainter>
#include <cmath>

class SelectionView : public SchematicView
{
public:
    void press(QPoint p, Qt::KeyboardModifiers modifiers = Qt::NoModifier,
               Qt::MouseButton button = Qt::LeftButton)
    {
        QMouseEvent event(QEvent::MouseButtonPress, p, p, button, button, modifiers);
        mousePressEvent(&event);
    }
    void move(QPoint p, Qt::MouseButtons buttons = Qt::LeftButton)
    {
        QMouseEvent event(QEvent::MouseMove, p, p, Qt::NoButton, buttons,
                          Qt::NoModifier);
        mouseMoveEvent(&event);
    }
    void release(QPoint p, Qt::MouseButton button = Qt::LeftButton)
    {
        QMouseEvent event(QEvent::MouseButtonRelease, p, p, button, Qt::NoButton,
                          Qt::NoModifier);
        mouseReleaseEvent(&event);
    }
    using SchematicView::keyPressEvent;
    using SchematicView::wheelEvent;
    using SchematicView::focusOutEvent;
};

class GroupSelectionTests : public QObject
{
    Q_OBJECT
private slots:
    void noteResizeMoveCancelAndUndo();
    void pastePositionTracksViewportThroughTransforms();
    void doubleClickFinishesAtFreePoint_data();
    void doubleClickFinishesAtFreePoint();
    void doubleClickOnTargetDoesNotStartAnotherWire();
    void doubleClickTerminalStartsWire();
    void doubleClickJunctionStartsWireAndEscapeCancels();
    void wireCompletionAfterSnappingOntoTerminal();
    void branchPreviewAndCommitSnapToTargetWire();
    void wireSegmentDragPreviewUndoAndCancel();
    void wireSegmentDragRejectsInvalidRelease();
    void propertyBlockDragResetAndCancel();
    void propertyJustificationAnchors();
    void propertyLabelsFollowVisibilityAndTransforms();
    void canvasFillLayeringAndTransforms();
    void selectedComponentsRenderRedWithoutRectangles();
    void componentLabelsAndTransforms();
    void routeShapesAndIdentity();
    void directionalGesture_data();
    void directionalGesture();
    void cancellationAndTools();
    void mixedDeletionAndSingleMove();
    void viewportEventsPanAndZoom();
    void groupPreviewCommitUndoAndPivot();
    void moveCancellationAndInvalidRelease();
    void wireOnlyTransformsAndModifierClicks();
};

void GroupSelectionTests::pastePositionTracksViewportThroughTransforms()
{
    SchematicScene scene;
    SelectionView view;
    view.setScene(&scene);
    view.resize(800,600); view.show();
    QVERIFY(QTest::qWaitForWindowActive(&view));
    QCOMPARE(view.pastePosition(), view.mapToScene(view.viewport()->rect().center()));
    const QPoint pointer(230,180);
    view.move(pointer, Qt::NoButton);
    QCOMPARE(view.pastePosition(), view.mapToScene(pointer));
    view.centerOn(200,100);
    view.zoomIn();
    QCOMPARE(view.pastePosition(), view.mapToScene(pointer));
    view.press({300,200}); view.release({300,200});
    QCOMPARE(view.pastePosition(), view.mapToScene(QPoint(300,200)));
}

void GroupSelectionTests::selectedComponentsRenderRedWithoutRectangles()
{
    Document document;
    const auto resistor = document.addComponent(SymbolIds::Resistor, {});
    const auto capacitor = document.addComponent(SymbolIds::Capacitor, {80,0});
    const auto wire = document.addWire(makeTerminalEndpoint(resistor,1), {}, makeTerminalEndpoint(capacitor,0));
    QVERIFY(wire);
    const auto saved = document.toJsonBytes();
    SchematicScene scene;
    scene.bindDocument(&document);
    const QRectF source(-30,-30,140,80);
    const auto render = [&]() {
        QImage image(560,320,QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::white);
        QPainter painter(&image);
        scene.render(&painter, QRectF(0,0,560,320), source);
        return image;
    };
    const auto pixel = [&](const QImage &image, QPointF point) {
        return image.pixelColor(qRound((point.x() - source.left()) * 4),
                                qRound((point.y() - source.top()) * 4));
    };
    const auto normal = render();
    scene.restoreSelection({{}, {*wire}});
    const auto selectedWire = render();
    QCOMPARE(pixel(selectedWire, {40,0}), QColor(Qt::red));
    QCOMPARE(pixel(selectedWire, {40,-3}), QColor(Qt::white)); // no selection rectangle
    scene.restoreSelection({{resistor,capacitor},{}});
    const auto selected = render();
    QCOMPARE(pixel(selected, {-15,0}), QColor(Qt::red)); // lead
    QCOMPARE(pixel(selected, {0,-5}), QColor(Qt::red)); // body outline
    QCOMPARE(pixel(selected, {0,0}), QColor(Qt::white)); // white interior
    QCOMPARE(pixel(selected, {76.5,0}), QColor(Qt::red)); // solid capacitor plate
    QCOMPARE(pixel(selected, {-15,-5}), QColor(Qt::white)); // no selection rectangle
    QCOMPARE(pixel(selected, {40,0}), QColor(Qt::black)); // attached wire unchanged
    for (auto *item : scene.items())
        if (auto *label = qgraphicsitem_cast<ComponentLabelItem *>(item))
            QCOMPARE(label->brush().color(), QColor(Qt::red));
    QVERIFY(selected.save(QCoreApplication::applicationDirPath() + "/red-component-selection.png"));
    scene.clearSelection();
    QCOMPARE(render(), normal);
    scene.restoreSelection({{resistor},{}});
    for (auto *item : scene.items())
        if (auto *label = qgraphicsitem_cast<ComponentLabelItem *>(item))
            QCOMPARE(label->brush().color(), QColor(label->componentId() == resistor ? Qt::red : Qt::black));
    scene.restoreAfterDocumentChange({{resistor,capacitor},{}});
    QCOMPARE(render(), selected); // restored selection after scene rebuild
    QCOMPARE(document.toJsonBytes(), saved);
}

static void connectEdits(SchematicScene &scene, DocumentEditController &edits)
{
    scene.setEditCommitter([&edits](const QString &description, const std::function<bool()> &operation,
                                    std::function<DocumentSelection()> after) {
        return edits.commit(description, operation, std::move(after));
    });
    edits.setSelectionCallbacks([&scene]() { return scene.selection(); },
        [&scene](const DocumentSelection &selection) { scene.restoreAfterDocumentChange(selection); });
}

void GroupSelectionTests::noteResizeMoveCancelAndUndo()
{
    Document document;
    const auto note = document.addNote({}, "A fairly long note with several words to wrap across multiple lines.\nA second paragraph.", 160);
    QVERIFY(note);
    SchematicScene scene; scene.bindDocument(&document);
    DocumentEditController edits(&document); connectEdits(scene,edits);
    SelectionView view; view.setScene(&scene); view.resize(800,600); view.show();
    QVERIFY(QTest::qWaitForWindowActive(&view)); view.centerOn(70,30);
    scene.restoreSelection({{},{},{note}});
    const auto item = [&]() -> NoteItem * {
        for (auto *entry : scene.items()) if (auto *noteItem = qgraphicsitem_cast<NoteItem *>(entry)) return noteItem;
        return nullptr;
    };
    QVERIFY(item());
    const qreal initialHeight = item()->boundingRect().height();
    QPoint start = view.mapFromScene(item()->mapToScene({158,item()->boundingRect().center().y()}));
    view.press(start); view.move(start-QPoint(50,0));
    QCOMPARE(document.note(note)->width,160.0); // Preview never mutates the document.
    QCOMPARE(item()->textWidth(),110.0);
    view.release(start-QPoint(50,0));
    QCOMPARE(document.note(note)->width,110.0);
    QVERIFY(item()->boundingRect().height() > initialHeight);
    QCOMPARE(edits.undoStack()->count(),1);
    start = view.mapFromScene(item()->mapToScene({108,item()->boundingRect().center().y()}));
    view.press(start); view.move(start+QPoint(50,0));
    QKeyEvent escape(QEvent::KeyPress,Qt::Key_Escape,Qt::NoModifier); view.keyPressEvent(&escape);
    view.release(start+QPoint(50,0));
    QCOMPARE(document.note(note)->width,110.0);
    QCOMPARE(edits.undoStack()->count(),1);
    start = view.mapFromScene(item()->mapToScene({1,item()->boundingRect().center().y()}));
    view.press(start); view.move(start+QPoint(20,0)); view.release(start+QPoint(20,0));
    QCOMPARE(document.note(note)->width,90.0);
    QCOMPARE(document.note(note)->position,QPointF(20,0)); // Opposite edge stays at x=110.
    start = view.mapFromScene(item()->mapToScene({40,3}));
    view.press(start); view.move(start+QPoint(20,30)); view.release(start+QPoint(20,30));
    QCOMPARE(document.note(note)->position,QPointF(40,30));
    edits.undoStack()->undo(); QCOMPARE(document.note(note)->position,QPointF(20,0));
    edits.undoStack()->undo(); QCOMPARE(document.note(note)->width,110.0);
    edits.undoStack()->undo(); QCOMPARE(document.note(note)->width,160.0);
    scene.selectInBox(item()->sceneBoundingRect().adjusted(-1,-1,1,1),false,{});
    QCOMPARE(scene.selection().notes,QVector<NoteId>{note});
    const auto component = document.addComponent(SymbolIds::Resistor,{-100,0});
    scene.restoreAfterDocumentChange({{component},{},{note}});
    start = view.mapFromScene(80,3);
    view.press(start); view.move(start+QPoint(20,20)); view.release(start+QPoint(20,20));
    QCOMPARE(document.note(note)->position,QPointF(20,20));
    QCOMPARE(document.component(component)->position(),QPointF(-80,20));
    QCOMPARE(item()->rotation(),0.0);
}

void GroupSelectionTests::doubleClickFinishesAtFreePoint_data()
{
    QTest::addColumn<bool>("firstClick");
    QTest::newRow("full-double-click-sequence") << true;
    QTest::newRow("direct-double-click-event") << false;
}

void GroupSelectionTests::doubleClickFinishesAtFreePoint()
{
    QFETCH(bool, firstClick);
    Document document;
    const auto a = document.addComponent(SymbolIds::Resistor, {});
    SchematicScene scene;
    scene.bindDocument(&document);
    DocumentEditController edits(&document);
    connectEdits(scene, edits);
    SelectionView view;
    view.setScene(&scene);
    view.resize(800,600); view.show();
    QVERIFY(QTest::qWaitForWindowActive(&view));
    view.centerOn(50,20);
    scene.setEditMode(SchematicScene::EditMode::DrawWire);
    const auto before = document.toJsonBytes();
    QTest::mouseClick(view.viewport(), Qt::LeftButton, Qt::NoModifier, view.mapFromScene(20,0));
    const QPoint end = view.mapFromScene(62,43);
    if (firstClick) QTest::mouseClick(view.viewport(), Qt::LeftButton, Qt::NoModifier, end);
    QTest::mouseDClick(view.viewport(), Qt::LeftButton, Qt::NoModifier, end);
    QCOMPARE(scene.editMode(), SchematicScene::EditMode::DrawWire);
    QCOMPARE(document.wires().size(), 1);
    QCOMPARE(document.nodes().size(), 1);
    QVERIFY(document.wires().first().start == makeTerminalEndpoint(a,1));
    QVERIFY(isNode(document.wires().first().end));
    QCOMPARE(document.wires().first().vertices, QVector<QPointF>({{20,0},{60,0},{60,40}}));
    QCOMPARE(document.nodes().cbegin().value(), QPointF(60,40));
    QVERIFY(document.junctionPoints().isEmpty());
    QCOMPARE(edits.undoStack()->count(), 1);
    const auto after = document.toJsonBytes();
    edits.undoStack()->undo();
    QCOMPARE(document.toJsonBytes(), before);
    edits.undoStack()->redo();
    QCOMPARE(document.toJsonBytes(), after);
    // Drawing from the free endpoint reuses that node and its topology.
    QTest::mouseClick(view.viewport(), Qt::LeftButton, Qt::NoModifier, view.mapFromScene(60,40));
    QTest::mouseClick(view.viewport(), Qt::LeftButton, Qt::NoModifier, view.mapFromScene(100,40));
    QTest::mouseDClick(view.viewport(), Qt::LeftButton, Qt::NoModifier, view.mapFromScene(100,40));
    QCOMPARE(document.wires().size(), 2);
    QVERIFY(document.wires()[1].start == document.wires()[0].end);
    QCOMPARE(edits.undoStack()->count(), 2);
}

void GroupSelectionTests::doubleClickOnTargetDoesNotStartAnotherWire()
{
    Document document;
    const auto a = document.addComponent(SymbolIds::Resistor, {});
    const auto b = document.addComponent(SymbolIds::Resistor, {100,0});
    Q_UNUSED(a); Q_UNUSED(b);
    SchematicScene scene;
    scene.bindDocument(&document);
    DocumentEditController edits(&document);
    connectEdits(scene, edits);
    SelectionView view;
    view.setScene(&scene);
    view.resize(800,600); view.show();
    QVERIFY(QTest::qWaitForWindowActive(&view));
    view.centerOn(50,0);
    scene.setEditMode(SchematicScene::EditMode::DrawWire);
    const auto click = [&](QPointF p) { QTest::mouseClick(view.viewport(), Qt::LeftButton, Qt::NoModifier, view.mapFromScene(p)); };
    click({20,0});
    click({80,0}); // The first click already completes on the terminal.
    QTest::mouseDClick(view.viewport(), Qt::LeftButton, Qt::NoModifier, view.mapFromScene(80,0));
    click({20,0}); // Starts a fresh draft; must not complete a second wire.
    QCOMPARE(document.wires().size(), 1);
    QCOMPARE(edits.undoStack()->count(), 1);
    QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
    view.keyPressEvent(&escape);
    QCOMPARE(scene.editMode(), SchematicScene::EditMode::Select);
}

void GroupSelectionTests::doubleClickTerminalStartsWire()
{
    Document document;
    const auto a = document.addComponent(SymbolIds::Resistor, {});
    const auto b = document.addComponent(SymbolIds::Resistor, {100,0});
    SchematicScene scene;
    scene.bindDocument(&document);
    DocumentEditController edits(&document);
    connectEdits(scene, edits);
    SelectionView view;
    view.setScene(&scene);
    view.resize(800,600); view.show();
    QVERIFY(QTest::qWaitForWindowActive(&view));
    view.centerOn(50,0);
    QSignalSpy componentEdits(&scene, &SchematicScene::componentDoubleClicked);
    QSignalSpy modes(&scene, &SchematicScene::editModeChanged);
    const auto before = document.toJsonBytes();
    const QPoint terminal = view.mapFromScene(20,0);
    QTest::mouseClick(view.viewport(), Qt::LeftButton, Qt::NoModifier, terminal);
    QTest::mouseDClick(view.viewport(), Qt::LeftButton, Qt::NoModifier, terminal);
    QCOMPARE(scene.editMode(), SchematicScene::EditMode::DrawWire);
    QCOMPARE(modes.size(), 1);
    QVERIFY(componentEdits.isEmpty());
    QCOMPARE(document.toJsonBytes(), before);
    QVERIFY(!edits.undoStack()->canUndo());
    QTest::mouseClick(view.viewport(), Qt::LeftButton, Qt::NoModifier, view.mapFromScene(80,0));
    QCOMPARE(document.wires().size(), 1);
    QVERIFY(document.wires().first().start == makeTerminalEndpoint(a,1));
    QVERIFY(document.wires().first().end == makeTerminalEndpoint(b,0));
    QCOMPARE(edits.undoStack()->count(), 1);
    edits.undoStack()->undo();
    QCOMPARE(document.toJsonBytes(), before);
    scene.setEditMode(SchematicScene::EditMode::Select);
    QTest::mouseDClick(view.viewport(), Qt::LeftButton, Qt::NoModifier, view.mapFromScene(0,0));
    QCOMPARE(componentEdits.size(), 1);
    QCOMPARE(componentEdits.first().first().value<ComponentId>(), a);
    QCOMPARE(scene.editMode(), SchematicScene::EditMode::Select);
}

void GroupSelectionTests::doubleClickJunctionStartsWireAndEscapeCancels()
{
    Document document;
    const auto a = document.addComponent(SymbolIds::Resistor, {});
    const auto b = document.addComponent(SymbolIds::Resistor, {100,0});
    const auto ground = document.addComponent(SymbolIds::Ground, {50,60});
    const auto wire = document.addWire(makeTerminalEndpoint(a,1), {}, makeTerminalEndpoint(b,0));
    QVERIFY(wire);
    const auto node = document.branchWireAt(*wire, {50,0});
    QVERIFY(node);
    const auto before = document.toJsonBytes();
    SchematicScene scene;
    scene.bindDocument(&document);
    DocumentEditController edits(&document);
    connectEdits(scene, edits);
    SelectionView view;
    view.setScene(&scene);
    view.resize(800,600); view.show();
    QVERIFY(QTest::qWaitForWindowActive(&view));
    view.centerOn(50,20);
    const QPoint junction = view.mapFromScene(50,0);
    QTest::mouseClick(view.viewport(), Qt::LeftButton, Qt::NoModifier, junction);
    QTest::mouseDClick(view.viewport(), Qt::LeftButton, Qt::NoModifier, junction);
    QCOMPARE(scene.editMode(), SchematicScene::EditMode::DrawWire);
    QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
    view.keyPressEvent(&escape);
    QCOMPARE(scene.editMode(), SchematicScene::EditMode::Select);
    QCOMPARE(document.toJsonBytes(), before);
    QVERIFY(!edits.undoStack()->canUndo());
    QTest::mouseDClick(view.viewport(), Qt::LeftButton, Qt::NoModifier, junction);
    QTest::mouseClick(view.viewport(), Qt::LeftButton, Qt::NoModifier, view.mapFromScene(50,40));
    QCOMPARE(document.wires().size(), 3);
    QVERIFY(document.wires().last().start == makeNodeEndpoint(*node));
    QVERIFY(document.wires().last().end == makeTerminalEndpoint(ground,0));
    QCOMPARE(document.junctionPoints(), QVector<QPointF>({{50,0}}));
    QCOMPARE(edits.undoStack()->count(), 1);
    scene.setEditMode(SchematicScene::EditMode::Select);
    QTest::mouseDClick(view.viewport(), Qt::LeftButton, Qt::NoModifier, view.mapFromScene(35,0));
    QCOMPARE(scene.editMode(), SchematicScene::EditMode::Select); // plain segment is not a connection point
}

void GroupSelectionTests::wireCompletionAfterSnappingOntoTerminal()
{
    Document document;
    const auto a = document.addComponent(SymbolIds::Resistor, {});
    const auto b = document.addComponent(SymbolIds::Resistor, {100,0});
    SchematicScene scene;
    scene.bindDocument(&document);
    DocumentEditController edits(&document);
    connectEdits(scene, edits);
    SelectionView view;
    view.setScene(&scene);
    view.resize(800,600); view.show();
    QVERIFY(QTest::qWaitForWindowActive(&view));
    view.scale(3,3);
    view.centerOn(50,0);
    scene.setEditMode(SchematicScene::EditMode::DrawWire);
    const auto click = [&](QPointF p) { const QPoint screen = view.mapFromScene(p); view.press(screen); view.release(screen); };
    click({20,0});
    click({84,4}); // outside the 8-pixel cue; snaps to terminal at (80,0)
    QVERIFY(document.wires().isEmpty());
    click({80,0});
    QCOMPARE(document.wires().size(), 1);
    QCOMPARE(document.wires().first().vertices, QVector<QPointF>({{20,0},{80,0}}));
    QVERIFY(document.wires().first().start == makeTerminalEndpoint(a,1));
    QVERIFY(document.wires().first().end == makeTerminalEndpoint(b,0));
    QCOMPARE(edits.undoStack()->count(), 1);
    edits.undoStack()->undo();
    QVERIFY(document.wires().isEmpty());
}

void GroupSelectionTests::branchPreviewAndCommitSnapToTargetWire()
{
    for (bool vertical : {false,true}) {
        Document document;
        // An off-grid target axis must remain exact; snap only along it.
        const auto a = document.addComponent(SymbolIds::Resistor, vertical ? QPointF(3,0) : QPointF(0,3),
                                             vertical ? Rotation::Deg90 : Rotation::Deg0);
        const auto b = document.addComponent(SymbolIds::Resistor, vertical ? QPointF(3,100) : QPointF(100,3),
                                             vertical ? Rotation::Deg90 : Rotation::Deg0);
        const auto c = document.addComponent(SymbolIds::Resistor, vertical ? QPointF(100,50) : QPointF(50,100),
                                             vertical ? Rotation::Deg0 : Rotation::Deg90);
        QVERIFY(document.addWire(makeTerminalEndpoint(a,1), {}, makeTerminalEndpoint(b,0)));
        SchematicScene scene;
        scene.bindDocument(&document);
        DocumentEditController edits(&document);
        connectEdits(scene, edits);
        SelectionView view;
        view.setScene(&scene);
        view.resize(800,600); view.show();
        QVERIFY(QTest::qWaitForWindowActive(&view));
        view.centerOn(50,50);
        scene.setEditMode(SchematicScene::EditMode::DrawWire);
        const QPoint start = view.mapFromScene(vertical ? QPointF(80,50) : QPointF(50,80));
        view.press(start); view.release(start);
        const QPoint target = view.mapFromScene(vertical ? QPointF(5,52) : QPointF(52,5));
        view.move(target, Qt::NoButton);
        const QPointF expected = vertical ? QPointF(3,50) : QPointF(50,3);
        bool previewFound = false;
        for (auto *item : scene.items()) {
            if (auto *path = qgraphicsitem_cast<QGraphicsPathItem *>(item)) {
                if (path->pen().color() == Qt::blue && path->pen().style() == Qt::DashLine) {
                    QCOMPARE(path->path().currentPosition(), expected);
                    previewFound = true;
                }
            }
        }
        QVERIFY(previewFound);
        view.press(target); view.release(target);
        QCOMPARE(document.nodes().size(), 1);
        QCOMPARE(document.nodes().cbegin().value(), expected);
        QCOMPARE(document.junctionPoints(), QVector<QPointF>{expected});
        QCOMPARE(document.wires().size(), 3);
        QCOMPARE(edits.undoStack()->count(), 1);
        const auto node = document.nodes().cbegin().key();
        // Just outside the node cue, snapping along the route still lands
        // on that node. Reuse it instead of rejecting an endpoint split.
        const auto d = document.addComponent(SymbolIds::Resistor, vertical ? QPointF(100,70) : QPointF(70,100),
                                             vertical ? Rotation::Deg0 : Rotation::Deg90);
        scene.restoreAfterDocumentChange({});
        scene.setEditMode(SchematicScene::EditMode::DrawWire);
        const QPoint secondStart = view.mapFromScene(vertical ? QPointF(80,70) : QPointF(70,80));
        view.press(secondStart); view.release(secondStart);
        const QPoint nearNode = view.mapFromScene(vertical ? QPointF(-5,54) : QPointF(54,-5));
        view.press(nearNode); view.release(nearNode);
        QCOMPARE(document.nodes().size(), 1);
        QCOMPARE(document.nodes().value(node), expected);
        QCOMPARE(document.wires().size(), 4);
        QVERIFY(document.wires().last().start == makeTerminalEndpoint(d,0));
        QVERIFY(document.wires().last().end == makeNodeEndpoint(node));
        QVERIFY(document.wires()[2].start == makeTerminalEndpoint(c,0));
    }
}

void GroupSelectionTests::wireSegmentDragPreviewUndoAndCancel()
{
    Document document;
    const auto a = document.addComponent(SymbolIds::Resistor, {});
    const auto b = document.addComponent(SymbolIds::Resistor, {100,100});
    const auto id = document.addWire(makeTerminalEndpoint(a,1), {{50,0},{50,100}}, makeTerminalEndpoint(b,0));
    QVERIFY(id);
    SchematicScene scene;
    scene.bindDocument(&document);
    DocumentEditController edits(&document);
    connectEdits(scene, edits);
    SelectionView view;
    view.setScene(&scene);
    view.resize(800,600); view.show();
    QVERIFY(QTest::qWaitForWindowActive(&view));
    view.centerOn(50,50);
    const auto before = document.toJsonBytes();
    // Pick several pixels away from the stroke, without starting a box.
    const QPoint start = view.mapFromScene(50,50) + QPoint(4,0);
    view.press(start); view.release(start);
    QCOMPARE(scene.selection().wires, QVector<WireId>{*id});
    QVERIFY(!edits.undoStack()->canUndo());
    const auto wireItem = [&]() -> WireItem * {
        for (auto *item : scene.items())
            if (auto *wire = qgraphicsitem_cast<WireItem *>(item))
                if (wire->wireId() == *id) return wire;
        return nullptr;
    };
    QVERIFY(wireItem());
    QVERIFY(wireItem()->isSelected());
    view.press(start);
    view.move(start + QPoint(17,27)); // x snaps to 70; y is ignored
    QCOMPARE(document.toJsonBytes(), before); // preview is detached
    QVERIFY(wireItem());
    QCOMPARE(wireItem()->path().elementAt(1).x, 70.0);
    view.release(start + QPoint(17,27));
    QCOMPARE(document.wire(*id)->vertices, QVector<QPointF>({{20,0},{70,0},{70,100},{80,100}}));
    QCOMPARE(edits.undoStack()->count(), 1);
    const auto moved = document.toJsonBytes();
    edits.undoStack()->undo();
    QCOMPARE(document.toJsonBytes(), before);
    QCOMPARE(scene.selection().wires, QVector<WireId>{*id});
    edits.undoStack()->redo();
    QCOMPARE(document.toJsonBytes(), moved);

    // First horizontal segment gains an endpoint bend; Escape cancels it.
    const QPoint horizontal = view.mapFromScene(40,0);
    view.press(horizontal); view.move(horizontal + QPoint(13,-23));
    QCOMPARE(document.toJsonBytes(), moved);
    QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
    view.keyPressEvent(&escape);
    view.release(horizontal + QPoint(13,-23));
    QCOMPARE(document.toJsonBytes(), moved);
    QCOMPARE(edits.undoStack()->count(), 1);
    view.press(horizontal); view.move(horizontal + QPoint(13,-23));
    view.release(horizontal + QPoint(13,-23));
    QCOMPARE(document.wire(*id)->vertices, QVector<QPointF>({{20,0},{20,-20},{70,-20},{70,100},{80,100}}));
    QCOMPARE(edits.undoStack()->count(), 2);
}

void GroupSelectionTests::wireSegmentDragRejectsInvalidRelease()
{
    Document document;
    const auto a = document.addComponent(SymbolIds::Resistor, {});
    const auto b = document.addComponent(SymbolIds::Resistor, {100,0});
    const auto id = document.addWire(makeTerminalEndpoint(a,1), {{40,0},{40,40},{80,40}}, makeTerminalEndpoint(b,0));
    QVERIFY(id);
    SchematicScene scene;
    scene.bindDocument(&document);
    DocumentEditController edits(&document);
    connectEdits(scene, edits);
    SelectionView view;
    view.setScene(&scene);
    view.resize(800,600); view.show();
    QVERIFY(QTest::qWaitForWindowActive(&view));
    view.centerOn(50,20);
    const auto before = document.toJsonBytes();
    const QPoint start = view.mapFromScene(40,20);
    view.press(start); view.move(view.mapFromScene(60,20)); // valid preview
    view.move(view.mapFromScene(80,20)); // reversal after neighbour collapses
    view.release(view.mapFromScene(80,20));
    QCOMPARE(document.toJsonBytes(), before);
    QVERIFY(!edits.undoStack()->canUndo());
    QCOMPARE(scene.selection().wires, QVector<WireId>{*id});
}

void GroupSelectionTests::componentLabelsAndTransforms()
{
    Document document;
    const auto id = document.addComponent(SymbolIds::Resistor, {0, 0}, Rotation::Deg90);
    QVERIFY(document.setComponentMirrored(id, true));
    QVERIFY(document.setComponentLabels(id, "R1", "10k"));
    const auto before = document.toJsonBytes();
    SchematicScene scene;
    scene.bindDocument(&document);
    QRectF allBounds;
    QRectF artworkBounds;
    ComponentLabelItem *label = nullptr;
    for (auto *item : scene.items()) {
        if (auto *component = qgraphicsitem_cast<ComponentItem *>(item)) {
            artworkBounds = component->sceneBoundingRect();
            allBounds = allBounds.united(artworkBounds);
        }
        if (auto *text = qgraphicsitem_cast<ComponentLabelItem *>(item)) {
            label = text;
            allBounds = allBounds.united(text->sceneBoundingRect());
        }
    }
    QVERIFY(label);
    scene.selectInBox(allBounds.adjusted(-1, -1, 1, 1), false, {});
    QCOMPARE(scene.selection().components, QVector<ComponentId>{id});
    scene.selectInBox(artworkBounds, false, {});
    QVERIFY(scene.selectedItems().isEmpty()); // labels are outside
    scene.selectInBox(label->sceneBoundingRect(), true, {});
    QCOMPARE(scene.selection().components, QVector<ComponentId>{id});
    scene.selectInBox(label->sceneBoundingRect(), false, {});
    QVERIFY(scene.selectedItems().isEmpty());
    // Gap between the top reference label and the symbol must not match.
    QRectF reference;
    for (auto *item : scene.items()) {
        if (auto *text = qgraphicsitem_cast<ComponentLabelItem *>(item))
            if (text->text() == "R1") reference = text->sceneBoundingRect();
    }
    if (reference.bottom() + 1 < artworkBounds.top()) {
        scene.selectInBox(QRectF(artworkBounds.left(), reference.bottom() + 0.2,
                                 1, artworkBounds.top() - reference.bottom() - 0.4), true, {});
        QVERIFY(scene.selectedItems().isEmpty());
    }
    QCOMPARE(document.toJsonBytes(), before);
}

void GroupSelectionTests::routeShapesAndIdentity()
{
    Document document;
    SchematicScene scene;
    scene.bindDocument(&document);
    // Synthetic scene targets isolate picking from route creation. Crossing
    // geometry deliberately shares coordinates but has distinct identities.
    auto addWire = [&](WireId id, const QPainterPath &path) {
        auto *wire = new WireItem(id);
        wire->setPath(path);
        wire->setPen(QPen(Qt::black, 1));
        wire->setFlag(QGraphicsItem::ItemIsSelectable);
        scene.addItem(wire);
        return wire;
    };
    QPainterPath bent;
    bent.moveTo(0, 0); bent.lineTo(100, 0); bent.lineTo(100, 100);
    auto *wire = addWire(1, bent);
    QPainterPath vertical;
    vertical.moveTo(50, -50); vertical.lineTo(50, 50);
    addWire(2, vertical);
    scene.selectInBox({10, 10, 10, 10}, true, {});
    QVERIFY(scene.selectedItems().isEmpty());
    scene.selectInBox({10, -2, 10, 4}, false, {});
    QVERIFY(scene.selectedItems().isEmpty());
    scene.selectInBox({10, -2, 10, 4}, true, {});
    QCOMPARE(scene.selection().wires, QVector<WireId>{1});
    scene.selectInBox({48, -2, 4, 4}, true, {});
    QCOMPARE(scene.selection().wires.size(), 2);
    scene.selectInBox(wire->sceneBoundingRect(), false, {});
    QCOMPARE(scene.selection().wires, QVector<WireId>{1});
    // Exactly touch the top edge of the stroked horizontal segment.
    scene.selectInBox({10, -5, 10, 4.5}, true, {});
    QCOMPARE(scene.selection().wires, QVector<WireId>{1});
}

void GroupSelectionTests::directionalGesture_data()
{
    QTest::addColumn<int>("zoomSteps");
    QTest::newRow("normal") << 0;
    QTest::newRow("zoomed-in") << 4;
    QTest::newRow("zoomed-out") << -4;
}

void GroupSelectionTests::directionalGesture()
{
    QFETCH(int, zoomSteps);
    Document document;
    QVERIFY(document.setGridSpacing(7));
    const auto first = document.addComponent(SymbolIds::Resistor, {0, 0});
    const auto second = document.addComponent(SymbolIds::Capacitor, {160, 0});
    const auto before = document.toJsonBytes();
    SchematicScene scene;
    scene.bindDocument(&document);
    SelectionView view;
    view.setScene(&scene);
    view.resize(800, 600);
    view.show();
    QVERIFY(QTest::qWaitForWindowActive(&view));
    QApplication::processEvents();
    for (int i = 0; i < qAbs(zoomSteps); ++i)
        zoomSteps > 0 ? view.zoomIn() : view.zoomOut();
    view.centerOn(0, 0);
    const auto point = [&](int x, int y) { return view.mapFromScene(QPointF(x, y)); };
    const QPoint left = point(-60, -60), right = point(0, 60);
    view.press(left); view.move(right); view.release(right);
    QVERIFY(scene.selectedItems().isEmpty()); // only half the resistor enclosed
    view.press(point(0, -60)); view.move(point(-60, 60));
    QCOMPARE(scene.selection().components, QVector<ComponentId>{first});
    view.release(point(-60, 60));
    // Full enclosure, then shrinking away removes the live match.
    view.press(left); view.move(point(60, 60));
    QCOMPARE(scene.selection().components, QVector<ComponentId>{first});
    view.move(point(-50, -40));
    QVERIFY(scene.selectedItems().isEmpty());
    view.release(point(-50, -40));
    // Cross the starting x coordinate during a single gesture.
    view.press(point(0, -60)); view.move(point(60, 60));
    QVERIFY(scene.selectedItems().isEmpty());
    view.move(point(-60, 60));
    QCOMPARE(scene.selection().components, QVector<ComponentId>{first});
    if (zoomSteps == 0)
        QVERIFY(view.grab().save(QCoreApplication::applicationDirPath() + "/selection-crossing.png"));
    view.release(point(-60, 60));
    scene.restoreSelection({{second}, {}});
    view.press(left, Qt::ControlModifier); view.move(point(60, 60));
    QCOMPARE(scene.selection().components.size(), 2);
    view.release(point(60, 60));
    view.press(left); view.move(point(60, 60)); view.release(point(60, 60));
    QCOMPARE(scene.selection().components, QVector<ComponentId>{first});
    if (zoomSteps == 0) {
        view.press(left); view.move(point(60, 60));
        QVERIFY(view.grab().save(QCoreApplication::applicationDirPath() + "/selection-enclosure.png"));
        view.release(point(60, 60));
    }
    QCOMPARE(document.toJsonBytes(), before);
}

void GroupSelectionTests::cancellationAndTools()
{
    Document document;
    const auto id = document.addComponent(SymbolIds::Resistor, {0, 0});
    SchematicScene scene;
    scene.bindDocument(&document);
    SelectionView view;
    view.setScene(&scene);
    view.resize(800, 600);
    view.show();
    QVERIFY(QTest::qWaitForWindowActive(&view));
    QApplication::processEvents();
    view.centerOn(0, 0);
    const QPoint start = view.mapFromScene(-60, -60), end = view.mapFromScene(60, 60);
    view.press(start); view.move(end);
    QCOMPARE(scene.selection().components, QVector<ComponentId>{id});
    QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
    view.keyPressEvent(&escape);
    QVERIFY(scene.selectedItems().isEmpty());
    view.release(end);
    scene.restoreSelection({{id}, {}});
    view.press(start); view.move(start + QPoint(15, 15));
    QVERIFY(scene.selectedItems().isEmpty());
    view.keyPressEvent(&escape);
    QCOMPARE(scene.selection().components, QVector<ComponentId>{id});
    view.release(end);
    view.press(start); view.move(end);
    scene.setEditMode(SchematicScene::EditMode::DrawWire);
    view.release(end);
    QVERIFY(scene.selectedItems().isEmpty());
    QVERIFY(!scene.canStartSelectionBox({-60, -60}));
    scene.setEditMode(SchematicScene::EditMode::PlaceComponent);
    QVERIFY(!scene.canStartSelectionBox({-60, -60}));
    scene.setEditMode(SchematicScene::EditMode::Select);
    // An item click retains native selection; a small empty click clears it.
    QTest::mouseClick(view.viewport(), Qt::LeftButton, Qt::NoModifier, view.mapFromScene(0, 0));
    QCOMPARE(scene.selection().components, QVector<ComponentId>{id});
    view.press(start); view.release(start + QPoint(1, 1));
    QVERIFY(scene.selectedItems().isEmpty());
    // Rebinding during a box must not leak old document identities.
    view.press(start); view.move(end);
    Document replacement;
    scene.bindDocument(&replacement);
    view.release(end);
    QVERIFY(scene.selectedItems().isEmpty());
}

void GroupSelectionTests::mixedDeletionAndSingleMove()
{
    Document document;
    const auto left = document.addComponent(SymbolIds::Resistor, {0, 0});
    const auto right = document.addComponent(SymbolIds::Resistor, {150, 0});
    const auto wire = document.addWire(makeTerminalEndpoint(left, 1), {},
                                      makeTerminalEndpoint(right, 0));
    QVERIFY(wire);
    DocumentEditController edits(&document);
    SchematicScene scene;
    scene.bindDocument(&document);
    scene.setEditCommitter([&](const QString &description, const std::function<bool()> &operation,
                               std::function<DocumentSelection()> after) {
        return edits.commit(description, operation, std::move(after));
    });
    SelectionView view;
    view.setScene(&scene);
    view.resize(800, 600);
    view.show();
    QVERIFY(QTest::qWaitForWindowActive(&view));
    QApplication::processEvents();
    view.centerOn(0, 0);
    view.press(view.mapFromScene(-70, -70));
    view.move(view.mapFromScene(80, 70));
    view.release(view.mapFromScene(80, 70));
    QCOMPARE(scene.selection().components, QVector<ComponentId>{left});
    QVERIFY(edits.undoStack()->isClean());
    QCOMPARE(edits.undoStack()->count(), 0);
    // Native component drag still commits a snapped move with attached wire.
    view.press(view.mapFromScene(0, 0));
    view.move(view.mapFromScene(10, 10));
    view.release(view.mapFromScene(10, 10));
    QCOMPARE(document.component(left)->position(), QPointF(10, 10));
    QCOMPARE(edits.undoStack()->count(), 1);
    scene.restoreSelection({{left}, {*wire}});
    QKeyEvent remove(QEvent::KeyPress, Qt::Key_Delete, Qt::NoModifier);
    view.keyPressEvent(&remove);
    QVERIFY(!document.component(left));
    QVERIFY(document.component(right));
    QVERIFY(document.wires().isEmpty());
    QCOMPARE(edits.undoStack()->count(), 2);
}

void GroupSelectionTests::viewportEventsPanAndZoom()
{
    Document document;
    const auto id = document.addComponent(SymbolIds::Resistor, {0, 0});
    const auto before = document.toJsonBytes();
    SchematicScene scene;
    scene.bindDocument(&document);
    SelectionView view;
    view.setScene(&scene);
    view.resize(800, 600);
    view.show();
    QVERIFY(QTest::qWaitForWindowActive(&view));
    QApplication::processEvents();
    view.centerOn(0, 0);
    const QPoint start = view.mapFromScene(-60, -60), end = view.mapFromScene(60, 60);
    QTest::mousePress(view.viewport(), Qt::LeftButton, Qt::NoModifier, start);
    QTest::mouseMove(view.viewport(), end);
    QTest::mouseRelease(view.viewport(), Qt::LeftButton, Qt::NoModifier, end);
    QCOMPARE(scene.selection().components, QVector<ComponentId>{id});
    const int previousScroll = view.horizontalScrollBar()->value();
    view.press(start, Qt::NoModifier, Qt::MiddleButton);
    view.move(start + QPoint(30, 20), Qt::MiddleButton);
    view.release(start + QPoint(30, 20), Qt::MiddleButton);
    QVERIFY(view.horizontalScrollBar()->value() != previousScroll);
    QCOMPARE(view.dragMode(), QGraphicsView::NoDrag);
    const qreal previousZoom = view.zoomFactor();
    QWheelEvent wheel(QPointF(200, 200), QPointF(200, 200), {}, QPoint(0, 120),
                       Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
    view.wheelEvent(&wheel);
    QVERIFY(view.zoomFactor() > previousZoom);
    const qreal cadZoom = view.zoomFactor();
    const int scrollBefore = view.verticalScrollBar()->value();
    QWheelEvent controlWheel(QPointF(200, 200), QPointF(200, 200), {}, QPoint(0, 120),
                              Qt::NoButton, Qt::ControlModifier, Qt::NoScrollPhase, false);
    view.wheelEvent(&controlWheel);
    QCOMPARE(view.zoomFactor(), cadZoom);
    QVERIFY(view.verticalScrollBar()->value() < scrollBefore);
    view.setCadStyleMouseWheel(false);
    view.centerOn(0,0);
    const int traditionalScroll = view.verticalScrollBar()->value();
    view.wheelEvent(&wheel);
    QCOMPARE(view.zoomFactor(), cadZoom);
    QVERIFY(view.verticalScrollBar()->value() < traditionalScroll);
    view.wheelEvent(&controlWheel);
    QVERIFY(view.zoomFactor() > cadZoom);
    QCOMPARE(document.toJsonBytes(), before);
}

void GroupSelectionTests::groupPreviewCommitUndoAndPivot()
{
    Document document;
    const auto left = document.addComponent(SymbolIds::Resistor, {0, 0});
    const auto right = document.addComponent(SymbolIds::Resistor, {100, 0});
    QVERIFY(document.setComponentLabels(left, "R1", "a very long label excluded from the pivot"));
    const auto wire = document.addWire(makeTerminalEndpoint(left, 1), {}, makeTerminalEndpoint(right, 0));
    QVERIFY(wire);
    SchematicScene scene;
    scene.bindDocument(&document);
    DocumentEditController edits(&document);
    connectEdits(scene, edits);
    const DocumentSelection selected{{left, right}, {*wire}};
    scene.restoreSelection(selected);
    SelectionView view;
    view.setScene(&scene);
    view.resize(800, 600); view.show();
    QVERIFY(QTest::qWaitForWindowActive(&view));
    view.centerOn(0, 0);
    const auto before = document.toJsonBytes();
    const QPoint start = view.mapFromScene(0, 0), end = view.mapFromScene(60, 0);
    // Use the actual viewport event path, including subsequent moves after
    // preview rendering has replaced all scene items.
    QTest::mousePress(view.viewport(), Qt::LeftButton, Qt::NoModifier, start);
    QTest::mouseMove(view.viewport(), view.mapFromScene(30, 0));
    QTest::mouseMove(view.viewport(), end);
    QVERIFY(scene.moveInProgress());
    QVERIFY(!scene.transformSelected(SelectionTransform::Kind::Rotate, 1));
    QCOMPARE(document.toJsonBytes(), before);
    QCOMPARE(edits.undoStack()->count(), 0);
    for (auto *item : scene.items()) {
        if (auto *component = qgraphicsitem_cast<ComponentItem *>(item))
            if (component->componentId() == left) QCOMPARE(component->pos(), QPointF(60, 0));
        if (auto *route = qgraphicsitem_cast<WireItem *>(item))
            QCOMPARE(route->path().pointAtPercent(0), QPointF(80, 0));
    }
    QVERIFY(view.grab().save(QCoreApplication::applicationDirPath() + "/group-move-preview.png"));
    QTest::mouseRelease(view.viewport(), Qt::LeftButton, Qt::NoModifier, end);
    QVERIFY(!scene.moveInProgress());
    QCOMPARE(document.component(left)->position(), QPointF(60, 0));
    QCOMPARE(document.component(right)->position(), QPointF(160, 0));
    QCOMPARE(edits.undoStack()->count(), 1);
    QVERIFY(scene.selection() == selected);
    edits.undoStack()->undo();
    QCOMPARE(document.toJsonBytes(), before);
    QVERIFY(edits.undoStack()->isClean());
    QVERIFY(scene.selection() == selected);
    edits.undoStack()->redo();
    QVERIFY(scene.selection() == selected);
    QVERIFY(scene.transformSelected(SelectionTransform::Kind::Rotate, 1));
    QCOMPARE(document.component(left)->position(), QPointF(110, -50));
    QCOMPARE(document.component(right)->position(), QPointF(110, 50));
    QCOMPARE(document.wire(*wire)->vertices, (QVector<QPointF>{{110, -30}, {110, 30}}));
    QCOMPARE(edits.undoStack()->count(), 2);
    QVERIFY(scene.selection() == selected);
    for (auto *item : scene.items()) {
        if (qgraphicsitem_cast<ComponentLabelItem *>(item)) QCOMPARE(item->rotation(), 0.0);
    }
    QVERIFY(scene.transformSelected(SelectionTransform::Kind::FlipVertical));
    QCOMPARE(document.component(left)->position(), QPointF(110, 50));
    QCOMPARE(document.component(right)->position(), QPointF(110, -50));
    QCOMPARE(edits.undoStack()->count(), 3);
    edits.undoStack()->undo(); edits.undoStack()->undo(); edits.undoStack()->undo();
    QCOMPARE(document.toJsonBytes(), before);
    QVERIFY(scene.selection() == selected);
}

void GroupSelectionTests::moveCancellationAndInvalidRelease()
{
    Document document;
    const auto left = document.addComponent(SymbolIds::Resistor, {3, 7});
    const auto right = document.addComponent(SymbolIds::Resistor, {103, 7});
    const auto wire = document.addWire(makeTerminalEndpoint(left, 1), {{50, 7}}, makeTerminalEndpoint(right, 0));
    QVERIFY(wire);
    SchematicScene scene;
    scene.bindDocument(&document);
    DocumentEditController edits(&document);
    connectEdits(scene, edits);
    scene.restoreSelection({{left}, {}});
    SelectionView view;
    view.setScene(&scene);
    view.resize(800, 600); view.show();
    QVERIFY(QTest::qWaitForWindowActive(&view));
    view.centerOn(0, 0);
    const auto before = document.toJsonBytes();
    const auto start = view.mapFromScene(3, 7);
    view.press(start); view.release(start + QPoint(1, 1));
    QCOMPARE(document.toJsonBytes(), before); // off-grid click never snaps
    view.press(start); view.move(view.mapFromScene(13, 17));
    view.move(start); view.release(start);
    QCOMPARE(document.toJsonBytes(), before); // returning to the press is a no-op
    QCOMPARE(edits.undoStack()->count(), 0);
    view.press(start); view.move(view.mapFromScene(13, 17));
    QCOMPARE(document.toJsonBytes(), before);
    QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
    view.keyPressEvent(&escape);
    view.release(view.mapFromScene(13, 17));
    QCOMPARE(document.toJsonBytes(), before);
    QVERIFY(!scene.moveInProgress());
    view.press(start); view.move(view.mapFromScene(13, 17));
    QFocusEvent focusOut(QEvent::FocusOut);
    view.focusOutEvent(&focusOut);
    QVERIFY(!scene.moveInProgress());
    QCOMPARE(document.toJsonBytes(), before);
    view.release(view.mapFromScene(13, 17));
    // The single component crossing the fixed endpoint introduces backtrack.
    view.press(start); view.move(view.mapFromScene(13, 17));
    view.move(view.mapFromScene(133, 7));
    view.release(view.mapFromScene(133, 7));
    QCOMPARE(document.toJsonBytes(), before);
    QCOMPARE(edits.undoStack()->count(), 0);
    view.press(start); view.move(view.mapFromScene(13, 17));
    scene.setEditMode(SchematicScene::EditMode::DrawWire);
    QVERIFY(!scene.moveInProgress());
    QCOMPARE(document.toJsonBytes(), before);
    view.release(view.mapFromScene(13, 17));
    scene.setEditMode(SchematicScene::EditMode::Select);
    scene.restoreSelection({{left, right}, {*wire}});
    view.press(start); view.move(view.mapFromScene(13, 17));
    view.release(view.mapFromScene(13, 17));
    QCOMPARE(document.component(left)->position(), QPointF(10, 20));
    QCOMPARE(document.component(right)->position(), QPointF(110, 20));
    edits.undoStack()->undo();
    QCOMPARE(document.toJsonBytes(), before);
    view.press(start); view.move(view.mapFromScene(13, 17));
    // Rebuild/undo callbacks cancel the candidate, even before a history entry.
    scene.restoreAfterDocumentChange({{left, right}, {*wire}});
    QVERIFY(!scene.moveInProgress());
    QCOMPARE(document.toJsonBytes(), before);
    view.release(view.mapFromScene(13, 17));
    view.press(start); view.move(view.mapFromScene(13, 17));
    Document replacement;
    scene.bindDocument(&replacement);
    view.release(view.mapFromScene(13, 17));
    QVERIFY(scene.selectedItems().isEmpty());
    QCOMPARE(document.toJsonBytes(), before);
}

void GroupSelectionTests::wireOnlyTransformsAndModifierClicks()
{
    Document document;
    const auto left = document.addComponent(SymbolIds::Resistor, {0, 0});
    const auto right = document.addComponent(SymbolIds::Resistor, {100, 100});
    const auto wire = document.addWire(makeTerminalEndpoint(left, 1), {{50, 0}, {50, 100}},
                                       makeTerminalEndpoint(right, 0));
    QVERIFY(wire);
    SchematicScene scene;
    scene.bindDocument(&document);
    DocumentEditController edits(&document);
    connectEdits(scene, edits);
    scene.restoreSelection({{}, {*wire}});
    const auto before = document.toJsonBytes();
    QVERIFY(scene.transformSelected(SelectionTransform::Kind::FlipVertical));
    QCOMPARE(document.component(left)->position(), QPointF(0, 0));
    QCOMPARE(document.component(right)->position(), QPointF(100, 100));
    QCOMPARE(document.wire(*wire)->vertices.first(), QPointF(20, 0));
    QCOMPARE(document.wire(*wire)->vertices.last(), QPointF(80, 100));
    QCOMPARE(scene.selection().wires, QVector<WireId>{*wire});
    edits.undoStack()->undo();
    QCOMPARE(document.toJsonBytes(), before);
    SelectionView view;
    view.setScene(&scene);
    view.resize(800, 600); view.show();
    QVERIFY(QTest::qWaitForWindowActive(&view));
    view.centerOn(0, 0);
    QTest::mouseClick(view.viewport(), Qt::LeftButton, Qt::ControlModifier, view.mapFromScene(0, 0));
    QCOMPARE(scene.selection().components, QVector<ComponentId>{left});
    QCOMPARE(scene.selection().wires, QVector<WireId>{*wire});
    QVERIFY(!scene.moveInProgress());
    QTest::mouseClick(view.viewport(), Qt::LeftButton, Qt::ControlModifier, view.mapFromScene(0, 0));
    QVERIFY(scene.selection().components.isEmpty());
    // A single vertical segment ignores parallel movement.
    view.press(view.mapFromScene(50, 50));
    view.move(view.mapFromScene(50, 40));
    view.release(view.mapFromScene(50, 40));
    QCOMPARE(document.toJsonBytes(), before);
    // Perpendicular movement reshapes the route with fixed endpoints.
    view.press(view.mapFromScene(50, 50));
    view.move(view.mapFromScene(40, 40));
    view.release(view.mapFromScene(40, 40));
    QVERIFY(document.toJsonBytes() != before);
    QCOMPARE(document.wire(*wire)->vertices, QVector<QPointF>({{20,0},{40,0},{40,100},{80,100}}));
    QCOMPARE(document.component(left)->position(), QPointF(0, 0));
    QCOMPARE(scene.selection().wires, QVector<WireId>{*wire});
    edits.undoStack()->undo();
    QCOMPARE(document.toJsonBytes(), before);
}

void GroupSelectionTests::canvasFillLayeringAndTransforms()
{
    for (int mode = 0; mode < 3; ++mode) {
        Document document;
        auto symbol = *builtinSymbolLibrary().find(SymbolIds::Resistor);
        symbol.id = SymbolId("CanvasFillTest");
        SymbolPrimitive polygon;
        polygon.kind = PrimitiveKind::Polygon;
        polygon.points = {{-10,-10}, {10,-10}, {10,10}, {-10,10}};
        polygon.filled = mode != 0;
        polygon.fillColor = mode == 2 ? PrimitiveFillColor::Black : PrimitiveFillColor::White;
        symbol.primitives = {polygon};
        QStringList errors; QVERIFY(document.useSymbol(symbol, &errors));
        const auto id = document.addComponent(symbol.id, {0,0}, Rotation::Deg90);
        QVERIFY(document.setComponentMirrored(id, true));
        SchematicScene scene; scene.bindDocument(&document);
        auto *underneath = scene.addLine(-20, 0, 20, 0, QPen(Qt::blue, 2));
        underneath->setZValue(-1);
        QImage image(100,100,QImage::Format_ARGB32_Premultiplied); image.fill(Qt::transparent);
        { QPainter painter(&image); scene.render(&painter, {0,0,100,100}, {-25,-25,50,50}); }
        QCOMPARE(image.pixelColor(50,50), QColor(mode == 0 ? Qt::blue : mode == 1 ? Qt::white : Qt::black));
        // Interior picking and box selection still resolve to the component.
        scene.selectInBox({-12,-12,24,24}, true, {});
        QCOMPARE(scene.selection().components, QVector<ComponentId>{id});
        if (mode == 2)
            QVERIFY(image.save(QCoreApplication::applicationDirPath() + "/black-fill-canvas.png"));
    }
}

void GroupSelectionTests::propertyLabelsFollowVisibilityAndTransforms()
{
    Document document;
    const auto id = document.addComponent(SymbolIds::Resistor, {}, Rotation::Deg90);
    QVERIFY(document.editProperties({id}, {{PropertyEdit::Kind::SetValue,"value",{},"10k"},
        {PropertyEdit::Kind::Add,"tolerance","Tolerance","1%",true},
        {PropertyEdit::Kind::Add,"manufacturer","Manufacturer","Example",false}}));
    SchematicScene scene; scene.bindDocument(&document);
    const auto checkLayout = [&]() {
        QHash<QString,QRectF> labels; QRectF body;
        for (auto *item : scene.items()) {
            if (auto *label = qgraphicsitem_cast<ComponentLabelItem *>(item)) {
                labels.insert(label->propertyId(),label->sceneBoundingRect());
                QCOMPARE(label->rotation(),0.0);
                if (label->propertyId() == "tolerance") QCOMPARE(label->text(),QString("Tolerance: 1%"));
            }
            if (auto *component = qgraphicsitem_cast<ComponentItem *>(item)) body = component->sceneBoundingRect();
        }
        QCOMPARE(labels.size(),3);
        QVERIFY(labels["reference"].top() > body.bottom());
        QVERIFY(labels["value"].top() > labels["reference"].top());
        QVERIFY(labels["value"].top() > body.bottom());
        QVERIFY(labels["tolerance"].top() >= labels["value"].bottom() - 0.1);
    };
    checkLayout();
    QVERIFY(document.transformSelection({id},{},{SelectionTransform::Kind::FlipHorizontal}));
    scene.restoreAfterDocumentChange({{id},{}}); checkLayout();
    QVERIFY(document.editProperties({id}, {{PropertyEdit::Kind::SetVisible,"reference",{},{},false},
                                           {PropertyEdit::Kind::SetVisible,"value",{},{},false}}));
    scene.restoreAfterDocumentChange({{id},{}});
    int count = 0;
    for (auto *item : scene.items()) if (auto *label = qgraphicsitem_cast<ComponentLabelItem *>(item)) {
        ++count; QCOMPARE(label->propertyId(),QString("tolerance"));
        scene.selectInBox(label->sceneBoundingRect(),true,{});
    }
    QCOMPARE(count,1); QCOMPARE(scene.selection().components,QVector<ComponentId>{id});
    QCOMPARE(document.component(id)->value(),QString("10k"));
}

void GroupSelectionTests::propertyBlockDragResetAndCancel()
{
    Document document; const auto a=document.addComponent(SymbolIds::Resistor,{});
    const auto b=document.addComponent(SymbolIds::Capacitor,{100,0});
    const auto wire=document.addWire(makeTerminalEndpoint(a,1),{},makeTerminalEndpoint(b,0)); QVERIFY(wire);
    QVERIFY(document.setComponentLabels(a,"R1","4k7"));
    SchematicScene scene; scene.bindDocument(&document);
    DocumentEditController edits(&document); connectEdits(scene,edits);
    const DocumentSelection selected{{a,b},{*wire}}; scene.restoreSelection(selected);
    SelectionView view; view.setScene(&scene); view.resize(800,600); view.show();
    QVERIFY(QTest::qWaitForWindowActive(&view)); view.setSceneRect(-400,-300,800,600); view.scale(2,2); view.centerOn(50,0);
    auto block = [&]() -> PropertyBlockItem * {
        for(auto *item:scene.items()) if(auto *p=qgraphicsitem_cast<PropertyBlockItem *>(item))
            if(p->componentId()==a)return p; return nullptr;
    };
    QVERIFY(block()); const auto anchor=block()->scenePos();
    const auto before=document.toJsonBytes(); const QJsonValue wires=document.toJson().value("wires");
    QPoint start=view.mapFromScene(anchor+QPointF(2,2)), end=start+QPoint(47,-83);
    view.press(start); view.release(start); QVERIFY(!document.component(a)->propertyBlockOffset());
    view.press(start); view.move(end); QCOMPARE(document.toJsonBytes(),before);
    view.release(end); QVERIFY(document.component(a)->propertyBlockOffset());
    const auto offset=*document.component(a)->propertyBlockOffset();
    const auto desired=anchor+QPointF(23.5,-41.5);
    QCOMPARE(offset,QPointF(std::round(desired.x()/2)*2,std::round(desired.y()/2)*2));
    QCOMPARE(document.component(a)->position(),QPointF());
    QCOMPARE(document.component(b)->position(),QPointF(100,0));
    QCOMPARE(document.toJson().value("wires"),wires);
    QCOMPARE(scene.selection().components,selected.components); QCOMPARE(scene.selection().wires,selected.wires);
    QCOMPARE(edits.undoStack()->count(),1); edits.undoStack()->undo();
    QCOMPARE(document.toJsonBytes(),before); edits.undoStack()->redo();
    QCOMPARE(*document.component(a)->propertyBlockOffset(),offset);
    start=view.mapFromScene(block()->scenePos()+QPointF(2,2));
    view.press(start); view.move(start+QPoint(30,20)); scene.cancelMove();
    view.release(start+QPoint(30,20)); QCOMPARE(*document.component(a)->propertyBlockOffset(),offset);
    QVERIFY(scene.resetPropertyPositions({a,b})); QVERIFY(!document.component(a)->propertyBlockOffset());
    QCOMPARE(edits.undoStack()->count(),2); edits.undoStack()->undo();
    QCOMPARE(*document.component(a)->propertyBlockOffset(),offset);
    // Actual text, not the empty space to the artwork, is a selection target.
    const auto textBounds=block()->sceneBoundingRect();
    scene.selectInBox(textBounds,true,{}); QCOMPARE(scene.selection().components,QVector<ComponentId>{a});
    scene.selectInBox(QRectF(50,-10,5,5),true,{}); QVERIFY(scene.selection().components.isEmpty());
    scene.restoreSelection(selected);
    start=view.mapFromScene(block()->scenePos()+QPointF(2,2));
    view.press(start); view.move(start+QPoint(30,20));
    QKeyEvent escape(QEvent::KeyPress,Qt::Key_Escape,Qt::NoModifier); view.keyPressEvent(&escape);
    view.release(start+QPoint(30,20)); QCOMPARE(*document.component(a)->propertyBlockOffset(),offset);
    scene.setPropertyBlockGridSpacing(3.5);
    const auto original=block()->scenePos();
    start=view.mapFromScene(original+QPointF(2,2)); end=start+QPoint(-41,29);
    const auto delta=view.mapToScene(end)-view.mapToScene(start);
    view.press(start); view.move(end); view.release(end);
    const auto desiredCustom=original+delta;
    QCOMPARE(block()->scenePos(),QPointF(std::round(desiredCustom.x()/3.5)*3.5,std::round(desiredCustom.y()/3.5)*3.5));
    QCOMPARE(document.toJson().value("wires"),wires);
    QVERIFY(view.grab().save(QCoreApplication::applicationDirPath()+"/property-block-placement.png"));
}

void GroupSelectionTests::propertyJustificationAnchors()
{
    Document document; const auto id=document.addComponent(SymbolIds::Resistor,{3.5,-7.25},Rotation::Deg90);
    QVERIFY(document.setComponentLabels(id,"R1","Longer value"));
    SchematicScene scene; scene.bindDocument(&document);
    for(auto mode : {PropertyJustification::Left,PropertyJustification::Middle,PropertyJustification::Right}) {
        document.setPropertyBlockJustification({id},mode);
        document.setPropertyBlockOffset({id},std::nullopt);
        scene.restoreAfterDocumentChange({{id},{}});
        QRectF body; PropertyBlockItem *block=nullptr;
        for(auto *item:scene.items()) {
            if(auto *p=qgraphicsitem_cast<ComponentItem *>(item)) body=p->sceneBoundingRect();
            if(auto *p=qgraphicsitem_cast<PropertyBlockItem *>(item)) block=p;
        }
        QVERIFY(block);
        const auto x=mode==PropertyJustification::Left ? body.left() : mode==PropertyJustification::Middle ? body.center().x() : body.right();
        QVERIFY(qAbs(block->scenePos().x()-x)<0.001);
        QVERIFY(document.setPropertyBlockOffset({id},QPointF(-21.5,19.25)));
        scene.restoreAfterDocumentChange({{id},{}});
        int rows=0;
        for(auto *item:scene.items()) if(auto *label=qgraphicsitem_cast<ComponentLabelItem *>(item)) {
            const auto rect=label->sceneBoundingRect();
            const auto edge=mode==PropertyJustification::Left ? rect.left() : mode==PropertyJustification::Middle ? rect.center().x() : rect.right();
            QVERIFY(qAbs(edge-(3.5-21.5))<0.001); QCOMPARE(label->rotation(),0.0); ++rows;
        }
        QCOMPARE(rows,2);
    }
}

QTEST_MAIN(GroupSelectionTests)
#include "tst_groupselection.moc"
