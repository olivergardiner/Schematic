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

void TstDocument::addWireBranchingAppliesSplitsAtomically()
{
    Document document;
    const ComponentId a = document.addComponent(SymbolKind::Resistor, QPointF(0, 0));
    const ComponentId b = document.addComponent(SymbolKind::Resistor, QPointF(100, 0));
    const ComponentId c = document.addComponent(SymbolKind::Resistor, QPointF(50, 50));
    const auto wire = document.addWire(makeTerminalEndpoint(a, 1), {}, makeTerminalEndpoint(b, 0));
    QVERIFY(wire);

    const auto bridge = document.addWireBranching(BranchSite{*wire, QPointF(40, 0)},
                                                   {QPointF(40, 50)}, makeTerminalEndpoint(c, 0));
    QVERIFY(bridge);
    QVERIFY(!document.wire(*wire)); // The branch split retires the original route identity.
    QCOMPARE(document.wires().size(), 3); // left leg + right leg + the new bridge wire.
    QCOMPARE(document.nodes().size(), 1);
    QCOMPARE(document.nodes().value(1), QPointF(40, 0));
    QCOMPARE(document.junctionPoints(), QVector<QPointF>({QPointF(40, 0)}));

    const QVector<Net> nets = document.computeNets();
    QCOMPARE(netContaining(nets, TerminalRef{a, 1}), netContaining(nets, TerminalRef{b, 0}));
    QCOMPARE(netContaining(nets, TerminalRef{a, 1}), netContaining(nets, TerminalRef{c, 0}));
}

void TstDocument::addWireBranchingFailureLeavesDocumentUnchanged()
{
    Document document;
    const ComponentId a = document.addComponent(SymbolKind::Resistor, QPointF(0, 0));
    const ComponentId b = document.addComponent(SymbolKind::Resistor, QPointF(100, 0));
    const ComponentId c = document.addComponent(SymbolKind::Resistor, QPointF(50, 50));
    const auto wire = document.addWire(makeTerminalEndpoint(a, 1), {}, makeTerminalEndpoint(b, 0));
    QVERIFY(wire);
    const int wiresBefore = document.wires().size();
    const int nodesBefore = document.nodes().size();

    // Rejected: both endpoints branch off the same wire (current scope limit).
    QVERIFY(!document.addWireBranching(BranchSite{*wire, QPointF(30, 0)}, {},
                                       BranchSite{*wire, QPointF(70, 0)}));
    QCOMPARE(document.wires().size(), wiresBefore);
    QCOMPARE(document.nodes().size(), nodesBefore);
    QVERIFY(document.wire(*wire)); // The original route must not have been split.

    // Rejected: the branch point does not lie on the given wire.
    QVERIFY(!document.addWireBranching(BranchSite{*wire, QPointF(500, 500)}, {},
                                       makeTerminalEndpoint(c, 0)));
    QCOMPARE(document.wires().size(), wiresBefore);
    QCOMPARE(document.nodes().size(), nodesBefore);

    // Rejected: the completed route's own geometry is invalid (diagonal).
    QVERIFY(!document.addWireBranching(BranchSite{*wire, QPointF(40, 0)}, {},
                                       makeTerminalEndpoint(c, 0)));
    QCOMPARE(document.wires().size(), wiresBefore);
    QCOMPARE(document.nodes().size(), nodesBefore);
    QVERIFY(document.wire(*wire)); // Still not split by any of the failed attempts.

    // None of the failures above may have advanced node/wire ID allocation:
    // a subsequent successful branch must still get the first available IDs.
    const auto bridge = document.addWireBranching(BranchSite{*wire, QPointF(40, 0)},
                                                   {QPointF(40, 50)}, makeTerminalEndpoint(c, 0));
    QVERIFY(bridge);
    QCOMPARE(document.nodes().size(), 1);
    QVERIFY(document.nodes().contains(1)); // The first available NodeId, unconsumed by failures above.
}

void TstDocument::setComponentLabelsTrimsBeforeValidationAndStorage()
{
    Document document;
    const ComponentId a = document.addComponent(SymbolKind::Resistor, QPointF(0, 0));

    QVERIFY(document.setComponentLabels(a, QStringLiteral(" R1 "), QStringLiteral(" 10k ")));
    QCOMPARE(document.component(a)->reference(), QStringLiteral("R1"));
    QCOMPARE(document.component(a)->value(), QStringLiteral("10k"));
}

void TstDocument::setComponentLabelsRejectsEmptyReference()
{
    Document document;
    const ComponentId a = document.addComponent(SymbolKind::Resistor, QPointF(0, 0));
    document.setComponentLabels(a, QStringLiteral("R1"), QStringLiteral("10k"));

    QVERIFY(!document.setComponentLabels(a, QString(), QStringLiteral("22k")));
    QVERIFY(!document.setComponentLabels(a, QStringLiteral("   "), QStringLiteral("22k")));
    // Rejected calls must not change the existing labels.
    QCOMPARE(document.component(a)->reference(), QStringLiteral("R1"));
    QCOMPARE(document.component(a)->value(), QStringLiteral("10k"));
}

