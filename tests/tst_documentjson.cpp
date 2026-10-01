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

namespace {

// Builds a version-1 document JSON whose components carry exactly the given
// references (a null QString means the "reference" key is omitted). Component
// ids are 1..N in the order given.
QJsonObject documentWithReferences(const QVector<QPair<SymbolKind, QString>> &parts)
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
        {SymbolKind::Resistor, QString()},               // id 1: key missing
        {SymbolKind::Resistor, QStringLiteral(" R1 ")},  // id 2: explicit, trimmed on load
        {SymbolKind::Resistor, QStringLiteral("   ")},   // id 3: whitespace only
        {SymbolKind::Capacitor, QStringLiteral("")},     // id 4: empty
        {SymbolKind::Resistor, QStringLiteral("R3")},    // id 5: explicit, later in the file
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
        {SymbolKind::Resistor, QStringLiteral("X1")},
        {SymbolKind::Diode, QStringLiteral("Y1")},
        {SymbolKind::Capacitor, QStringLiteral(" X1")},
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
        {SymbolKind::Resistor, QStringLiteral("r1")},
        {SymbolKind::Resistor, QStringLiteral("R1")},
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
    const auto a = original.addComponent(SymbolKind::Resistor, {0, 0});
    const auto b = original.addComponent(SymbolKind::Resistor, {600, 0});
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
