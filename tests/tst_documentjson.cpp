#include "tst_documentjson.h"

#include "document.h"
#include "documentloadresult.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QTest>

void TstDocumentJson::roundTripsBranchedDocument()
{
    Document original;
    const auto a = original.addComponent(SymbolKind::Resistor, {0, 0});
    const auto b = original.addComponent(SymbolKind::Resistor, {100, 0});
    const auto c = original.addComponent(SymbolKind::Resistor, {40, 50}, Rotation::Deg90);
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
    QCOMPARE(loaded.document->component(a)->reference(), QStringLiteral("Resistor1"));
    QCOMPARE(loaded.document->gridSpacing(), original.gridSpacing());
}

void TstDocumentJson::rejectsInvalidAndRepairsStaleEndpoints()
{
    Document existing;
    existing.addComponent(SymbolKind::Diode, {5, 6});
    const DocumentLoadResult bad = Document::fromJsonBytes("{ broken");
    QVERIFY(!bad.errors.isEmpty());
    QCOMPARE(existing.components().size(), 1); // A failed factory cannot mutate its caller.

    Document source;
    const auto id = source.addComponent(SymbolKind::Resistor, {0, 0});
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

    root.insert(QStringLiteral("formatVersion"), 2);
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
    const auto a = source.addComponent(SymbolKind::Resistor, {0, 0});
    const auto b = source.addComponent(SymbolKind::Resistor, {100, 0});
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
    // derived from its start/end identities on load (see AGENTS.md/CLAUDE.md
    // "Endpoint coordinates are always derived from identity"), so corrupting
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
