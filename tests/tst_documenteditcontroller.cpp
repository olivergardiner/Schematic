#include "../documenteditcontroller.h"

#include <QTest>

class TstDocumentEditController : public QObject
{
    Q_OBJECT

private slots:
    void fillArtworkUndoRoundTrip();
    void snapshotUndoRedoAndCleanIndex();
    void rejectedAndNoOpEditsDoNotEnterHistory();
    void branchedWireIsOneAtomicCommand();
    void labelsGridTransformsAndDeletionRoundTrip();
};

void TstDocumentEditController::snapshotUndoRedoAndCleanIndex()
{
    Document document;
    DocumentEditController edits(&document);
    DocumentSelection selection;
    edits.setSelectionCallbacks([&selection]() { return selection; },
                                [&selection](const DocumentSelection &restored) {
                                    selection = restored;
                                });

    ComponentId added = kInvalidComponentId;
    QVERIFY(edits.commit(QStringLiteral("Place component"), [&]() {
        added = document.addComponent(SymbolIds::Resistor, QPointF(0, 0));
        if (added == kInvalidComponentId)
            return false;
        selection = {{added}, {}};
        return true;
    }));
    QCOMPARE(document.components().size(), 1);
    QCOMPARE(edits.undoStack()->count(), 1);
    QVERIFY(!edits.undoStack()->isClean());
    QCOMPARE(selection.components, QVector<ComponentId>({added}));

    edits.setClean();
    QVERIFY(edits.undoStack()->isClean());
    QVERIFY(edits.commit(QStringLiteral("Move component"), [&]() {
        return document.moveComponent(added, QPointF(10, 0));
    }));
    QVERIFY(!edits.undoStack()->isClean());
    QCOMPARE(document.component(added)->position(), QPointF(10, 0));

    edits.undoStack()->undo();
    QVERIFY(edits.undoStack()->isClean());
    QCOMPARE(document.component(added)->position(), QPointF(0, 0));
    QCOMPARE(selection.components, QVector<ComponentId>({added}));

    edits.undoStack()->redo();
    QVERIFY(!edits.undoStack()->isClean());
    QCOMPARE(document.component(added)->position(), QPointF(10, 0));
    QCOMPARE(selection.components, QVector<ComponentId>({added}));

    edits.resetHistory();
    QVERIFY(edits.undoStack()->isClean());
    QCOMPARE(edits.undoStack()->count(), 0);
}

void TstDocumentEditController::rejectedAndNoOpEditsDoNotEnterHistory()
{
    Document document;
    DocumentEditController edits(&document);
    const QByteArray initial = document.toJsonBytes();

    QVERIFY(!edits.commit(QStringLiteral("Rejected"), []() { return false; }));
    QVERIFY(!edits.commit(QStringLiteral("No-op"), [&]() {
        return document.setGridSpacing(document.gridSpacing());
    }));
    QCOMPARE(document.toJsonBytes(), initial);
    QCOMPARE(edits.undoStack()->count(), 0);
    QVERIFY(edits.undoStack()->isClean());
}

void TstDocumentEditController::branchedWireIsOneAtomicCommand()
{
    Document document;
    const ComponentId left = document.addComponent(SymbolIds::Resistor, QPointF(0, 0));
    const ComponentId right = document.addComponent(SymbolIds::Resistor, QPointF(100, 0));
    const ComponentId branch = document.addComponent(SymbolIds::Resistor,
                                                      QPointF(50, 70), Rotation::Deg90);
    const auto original = document.addWire(makeTerminalEndpoint(left, 1), {},
                                           makeTerminalEndpoint(right, 0));
    QVERIFY(original);
    const QByteArray before = document.toJsonBytes();
    DocumentEditController edits(&document);

    QVERIFY(edits.commit(QStringLiteral("Draw wire"), [&]() {
        return document.addWireBranching(BranchSite{*original, QPointF(50, 0)}, {},
                                          makeTerminalEndpoint(branch, 0)).has_value();
    }));
    QCOMPARE(edits.undoStack()->count(), 1);
    QCOMPARE(document.wires().size(), 3);
    QCOMPARE(document.nodes().size(), 1);

    edits.undoStack()->undo();
    QCOMPARE(document.toJsonBytes(), before);
    QCOMPARE(document.wires().size(), 1);
    QVERIFY(document.nodes().isEmpty());

    edits.undoStack()->redo();
    QCOMPARE(document.wires().size(), 3);
    QCOMPARE(document.nodes().size(), 1);
}

void TstDocumentEditController::labelsGridTransformsAndDeletionRoundTrip()
{
    Document document;
    const ComponentId first = document.addComponent(SymbolIds::Resistor, QPointF(0, 0));
    const ComponentId second = document.addComponent(SymbolIds::Capacitor, QPointF(100, 0));
    DocumentEditController edits(&document);

    QVERIFY(edits.commit(QStringLiteral("Edit labels"), [&]() {
        return document.setComponentLabels(first, QStringLiteral("R10"),
                                           QStringLiteral("10k"));
    }));
    QCOMPARE(document.component(first)->reference(), QStringLiteral("R10"));
    edits.undoStack()->undo();
    QCOMPARE(document.component(first)->reference(), QStringLiteral("R1"));
    edits.undoStack()->redo();
    QCOMPARE(document.component(first)->value(), QStringLiteral("10k"));

    QVERIFY(edits.commit(QStringLiteral("Change grid spacing"), [&]() {
        return document.setGridSpacing(5.0);
    }));
    QCOMPARE(document.gridSpacing(), 5.0);
    edits.undoStack()->undo();
    QCOMPARE(document.gridSpacing(), 10.0);
    edits.undoStack()->redo();
    QCOMPARE(document.gridSpacing(), 5.0);

    QVERIFY(edits.commit(QStringLiteral("Rotate component"), [&]() {
        return document.rotateComponent(first, Rotation::Deg90);
    }));
    QCOMPARE(document.component(first)->rotation(), Rotation::Deg90);
    edits.undoStack()->undo();
    QCOMPARE(document.component(first)->rotation(), Rotation::Deg0);
    edits.undoStack()->redo();
    QCOMPARE(document.component(first)->rotation(), Rotation::Deg90);

    QVERIFY(edits.commit(QStringLiteral("Delete component"), [&]() {
        return document.removeComponent(second);
    }));
    QVERIFY(!document.component(second));
    edits.undoStack()->undo();
    QVERIFY(document.component(second));
    edits.undoStack()->redo();
    QVERIFY(!document.component(second));
}

void TstDocumentEditController::fillArtworkUndoRoundTrip()
{
    Document document;
    const auto id = document.addComponent(SymbolIds::Resistor, {0,0});
    auto changed = *document.symbol(SymbolIds::Resistor);
    changed.primitives.first().fillColor = PrimitiveFillColor::Black;
    DocumentEditController edits(&document);
    const auto before = document.toJsonBytes();
    QStringList errors;
    QVERIFY(edits.commit("Change artwork fill", [&]() { return document.useSymbol(changed, &errors); }));
    QCOMPARE(document.symbol(SymbolIds::Resistor)->primitives.first().fillColor, PrimitiveFillColor::Black);
    const auto black = document.toJsonBytes();
    edits.undoStack()->undo();
    QCOMPARE(document.toJsonBytes(), before);
    QVERIFY(edits.undoStack()->isClean());
    edits.undoStack()->redo();
    QCOMPARE(document.toJsonBytes(), black);
    QCOMPARE(document.component(id)->position(), QPointF(0,0));
}

QTEST_GUILESS_MAIN(TstDocumentEditController)
#include "tst_documenteditcontroller.moc"
