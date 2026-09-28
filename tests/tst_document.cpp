#include "tst_document.h"

#include "document.h"

#include <QTest>

#include <limits>

namespace {

int netContaining(const QVector<Net> &nets, TerminalRef wanted)
{
    for (int i = 0; i < nets.size(); ++i) {
        if (nets[i].terminals.contains(wanted))
            return i;
    }
    return -1;
}

} // namespace

void TstDocument::connectivityUsesEndpointIdentity()
{
    Document document;
    const ComponentId a = document.addComponent(SymbolKind::Resistor, QPointF(0, 0));
    const ComponentId b = document.addComponent(SymbolKind::Resistor, QPointF(100, 0));
    const ComponentId c = document.addComponent(SymbolKind::Resistor, QPointF(50, -50), Rotation::Deg90);
    const ComponentId d = document.addComponent(SymbolKind::Resistor, QPointF(50, 50), Rotation::Deg90);
    QVERIFY(document.addWire(makeTerminalEndpoint(a, 1), {}, makeTerminalEndpoint(b, 0)));
    QVERIFY(document.addWire(makeTerminalEndpoint(c, 1), {}, makeTerminalEndpoint(d, 0)));

    const QVector<Net> nets = document.computeNets();
    QCOMPARE(nets.size(), 6); // 8 terminals, with two independent pairs joined.
    const int horizontal = netContaining(nets, TerminalRef{a, 1});
    const int vertical = netContaining(nets, TerminalRef{c, 1});
    QVERIFY(horizontal >= 0);
    QVERIFY(vertical >= 0);
    QCOMPARE(horizontal, netContaining(nets, TerminalRef{b, 0}));
    QCOMPARE(vertical, netContaining(nets, TerminalRef{d, 0}));
    QVERIFY(horizontal != vertical); // The routes cross at (50, 0) only geometrically.
}

void TstDocument::movementPreservesOrthogonalRoutesAndNets()
{
    Document document;
    const ComponentId a = document.addComponent(SymbolKind::Resistor, QPointF(0, 0));
    const ComponentId b = document.addComponent(SymbolKind::Resistor, QPointF(100, 0));
    const auto id = document.addWire(makeTerminalEndpoint(a, 1), {}, makeTerminalEndpoint(b, 0));
    QVERIFY(id);
    const QVector<Net> before = document.computeNets();

    QVERIFY(document.moveComponent(a, QPointF(10, 0)));
    QCOMPARE(document.wire(*id)->vertices, QVector<QPointF>({QPointF(30, 0), QPointF(80, 0)}));
    QVERIFY(document.moveComponent(a, QPointF(10, 10)));
    const WireRoute *route = document.wire(*id);
    QVERIFY(route);
    QCOMPARE(route->vertices, QVector<QPointF>({QPointF(30, 10), QPointF(30, 0), QPointF(80, 0)}));
    QCOMPARE(route->vertices.last(), QPointF(80, 0));
    QCOMPARE(validateRouteGeometry(route->vertices), RouteGeometryError::None);
    const QVector<Net> after = document.computeNets();
    QCOMPARE(netContaining(after, TerminalRef{a, 1}), netContaining(after, TerminalRef{b, 0}));
    QCOMPARE(before.size(), after.size());
}

void TstDocument::coincidentDirectEndpointsRejectMove()
{
    Document document;
    const ComponentId a = document.addComponent(SymbolKind::Resistor, QPointF(0, 0));
    const ComponentId b = document.addComponent(SymbolKind::Resistor, QPointF(80, 0));
    const auto id = document.addWire(makeTerminalEndpoint(a, 1), {}, makeTerminalEndpoint(b, 0));
    QVERIFY(id);
    const QVector<QPointF> before = document.wire(*id)->vertices;

    QVERIFY(!document.moveComponent(b, QPointF(40, 0)));
    QCOMPARE(document.component(b)->position(), QPointF(80, 0));
    QCOMPARE(document.wire(*id)->vertices, before);
}

void TstDocument::movementCollapsesRedundantCorner()
{
    Document document;
    const ComponentId a = document.addComponent(SymbolKind::Resistor, QPointF(0, 0));
    const ComponentId b = document.addComponent(SymbolKind::Resistor, QPointF(40, 40));
    const auto id = document.addWire(makeTerminalEndpoint(a, 1), {QPointF(20, 20)},
                                     makeTerminalEndpoint(b, 0));
    QVERIFY(id);

    QVERIFY(document.moveComponent(a, QPointF(0, 20)));
    const WireRoute *route = document.wire(*id);
    QVERIFY(route);
    QCOMPARE(route->vertices, QVector<QPointF>({QPointF(20, 20), QPointF(20, 40)}));
    QCOMPARE(validateRouteGeometry(route->vertices), RouteGeometryError::None);
}

