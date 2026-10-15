#include "tst_documentjson.h"

#include "document.h"
#include "documentloadresult.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTest>

namespace {
QJsonObject asVersion1(QJsonObject root)
{
    root.insert(QStringLiteral("formatVersion"), 1);
    QJsonArray comps = root.value(QStringLiteral("components")).toArray();
    for (int i = 0; i < comps.size(); ++i) {
        QJsonObject c = comps[i].toObject();
        c.remove(QStringLiteral("mirrored"));
        comps[i] = c;
    }
    root.insert(QStringLiteral("components"), comps);
    return root;
}
QJsonObject withComponentField(QJsonObject root, const QString &key, const QJsonValue &v)
{
    QJsonArray comps = root.value(QStringLiteral("components")).toArray();
    QJsonObject c = comps[0].toObject();
    if (v.isUndefined()) c.remove(key); else c.insert(key, v);
    comps[0] = c;
    root.insert(QStringLiteral("components"), comps);
    return root;
}
}

void TstDocumentJson::roundTripsBranchedDocument()
{
    Document original;
    const auto a = original.addComponent(SymbolIds::Resistor, {0, 0});
    const auto b = original.addComponent(SymbolIds::Resistor, {100, 0});
    const auto c = original.addComponent(SymbolIds::Resistor, {40, 50}, Rotation::Deg90);
    const auto wire = original.addWire(makeTerminalEndpoint(a, 1), {QPointF(40, 0)}, makeTerminalEndpoint(b, 0));
    QVERIFY(wire);
    const auto node = original.branchWireAt(*wire, {40, 0});
    QVERIFY(node);
    QVERIFY(original.addWire(makeNodeEndpoint(*node), {}, makeTerminalEndpoint(c, 0)));

    const DocumentLoadResult loaded = Document::fromJsonBytes(original.toJsonBytes());
    QVERIFY2(loaded.errors.isEmpty(), qPrintable(loaded.errors.join(QLatin1Char('\n'))));
    QVERIFY(loaded.document);
    QCOMPARE(loaded.document->components().size(), original.components().size());
    QCOMPARE(loaded.document->wires().size(), original.wires().size());
    QCOMPARE(loaded.document->nodes(), original.nodes());
    QCOMPARE(loaded.document->junctionPoints(), original.junctionPoints());
    QCOMPARE(loaded.document->computeNets().size(), original.computeNets().size());
    QCOMPARE(loaded.document->component(a)->reference(), QStringLiteral("R1"));
    QCOMPARE(loaded.document->gridSpacing(), original.gridSpacing());
}

void TstDocumentJson::rejectsInvalidAndRepairsStaleEndpoints()
{
    Document existing;
    existing.addComponent(SymbolIds::Diode, {5, 6});
    const DocumentLoadResult bad = Document::fromJsonBytes("{ broken");
    QVERIFY(!bad.errors.isEmpty());
    QCOMPARE(existing.components().size(), 1); // A failed factory cannot mutate its caller.

    Document source;
    const auto id = source.addComponent(SymbolIds::Resistor, {0, 0});
    QVERIFY(source.addWire(makeTerminalEndpoint(id, 1), {}, makeTerminalEndpoint(id, 0)));
    QJsonObject root = source.toJson();
    QJsonArray wires = root.value(QStringLiteral("wires")).toArray();
    QJsonObject wire = wires[0].toObject();
    QJsonArray points = wire.value(QStringLiteral("points")).toArray();
    points[0] = QJsonArray{999, 999};
    wire.insert(QStringLiteral("points"), points);
    wires[0] = wire;
    root.insert(QStringLiteral("wires"), wires);
    const DocumentLoadResult repaired = Document::fromJson(root);
    QVERIFY2(repaired.errors.isEmpty(), qPrintable(repaired.errors.join(QLatin1Char('\n'))));
    QVERIFY(repaired.document);
    QVERIFY(!repaired.warnings.isEmpty());
    QCOMPARE(repaired.document->wires().first().vertices.first(), QPointF(20, 0));

    root.insert(QStringLiteral("formatVersion"), 3);
    QVERIFY(!Document::fromJson(root).document);
}

