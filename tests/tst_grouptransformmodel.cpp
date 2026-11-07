#include "tst_grouptransformmodel.h"
#include "document.h"
#include "documentloadresult.h"
#include <QTest>
#include <limits>

using Kind = SelectionTransform::Kind;

void TstGroupTransformModel::rigidInternalRoutesAndOrder()
{
    Document document;
    const auto left = document.addComponent(SymbolIds::Resistor, {3, 7});
    const auto right = document.addComponent(SymbolIds::Resistor, {103, 7});
    const auto wire = document.addWire(makeTerminalEndpoint(left, 1), {}, makeTerminalEndpoint(right, 0));
    QVERIFY(wire);
    Document sequential = document;
    QVERIFY(!sequential.moveComponent(left, {63, 7})); // transient collapse
    Document reordered = document;
    const QVector<QPointF> original = document.wire(*wire)->vertices;
    QVERIFY(document.transformSelection({left, right}, {}, {Kind::Translate, {60, 30}}));
    QVERIFY(reordered.transformSelection({right, left, right}, {}, {Kind::Translate, {60, 30}}));
    QCOMPARE(reordered.toJsonBytes(), document.toJsonBytes());
    QCOMPARE(document.component(left)->position(), QPointF(63, 37));
    QCOMPARE(document.component(right)->position(), QPointF(163, 37));
    QCOMPARE(document.wire(*wire)->vertices.size(), original.size());
    for (int i = 0; i < original.size(); ++i)
        QCOMPARE(document.wire(*wire)->vertices[i], original[i] + QPointF(60, 30));
    QCOMPARE(document.computeNets().size(), 3); // joined pair plus two free terminals
    QCOMPARE(std::get<TerminalRef>(document.wire(*wire)->start), (TerminalRef{left, 1}));
    const auto before = document.toJsonBytes();
    for (int i = 0; i < 4; ++i)
        QVERIFY(document.transformSelection({left, right}, {}, {Kind::Rotate, {}, {90.5, 12.5}, 1}));
    QCOMPARE(document.toJsonBytes(), before);
    QVERIFY(document.transformSelection({left, right}, {}, {Kind::Translate, {}}));
    QCOMPARE(document.toJsonBytes(), before);
}

void TstGroupTransformModel::branchesMoveOrAnchorByIdentity()
{
    Document original;
    const auto left = original.addComponent(SymbolIds::Resistor, {0, 0});
    const auto right = original.addComponent(SymbolIds::Resistor, {100, 0});
    const auto branch = original.addComponent(SymbolIds::Resistor, {50, 70}, Rotation::Deg90);
    const auto straight = original.addWire(makeTerminalEndpoint(left, 1), {}, makeTerminalEndpoint(right, 0));
    QVERIFY(straight);
    const auto leg = original.addWireBranching(BranchSite{*straight, {50, 0}}, {},
                                               makeTerminalEndpoint(branch, 0));
    QVERIFY(leg);
    const NodeId node = original.nodes().constBegin().key();
    QVector<WireId> allWires, horizontalWires;
    for (const auto &route : original.wires()) {
        allWires.append(route.id);
        if (route.id != *leg) horizontalWires.append(route.id);
    }
    Document full = original;
    QVERIFY(full.transformSelection({left, right, branch}, allWires, {Kind::Translate, {0, 10}}));
    QCOMPARE(full.nodes().value(node), QPointF(50, 10));
    QCOMPARE(full.junctionPoints(), QVector<QPointF>{QPointF(50, 10)});
    for (const auto &route : original.wires()) {
        QCOMPARE(full.wire(route.id)->vertices.size(), route.vertices.size());
        for (int i = 0; i < route.vertices.size(); ++i)
            QCOMPARE(full.wire(route.id)->vertices[i], route.vertices[i] + QPointF(0, 10));
    }
    Document partial = original;
    QVERIFY(partial.transformSelection({left, right}, horizontalWires, {Kind::Translate, {0, 10}}));
    QCOMPARE(partial.nodes().value(node), QPointF(50, 0));
    QCOMPARE(partial.component(branch)->position(), original.component(branch)->position());
    QCOMPARE(partial.wire(*leg)->vertices, original.wire(*leg)->vertices);
    QCOMPARE(partial.computeNets().first().terminals, original.computeNets().first().terminals);
    const auto loaded = Document::fromJsonBytes(partial.toJsonBytes());
    QVERIFY(loaded.document);
    QCOMPARE(loaded.document->toJsonBytes(), partial.toJsonBytes());
}