void TstDocument::componentDeletionCascadesAcrossBranches()
{
    Document document;
    const ComponentId a = document.addComponent(SymbolKind::Resistor, QPointF(0, 0));
    const ComponentId b = document.addComponent(SymbolKind::Resistor, QPointF(100, 0));
    const ComponentId c = document.addComponent(SymbolKind::Resistor, QPointF(60, 40));
    const auto wire = document.addWire(makeTerminalEndpoint(a, 1), {}, makeTerminalEndpoint(b, 0));
    QVERIFY(wire);
    const auto node = document.branchWireAt(*wire, QPointF(40, 0));
    QVERIFY(node);
    QVERIFY(document.addWire(makeNodeEndpoint(*node), {}, makeTerminalEndpoint(c, 0)));
    QCOMPARE(document.junctionPoints(), QVector<QPointF>({QPointF(40, 0)}));

    QVERIFY(document.removeComponent(c));
    QVERIFY(document.junctionPoints().isEmpty()); // Two route ends now pass through the node.
    QVERIFY(document.nodes().contains(*node));
    QVERIFY(document.removeComponent(a));
    QCOMPARE(document.wires().size(), 1);
    QVERIFY(document.nodes().contains(*node));
    const WireId remaining = document.wires().first().id;
    QVERIFY(document.removeWire(remaining));
    QVERIFY(!document.nodes().contains(*node));
    QVERIFY(!document.addWire(makeNodeEndpoint(*node), {}, makeTerminalEndpoint(b, 1)));
}

void TstDocument::branchingSplitsRoutesAtInteriorPoints()
{
    Document document;
    const ComponentId a = document.addComponent(SymbolKind::Resistor, QPointF(0, 0));
    const ComponentId b = document.addComponent(SymbolKind::Resistor, QPointF(100, 0));
    const ComponentId c = document.addComponent(SymbolKind::Resistor, QPointF(60, 40));
    const auto wire = document.addWire(makeTerminalEndpoint(a, 1), {}, makeTerminalEndpoint(b, 0));
    QVERIFY(wire);
    const auto node = document.branchWireAt(*wire, QPointF(40, 0));
    QVERIFY(node);
    QCOMPARE(document.wires().size(), 2);
    QVERIFY(!document.wire(*wire)); // Split retires the original route identity.
    QVERIFY(document.addWire(makeNodeEndpoint(*node), {}, makeTerminalEndpoint(c, 0)));
    QCOMPARE(document.junctionPoints(), QVector<QPointF>({QPointF(40, 0)}));
    const QVector<Net> nets = document.computeNets();
    QCOMPARE(netContaining(nets, TerminalRef{a, 1}), netContaining(nets, TerminalRef{b, 0}));
    QCOMPARE(netContaining(nets, TerminalRef{a, 1}), netContaining(nets, TerminalRef{c, 0}));

    const auto another = document.addWire(makeTerminalEndpoint(a, 0),
                                          {QPointF(-20, 20), QPointF(120, 20)},
                                          makeTerminalEndpoint(b, 1));
    QVERIFY(another);
    const int routeCountBeforeRejectedBranches = document.wires().size();
    QVERIFY(!document.branchWireAt(*another, QPointF(10, 10)));
    QVERIFY(!document.branchWireAt(*another, QPointF(-20, 0)));
    QCOMPARE(document.wires().size(), routeCountBeforeRejectedBranches);

    const auto cornerBranch = document.branchWireAt(*another, QPointF(-20, 20));
    QVERIFY(cornerBranch);
    QVERIFY(!document.wire(*another));
    QCOMPARE(document.wires().size(), 5);
    for (const WireRoute &route : document.wires())
        QCOMPARE(validateRouteGeometry(route.vertices), RouteGeometryError::None);
}

void TstDocument::invalidRoutesAreRejectedAtomically()
{
    Document document;
    const ComponentId a = document.addComponent(SymbolKind::Resistor, QPointF(0, 0));
    const ComponentId b = document.addComponent(SymbolKind::Resistor, QPointF(100, 0));
    const int componentCount = document.components().size();
    const auto reject = [&document, a, b](const QVector<QPointF> &corners) {
        return !document.addWire(makeTerminalEndpoint(a, 1), corners, makeTerminalEndpoint(b, 0));
    };
    QVERIFY(reject({QPointF(40, 10)})); // Diagonal from the first endpoint.
    QVERIFY(reject({QPointF(20, 0)})); // Zero-length first segment.
    QVERIFY(reject({QPointF(std::numeric_limits<qreal>::infinity(), 0)}));
    QVERIFY(!document.addWire(makeTerminalEndpoint(a, 9), {}, makeTerminalEndpoint(b, 0)));
    QVERIFY(!document.addWire(makeTerminalEndpoint(999, 0), {}, makeTerminalEndpoint(b, 0)));
    QVERIFY(!document.addWire(makeTerminalEndpoint(a, 1), {}, makeNodeEndpoint(999)));
    QCOMPARE(document.components().size(), componentCount);
    QVERIFY(document.wires().isEmpty());
    QCOMPARE(document.nodes().size(), 0);
}

void TstDocument::gridSpacingMustBePositiveAndFinite()
{
    Document document;
    QCOMPARE(document.gridSpacing(), 10.0);
    QVERIFY(document.setGridSpacing(12.5));
    QCOMPARE(document.gridSpacing(), 12.5);
    QVERIFY(!document.setGridSpacing(0.0));
    QVERIFY(!document.setGridSpacing(-1.0));
    QVERIFY(!document.setGridSpacing(std::numeric_limits<qreal>::quiet_NaN()));
    QCOMPARE(document.gridSpacing(), 12.5);
}