void TstDocumentJson::discardsOrphanNodes()
{
    Document doc;
    QJsonObject root = doc.toJson();
    root.insert(QStringLiteral("nodes"), QJsonArray{QJsonObject{{QStringLiteral("id"), 3},
        {QStringLiteral("x"), 1}, {QStringLiteral("y"), 2}}});
    const DocumentLoadResult loaded = Document::fromJson(root);
    QVERIFY(loaded.document);
    QVERIFY(loaded.document->nodes().isEmpty());
    QCOMPARE(loaded.warnings.size(), 1);
}

void TstDocumentJson::rejectsInvalidIdsReferencesAndGeometry()
{
    Document source;
    const auto a = source.addComponent(SymbolIds::Resistor, {0, 0});
    const auto b = source.addComponent(SymbolIds::Resistor, {100, 0});
    QVERIFY(source.addWire(makeTerminalEndpoint(a, 1), {}, makeTerminalEndpoint(b, 0)));
    const QJsonObject valid = source.toJson();

    QJsonObject duplicate = valid;
    QJsonArray components = duplicate.value(QStringLiteral("components")).toArray();
    QJsonObject second = components[1].toObject();
    second.insert(QStringLiteral("id"), components[0].toObject().value(QStringLiteral("id")));
    components[1] = second;
    duplicate.insert(QStringLiteral("components"), components);
    QVERIFY(!Document::fromJson(duplicate).document);

    QJsonObject unknownKind = valid;
    components = unknownKind.value(QStringLiteral("components")).toArray();
    QJsonObject first = components[0].toObject();
    first.insert(QStringLiteral("kind"), QStringLiteral("UnknownPart"));
    components[0] = first;
    unknownKind.insert(QStringLiteral("components"), components);
    QVERIFY(!Document::fromJson(unknownKind).document);

    QJsonObject badTerminal = valid;
    QJsonArray wires = badTerminal.value(QStringLiteral("wires")).toArray();
    QJsonObject wire = wires[0].toObject();
    wire.insert(QStringLiteral("start"), QJsonObject{{QStringLiteral("component"), static_cast<double>(a)},
        {QStringLiteral("terminal"), 99}});
    wires[0] = wire;
    badTerminal.insert(QStringLiteral("wires"), wires);
    QVERIFY(!Document::fromJson(badTerminal).document);

    // The first/last vertices of a wire are always overwritten by positions
    // derived from its start/end identities on load (see DECISIONS.md "File
    // format"), so corrupting
    // only those two coordinates can never produce invalid geometry after
    // derivation - a 2-vertex wire's corrupted endpoints simply get replaced
    // by the correct terminal-to-terminal line. To actually exercise
    // rejection of invalid geometry, the diagonal segment must involve an
    // *interior* vertex, which is kept as-is by derivation.
    QJsonObject diagonal = valid;
    wires = diagonal.value(QStringLiteral("wires")).toArray();
    wire = wires[0].toObject();
    wire.insert(QStringLiteral("points"),
                QJsonArray{QJsonArray{20, 0}, QJsonArray{50, 30}, QJsonArray{80, 0}});
    wires[0] = wire;
    diagonal.insert(QStringLiteral("wires"), wires);
    QVERIFY(!Document::fromJson(diagonal).document);
}

