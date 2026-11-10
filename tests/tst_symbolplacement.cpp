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
    void clickPlacementModePreference();
    void placementOnWireConnectsAndUndoRedoes_data();
    void placementOnWireConnectsAndUndoRedoes();
    void ambiguousPlacementReportsUnconnectedContact();
    void newPackagedSymbolsPlaceWireAndUndo();
    void dropPlacementSnapsSelectsAndUndoRedoes();
    void unknownSymbolIsRejectedWithoutHistory();
    void escapeCancelsDraftAndReturnsToSelect();
};

void SymbolPlacementTests::clickPlacementModePreference()
{
    for (bool repeat : {false, true}) {
        Document document;
        DocumentEditController edits(&document);
        TestSchematicScene scene;
        scene.bindDocument(&document);
        scene.setEditCommitter([&edits](const QString &description, const std::function<bool()> &operation,
                                       std::function<DocumentSelection()> after) {
            return edits.commit(description, operation, std::move(after));
        });
        edits.setSelectionCallbacks([&scene] { return scene.selection(); },
            [&scene](const DocumentSelection &selection) { scene.restoreAfterDocumentChange(selection); });
        // First iteration exercises the scene's default (single placement).
        if (repeat) scene.setKeepPlacingSymbols(true);
        scene.setEditMode(SchematicScene::EditMode::PlaceComponent);
        scene.setPlaceComponentKind(SymbolIds::Resistor);
        scene.pressAt({100, 100});
        QCOMPARE(document.components().size(), 1);
        QCOMPARE(scene.selectedComponentIds(), QVector<ComponentId>{document.components().first().id()});
        QCOMPARE(scene.editMode(), repeat ? SchematicScene::EditMode::PlaceComponent
                                          : SchematicScene::EditMode::Select);
        scene.pressAt({200, 100});
        QCOMPARE(document.components().size(), repeat ? 2 : 1);
        if (repeat)
            QCOMPARE(scene.selectedComponentIds(), QVector<ComponentId>{document.components().last().id()});
    }

    TestSchematicScene scene;
    scene.setEditMode(SchematicScene::EditMode::PlaceComponent);
    scene.pressAt({100, 100}); // Unbound scene: failed placement must retain the tool.
    QCOMPARE(scene.editMode(), SchematicScene::EditMode::PlaceComponent);
}

void SymbolPlacementTests::placementOnWireConnectsAndUndoRedoes_data()
{
    QTest::addColumn<bool>("clickPlacement");
    QTest::newRow("canvas-click") << true;
    QTest::newRow("palette-placement-path") << false;
}

void SymbolPlacementTests::placementOnWireConnectsAndUndoRedoes()
{
    QFETCH(bool, clickPlacement);
    Document document;
    const auto a = document.addComponent(SymbolIds::Resistor, {});
    const auto b = document.addComponent(SymbolIds::Resistor, {100,0});
    const auto wire = document.addWire(makeTerminalEndpoint(a,1), {}, makeTerminalEndpoint(b,0));
    QVERIFY(wire);
    DocumentEditController edits(&document);
    TestSchematicScene scene;
    scene.bindDocument(&document);
    scene.setEditCommitter([&edits](const QString &description, const std::function<bool()> &operation,
                                    std::function<DocumentSelection()> after) {
        return edits.commit(description, operation, std::move(after));
    });
    edits.setSelectionCallbacks([&scene] { return scene.selection(); },
        [&scene](const DocumentSelection &selection) { scene.restoreAfterDocumentChange(selection); });
    scene.setEditMode(SchematicScene::EditMode::PlaceComponent);
    scene.setPlaceComponentKind(SymbolIds::RailVCC);
    scene.restoreSelection({{a},{}});
    const auto before = document.toJsonBytes();
    // The final snapped origin (50,-20) puts the terminal at (50,0).
    if (clickPlacement) scene.pressAt({48,-18});
    else QVERIFY(scene.placeComponentAt(SymbolIds::RailVCC, {48,-18}));
    QCOMPARE(document.components().size(), 3);
    const auto rail = document.components().last().id();
    QCOMPARE(document.component(rail)->position(), QPointF(50,-20));
    QCOMPARE(document.wires().size(), 2);
    QCOMPARE(document.junctionPoints(), QVector<QPointF>({{50,0}}));
    QCOMPARE(scene.selectedComponentIds(), QVector<ComponentId>{rail});
    bool dotFound = false;
    for (auto *item : scene.items()) {
        if (auto *ellipse = qgraphicsitem_cast<QGraphicsEllipseItem *>(item))
            if (ellipse->brush().color() == Qt::black && ellipse->sceneBoundingRect().center() == QPointF(50,0))
                dotFound = true;
    }
    QVERIFY(dotFound);
    QCOMPARE(edits.undoStack()->count(), 1);
    const auto after = document.toJsonBytes();
    edits.undoStack()->undo();
    QCOMPARE(document.toJsonBytes(), before);
    QCOMPARE(scene.selectedComponentIds(), QVector<ComponentId>{a});
    edits.undoStack()->redo();
    QCOMPARE(document.toJsonBytes(), after);
    QCOMPARE(scene.selectedComponentIds(), QVector<ComponentId>{rail});
    scene.deleteSelection();
    QCOMPARE(document.wires().size(), 2);
    QCOMPARE(document.nodes().size(), 1);
    QVERIFY(document.wires()[0].end == document.wires()[1].start);
    QVERIFY(document.junctionPoints().isEmpty());
    QCOMPARE(edits.undoStack()->count(), 2);
    edits.undoStack()->undo();
    QCOMPARE(document.toJsonBytes(), after);
    edits.undoStack()->redo();
    QCOMPARE(document.wires().size(), 2);
    QCOMPARE(document.nodes().size(), 1);
}

