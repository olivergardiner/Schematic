#include "../documenteditcontroller.h"
#include "../schematicitems.h"
#include "../schematicscene.h"

#include <QGraphicsItem>
#include <QtTest>

class SymbolPlacementTests : public QObject
{
    Q_OBJECT

private slots:
    void dropPlacementSnapsSelectsAndUndoRedoes();
    void unknownSymbolIsRejectedWithoutHistory();
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
                                    std::function<QVector<ComponentId>()> afterSelection) {
        return edits.commit(description, operation, std::move(afterSelection));
    });
    edits.setSelectionCallbacks([&scene]() { return scene.selectedComponentIds(); },
                               [&scene](const QVector<ComponentId> &selection) {
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
                                    std::function<QVector<ComponentId>()> afterSelection) {
        return edits.commit(description, operation, std::move(afterSelection));
    });

    QVERIFY(!scene.placeComponentAt(SymbolId(QStringLiteral("not-in-library")),
                                    QPointF(13, 17)));
    QVERIFY(document.components().isEmpty());
    QCOMPARE(edits.undoStack()->count(), 0);
}

QTEST_MAIN(SymbolPlacementTests)
#include "tst_symbolplacement.moc"