void TstDocument::setComponentLabelsRejectsDuplicateReferenceButAllowsSelfRename()
{
    Document document;
    const ComponentId a = document.addComponent(SymbolKind::Resistor, QPointF(0, 0));
    const ComponentId b = document.addComponent(SymbolKind::Resistor, QPointF(100, 0));
    QVERIFY(document.setComponentLabels(a, QStringLiteral("R1"), QString()));
    QVERIFY(document.setComponentLabels(b, QStringLiteral("R2"), QString()));

    // Duplicate (including a whitespace-variant duplicate) is rejected.
    QVERIFY(!document.setComponentLabels(b, QStringLiteral("R1"), QString()));
    QVERIFY(!document.setComponentLabels(b, QStringLiteral(" R1 "), QString()));
    QCOMPARE(document.component(b)->reference(), QStringLiteral("R2"));

    // Renaming a component to its own current reference is allowed.
    QVERIFY(document.setComponentLabels(a, QStringLiteral("R1"), QStringLiteral("10k")));
    QCOMPARE(document.component(a)->reference(), QStringLiteral("R1"));
    QCOMPARE(document.component(a)->value(), QStringLiteral("10k"));
}

void TstDocument::setComponentLabelsValueOnlyEditSucceeds()
{
    Document document;
    const ComponentId a = document.addComponent(SymbolKind::Resistor, QPointF(0, 0));
    QVERIFY(document.setComponentLabels(a, QStringLiteral("R1"), QStringLiteral("10k")));

    QVERIFY(document.setComponentLabels(a, QStringLiteral("R1"), QStringLiteral("22k")));
    QCOMPARE(document.component(a)->reference(), QStringLiteral("R1"));
    QCOMPARE(document.component(a)->value(), QStringLiteral("22k"));
}

void TstDocument::routeValidationRejectsCollinearBacktracking()
{
    // Horizontal and vertical reversals, including a full retrace.
    QCOMPARE(validateRouteGeometry({QPointF(0, 0), QPointF(50, 0), QPointF(30, 0)}),
             RouteGeometryError::CollinearBacktrack);
    QCOMPARE(validateRouteGeometry({QPointF(0, 0), QPointF(0, 50), QPointF(0, 30)}),
             RouteGeometryError::CollinearBacktrack);
    QCOMPARE(validateRouteGeometry({QPointF(0, 0), QPointF(50, 0), QPointF(0, 0)}),
             RouteGeometryError::CollinearBacktrack);
    // A reversal after a bend, away from the first segment.
    QCOMPARE(validateRouteGeometry({QPointF(0, 10), QPointF(0, 0), QPointF(50, 0), QPointF(30, 0)}),
             RouteGeometryError::CollinearBacktrack);

    // Collinear segments that keep their direction and genuine bends stay valid.
    QCOMPARE(validateRouteGeometry({QPointF(0, 0), QPointF(30, 0), QPointF(50, 0)}),
             RouteGeometryError::None);
    QCOMPARE(validateRouteGeometry({QPointF(50, 0), QPointF(30, 0), QPointF(0, 0)}),
             RouteGeometryError::None);
    QCOMPARE(validateRouteGeometry({QPointF(0, 0), QPointF(50, 0), QPointF(50, 30), QPointF(20, 30)}),
             RouteGeometryError::None);

    // The same validator guards new routes: a reversing route is rejected.
    Document document;
    const ComponentId a = document.addComponent(SymbolKind::Resistor, QPointF(0, 0));
    const ComponentId b = document.addComponent(SymbolKind::Resistor, QPointF(100, 0));
    QVERIFY(!document.addWire(makeTerminalEndpoint(a, 1), {QPointF(90, 0)},
                              makeTerminalEndpoint(b, 0)));
    QVERIFY(document.wires().isEmpty());
}

void TstDocument::movementRejectsCollinearReversal()
{
    Document document;
    const ComponentId a = document.addComponent(SymbolKind::Resistor, QPointF(0, 0));
    const ComponentId b = document.addComponent(SymbolKind::Resistor, QPointF(100, 0));
    // Terminals sit 20 units either side of the component origin: this route
    // runs (20,0) -> (60,0) -> (80,0) and keeps its direction.
    const auto id = document.addWire(makeTerminalEndpoint(a, 1), {QPointF(60, 0)},
                                     makeTerminalEndpoint(b, 0));
    QVERIFY(id);
    const QVector<QPointF> before = document.wire(*id)->vertices;

    // Moving b left puts its terminal at (40,0), behind the corner at (60,0).
    QVERIFY(!document.moveComponent(b, QPointF(60, 0)));
    QCOMPARE(document.component(b)->position(), QPointF(100, 0));
    QCOMPARE(document.wire(*id)->vertices, before);

    // A move that keeps the same direction is still accepted.
    QVERIFY(document.moveComponent(b, QPointF(110, 0)));
    QCOMPARE(document.wire(*id)->vertices,
             QVector<QPointF>({QPointF(20, 0), QPointF(60, 0), QPointF(90, 0)}));
}