void SymbolPlacementTests::ambiguousPlacementReportsUnconnectedContact()
{
    Document document;
    const auto a = document.addComponent(SymbolIds::Resistor, {});
    const auto b = document.addComponent(SymbolIds::Resistor, {100,0});
    const auto c = document.addComponent(SymbolIds::Resistor, {50,-50}, Rotation::Deg90);
    const auto d = document.addComponent(SymbolIds::Resistor, {50,50}, Rotation::Deg90);
    QVERIFY(document.addWire(makeTerminalEndpoint(a,1), {}, makeTerminalEndpoint(b,0)));
    QVERIFY(document.addWire(makeTerminalEndpoint(c,1), {}, makeTerminalEndpoint(d,0)));
    DocumentEditController edits(&document);
    SchematicScene scene;
    scene.bindDocument(&document);
    scene.setEditCommitter([&edits](const QString &description, const std::function<bool()> &operation,
                                    std::function<DocumentSelection()> after) {
        return edits.commit(description, operation, std::move(after));
    });
    edits.setSelectionCallbacks([&scene] { return scene.selection(); },
        [&scene](const DocumentSelection &selection) { scene.restoreAfterDocumentChange(selection); });
    QSignalSpy messages(&scene, &SchematicScene::statusMessage);
    QVERIFY(scene.placeComponentAt(SymbolIds::RailVCC, {50,-20}));
    QCOMPARE(messages.size(), 1);
    QVERIFY(messages.first().first().toString().contains("left unconnected"));
    QCOMPARE(document.wires().size(), 2);
    QVERIFY(document.junctionPoints().isEmpty());
    QCOMPARE(edits.undoStack()->count(), 1);
}

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
    for (const auto &symbol : {SymbolIds::ElectrolyticCapacitor, SymbolIds::OpAmpGrid10,
                               SymbolIds::Potentiometer, SymbolIds::PresetResistor,
                               SymbolIds::SchottkyDiode, SymbolIds::ZenerDiode, SymbolIds::LED,
                               SymbolIds::PNPCompact30,
                               SymbolIds::NPNCompact30,
                               SymbolIds::NChannelJFETCompact30,
                               SymbolIds::PChannelJFETCompact30,
                               SymbolIds::NChannelMOSFETCompact30,
                               SymbolIds::PChannelMOSFETCompact30,
                               SymbolIds::OpAmpNoSupplies,
                               SymbolIds::OTAWithSuppliesVerticalBias,
                               SymbolIds::OTANoSuppliesVerticalBias,
                               SymbolIds::DarlingtonBufferLM13700,
                               SymbolIds::RailVCC,
                               SymbolIds::GroundChassis,
                               SymbolIds::GroundSignal,
                               SymbolIds::PortInput,
                               SymbolIds::PortOutput,
                               SymbolIds::PortBidirectional}) {
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
        QCOMPARE(document.component(a)->value(), document.symbol(symbol)->defaultValue);
    }
}

QTEST_MAIN(SymbolPlacementTests)
#include "tst_symbolplacement.moc"