void TstDocumentJson::rejectsCollinearReversalOnLoad()
{
    Document source;
    const auto a = source.addComponent(SymbolIds::Resistor, {0, 0});
    const auto b = source.addComponent(SymbolIds::Resistor, {100, 0});
    QVERIFY(source.addWire(makeTerminalEndpoint(a, 1), {}, makeTerminalEndpoint(b, 0)));

    QJsonObject root = source.toJson();
    QJsonArray wires = root.value(QStringLiteral("wires")).toArray();
    QJsonObject wire = wires[0].toObject();
    wire.insert(QStringLiteral("points"),
                QJsonArray{QJsonArray{20, 0}, QJsonArray{60, 0},
                           QJsonArray{50, 0}, QJsonArray{80, 0}});
    wires[0] = wire;
    root.insert(QStringLiteral("wires"), wires);

    const DocumentLoadResult loaded = Document::fromJsonBytes(QJsonDocument(root).toJson());
    QVERIFY(!loaded.document);
    QVERIFY2(loaded.errors.join(QLatin1Char('\n')).contains(QStringLiteral("route geometry")),
             qPrintable(loaded.errors.join(QLatin1Char('\n'))));
}
namespace {

// Builds a version-1 document JSON whose components carry exactly the given
// references (a null QString means the "reference" key is omitted). Component
// ids are 1..N in the order given.
QJsonObject documentWithReferences(const QVector<QPair<SymbolId, QString>> &parts)
{
    Document source;
    for (int i = 0; i < parts.size(); ++i)
        source.addComponent(parts[i].first, {i * 100.0, 0});
    QJsonObject root = source.toJson();
    QJsonArray components = root.value(QStringLiteral("components")).toArray();
    for (int i = 0; i < parts.size(); ++i) {
        QJsonObject component = components[i].toObject();
        if (parts[i].second.isNull())
            component.remove(QStringLiteral("reference"));
        else
            component.insert(QStringLiteral("reference"), parts[i].second);
        components[i] = component;
    }
    root.insert(QStringLiteral("components"), components);
    return root;
}

} // namespace

void TstDocumentJson::generatesMissingReferences()
{
    QJsonObject root = documentWithReferences({
        {SymbolIds::Resistor, QString()},               // id 1: key missing
        {SymbolIds::Resistor, QStringLiteral(" R1 ")},  // id 2: explicit, trimmed on load
        {SymbolIds::Resistor, QStringLiteral("   ")},   // id 3: whitespace only
        {SymbolIds::Capacitor, QStringLiteral("")},     // id 4: empty
        {SymbolIds::Resistor, QStringLiteral("R3")},    // id 5: explicit, later in the file
    });
    QJsonArray components = root.value(QStringLiteral("components")).toArray();
    QJsonObject componentWithValue = components[1].toObject();
    componentWithValue.insert(QStringLiteral("value"), QStringLiteral(" 10k "));
    components[1] = componentWithValue;
    root.insert(QStringLiteral("components"), components);

    const DocumentLoadResult loaded = Document::fromJson(root);
    QVERIFY2(loaded.errors.isEmpty(), qPrintable(loaded.errors.join(QLatin1Char('\n'))));
    QVERIFY(loaded.document);

    // R1 and R3 are taken by explicit labels, so the generated resistor
    // labels skip them (R2, then R4) and the capacitor starts at C1.
    QCOMPARE(loaded.document->component(1)->reference(), QStringLiteral("R2"));
    QCOMPARE(loaded.document->component(2)->reference(), QStringLiteral("R1"));
    QCOMPARE(loaded.document->component(3)->reference(), QStringLiteral("R4"));
    QCOMPARE(loaded.document->component(4)->reference(), QStringLiteral("C1"));
    QCOMPARE(loaded.document->component(5)->reference(), QStringLiteral("R3"));
    QCOMPARE(loaded.document->component(2)->value(), QStringLiteral("10k"));
    QCOMPARE(loaded.warnings.size(), 3); // One per generated reference.

    // Generation is deterministic.
    const DocumentLoadResult again = Document::fromJson(root);
    QVERIFY(again.document);
    for (ComponentId id = 1; id <= 5; ++id)
        QCOMPARE(again.document->component(id)->reference(),
                 loaded.document->component(id)->reference());
}