void TstDocument::addWireBranchingRejectsReversalWithoutSideEffects()
{
    Document document;
    const ComponentId a = document.addComponent(SymbolKind::Resistor, QPointF(0, 0));
    const ComponentId b = document.addComponent(SymbolKind::Resistor, QPointF(100, 0));
    // d's terminal 1 is at (60,0).
    const ComponentId d = document.addComponent(SymbolKind::Resistor, QPointF(40, 0));
    const auto wire = document.addWire(makeTerminalEndpoint(a, 1), {}, makeTerminalEndpoint(b, 0));
    QVERIFY(wire);
    const int wiresBefore = document.wires().size();

    // (40,0) -> (70,0) -> (60,0) doubles back on itself.
    QVERIFY(!document.addWireBranching(BranchSite{*wire, QPointF(40, 0)}, {QPointF(70, 0)},
                                       makeTerminalEndpoint(d, 1)));
    QCOMPARE(document.wires().size(), wiresBefore);
    QCOMPARE(document.nodes().size(), 0);
    QVERIFY(document.wire(*wire)); // Not split by the failed attempt.

    // The failed attempt consumed no IDs: a valid route gets the first node.
    QVERIFY(document.addWireBranching(BranchSite{*wire, QPointF(40, 0)},
                                      {QPointF(40, -20), QPointF(60, -20)},
                                      makeTerminalEndpoint(d, 1)));
    QVERIFY(document.nodes().contains(1));
}

void TstDocument::rotateAndMirrorUpdateRoutesTransactionally()
{
    Document d;
    const ComponentId a = d.addComponent(SymbolKind::Resistor, {0, 0});
    const ComponentId b = d.addComponent(SymbolKind::Resistor, {40, 0}, Rotation::Deg180);
    // b.0 is at (60,0); a.1 at (20,0).
    const auto w = d.addWire(makeTerminalEndpoint(a, 1), {}, makeTerminalEndpoint(b, 0));
    QVERIFY(w);
    const QVector<QPointF> before = d.wire(*w)->vertices;

    // Rotating b to 0 would put b.0 on (20,0): zero-length route -> rejected.
    QVERIFY(!d.rotateComponent(b, Rotation::Deg0));
    QCOMPARE(d.component(b)->rotation(), Rotation::Deg180);
    QCOMPARE(d.wire(*w)->vertices, before);

    // Mirroring b makes b.0 land on (20,0) as well -> rejected.
    QVERIFY(!d.setComponentMirrored(b, true));
    QVERIFY(!d.component(b)->mirrored());
    QCOMPARE(d.wire(*w)->vertices, before);

    // A valid mirror of a moves a.1 to (-20,0) and the route follows.
    QVERIFY(d.setComponentMirrored(a, true));
    QCOMPARE(d.wire(*w)->vertices.first(), QPointF(-20, 0));
    QCOMPARE(d.wire(*w)->vertices.last(), QPointF(60, 0));

    QVERIFY(!d.rotateComponent(999, Rotation::Deg90));
    QVERIFY(!d.setComponentMirrored(999, true));
}

void TstDocument::defaultReferencesUseSmallestUnusedPrefixNumber()
{
    Document d;
    const ComponentId r1 = d.addComponent(SymbolKind::Resistor, {0, 0});
    d.addComponent(SymbolKind::Resistor, {50, 0});
    const ComponentId c1 = d.addComponent(SymbolKind::Capacitor, {100, 0});
    const ComponentId g = d.addComponent(SymbolKind::Ground, {150, 0});
    const ComponentId rv = d.addComponent(SymbolKind::Potentiometer, {200, 0});
    const ComponentId u = d.addComponent(SymbolKind::OpAmp, {250, 0});
    QCOMPARE(d.component(r1)->reference(), QStringLiteral("R1"));
    QCOMPARE(d.components()[1].reference(), QStringLiteral("R2"));
    QCOMPARE(d.component(c1)->reference(), QStringLiteral("C1"));
    QCOMPARE(d.component(g)->reference(), QStringLiteral("GND1"));
    QCOMPARE(d.component(rv)->reference(), QStringLiteral("VR1"));
    QCOMPARE(d.component(u)->reference(), QStringLiteral("U1"));

    QVERIFY(d.setComponentLabels(r1, QStringLiteral("X"), QString()));
    const ComponentId r3 = d.addComponent(SymbolKind::Resistor, {300, 0});
    QCOMPARE(d.component(r3)->reference(), QStringLiteral("R1")); // freed number reused
}