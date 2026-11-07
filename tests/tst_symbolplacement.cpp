#include "../documenteditcontroller.h"
#include "../schematicitems.h"
#include "../schematicscene.h"

#include <QGraphicsItem>
#include <QGraphicsSceneMouseEvent>
#include <QKeyEvent>
#include <QtTest>

class TestSchematicScene : public SchematicScene
{
public:
    using SchematicScene::keyPressEvent;

    void pressAt(QPointF point)
    {
        QGraphicsSceneMouseEvent event(QEvent::GraphicsSceneMousePress);
        event.setScenePos(point);
        event.setButton(Qt::LeftButton);
        event.setButtons(Qt::LeftButton);
        mousePressEvent(&event);
    }

    void pressEscape()
    {
        QKeyEvent event(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
        keyPressEvent(&event);
        Q_ASSERT(event.isAccepted());
    }

    using SchematicScene::mousePressEvent;
};

class SymbolPlacementTests : public QObject
{
    Q_OBJECT

private slots:
    void newPackagedSymbolsPlaceWireAndUndo();
    void dropPlacementSnapsSelectsAndUndoRedoes();
    void unknownSymbolIsRejectedWithoutHistory();
    void escapeCancelsDraftAndReturnsToSelect();
};

void SymbolPlacementTests::dropPlacementSnapsSelectsAndUndoRedoes()
{
    Document document;
    QVERIFY(document.setGridSpacing(5.0));
    const ComponentId prior = document.addComponent(SymbolIds::Resistor, QPointF(0, 0));
    QVERIFY(prior != kInvalidComponentId);

    DocumentEditController edits(&document);
    SchematicScene scene;
    scene.bindDocument(&document);
    scene.setEditMode(SchematicScene::EditMode::PlaceComponent);
    scene.setPlaceComponentKind(SymbolIds::Resistor);
    scene.setEditCommitter([&edits](const QString &description,
                                    const std::function<bool()> &operation,
                                    std::function<DocumentSelection()> afterSelection) {
        return edits.commit(description, operation, std::move(afterSelection));
    });
    edits.setSelectionCallbacks([&scene]() { return scene.selection(); },
                               [&scene](const DocumentSelection &selection) {
        scene.restoreAfterDocumentChange(selection);
    });

    for (QGraphicsItem *item : scene.items()) {
        if (auto *componentItem = qgraphicsitem_cast<ComponentItem *>(item)) {
            if (componentItem->componentId() == prior)
                componentItem->setSelected(true);
        }
    }
    QCOMPARE(scene.selectedComponentIds(), QVector<ComponentId>{prior});

    QVERIFY(scene.placeComponentAt(SymbolIds::Capacitor, QPointF(13, 17)));
    QCOMPARE(scene.editMode(), SchematicScene::EditMode::PlaceComponent);
    QCOMPARE(document.components().size(), 2);
    const ComponentId placed = document.components().last().id();
    QCOMPARE(document.component(placed)->position(), QPointF(15, 15));
    QCOMPARE(scene.selectedComponentIds(), QVector<ComponentId>{placed});
    QCOMPARE(edits.undoStack()->count(), 1);

    edits.undoStack()->undo();
    QCOMPARE(document.components().size(), 1);
    QCOMPARE(scene.selectedComponentIds(), QVector<ComponentId>{prior});

    edits.undoStack()->redo();
    QCOMPARE(document.components().size(), 2);
    QCOMPARE(document.component(placed)->position(), QPointF(15, 15));
    QCOMPARE(scene.selectedComponentIds(), QVector<ComponentId>{placed});
}

void SymbolPlacementTests::unknownSymbolIsRejectedWithoutHistory()
{
    Document document;
    DocumentEditController edits(&document);
    SchematicScene scene;
    scene.bindDocument(&document);
    scene.setEditCommitter([&edits](const QString &description,
                                    const std::function<bool()> &operation,
                                    std::function<DocumentSelection()> afterSelection) {
        return edits.commit(description, operation, std::move(afterSelection));
    });

    QVERIFY(!scene.placeComponentAt(SymbolId(QStringLiteral("not-in-library")),
                                    QPointF(13, 17)));
    QVERIFY(document.components().isEmpty());
    QCOMPARE(edits.undoStack()->count(), 0);
}

void SymbolPlacementTests::escapeCancelsDraftAndReturnsToSelect()
{
    Document document;
    const ComponentId left = document.addComponent(SymbolIds::Resistor, QPointF(0, 0));
    const ComponentId right = document.addComponent(SymbolIds::Resistor, QPointF(100, 0));
    QVERIFY(left != kInvalidComponentId);
    QVERIFY(right != kInvalidComponentId);

    DocumentEditController edits(&document);
    TestSchematicScene scene;
    scene.bindDocument(&document);
    scene.setEditCommitter([&edits](const QString &description,
                                    const std::function<bool()> &operation,
                                    std::function<DocumentSelection()> afterSelection) {
        return edits.commit(description, operation, std::move(afterSelection));
    });
    QVector<SchematicScene::EditMode> modeChanges;
    connect(&scene, &SchematicScene::editModeChanged, &scene,
            [&modeChanges](SchematicScene::EditMode mode) { modeChanges.append(mode); });

    scene.setEditMode(SchematicScene::EditMode::DrawWire);
    scene.pressAt(document.terminalPosition(*document.component(left), 0));
    scene.pressEscape();
    QCOMPARE(scene.editMode(), SchematicScene::EditMode::Select);
    QCOMPARE(modeChanges.size(), 2);
    QVERIFY(modeChanges.last() == SchematicScene::EditMode::Select);

    // A first click after re-entering Draw Wire must start a fresh draft. If
    // Escape left the earlier draft active, this click would complete a wire.
    scene.setEditMode(SchematicScene::EditMode::DrawWire);
    scene.pressAt(document.terminalPosition(*document.component(right), 0));
    QVERIFY(document.wires().isEmpty());
    scene.pressEscape();
}

void SymbolPlacementTests::newPackagedSymbolsPlaceWireAndUndo()
{
    for (const auto &symbol : {SymbolIds::ElectrolyticCapacitor, SymbolIds::OpAmpGrid10}) {
        Document document;
        DocumentEditController edits(&document);
        SchematicScene scene;
        scene.bindDocument(&document);
        scene.setEditCommitter([&](const QString &label, const std::function<bool()> &operation,
                                   std::function<DocumentSelection()> after) {
            return edits.commit(label, operation, std::move(after));
        });
        edits.setSelectionCallbacks([&]() { return scene.selection(); },
            [&](const DocumentSelection &selected) { scene.restoreAfterDocumentChange(selected); });
        QVERIFY(scene.placeComponentAt(symbol, {3,7}));
        const auto a = *scene.singleSelectedComponent();
        QCOMPARE(document.component(a)->position(), QPointF(0,10));
        QVERIFY(scene.placeComponentAt(symbol, {103,7}));
        const auto b = *scene.singleSelectedComponent();
        QVERIFY(edits.commit("Connect new symbols", [&]() {
            return document.addWire(makeTerminalEndpoint(a,0), {}, makeTerminalEndpoint(b,0)).has_value();
        }));
        QCOMPARE(document.wires().size(), 1);
        edits.undoStack()->undo(); QVERIFY(document.wires().isEmpty());
        edits.undoStack()->undo(); QVERIFY(!document.component(b));
        edits.undoStack()->undo(); QVERIFY(document.components().isEmpty());
        edits.undoStack()->redo(); edits.undoStack()->redo(); edits.undoStack()->redo();
        QCOMPARE(document.components().size(), 2);
        QCOMPARE(document.wires().size(), 1);
        QCOMPARE(document.component(a)->symbolId(), symbol);
    }
}

QTEST_MAIN(SymbolPlacementTests)
#include "tst_symbolplacement.moc"