void TstDocumentJson::rejectsDuplicateReferences()
{
    // Equal after trimming -> rejected, naming both components and the text.
    const DocumentLoadResult duplicate = Document::fromJson(documentWithReferences({
        {SymbolIds::Resistor, QStringLiteral("X1")},
        {SymbolIds::Diode, QStringLiteral("Y1")},
        {SymbolIds::Capacitor, QStringLiteral(" X1")},
    }));
    QVERIFY(!duplicate.document);
    QCOMPARE(duplicate.errors.size(), 1);
    const QString message = duplicate.errors.first();
    QVERIFY2(message.contains(QStringLiteral("Resistor (id 1)")), qPrintable(message));
    QVERIFY2(message.contains(QStringLiteral("Capacitor (id 3)")), qPrintable(message));
    QVERIFY2(message.contains(QStringLiteral("\"X1\"")), qPrintable(message));

    // Comparison is case-sensitive, so these are distinct and accepted
    // unchanged.
    const DocumentLoadResult distinct = Document::fromJson(documentWithReferences({
        {SymbolIds::Resistor, QStringLiteral("r1")},
        {SymbolIds::Resistor, QStringLiteral("R1")},
    }));
    QVERIFY2(distinct.errors.isEmpty(), qPrintable(distinct.errors.join(QLatin1Char('\n'))));
    QVERIFY(distinct.document);
    QCOMPARE(distinct.document->component(1)->reference(), QStringLiteral("r1"));
    QCOMPARE(distinct.document->component(2)->reference(), QStringLiteral("R1"));
    QVERIFY(distinct.warnings.isEmpty());
}

void TstDocumentJson::preservesPreciseGridSpacing()
{
    for (const qreal spacing : {12.3456789, 0.05, 0.001, 2500.125}) {
        Document original;
        QVERIFY(original.setGridSpacing(spacing));
        const DocumentLoadResult loaded = Document::fromJsonBytes(original.toJsonBytes());
        QVERIFY2(loaded.errors.isEmpty(), qPrintable(loaded.errors.join(QLatin1Char('\n'))));
        QVERIFY(loaded.document);
        QCOMPARE(loaded.document->gridSpacing(), spacing);
    }
}

void TstDocumentJson::serializesNodesInStableOrder()
{
    // A long horizontal wire branched at many points creates many nodes.
    Document original;
    const auto a = original.addComponent(SymbolIds::Resistor, {0, 0});
    const auto b = original.addComponent(SymbolIds::Resistor, {600, 0});
    QVERIFY(original.addWire(makeTerminalEndpoint(a, 1), {}, makeTerminalEndpoint(b, 0)));
    for (int x = 50; x <= 500; x += 30) {
        WireId target = 0;
        for (const WireRoute &route : original.wires()) {
            if (route.vertices.first().x() < x && route.vertices.last().x() > x)
                target = route.id;
        }
        QVERIFY(target != 0);
        QVERIFY(original.branchWireAt(target, {double(x), 0}));
    }
    QVERIFY(original.nodes().size() >= 10);

    const QJsonArray nodes = original.toJson().value(QStringLiteral("nodes")).toArray();
    QCOMPARE(nodes.size(), original.nodes().size());
    double previous = 0;
    for (const QJsonValue &value : nodes) {
        const double id = value.toObject().value(QStringLiteral("id")).toDouble();
        QVERIFY2(id > previous, "nodes must be in strictly ascending ID order");
        previous = id;
    }

    // Equal documents serialize to identical bytes.
    const QByteArray bytes = original.toJsonBytes();
    const DocumentLoadResult loaded = Document::fromJsonBytes(bytes);
    QVERIFY(loaded.document);
    QCOMPARE(loaded.document->toJsonBytes(), bytes);
}

void TstDocumentJson::writesV2AndRoundTripsMirroring()
{
    Document d;
    const auto a = d.addComponent(SymbolIds::Resistor, {0, 0});
    const auto b = d.addComponent(SymbolIds::Resistor, {100, 0});
    QVERIFY(d.setComponentMirrored(a, true));
    QVERIFY(d.addWire(makeTerminalEndpoint(a, 1), {}, makeTerminalEndpoint(b, 0)));
    const QJsonObject root = d.toJson();
    QCOMPARE(root.value(QStringLiteral("formatVersion")).toInt(), 2);
    for (const QJsonValue &v : root.value(QStringLiteral("components")).toArray())
        QVERIFY(v.toObject().value(QStringLiteral("mirrored")).isBool());
    const DocumentLoadResult loaded = Document::fromJsonBytes(d.toJsonBytes());
    QVERIFY(loaded.document);
    QVERIFY(loaded.document->component(a)->mirrored());
    QVERIFY(!loaded.document->component(b)->mirrored());
    QCOMPARE(loaded.document->toJsonBytes(), d.toJsonBytes());
}