void TstGroupTransformModel::boundaryWiresAndAtomicRejection()
{
    Document document;
    const auto left = document.addComponent(SymbolIds::Resistor, {0, 0});
    const auto right = document.addComponent(SymbolIds::Resistor, {100, 100});
    const auto free = document.addComponent(SymbolIds::Capacitor, {-100, 0});
    const auto wire = document.addWire(makeTerminalEndpoint(left, 1), {{50, 0}, {50, 100}},
                                       makeTerminalEndpoint(right, 0));
    QVERIFY(wire);
    const auto before = document.toJsonBytes();
    // A selected wire's interior shifts; its terminal identities stay anchored.
    QVERIFY(document.transformSelection({}, {*wire}, {Kind::Translate, {0, -10}}));
    QCOMPARE(document.component(left)->position(), QPointF(0, 0));
    QCOMPARE(document.component(right)->position(), QPointF(100, 100));
    QCOMPARE(document.wire(*wire)->vertices.first(), QPointF(20, 0));
    QCOMPARE(document.wire(*wire)->vertices.last(), QPointF(80, 100));
    QCOMPARE(validateRouteGeometry(document.wire(*wire)->vertices), RouteGeometryError::None);
    QVERIFY(document.toJsonBytes() != before);
    const auto valid = document.toJsonBytes();
    // Force a reversal at the start of the unselected external route.
    QVERIFY(!document.transformSelection({left, free}, {}, {Kind::Translate, {100, 0}}));
    QCOMPARE(document.toJsonBytes(), valid);
    QCOMPARE(document.component(free)->position(), QPointF(-100, 0));
}

void TstGroupTransformModel::quadrantCompositionAndRoundTrip()
{
    Document original;
    const auto first = original.addComponent(SymbolIds::OpAmp, {3.5, 7.5}, Rotation::Deg90);
    const auto second = original.addComponent(SymbolIds::Resistor, {103.5, 7.5}, Rotation::Deg270);
    QVERIFY(original.setComponentMirrored(first, true));
    const QPointF pivot(42.25, 10.75);
    const auto before = original.toJsonBytes();
    for (Kind kind : {Kind::Rotate, Kind::FlipHorizontal, Kind::FlipVertical}) {
        Document changed = original;
        for (int iteration = 0; iteration < (kind == Kind::Rotate ? 4 : 2); ++iteration) {
            const Document prior = changed;
            QVERIFY(changed.transformSelection({first, second}, {}, {kind, {}, pivot, 1}));
            for (ComponentId id : {first, second}) {
                for (int terminal = 0; terminal < changed.terminalCount(*changed.component(id)); ++terminal) {
                    QPointF offset = prior.terminalPosition(*prior.component(id), terminal) - pivot;
                    if (kind == Kind::Rotate) offset = rotateOffset(offset, Rotation::Deg90);
                    else if (kind == Kind::FlipHorizontal) offset.setX(-offset.x());
                    else offset.setY(-offset.y());
                    QCOMPARE(changed.terminalPosition(*changed.component(id), terminal), pivot + offset);
                }
            }
            const auto loaded = Document::fromJsonBytes(changed.toJsonBytes());
            QVERIFY(loaded.document);
            QCOMPARE(loaded.document->toJsonBytes(), changed.toJsonBytes());
        }
        QCOMPARE(changed.toJsonBytes(), before);
    }
}

void TstGroupTransformModel::invalidRequestsDoNotMutate()
{
    Document document;
    const auto id = document.addComponent(SymbolIds::Resistor, {0, 0});
    const auto before = document.toJsonBytes();
    QVERIFY(!document.transformSelection({}, {}, {}));
    QVERIFY(!document.transformSelection({id, 999}, {}, {Kind::Translate, {10, 0}}));
    QVERIFY(!document.transformSelection({id}, {999}, {Kind::Translate, {10, 0}}));
    QVERIFY(!document.transformSelection({id}, {}, {Kind::Translate,
                                          {std::numeric_limits<double>::infinity(), 0}}));
    QVERIFY(!document.transformSelection({id}, {}, {static_cast<Kind>(999)}));
    QCOMPARE(document.toJsonBytes(), before);
}