void TstDocumentJson::loadsV1AsUnmirroredAndUpgradesOnSave()
{
    Document d;
    const auto a = d.addComponent(SymbolIds::Resistor, {0, 0});
    const DocumentLoadResult loaded = Document::fromJson(asVersion1(d.toJson()));
    QVERIFY2(loaded.errors.isEmpty(), qPrintable(loaded.errors.join(QLatin1Char('\n'))));
    QVERIFY(loaded.document);
    QVERIFY(!loaded.document->component(a)->mirrored());
    QCOMPARE(loaded.document->toJson().value(QStringLiteral("formatVersion")).toInt(), 2);
}

void TstDocumentJson::rejectsInvalidVersionAndMirroredCombinations()
{
    Document d;
    d.addComponent(SymbolIds::Resistor, {0, 0});
    const QJsonObject v2 = d.toJson();
    const QJsonObject v1 = asVersion1(v2);

    QVERIFY(!Document::fromJson(withComponentField(v1, QStringLiteral("mirrored"), false)).document); // v1 forbids it
    QVERIFY(!Document::fromJson(withComponentField(v2, QStringLiteral("mirrored"), QJsonValue::Undefined)).document); // v2 requires it
    QVERIFY(!Document::fromJson(withComponentField(v2, QStringLiteral("mirrored"), 1)).document); // must be bool
    QVERIFY(!Document::fromJson(withComponentField(v2, QStringLiteral("mirrored"), QStringLiteral("true"))).document);
    QJsonObject v3 = v2; v3.insert(QStringLiteral("formatVersion"), 3);
    QVERIFY(!Document::fromJson(v3).document);
    QJsonObject v0 = v2; v0.insert(QStringLiteral("formatVersion"), 0);
    QVERIFY(!Document::fromJson(v0).document);
}

void TstDocumentJson::rejectsUnknownSymbolIdNamingIt()
{
    Document d;
    d.addComponent(SymbolIds::Resistor, {0, 0});
    QJsonObject root = d.toJson();
    QJsonArray components = root.value(QStringLiteral("components")).toArray();
    QJsonObject first = components[0].toObject();
    first.insert(QStringLiteral("kind"), QStringLiteral("UnknownPart"));
    components[0] = first;
    root.insert(QStringLiteral("components"), components);

    const DocumentLoadResult loaded = Document::fromJson(root);
    QVERIFY(!loaded.document);
    QCOMPARE(loaded.errors.size(), 1);
    QVERIFY2(loaded.errors.first().contains(QStringLiteral("UnknownPart")), qPrintable(loaded.errors.first()));
}

void TstDocumentJson::loadsPreLibraryDocumentUnchanged()
{
    // Expected positions are worked out by hand from the symbol terminal
    // offsets (rotation: (x,y)->(-y,x) for 90, (-x,-y) for 180; mirroring
    // negates x first), not by calling the model.
    const QByteArray json = R"JSON({
  "formatVersion": 2,
  "grid": { "spacing": 10 },
  "components": [
    { "id": 1, "kind": "Resistor",      "x": 0,   "y": 0,   "rotation": 0,   "mirrored": false, "reference": "R1",   "value": "10k" },
    { "id": 2, "kind": "Capacitor",     "x": 100, "y": 0,   "rotation": 0,   "mirrored": false, "reference": "C1",   "value": "" },
    { "id": 3, "kind": "Diode",         "x": 200, "y": 0,   "rotation": 90,  "mirrored": false, "reference": "D1",   "value": "" },
    { "id": 4, "kind": "Ground",        "x": 200, "y": 60,  "rotation": 0,   "mirrored": false, "reference": "GND1", "value": "" },
    { "id": 5, "kind": "OpAmp",         "x": 300, "y": 0,   "rotation": 0,   "mirrored": false, "reference": "U1",   "value": "" },
    { "id": 6, "kind": "Jack",          "x": 400, "y": 0,   "rotation": 0,   "mirrored": false, "reference": "J1",   "value": "" },
    { "id": 7, "kind": "Potentiometer", "x": 300, "y": 100, "rotation": 180, "mirrored": true,  "reference": "VR1",  "value": "100k" }
  ],
  "nodes": [],
  "wires": [
    { "id": 1, "points": [[20, 0], [80, 0]],
      "start": { "component": 1, "terminal": 1 }, "end": { "component": 2, "terminal": 0 } },
    { "id": 2, "points": [[120, 0], [120, -20], [200, -20]],
      "start": { "component": 2, "terminal": 1 }, "end": { "component": 3, "terminal": 0 } },
    { "id": 3, "points": [[200, 20], [200, 40]],
      "start": { "component": 3, "terminal": 1 }, "end": { "component": 4, "terminal": 0 } },
    { "id": 4, "points": [[328, 0], [380, 0]],
      "start": { "component": 5, "terminal": 2 }, "end": { "component": 6, "terminal": 0 } }
  ]
})JSON";

    const DocumentLoadResult loaded = Document::fromJsonBytes(json);
    QVERIFY2(loaded.errors.isEmpty(), qPrintable(loaded.errors.join(QLatin1Char('\n'))));
    QVERIFY(loaded.document);
    // No warnings means every saved wire endpoint already matched the
    // geometry derived from the symbol library.
    QVERIFY2(loaded.warnings.isEmpty(), qPrintable(loaded.warnings.join(QLatin1Char('\n'))));
    const Document &d = *loaded.document;
    QCOMPARE(d.components().size(), 7);
    QCOMPARE(d.wires().size(), 4);
    QCOMPARE(d.nodes().size(), 0);

    QCOMPARE(d.component(1)->symbolId(), SymbolIds::Resistor);
    QCOMPARE(d.component(2)->symbolId(), SymbolIds::Capacitor);
    QCOMPARE(d.component(3)->symbolId(), SymbolIds::Diode);
    QCOMPARE(d.component(4)->symbolId(), SymbolIds::Ground);
    QCOMPARE(d.component(5)->symbolId(), SymbolIds::OpAmp);
    QCOMPARE(d.component(6)->symbolId(), SymbolIds::Jack);
    QCOMPARE(d.component(7)->symbolId(), SymbolIds::Potentiometer);
    QCOMPARE(d.component(1)->value(), QStringLiteral("10k"));
    QCOMPARE(d.component(7)->reference(), QStringLiteral("VR1"));

    QCOMPARE(d.component(3)->terminalPosition(0), QPointF(200, -20)); // anode, rotated 90
    QCOMPARE(d.component(3)->terminalPosition(1), QPointF(200, 20));  // cathode
    QCOMPARE(d.component(4)->terminalPosition(0), QPointF(200, 40));
    QCOMPARE(d.component(5)->terminalPosition(2), QPointF(328, 0));
    QCOMPARE(d.component(6)->terminalPosition(0), QPointF(380, 0));
    QCOMPARE(d.component(7)->terminalPosition(0), QPointF(280, 100)); // mirrored, rotated 180
    QCOMPARE(d.component(7)->terminalPosition(1), QPointF(320, 100));
    QCOMPARE(d.component(7)->terminalPosition(2), QPointF(300, 120));

    QCOMPARE(d.wire(2)->vertices,
             QVector<QPointF>({QPointF(120, 0), QPointF(120, -20), QPointF(200, -20)}));
    QCOMPARE(d.wire(3)->vertices, QVector<QPointF>({QPointF(200, 20), QPointF(200, 40)}));

    // 17 terminals, four of them pairwise joined by wires.
    QCOMPARE(d.computeNets().size(), 13);

    // Saving and reloading is stable.
    const DocumentLoadResult again = Document::fromJsonBytes(d.toJsonBytes());
    QVERIFY(again.document);
    QCOMPARE(again.document->toJsonBytes(), d.toJsonBytes());
}
