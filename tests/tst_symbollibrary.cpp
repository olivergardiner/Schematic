#include "tst_symbollibrary.h"

#include "symbollibrary.h"
#include "symbolmanifest.h"
#include "symbolsvg.h"
#include "document.h"
#include "documentloadresult.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QTest>

#include <functional>
#include <cmath>
#include <algorithm>
#include <limits>

namespace {

LibrarySymbol makeValid()
{
    LibrarySymbol s;
    s.id = SymbolId(QStringLiteral("Test"));
    s.displayName = QStringLiteral("Test part");
    s.category = QStringLiteral("Passives");
    s.referencePrefix = QStringLiteral("T");
    s.primitives = { {PrimitiveKind::Line, {{-10, 0}, {10, 0}}, 0.0} };
    s.terminals = { {QStringLiteral("a"), {-10, 0}}, {QStringLiteral("b"), {10, 0}} };
    return s;
}

QJsonObject validJson()
{
    return QJsonObject{
        {QStringLiteral("id"), QStringLiteral("Test")},
        {QStringLiteral("name"), QStringLiteral("Test part")},
        {QStringLiteral("category"), QStringLiteral("Passives")},
        {QStringLiteral("referencePrefix"), QStringLiteral("T")},
        {QStringLiteral("showsReference"), true},
        {QStringLiteral("primitives"), QJsonArray{QJsonObject{
            {QStringLiteral("kind"), QStringLiteral("Line")},
            {QStringLiteral("points"), QJsonArray{QJsonArray{-10, 0}, QJsonArray{10, 0}}}}}},
        {QStringLiteral("terminals"), QJsonArray{
            QJsonObject{{QStringLiteral("name"), QStringLiteral("a")}, {QStringLiteral("x"), -10}, {QStringLiteral("y"), 0}},
            QJsonObject{{QStringLiteral("name"), QStringLiteral("b")}, {QStringLiteral("x"), 10}, {QStringLiteral("y"), 0}}}},
    };
}

bool writeFile(const QString &path, const QByteArray &bytes)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}

struct ExpectedTerminal { const char *name; qreal x; qreal y; };

void checkTerminals(const SymbolId &id, const QVector<ExpectedTerminal> &expected)
{
    const LibrarySymbol *symbol = builtinSymbolLibrary().find(id);
    QVERIFY2(symbol, qPrintable(id.value()));
    QCOMPARE(symbol->terminals.size(), expected.size());
    for (int i = 0; i < expected.size(); ++i) {
        QCOMPARE(symbol->terminals[i].name, QString::fromLatin1(expected[i].name));
        QCOMPARE(symbol->terminals[i].offset, QPointF(expected[i].x, expected[i].y));
    }
}

void checkKinds(const SymbolId &id, const QVector<PrimitiveKind> &expected)
{
    const LibrarySymbol *symbol = builtinSymbolLibrary().find(id);
    QVERIFY2(symbol, qPrintable(id.value()));
    QCOMPARE(symbol->primitives.size(), expected.size());
    for (int i = 0; i < expected.size(); ++i)
        QVERIFY2(symbol->primitives[i].kind == expected[i], qPrintable(id.value()));
}

} // namespace

void TstSymbolLibrary::builtinLibraryHasExpectedMetadata()
{
    struct Expected { SymbolId id; const char *name; const char *category; const char *prefix; bool shows; };
    const QVector<Expected> expected = {
        {SymbolIds::Resistor,      "Resistor",          "Passives",   "R",   true},
        {SymbolIds::Capacitor,     "Capacitor",         "Passives",   "C",   true},
        {SymbolIds::ElectrolyticCapacitor, "Electrolytic Capacitor", "Passives", "C", true},
        {SymbolIds::Potentiometer, "Potentiometer",     "Passives",   "VR",  true},
        {SymbolIds::PresetResistor, "Preset Resistor", "Passives", "VR", true},
        {SymbolIds::Diode,         "Diode",             "Semiconductors/Diodes", "D", true},
        {SymbolIds::SchottkyDiode, "Schottky Diode", "Semiconductors/Diodes", "D", true},
        {SymbolIds::ZenerDiode, "Zener Diode", "Semiconductors/Diodes", "D", true},
        {SymbolIds::LED, "LED", "Semiconductors/Diodes", "D", true},
        {SymbolIds::PNPCompact30, "PNP", "Semiconductors/Transistors", "Q", true},
        {SymbolIds::NPNCompact30, "NPN", "Semiconductors/Transistors", "Q", true},
        {SymbolIds::NChannelJFETCompact30, "N Channel JFET", "Semiconductors/Transistors", "Q", true},
        {SymbolIds::PChannelJFETCompact30, "P Channel JFET", "Semiconductors/Transistors", "Q", true},
        {SymbolIds::NChannelMOSFETCompact30, "N Channel MOSFET", "Semiconductors/Transistors", "Q", true},
        {SymbolIds::PChannelMOSFETCompact30, "P Channel MOSFET", "Semiconductors/Transistors", "Q", true},
        {SymbolIds::OpAmpGrid10,   "Op-Amp",            "Semiconductors/ICs/Op Amps", "U", true},
        {SymbolIds::OpAmpNoSupplies, "Op-Amp (No Supplies)", "Semiconductors/ICs/Op Amps", "U", true},
        {SymbolIds::OTAWithSuppliesVerticalBias, "OTA (With Supplies)", "Semiconductors/ICs/Op Amps", "U", true},
        {SymbolIds::OTAWithSuppliesBiasAbove, "OTA (With Supplies, Bias Above)", "Semiconductors/ICs/Op Amps", "U", true},
        {SymbolIds::OTANoSuppliesVerticalBias, "OTA (No Supplies)", "Semiconductors/ICs/Op Amps", "U", true},
        {SymbolIds::OTANoSuppliesBiasAbove, "OTA (No Supplies, Bias Above)", "Semiconductors/ICs/Op Amps", "U", true},
        {SymbolIds::DarlingtonBufferLM13700, "Darlington Buffer (LM13700)", "Semiconductors/ICs/Buffers", "U", true},
        {SymbolIds::RailVCC, "Power Rail", "Signals/Power Rails", "PWR", false},
        {SymbolIds::Ground, "Earth", "Signals/Grounds", "GND", false},
        {SymbolIds::GroundChassis, "Chassis Ground", "Signals/Grounds", "GND", false},
        {SymbolIds::GroundSignal, "Signal Ground", "Signals/Grounds", "GND", false},
        {SymbolIds::PortInput, "Input", "Signals/Ports", "PORT", false},
        {SymbolIds::PortOutput, "Output", "Signals/Ports", "PORT", false},
        {SymbolIds::PortBidirectional, "Bidirectional", "Signals/Ports", "PORT", false},
        {SymbolIds::Jack,          "Input/Output Jack", "Connectors", "J",   true},
        {SymbolIds::BlockBuffer, "Buffer", "Block Schematics", "B", true},
        {SymbolIds::BlockSummingNode, "Summing Node", "Block Schematics", "B", true},
        {SymbolIds::BlockHighPass, "High Pass Filter", "Block Schematics/Filters", "B", true},
        {SymbolIds::BlockLowPass, "Low Pass Filter", "Block Schematics/Filters", "B", true},
        {SymbolIds::BlockBandPass, "Band Pass Filter", "Block Schematics/Filters", "B", true},
        {SymbolIds::BlockBandStop, "Band Stop Filter", "Block Schematics/Filters", "B", true},
        {SymbolIds::BlockVariableHighPass, "Variable High Pass Filter", "Block Schematics/Filters", "B", true},
        {SymbolIds::BlockVariableLowPass, "Variable Low Pass Filter", "Block Schematics/Filters", "B", true},
        {SymbolIds::BlockVariableBandPass, "Variable Band Pass Filter", "Block Schematics/Filters", "B", true},
        {SymbolIds::BlockVariableBandStop, "Variable Band Stop Filter", "Block Schematics/Filters", "B", true},
    };
    const SymbolLibrary &library = builtinSymbolLibrary();
    QCOMPARE(library.size(), expected.size());
    for (int i = 0; i < expected.size(); ++i) {
        const LibrarySymbol &symbol = library.symbols()[i];
        QCOMPARE(symbol.id, expected[i].id);
        QCOMPARE(symbol.displayName, QString::fromLatin1(expected[i].name));
        QCOMPARE(symbol.category, QString::fromLatin1(expected[i].category));
        QCOMPARE(symbol.referencePrefix, QString::fromLatin1(expected[i].prefix));
        QCOMPARE(symbol.showsReference, expected[i].shows);
        QVERIFY(validateLibrarySymbol(symbol).isEmpty());
    }
}

void TstSymbolLibrary::packagedArtworkMatchesTemplates()
{
    struct Template { const char *file; SymbolId id; };
    const QVector<Template> templates{
        {"resistor", SymbolIds::Resistor}, {"capacitor", SymbolIds::Capacitor},
        {"electrolytic-capacitor", SymbolIds::ElectrolyticCapacitor}, {"op-amp", SymbolIds::OpAmpGrid10}};
    SymbolLibrary coexist = builtinSymbolLibrary();
    for (const auto &entry : templates) {
        const QString base = QString("../symbol-templates/grid10/%1").arg(entry.file);
        const QString metadataPath = QTest::qFindTestData(qPrintable(base + ".json"), __FILE__, __LINE__);
        const QString svgPath = QTest::qFindTestData(qPrintable(base + ".svg"), __FILE__, __LINE__);
        QFile metadata(metadataPath), svg(svgPath);
        QVERIFY(metadata.open(QIODevice::ReadOnly)); QVERIFY(svg.open(QIODevice::ReadOnly));
        auto imported = SymbolSvgImporter::importSymbol(metadata.readAll(), svg.readAll(), QFileInfo(svg).absolutePath());
        QVERIFY2(imported.symbol, qPrintable(imported.errors.join('\n')));
        const auto *packaged = builtinSymbolLibrary().find(entry.id);
        QVERIFY(packaged);
        QStringList errors;
        // Existing manually imported Grid10.* entries have distinct identities.
        QVERIFY2(coexist.add(*imported.symbol, &errors), qPrintable(errors.join('\n')));
        imported.symbol->id = packaged->id;
        imported.symbol->displayName = packaged->displayName;
        imported.symbol->category = packaged->category;
        QCOMPARE(SymbolLibrary::symbolToJson(*imported.symbol), SymbolLibrary::symbolToJson(*packaged));
        Document document;
        const auto a = document.addComponent(entry.id, {0,0});
        const auto b = document.addComponent(entry.id, {100,0});
        QVERIFY(a != kInvalidComponentId && b != kInvalidComponentId);
        const auto wire = document.addWire(makeTerminalEndpoint(a,0), {}, makeTerminalEndpoint(b,0));
        QVERIFY(wire);
        QVERIFY(document.transformSelection({a,b}, {}, {SelectionTransform::Kind::Translate, {10,10}}));
        QVERIFY(document.transformSelection({a,b}, {}, {SelectionTransform::Kind::Rotate, {}, {}, 1}));
        QVERIFY(document.transformSelection({a,b}, {}, {SelectionTransform::Kind::FlipHorizontal}));
        const auto loaded = Document::fromJsonBytes(document.toJsonBytes());
        QVERIFY(loaded.document);
        QCOMPARE(loaded.document->toJsonBytes(), document.toJsonBytes());
    }
    QCOMPARE(coexist.size(), builtinSymbolLibrary().size() + templates.size());
    const auto *capacitor = builtinSymbolLibrary().find(SymbolIds::Capacitor);
    QCOMPARE(capacitor->primitives[2].fillColor, PrimitiveFillColor::Black);
    QCOMPARE(capacitor->primitives[3].fillColor, PrimitiveFillColor::Black);
    const auto *electrolytic = builtinSymbolLibrary().find(SymbolIds::ElectrolyticCapacitor);
    QCOMPARE(electrolytic->primitives[2].fillColor, PrimitiveFillColor::White);
    QCOMPARE(electrolytic->primitives[3].fillColor, PrimitiveFillColor::Black);
}

void TstSymbolLibrary::blockSymbolsGeometryAndConnectedPersistence()
{
    for (const auto &id : {SymbolIds::BlockHighPass, SymbolIds::BlockLowPass,
                           SymbolIds::BlockBandPass, SymbolIds::BlockBuffer,
                           SymbolIds::BlockSummingNode,
                           SymbolIds::BlockBandStop,
                           SymbolIds::BlockVariableHighPass,
                           SymbolIds::BlockVariableLowPass,
                           SymbolIds::BlockVariableBandPass,
                           SymbolIds::BlockVariableBandStop}) {
        const auto *symbol = builtinSymbolLibrary().find(id);
        QVERIFY(symbol);
        const bool summing = id == SymbolIds::BlockSummingNode;
        checkTerminals(id, summing
            ? QVector<ExpectedTerminal>{{"in1", -30, -10}, {"in2", -30, 10}, {"out", 30, 0}}
            : QVector<ExpectedTerminal>{{"in", -30, 0}, {"out", 30, 0}});
        if (QTest::currentTestFailed()) return;
        QCOMPARE(symbol->primitives.first().kind, PrimitiveKind::Polygon);
        QCOMPARE(symbol->primitives.first().points,
                 QVector<QPointF>({{-20,-20}, {20,-20}, {20,20}, {-20,20}}));
        QCOMPARE(symbolBounds(*symbol), QRectF(-30, -20, 60, 40));
        if (id.value().startsWith(QStringLiteral("Block.Variable"))) {
            QCOMPARE(symbol->primitives.size(), 6);
            QCOMPARE(symbol->primitives[4].points, QVector<QPointF>({{-14,14}, {14,-14}}));
            QCOMPARE(symbol->primitives[5].points, QVector<QPointF>({{7,-14}, {14,-14}, {14,-7}}));
            QCOMPARE(symbol->primitives[5].kind, PrimitiveKind::Polygon);
            QVERIFY(primitiveIsFilled(symbol->primitives[5]));
            QCOMPARE(symbol->primitives[5].fillColor, PrimitiveFillColor::Black);
            const SymbolId fixedId(QStringLiteral("Block.") + id.value().mid(14));
            const auto *fixed = builtinSymbolLibrary().find(fixedId);
            QVERIFY(fixed);
            QCOMPARE(symbol->primitives[3].points, fixed->primitives[3].points);
        }
        Document document;
        const auto source = document.addComponent(id, {0,0});
        const auto sink = document.addComponent(id, {100,0});
        const int output = symbol->terminals.size() - 1;
        // Exercise every input, including both independent summing inputs.
        for (int input = 0; input < output; ++input) {
            const QVector<QPointF> corners = summing
                ? QVector<QPointF>{{50, 0}, {50, symbol->terminals[input].offset.y()}}
                : QVector<QPointF>{};
            const auto wire = document.addWire(makeTerminalEndpoint(source, output), corners,
                                               makeTerminalEndpoint(sink, input));
            QVERIFY(wire);
        }
        QVERIFY(document.moveComponent(sink, {120,20}));
        for (int input = 0; input < output; ++input) {
            const auto &wire = document.wires()[input];
            QCOMPARE(wire.start, makeTerminalEndpoint(source, output));
            QCOMPARE(wire.end, makeTerminalEndpoint(sink, input));
            QCOMPARE(wire.vertices.last(), QPointF(120,20) + symbol->terminals[input].offset);
        }
        const auto loaded = Document::fromJsonBytes(document.toJsonBytes());
        QVERIFY(loaded.document);
        QCOMPARE(loaded.document->toJsonBytes(), document.toJsonBytes());
    }
}

void TstSymbolLibrary::adjustableResistorsArtworkAndConnections()
{
    const auto *resistor = builtinSymbolLibrary().find(SymbolIds::Resistor);
    QVERIFY(resistor);
    for (const auto &id : {SymbolIds::Potentiometer, SymbolIds::PresetResistor}) {
        const auto *symbol = builtinSymbolLibrary().find(id);
        QVERIFY(symbol);
        checkTerminals(id, {{"1", -20, 0}, {"2", 20, 0}, {"wiper", 0, -20}});
        if (QTest::currentTestFailed()) return;
        QCOMPARE(symbol->primitives[2].points, resistor->primitives[0].points);
        QVERIFY(primitiveIsFilled(symbol->primitives[2]));
        QCOMPARE(symbol->primitives[2].fillColor, PrimitiveFillColor::White);
        const auto &arrow = symbol->primitives[4];
        if (id == SymbolIds::PresetResistor) {
            QCOMPARE(arrow.kind, PrimitiveKind::Line);
            QCOMPARE(arrow.points, QVector<QPointF>({{-4,-9}, {4,-9}}));
            QCOMPARE(symbol->primitives[3].points, QVector<QPointF>({{0,-20}, {0,-9}}));
        } else {
            QCOMPARE(arrow.kind, PrimitiveKind::Polygon);
            QCOMPARE(arrow.points, QVector<QPointF>({{-4,-13}, {0,-5}, {4,-13}}));
            QVERIFY(primitiveIsFilled(arrow));
            QCOMPARE(arrow.fillColor, PrimitiveFillColor::Black);
        }
        const auto &clockwiseShaft = symbol->primitives[5];
        QCOMPARE(clockwiseShaft.kind, PrimitiveKind::Line);
        QCOMPARE(clockwiseShaft.points, QVector<QPointF>({{-6,0}, {5,0}}));
        const auto &clockwiseHead = symbol->primitives[6];
        QCOMPARE(clockwiseHead.kind, PrimitiveKind::Polygon);
        QCOMPARE(clockwiseHead.points, QVector<QPointF>({{2,-2}, {6,0}, {2,2}}));
        QVERIFY(primitiveIsFilled(clockwiseHead));
        QCOMPARE(clockwiseHead.fillColor, PrimitiveFillColor::Black);
        QCOMPARE(symbol->primitives.size(), 7);
        Document document;
        const auto a = document.addComponent(id, {});
        const auto b = document.addComponent(id, {100,0});
        for (int terminal = 0; terminal < 3; ++terminal)
            QVERIFY(document.addWire(makeTerminalEndpoint(a, terminal), {},
                                     makeTerminalEndpoint(b, terminal)));
        QVERIFY(document.moveComponent(b, {120,20}));
        for (int terminal = 0; terminal < 3; ++terminal) {
            QCOMPARE(document.wires()[terminal].end, makeTerminalEndpoint(b, terminal));
            QCOMPARE(document.wires()[terminal].vertices.last(),
                     QPointF(120,20) + symbol->terminals[terminal].offset);
        }
        const auto loaded = Document::fromJsonBytes(document.toJsonBytes());
        QVERIFY(loaded.document);
        QCOMPARE(loaded.document->toJsonBytes(), document.toJsonBytes());
    }
}

void TstSymbolLibrary::manifestCategoriesValidateAndKeepEmptyPaths()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto path = directory.filePath(QStringLiteral("manifest.json"));
    const auto load = [&](const QJsonValue &categories, QStringList *errors) {
        if (!writeFile(path, QJsonDocument(QJsonObject{
                {"symbols", QJsonArray{}}, {"categories", categories}}).toJson()))
            return SymbolLibrary{};
        return SymbolLibrary::loadFromDirectory(directory.path(), errors);
    };
    QStringList errors;
    const auto library = load(QJsonArray{"Semiconductors/Transistors"}, &errors);
    QVERIFY(errors.isEmpty());
    QCOMPARE(library.size(), 0);
    QCOMPARE(library.categories(), QStringList({"Semiconductors","Semiconductors/Transistors"}));
    const SymbolLibrary copied = library;
    QCOMPARE(copied.categories(), library.categories());
    for (const auto &invalid : {QJsonValue("bad"), QJsonValue(QJsonArray{3}),
                               QJsonValue(QJsonArray{"A//B"}), QJsonValue(QJsonArray{" A"}),
                               QJsonValue(QJsonArray{""}), QJsonValue(QJsonArray{"A", "A"})}) {
        errors.clear();
        const auto rejected = load(invalid, &errors);
        QVERIFY(!errors.isEmpty());
        QVERIFY(rejected.categories().isEmpty());
    }
    SymbolLibrary dynamic;
    auto symbol = makeValid();
    QVERIFY(dynamic.add(symbol, &errors));
    QCOMPARE(dynamic.categories(), QStringList{"Passives"});
    symbol.category = QStringLiteral("Updated/Category");
    QVERIFY(dynamic.replace(symbol, &errors));
    QCOMPARE(dynamic.categories(), QStringList({"Updated","Updated/Category"}));
}

void TstSymbolLibrary::nestedManifestValidatesHierarchyAndOrder()
{
    QTemporaryDir directory; QVERIFY(directory.isValid());
    auto parent = makeValid(); parent.id = SymbolId("Parent"); parent.category = "Group";
    auto zed = parent; zed.id = SymbolId("Zed"); zed.category = "Group/Zed";
    auto hidden = zed; hidden.id = SymbolId("Hidden"); hidden.paletteVisible = false;
    auto alpha = parent; alpha.id = SymbolId("Alpha"); alpha.category = "Group/Alpha";
    const auto saveSymbol = [&](const char *file, const LibrarySymbol &symbol) {
        return writeFile(directory.filePath(file), QJsonDocument(SymbolLibrary::symbolToJson(symbol)).toJson());
    };
    QVERIFY(saveSymbol("parent.json",parent)); QVERIFY(saveSymbol("z.json",zed));
    QVERIFY(saveSymbol("hidden.json",hidden)); QVERIFY(saveSymbol("a.json",alpha));
    const QJsonObject manifest{{"categories", QJsonArray{
        QJsonObject{{"name","Group"}, {"symbols",QJsonArray{"parent.json"}}, {"children",QJsonArray{
            QJsonObject{{"name","Zed"}, {"symbols",QJsonArray{"z.json","hidden.json"}}},
            QJsonObject{{"name","Empty"}},
            QJsonObject{{"name","Alpha"}, {"symbols",QJsonArray{"a.json"}}}}}},
        QJsonObject{{"name","Other"}, {"children",QJsonArray{QJsonObject{{"name","Empty"}}}}}}}};
    QVERIFY(writeFile(directory.filePath("manifest.json"),QJsonDocument(manifest).toJson()));
    QStringList errors;
    const auto parsed = SymbolManifest::fromJson(manifest,&errors);
    QVERIFY(parsed); QVERIFY(!parsed->legacy); QCOMPARE(parsed->toJson(),manifest);
    auto library = SymbolLibrary::loadFromDirectory(directory.path(),&errors);
    QVERIFY2(errors.isEmpty(),qPrintable(errors.join('\n')));
    QCOMPARE(library.categories(),QStringList({"Group","Group/Zed","Group/Empty","Group/Alpha","Other","Other/Empty"}));
    const auto tree = library.categoryTree(); QCOMPARE(tree.size(),2);
    QCOMPARE(tree[0].symbols,QVector<SymbolId>{parent.id});
    QCOMPARE(tree[0].children[0].symbols,QVector<SymbolId>({zed.id,hidden.id}));
    QCOMPARE(tree[0].children[1].name,QString("Empty"));
    QVERIFY(tree[0].children[1].symbols.isEmpty());
    const auto paletteIds = [](const SymbolLibrary &source) {
        QVector<SymbolId> ids; for (const auto *symbol : source.paletteSymbols()) ids.append(symbol->id); return ids;
    };
    QCOMPARE(paletteIds(library),QVector<SymbolId>({parent.id,zed.id,alpha.id}));
    zed.category = alpha.category;
    QVERIFY(library.replace(zed,&errors));
    QCOMPARE(paletteIds(library),QVector<SymbolId>({parent.id,alpha.id,zed.id}));
    QCOMPARE(library.categoryTree()[0].children[0].name,QString("Zed")); // declared empty/hidden-only category retained
    // The manifest owns placement: a mismatching standalone category is rejected.
    QVERIFY(saveSymbol("z.json",zed)); errors.clear();
    const auto rejected = SymbolLibrary::loadFromDirectory(directory.path(),&errors);
    QVERIFY(!errors.isEmpty()); QCOMPARE(rejected.size(),0);
    QVERIFY(errors.join('\n').contains("Category mismatch"));
    const QVector<QJsonObject> invalid{
        {{"categories",QJsonArray{QJsonObject{{"name","Same"}},QJsonObject{{"name","Same"}}}}},
        {{"categories",QJsonArray{QJsonObject{{"name","A/B"}}}}},
        {{"categories",QJsonArray{QJsonObject{{"name"," A"}}}}},
        {{"categories",QJsonArray{QJsonObject{{"name",""}}}}},
        {{"categories",QJsonArray{QJsonObject{{"name","A"},{"children","bad"}}}}},
        {{"categories",QJsonArray{QJsonObject{{"name","A"},{"symbols","bad"}}}}},
        {{"categories",QJsonArray{QJsonObject{{"name","A"},{"symbols",QJsonArray{"../bad.json"}}}}}},
        {{"categories",QJsonArray{QJsonObject{{"name","A"},{"symbols",QJsonArray{"same.json"}}},
                                  QJsonObject{{"name","B"},{"symbols",QJsonArray{"same.json"}}}}}},
        {{"symbols",QJsonArray{}},{"categories",QJsonArray{QJsonObject{{"name","Mixed"}}}}},
        {{"categories",QJsonArray{"not-an-object"}}}
    };
    for (const auto &bad : invalid) {
        errors.clear(); QVERIFY(!SymbolManifest::fromJson(bad,&errors)); QVERIFY(!errors.isEmpty());
    }
    errors.clear();
    const auto unresolved = SymbolManifest::fromJson(QJsonObject{{"symbols",QJsonArray{"a.json"}}},&errors);
    QVERIFY(unresolved); QVERIFY(unresolved->legacy);
    QCOMPARE(unresolved->toJson()["symbols"].toArray(),QJsonArray{"a.json"});
}

void TstSymbolLibrary::diodeVariantsAndConnections()
{
    const auto *base = builtinSymbolLibrary().find(SymbolIds::Diode);
    QVERIFY(base);
    for (const auto &id : {SymbolIds::SchottkyDiode, SymbolIds::ZenerDiode, SymbolIds::LED}) {
        const auto *symbol = builtinSymbolLibrary().find(id);
        QVERIFY(symbol);
        checkTerminals(id, {{"anode", -20, 0}, {"cathode", 20, 0}});
        if (QTest::currentTestFailed()) return;
        for (int i = 0; i < 3; ++i)
            QCOMPARE(symbol->primitives[i].points, base->primitives[i].points);
        if (id == SymbolIds::SchottkyDiode)
            QCOMPARE(symbol->primitives[3].points,
                     QVector<QPointF>({{2,-3}, {2,-5}, {5,-5}, {5,5}, {8,5}, {8,3}}));
        else if (id == SymbolIds::ZenerDiode)
            QCOMPARE(symbol->primitives[3].points,
                     QVector<QPointF>({{2,-7}, {5,-5}, {5,5}, {8,7}}));
        else {
            QCOMPARE(symbol->primitives.size(), 8);
            for (int i : {5, 7}) {
                QCOMPARE(symbol->primitives[i].kind, PrimitiveKind::Polygon);
                QVERIFY(primitiveIsFilled(symbol->primitives[i]));
                QCOMPARE(symbol->primitives[i].fillColor, PrimitiveFillColor::Black);
            }
        }
        Document document;
        const auto a = document.addComponent(id, {});
        const auto b = document.addComponent(id, {100,0});
        for (int terminal = 0; terminal < 2; ++terminal)
            QVERIFY(document.addWire(makeTerminalEndpoint(a, terminal), {}, makeTerminalEndpoint(b, terminal)));
        QVERIFY(document.moveComponent(b, {120,20}));
        for (int terminal = 0; terminal < 2; ++terminal) {
            QCOMPARE(document.wires()[terminal].end, makeTerminalEndpoint(b, terminal));
            QCOMPARE(document.wires()[terminal].vertices.last(), QPointF(120,20) + symbol->terminals[terminal].offset);
        }
        const auto loaded = Document::fromJsonBytes(document.toJsonBytes());
        QVERIFY(loaded.document);
        QCOMPARE(loaded.document->toJsonBytes(), document.toJsonBytes());
    }
}

void TstSymbolLibrary::transistorsShareTerminalLayoutAndConnections()
{
    for (const auto &id : {SymbolIds::PNPCompact30, SymbolIds::NPNCompact30, SymbolIds::NChannelJFETCompact30,
                           SymbolIds::PChannelJFETCompact30, SymbolIds::NChannelMOSFETCompact30, SymbolIds::PChannelMOSFETCompact30}) {
        const auto *symbol = builtinSymbolLibrary().find(id);
        QVERIFY(symbol);
        const bool bjt = id == SymbolIds::PNPCompact30 || id == SymbolIds::NPNCompact30;
        const bool pType = id.value().startsWith(QLatin1Char('P'));
        const qreal collectorY = pType ? 20.0 : -20.0;
        checkTerminals(id, bjt
            ? QVector<ExpectedTerminal>{{"base", -20, 0}, {"collector", 10, collectorY}, {"emitter", 10, -collectorY}}
            : QVector<ExpectedTerminal>{{"gate", -20, 0}, {"drain", 10, collectorY}, {"source", 10, -collectorY}});
        if (QTest::currentTestFailed()) return;
        for (const auto &primitive : symbol->primitives)
            QVERIFY(primitive.kind != PrimitiveKind::Circle);
        const auto &arrow = symbol->primitives.last();
        QCOMPARE(arrow.kind, PrimitiveKind::Polygon);
        QVERIFY(primitiveIsFilled(arrow));
        QCOMPARE(arrow.fillColor, PrimitiveFillColor::Black);
        const auto tip = arrow.points[0];
        const auto base = (arrow.points[1] + arrow.points[2]) / 2;
        if (id == SymbolIds::NPNCompact30) { QVERIFY(tip.x() > base.x()); QVERIFY(tip.y() > base.y()); }
        if (id == SymbolIds::PNPCompact30) { QVERIFY(tip.x() < base.x()); QVERIFY(tip.y() > base.y()); }
        if (id == SymbolIds::NChannelJFETCompact30 || id == SymbolIds::PChannelMOSFETCompact30)
            QVERIFY(tip.x() > base.x());
        if (id == SymbolIds::PChannelJFETCompact30 || id == SymbolIds::NChannelMOSFETCompact30)
            QVERIFY(tip.x() < base.x());
        const bool mosfet = id == SymbolIds::NChannelMOSFETCompact30 || id == SymbolIds::PChannelMOSFETCompact30;
        if (!bjt && !mosfet) {
            const qreal drainY = pType ? 8.0 : -8.0;
            QCOMPARE(symbol->primitives[2].points,
                     QVector<QPointF>({{-5,drainY}, {10,drainY}, {10,collectorY}}));
            QCOMPARE(symbol->primitives[3].points,
                     QVector<QPointF>({{-5,-drainY}, {10,-drainY}, {10,-collectorY}}));
        }
        QCOMPARE(symbolBounds(*symbol).width(), 30.0);
        QCOMPARE(symbol->primitives[1].points,
                 pType ? QVector<QPointF>({{-5,10}, {-5,-10}})
                       : QVector<QPointF>({{-5,-10}, {-5,10}}));
        // Collector/drain and emitter/source finish vertically at their tips.
        for (int terminal = 1; terminal <= 2; ++terminal) {
            const auto &lead = symbol->primitives[(mosfet ? 4 : 1) + terminal].points;
            QCOMPARE(lead.last(), symbol->terminals[terminal].offset);
            QCOMPARE(lead[lead.size() - 2].x(), lead.last().x());
        }
        if (mosfet) {
            QCOMPARE(symbol->primitives.size(), 9);
            // Enhancement channel has three separate strokes; body ties to source.
            for (int i = 2; i < 5; ++i) {
                QCOMPARE(symbol->primitives[i].kind, PrimitiveKind::Line);
                QCOMPARE(symbol->primitives[i].points[0].x(), 2.0);
                QCOMPARE(symbol->primitives[i].points[1].x(), 2.0);
            }
            QCOMPARE(symbol->primitives[7].points, QVector<QPointF>({{2,0}, {10,0}, {10,pType ? -8.0 : 8.0}}));
        }
        Document document;
        const auto a = document.addComponent(id, {});
        const auto b = document.addComponent(id, {100,0});
        for (int terminal = 0; terminal < 3; ++terminal)
            QVERIFY(document.addWire(makeTerminalEndpoint(a, terminal), {}, makeTerminalEndpoint(b, terminal)));
        QVERIFY(document.moveComponent(b, {120,20}));
        for (int terminal = 0; terminal < 3; ++terminal) {
            QCOMPARE(document.wires()[terminal].end, makeTerminalEndpoint(b, terminal));
            QCOMPARE(document.wires()[terminal].vertices.last(), QPointF(120,20) + symbol->terminals[terminal].offset);
        }
        QVERIFY(document.transformSelection({a,b}, {}, {SelectionTransform::Kind::Rotate, {}, {}, 1}));
        QVERIFY(document.transformSelection({a,b}, {}, {SelectionTransform::Kind::FlipHorizontal}));
        const auto loaded = Document::fromJsonBytes(document.toJsonBytes());
        QVERIFY(loaded.document);
        QCOMPARE(loaded.document->toJsonBytes(), document.toJsonBytes());
    }
}

void TstSymbolLibrary::amplifierVariantsAndBufferConnections()
{
    const auto *opamp = builtinSymbolLibrary().find(SymbolIds::OpAmpGrid10);
    QVERIFY(opamp);
    for (const auto &id : {SymbolIds::OpAmpNoSupplies, SymbolIds::OTAWithSuppliesVerticalBias,
                           SymbolIds::OTANoSuppliesVerticalBias, SymbolIds::DarlingtonBufferLM13700}) {
        const auto *symbol = builtinSymbolLibrary().find(id);
        QVERIFY(symbol);
        QVector<ExpectedTerminal> expected{{"in+",-30,-10}, {"in-",-30,10}, {"out",30,0}};
        if (id == SymbolIds::DarlingtonBufferLM13700)
            expected = {{"in",-20,0}, {"out",10,20}};
        else if (id != SymbolIds::OpAmpNoSupplies) {
            expected[2].x = 50;
            expected.append({"Iabc",30,20});
            if (id == SymbolIds::OTAWithSuppliesVerticalBias) {
                expected.append({"v+",0,-20});
                expected.append({"v-",0,20});
            }
        }
        checkTerminals(id, expected);
        if (QTest::currentTestFailed()) return;
        if (id == SymbolIds::OpAmpNoSupplies) {
            QCOMPARE(symbol->primitives.size(), 7);
            for (int i = 0; i < 7; ++i)
                QCOMPARE(symbol->primitives[i].points, opamp->primitives[i < 4 ? i : i + 2].points);
        } else if (id == SymbolIds::DarlingtonBufferLM13700) {
            // Collectors meet internally; only base input and open emitter are exposed.
            QCOMPARE(symbol->primitives[2].points.last(), symbol->primitives[6].points.last());
            QCOMPARE(symbol->primitives[7].points.last(), QPointF(10,20));
            for (int i : {4,8}) {
                QCOMPARE(symbol->primitives[i].kind, PrimitiveKind::Polygon);
                QCOMPARE(symbol->primitives[i].fillColor, PrimitiveFillColor::Black);
            }
        } else {
            QCOMPARE(symbol->primitives[0].points, opamp->primitives[0].points);
            QCOMPARE(symbol->primitives[3].points, QVector<QPointF>({{40,0}, {50,0}}));
            for (int i : {7,8}) {
                QCOMPARE(symbol->primitives[i].kind, PrimitiveKind::Circle);
                QCOMPARE(symbol->primitives[i].radius, 6.0);
                QVERIFY(!primitiveIsFilled(symbol->primitives[i]));
            }
            QCOMPARE(symbol->primitives[7].points, QVector<QPointF>({{26,0}}));
            QCOMPARE(symbol->primitives[8].points, QVector<QPointF>({{34,0}}));
            const auto &bias = symbol->primitives[9].points;
            QCOMPARE(bias, QVector<QPointF>({{30,20}, {30,std::sqrt(20.0)}}));
            // Bias meets both circles at their lower intersection.
            for (int i : {7,8}) {
                const QPointF delta = bias.last() - symbol->primitives[i].points.first();
                QVERIFY(qAbs(QPointF::dotProduct(delta, delta) - 36.0) < 1e-9);
            }
            for (int i = 7; i < symbol->primitives.size(); ++i)
                QVERIFY(symbol->primitives[i].kind != PrimitiveKind::Polygon);
            QCOMPARE(symbol->primitives.size(), id == SymbolIds::OTAWithSuppliesVerticalBias ? 12 : 10);
        }
        // Wire every terminal, including bias, supplies and buffer output.
        Document document;
        const auto component = document.addComponent(id, {});
        const auto partner = document.addComponent(id, {100,0});
        for (int terminal = 0; terminal < expected.size(); ++terminal) {
            QVERIFY(document.addWire(makeTerminalEndpoint(component, terminal), {},
                                     makeTerminalEndpoint(partner, terminal)));
        }
        QVERIFY(document.moveComponent(component, {10,10}));
        for (int terminal = 0; terminal < expected.size(); ++terminal) {
            QCOMPARE(document.wires()[terminal].start, makeTerminalEndpoint(component, terminal));
            QCOMPARE(document.wires()[terminal].vertices.first(),
                     QPointF(10,10) + symbol->terminals[terminal].offset);
        }
        QVERIFY(document.transformSelection({component,partner}, {}, {SelectionTransform::Kind::Rotate, {}, {}, 1}));
        QVERIFY(document.transformSelection({component,partner}, {}, {SelectionTransform::Kind::FlipHorizontal}));
        const auto loaded = Document::fromJsonBytes(document.toJsonBytes());
        QVERIFY(loaded.document);
        QCOMPARE(loaded.document->toJsonBytes(), document.toJsonBytes());
    }
}

void TstSymbolLibrary::layoutMetadataValidationAndRoundTrip()
{
    const auto &library = builtinSymbolLibrary();
    for (const auto &id : {SymbolIds::OTAWithSuppliesVerticalBias, SymbolIds::OTANoSuppliesVerticalBias,
                           SymbolIds::OTAWithSuppliesBiasAbove, SymbolIds::OTANoSuppliesBiasAbove}) {
        const auto *symbol = library.find(id);
        QVERIFY(symbol);
        QCOMPARE(symbol->layoutControls.size(), 1);
        const auto &control = symbol->layoutControls.first();
        QCOMPARE(control.key, QString("bias_position"));
        QCOMPARE(control.choices.size(), 2);
        const bool above = control.current == "above";
        QCOMPARE(symbol->paletteVisible, !above);
        QCOMPARE(symbol->terminals[3].offset, QPointF(30, above ? -20 : 20));
        QCOMPARE(symbol->primitives[9].points, QVector<QPointF>({{30,above ? -20.0 : 20.0},
                     {30,above ? -std::sqrt(20.0) : std::sqrt(20.0)}}));
        const auto otherId = control.choices[above ? 0 : 1].symbolId;
        const auto *other = library.find(otherId);
        QVERIFY(other);
        for (int i = 0; i < symbol->terminals.size(); ++i)
            if (i != 3) QCOMPARE(symbol->terminals[i].offset, other->terminals[i].offset);
        for (int i = 0; i < symbol->primitives.size(); ++i)
            if (i != 9) QCOMPARE(symbol->primitives[i].points, other->primitives[i].points);
        QStringList errors;
        const auto json = SymbolLibrary::symbolToJson(*symbol);
        const auto parsed = SymbolLibrary::parseSymbol(json, &errors);
        QVERIFY(parsed); QCOMPARE(SymbolLibrary::symbolToJson(*parsed), json);
        const auto exported = SymbolSvgExporter::exportSymbol(*symbol);
        QVERIFY(exported.errors.isEmpty());
        const auto imported = SymbolSvgImporter::importSymbol(exported.sidecar, exported.svg, "/symbols");
        QVERIFY2(imported.symbol, qPrintable(imported.errors.join('\n')));
        QCOMPARE(symbolLayoutControlsJson(*imported.symbol), symbolLayoutControlsJson(*symbol));
        QCOMPARE(imported.symbol->paletteVisible, symbol->paletteVisible);
        auto changed = *symbol;
        changed.layoutControls[0].current = "missing";
        QVERIFY(!validateLibrarySymbol(changed).isEmpty());
        changed = *symbol; changed.layoutControls[0].choices[0].terminals[0].from = "unknown";
        QVERIFY(!validateLibrarySymbol(changed).isEmpty());
        changed = *symbol; changed.layoutControls.append(changed.layoutControls.first());
        QVERIFY(!validateLibrarySymbol(changed).isEmpty());
        changed = *symbol; changed.layoutControls[0].choices[1].key = "below";
        QVERIFY(!validateLibrarySymbol(changed).isEmpty());
        auto badJson = json; badJson["layoutControls"] = "invalid";
        errors.clear(); QVERIFY(!SymbolLibrary::parseSymbol(badJson, &errors));
        auto incompatible = *other; incompatible.terminals[0].name = "different";
        errors.clear();
        QVERIFY(layoutTerminalMapping(*symbol, control, control.choices[above ? 0 : 1], incompatible, &errors).isEmpty());
        QVERIFY(!errors.isEmpty());
    }
}

void TstSymbolLibrary::paletteVisibilityIsPresentationMetadata()
{
    auto json = validJson();
    QStringList errors;
    auto symbol = SymbolLibrary::parseSymbol(json, &errors);
    QVERIFY(symbol); QVERIFY(symbol->paletteVisible);
    QVERIFY(!SymbolLibrary::symbolToJson(*symbol).contains("paletteVisible"));
    json["paletteVisible"] = false;
    symbol = SymbolLibrary::parseSymbol(json, &errors);
    QVERIFY(symbol); QVERIFY(!symbol->paletteVisible);
    QCOMPARE(SymbolLibrary::symbolToJson(*symbol)["paletteVisible"], QJsonValue(false));
    SymbolLibrary library;
    QVERIFY(library.add(*symbol, &errors));
    QVERIFY(library.find(symbol->id)); // hidden entries remain ordinary definitions
    QVERIFY(library.categories().isEmpty()); // no derived category for hidden-only entries
    json["paletteVisible"] = "false";
    QVERIFY(!SymbolLibrary::parseSymbol(json, &errors));
    Document document;
    const auto id = document.addComponent(SymbolIds::OTANoSuppliesVerticalBias, {});
    QVERIFY(document.setSymbolLayout(id,"bias_position","above"));
    const auto loaded = Document::fromJson(document.toJson());
    QVERIFY(loaded.document);
    const auto *hidden = loaded.document->symbol(loaded.document->component(id)->symbolId());
    QVERIFY(hidden); QVERIFY(!hidden->paletteVisible);
    QVERIFY(loaded.document->canSetSymbolLayout(id,"bias_position","below"));
}

void TstSymbolLibrary::symbolDefaultValuesAreCreationOnly()
{
    auto json = validJson();
    QStringList errors;
    auto parsed = SymbolLibrary::parseSymbol(json, &errors);
    QVERIFY(parsed); QVERIFY(parsed->defaultValue.isEmpty());
    QVERIFY(!SymbolLibrary::symbolToJson(*parsed).contains("defaultValue"));
    json["defaultValue"] = " VCC ";
    parsed = SymbolLibrary::parseSymbol(json, &errors);
    QVERIFY(parsed); QCOMPARE(parsed->defaultValue, QString("VCC"));
    QCOMPARE(SymbolLibrary::symbolToJson(*parsed)["defaultValue"].toString(), QString("VCC"));
    const auto exported = SymbolSvgExporter::exportSymbol(*parsed);
    QVERIFY(exported.errors.isEmpty());
    const auto imported = SymbolSvgImporter::importSymbol(exported.sidecar, exported.svg, "/symbols");
    QVERIFY(imported.symbol); QCOMPARE(imported.symbol->defaultValue, QString("VCC"));
    for (const auto &value : {QJsonValue(3), QJsonValue(false), QJsonValue("A\nB"), QJsonValue("VCC\n"),
                              QJsonValue("A\rB"), QJsonValue(QString("A") + QChar(0x2028) + "B")}) {
        auto bad = json; bad["defaultValue"] = value;
        errors.clear(); QVERIFY(!SymbolLibrary::parseSymbol(bad, &errors)); QVERIFY(!errors.isEmpty());
    }
    Document document;
    QVERIFY(document.useSymbol(*parsed, &errors));
    const auto first = document.addComponent(parsed->id, {});
    QCOMPARE(document.component(first)->value(), QString("VCC"));
    QVERIFY(document.component(first)->property("value")->visible);
    QVERIFY(document.setComponentLabels(first,"PWR1",""));
    auto changed = *parsed; changed.defaultValue = "VDD";
    QVERIFY(document.useSymbol(changed, &errors));
    QVERIFY(document.component(first)->value().isEmpty()); // no library-update overwrite
    const auto second = document.addComponent(parsed->id, {100,0});
    QCOMPARE(document.component(second)->value(), QString("VDD"));
    const auto loaded = Document::fromJson(document.toJson());
    QVERIFY(loaded.document); QVERIFY(loaded.document->component(first)->value().isEmpty());
    QCOMPARE(loaded.document->component(second)->value(), QString("VDD"));
    Document pasted; QVector<ComponentId> ids;
    QVERIFY(pasted.pasteFragment(document.copyFragment({first}, {}), {20,20}, &ids, nullptr, &errors));
    QVERIFY(pasted.component(ids.first())->value().isEmpty()); // even an empty explicit value survives paste
    auto legacy = document.toJson(); legacy["formatVersion"] = 6;
    { auto cs=legacy["components"].toArray(); for(int i=0;i<cs.size();++i) { auto c=cs[i].toObject(); c.remove("propertyBlockJustification"); cs[i]=c; } legacy["components"]=cs; }
    auto definitions = legacy["symbols"].toArray(); auto definition = definitions[0].toObject();
    definition.remove("defaultValue"); definitions[0] = definition; legacy["symbols"] = definitions;
    const auto old = Document::fromJson(legacy);
    QVERIFY(old.document); QVERIFY(old.document->component(first)->value().isEmpty());
    QCOMPARE(old.document->component(second)->value(), QString("VDD"));
}

void TstSymbolLibrary::signalConnectionsGeometryAndTopology()
{
    for (const auto &retired : {"Rail.VEE", "Rail.VDD", "Rail.VSS", "Rail.Named", "Ground.Analog", "Ground.Digital"})
        QVERIFY(!builtinSymbolLibrary().find(SymbolId(QString::fromLatin1(retired))));
    struct Expected { SymbolId id; qreal x; qreal y; const char *value; };
    const QVector<Expected> symbols{
        {SymbolIds::Ground,0,-20,""}, {SymbolIds::RailVCC,0,20,"Vcc"}, {SymbolIds::GroundChassis,0,-20,""}, {SymbolIds::GroundSignal,0,-20,""},
        {SymbolIds::PortInput,20,0,"IN"}, {SymbolIds::PortOutput,-20,0,"OUT"}, {SymbolIds::PortBidirectional,20,0,"IO"}};
    for (const auto &entry : symbols) {
        const auto *symbol = builtinSymbolLibrary().find(entry.id);
        QVERIFY(symbol);
        checkTerminals(entry.id, {{entry.id == SymbolIds::Ground ? "1" : "connection",entry.x,entry.y}});
        if (QTest::currentTestFailed()) return;
        QCOMPARE(symbol->defaultValue, QString::fromLatin1(entry.value));
        QVERIFY(!symbol->showsReference);
        if (entry.id == SymbolIds::Ground) {
            QCOMPARE(symbol->primitives.size(), 4);
            QCOMPARE(symbol->primitives[0].points, QVector<QPointF>({{0,-20},{0,0}}));
            QCOMPARE(symbol->primitives[1].points, QVector<QPointF>({{-10,0},{10,0}}));
            QCOMPARE(symbol->primitives[2].points, QVector<QPointF>({{-6,4},{6,4}}));
            QCOMPARE(symbol->primitives[3].points, QVector<QPointF>({{-2,8},{2,8}}));
        }
        if (entry.id == SymbolIds::GroundSignal) {
            QVERIFY(symbol->paletteVisible);
            QVERIFY(symbol->defaultValue.isEmpty());
        }
        if (entry.id == SymbolIds::RailVCC) {
            QVERIFY(symbol->paletteVisible);
            QCOMPARE(symbol->primitives.size(),3);
            QCOMPARE(symbol->primitives[0].points, QVector<QPointF>({{0,20}, {0,0}}));
            QCOMPARE(symbol->primitives[1].points, QVector<QPointF>({{-10,0}, {10,0}}));
            QCOMPARE(symbol->primitives[2].kind,PrimitiveKind::Circle);
            QCOMPARE(symbol->primitives[2].points,QVector<QPointF>({{0,0}}));
            QCOMPARE(symbol->primitives[2].radius,2.0);
            QVERIFY(primitiveIsFilled(symbol->primitives[2]));
            QCOMPARE(symbol->primitives[2].fillColor,PrimitiveFillColor::Black);
        }
        if (entry.id == SymbolIds::GroundSignal)
            QCOMPARE(symbol->primitives[1].points, QVector<QPointF>({{-10,0}, {10,0}, {0,10}}));
        if (entry.id == SymbolIds::GroundChassis) {
            QCOMPARE(symbol->primitives.size(), 5);
            QCOMPARE(symbol->primitives[2].points, QVector<QPointF>({{-10,0}, {-15,7}}));
            QCOMPARE(symbol->primitives[4].points, QVector<QPointF>({{10,0}, {5,7}}));
        }
        if (entry.id == SymbolIds::PortInput || entry.id == SymbolIds::PortOutput)
            QCOMPARE(symbol->primitives[0].points, QVector<QPointF>({{-10,-6}, {4,-6}, {10,0}, {4,6}, {-10,6}}));
        if (entry.id == SymbolIds::PortBidirectional)
            QCOMPARE(symbol->primitives[0].points, QVector<QPointF>({{-10,0}, {-4,-6}, {4,-6}, {10,0}, {4,6}, {-4,6}}));
        for (const auto rotation : {Rotation::Deg0, Rotation::Deg90, Rotation::Deg180, Rotation::Deg270})
            for (bool mirrored : {false,true}) {
                Document document;
                const auto a = document.addComponent(entry.id, {}, rotation);
                const auto b = document.addComponent(entry.id, {100,0}, rotation);
                QVERIFY(document.setComponentMirrored(a, mirrored));
                QVERIFY(document.setComponentMirrored(b, mirrored));
                QCOMPARE(document.component(a)->value(), QString::fromLatin1(entry.value));
                QVERIFY(!document.component(a)->property("reference")->visible);
                QVERIFY(document.component(a)->reference() != document.component(b)->reference());
                QCOMPARE(document.computeNets().size(), 2); // equal names do not connect
                const auto wire = document.addWire(makeTerminalEndpoint(a,0), {}, makeTerminalEndpoint(b,0));
                QVERIFY(wire); QCOMPARE(document.computeNets().size(), 1);
                QVERIFY(document.setComponentLabels(a,document.component(a)->reference(),"Custom name"));
                QCOMPARE(document.computeNets().size(), 1);
                QVERIFY(document.moveComponent(b, {120,10}));
                QCOMPARE(document.wire(*wire)->end, makeTerminalEndpoint(b,0));
                QCOMPARE(document.wire(*wire)->vertices.last(), document.terminalPosition(*document.component(b),0));
                QVERIFY(document.transformSelection({a,b}, {}, {SelectionTransform::Kind::Rotate, {}, {}, 1}));
                QVERIFY(document.transformSelection({a,b}, {}, {SelectionTransform::Kind::FlipHorizontal}));
                const auto loaded = Document::fromJson(document.toJson());
                QVERIFY(loaded.document); QCOMPARE(loaded.document->toJson(), document.toJson());
            }
    }
}

void TstSymbolLibrary::layoutSwitchingPreservesWiresAndRollsBack()
{
    for (const auto &kind : {SymbolIds::OTAWithSuppliesVerticalBias, SymbolIds::OTANoSuppliesVerticalBias})
        for (const auto rotation : {Rotation::Deg0, Rotation::Deg90, Rotation::Deg180, Rotation::Deg270})
            for (bool mirrored : {false,true}) {
                Document document;
                const auto a = document.addComponent(kind, {}, rotation);
                const auto b = document.addComponent(kind, {200,0}, rotation);
                QVERIFY(document.setComponentMirrored(a, mirrored));
                QVERIFY(document.setComponentMirrored(b, mirrored));
                QVERIFY(document.setComponentLabels(a,"U1a","LM13700"));
                QVERIFY(document.setPropertyBlockOffset({a}, QPointF(17,31)));
                for (int i = 0; i < document.terminalCount(*document.component(a)); ++i)
                    QVERIFY(document.addWire(makeTerminalEndpoint(a,i), {}, makeTerminalEndpoint(b,i)));
                const auto properties = document.component(a)->properties();
                const auto nets = document.computeNets().size();
                QStringList errors;
                QVERIFY2(document.setSymbolLayout(a,"bias_position","above",&errors), qPrintable(errors.join('\n')));
                QCOMPARE(document.computeNets().size(), nets);
                QCOMPARE(document.component(a)->reference(), QString("U1a"));
                QCOMPARE(document.component(a)->value(), QString("LM13700"));
                QCOMPARE(document.component(a)->properties().size(), properties.size());
                QCOMPARE(document.component(a)->propertyBlockOffset(), std::optional<QPointF>(QPointF(17,31)));
                QCOMPARE(document.component(a)->rotation(), rotation);
                QCOMPARE(document.component(a)->mirrored(), mirrored);
                for (const auto &wire : document.wires()) {
                    const auto ref = std::get<TerminalRef>(wire.start);
                    QCOMPARE(wire.vertices.first(), document.terminalPosition(*document.component(a), ref.terminal));
                    QCOMPARE(wire.end, makeTerminalEndpoint(b, ref.terminal));
                }
                const auto switched = document.toJsonBytes();
                QVERIFY(document.setSymbolLayout(a,"bias_position","above"));
                QCOMPARE(document.toJsonBytes(), switched);
                QVERIFY(document.setSymbolLayout(a,"bias_position","below"));
                QCOMPARE(document.component(a)->symbolId(), kind);
                QVERIFY(!document.setSymbolLayout(a,"bias_position","unknown"));
                QVERIFY(!document.setSymbolLayout(a,"unknown","above"));
                QVERIFY(!document.setSymbolLayout(999,"bias_position","above"));
            }
    // A move onto an anchored endpoint would collapse the wire: roll back everything.
    Document document;
    const auto a = document.addComponent(SymbolIds::OTANoSuppliesVerticalBias, {});
    const auto b = document.addComponent(SymbolIds::Resistor, {50,-20});
    QVERIFY(document.addWire(makeTerminalEndpoint(a,3), {}, makeTerminalEndpoint(b,0)));
    const auto before = document.toJsonBytes();
    QStringList errors;
    QVERIFY(!document.setSymbolLayout(a,"bias_position","above", &errors));
    QVERIFY(!errors.isEmpty()); QCOMPARE(document.toJsonBytes(), before);
    // Branch identities and the other arms stay anchored.
    Document branched;
    const auto ota = branched.addComponent(SymbolIds::OTANoSuppliesVerticalBias, {});
    const auto resistor = branched.addComponent(SymbolIds::Resistor, {120,20});
    const auto ground = branched.addComponent(SymbolIds::Ground, {60,100});
    const auto wire = branched.addWire(makeTerminalEndpoint(ota,3), {}, makeTerminalEndpoint(resistor,0));
    QVERIFY(wire);
    const auto node = branched.branchWireAt(*wire, {60,20}); QVERIFY(node);
    QVERIFY(branched.addWire(makeNodeEndpoint(*node), {}, makeTerminalEndpoint(ground,0)));
    const auto nodes = branched.nodes(); const auto junctions = branched.junctionPoints();
    QVERIFY(branched.setSymbolLayout(ota,"bias_position","above"));
    QCOMPARE(branched.nodes(), nodes); QCOMPARE(branched.junctionPoints(), junctions);
    QCOMPARE(branched.wires().size(), 3);
    // Mapping is by declared terminal roles, not an assumption about indices.
    const auto installed = applicationSymbolLibrary();
    struct Restore { SymbolLibrary saved; ~Restore() { applicationSymbolLibrary() = saved; } } restore{installed};
    auto source = *installed.find(SymbolIds::OTANoSuppliesVerticalBias);
    auto target = *installed.find(SymbolIds::OTANoSuppliesBiasAbove);
    source.id = SymbolId("LayoutMapping.Below"); target.id = SymbolId("LayoutMapping.Above");
    std::swap(target.terminals[0], target.terminals[3]);
    for (auto *definition : {&source,&target}) {
        definition->layoutControls[0].choices[0].symbolId = source.id;
        definition->layoutControls[0].choices[1].symbolId = target.id;
    }
    QVERIFY(applicationSymbolLibrary().add(source, &errors));
    QVERIFY(applicationSymbolLibrary().add(target, &errors));
    Document mapped;
    const auto device = mapped.addComponent(source.id, {});
    const auto input = mapped.addComponent(SymbolIds::Resistor, {100,-10});
    const auto bias = mapped.addComponent(SymbolIds::Resistor, {100,20});
    const auto inputWire = mapped.addWire(makeTerminalEndpoint(device,0), {}, makeTerminalEndpoint(input,0));
    const auto biasWire = mapped.addWire(makeTerminalEndpoint(device,3), {}, makeTerminalEndpoint(bias,0));
    QVERIFY(inputWire); QVERIFY(biasWire);
    QVERIFY(mapped.setSymbolLayout(device,"bias_position","above"));
    QCOMPARE(mapped.wire(*inputWire)->start, makeTerminalEndpoint(device,3));
    QCOMPARE(mapped.wire(*biasWire)->start, makeTerminalEndpoint(device,0));
    QCOMPARE(mapped.wire(*inputWire)->vertices.first(), QPointF(-30,-10));
    QCOMPARE(mapped.wire(*biasWire)->vertices.first(), QPointF(30,-20));
}

void TstSymbolLibrary::layoutPersistenceAndClipboardAreSelfContained()
{
    Document document;
    const auto id = document.addComponent(SymbolIds::OTANoSuppliesVerticalBias, {});
    const auto json = document.toJson();
    QCOMPARE(json["formatVersion"].toInt(), 10);
    QCOMPARE(json["symbols"].toArray().size(), 2);
    const auto clipboard = document.copyFragment({id}, {});
    const auto installed = applicationSymbolLibrary();
    struct Restore { SymbolLibrary saved; ~Restore() { applicationSymbolLibrary() = saved; } } restore{installed};
    applicationSymbolLibrary() = {};
    auto loaded = Document::fromJson(json);
    QVERIFY2(loaded.document, qPrintable(loaded.errors.join('\n')));
    QVERIFY(loaded.document->setSymbolLayout(id,"bias_position","above"));
    QVERIFY(Document::fromJson(loaded.document->toJson()).document);
    Document pasted; QVector<ComponentId> components;
    QStringList errors;
    QVERIFY2(pasted.pasteFragment(clipboard, {10,10}, &components, nullptr, &errors), qPrintable(errors.join('\n')));
    QVERIFY(pasted.setSymbolLayout(components.first(),"bias_position","above"));
    auto missing = json;
    auto definitions = missing["symbols"].toArray(); definitions.removeAt(0); missing["symbols"] = definitions;
    QVERIFY(!Document::fromJson(missing).document);
    auto invalid = json;
    auto entries = invalid["symbols"].toArray();
    auto definition = entries[0].toObject();
    auto controls = definition["layoutControls"].toArray();
    auto control = controls[0].toObject(); auto choices = control["choices"].toArray();
    auto choice = choices[0].toObject(); auto mappings = choice["terminals"].toArray();
    auto mapping = mappings[0].toObject(); mapping["to"] = "unknown"; mappings[0] = mapping;
    choice["terminals"] = mappings; choices[0] = choice; control["choices"] = choices;
    controls[0] = control; definition["layoutControls"] = controls; entries[0] = definition;
    invalid["symbols"] = entries;
    QVERIFY(!Document::fromJson(invalid).document);
    auto v6 = json; v6["formatVersion"] = 6;
    { auto cs=v6["components"].toArray(); for(int i=0;i<cs.size();++i) { auto c=cs[i].toObject(); c.remove("propertyBlockJustification"); cs[i]=c; } v6["components"]=cs; }
    auto symbols = v6["symbols"].toArray();
    for (int i = symbols.size()-1; i >= 0; --i) {
        auto entry = symbols[i].toObject();
        if (entry["id"].toString() != document.component(id)->symbolId().value()) symbols.removeAt(i);
        else { entry.remove("layoutControls"); symbols[i] = entry; }
    }
    v6["symbols"] = symbols;
    const auto legacy = Document::fromJson(v6);
    QVERIFY(legacy.document);
    QVERIFY(!legacy.document->canSetSymbolLayout(id,"bias_position","above"));
    QCOMPARE(legacy.document->component(id)->symbolId(), document.component(id)->symbolId());
    QCOMPARE(legacy.document->symbol(document.component(id)->symbolId())->primitives[9].points,
             document.symbol(document.component(id)->symbolId())->primitives[9].points);
}

void TstSymbolLibrary::builtinTerminalsAreFrozen()
{
    checkTerminals(SymbolIds::Resistor, {{"1", -20, 0}, {"2", 20, 0}});
    if (QTest::currentTestFailed()) return;
    checkTerminals(SymbolIds::Capacitor, {{"1", -20, 0}, {"2", 20, 0}});
    if (QTest::currentTestFailed()) return;
    checkTerminals(SymbolIds::ElectrolyticCapacitor, {{"+", -20, 0}, {"-", 20, 0}});
    if (QTest::currentTestFailed()) return;
    checkTerminals(SymbolIds::OpAmpGrid10, {{"in+", -30, -10}, {"in-", -30, 10}, {"out", 30, 0},
                                         {"v+", 0, -20}, {"v-", 0, 20}});
    if (QTest::currentTestFailed()) return;
    checkTerminals(SymbolIds::Diode, {{"anode", -20, 0}, {"cathode", 20, 0}});
    if (QTest::currentTestFailed()) return;
    checkTerminals(SymbolIds::Ground, {{"1", 0, -20}});
    if (QTest::currentTestFailed()) return;
    checkTerminals(SymbolIds::Potentiometer, {{"1", -20, 0}, {"2", 20, 0}, {"wiper", 0, -20}});
    if (QTest::currentTestFailed()) return;
    checkTerminals(SymbolIds::Jack, {{"tip", -20, 0}, {"sleeve", 20, 0}});
}

void TstSymbolLibrary::builtinPrimitiveShapes()
{
    using K = PrimitiveKind;
    checkKinds(SymbolIds::Resistor, {K::Polygon, K::Line, K::Line});
    if (QTest::currentTestFailed()) return;
    checkKinds(SymbolIds::Capacitor, {K::Line, K::Line, K::Polygon, K::Polygon});
    if (QTest::currentTestFailed()) return;
    checkKinds(SymbolIds::ElectrolyticCapacitor, {K::Line, K::Line, K::Polygon, K::Polygon, K::Line, K::Line});
    if (QTest::currentTestFailed()) return;
    checkKinds(SymbolIds::OpAmpGrid10, {K::Polygon, K::Line, K::Line, K::Line, K::Line, K::Line, K::Line, K::Line, K::Line});
    if (QTest::currentTestFailed()) return;
    checkKinds(SymbolIds::Diode, {K::Line, K::Line, K::Polygon, K::Line});
    if (QTest::currentTestFailed()) return;
    checkKinds(SymbolIds::Ground, {K::Line, K::Line, K::Line, K::Line});
    if (QTest::currentTestFailed()) return;
    checkKinds(SymbolIds::Potentiometer, {K::Line, K::Line, K::Polygon, K::Line, K::Polygon, K::Line, K::Polygon});
    if (QTest::currentTestFailed()) return;
    checkKinds(SymbolIds::Jack, {K::Circle, K::Line, K::Line});
    if (QTest::currentTestFailed()) return;

    const LibrarySymbol *jack = builtinSymbolLibrary().find(SymbolIds::Jack);
    QCOMPARE(jack->primitives[0].points, QVector<QPointF>({QPointF(0, 0)}));
    QCOMPARE(jack->primitives[0].radius, 10.0);
    const LibrarySymbol *diode = builtinSymbolLibrary().find(SymbolIds::Diode);
    QCOMPARE(diode->primitives[0].points, QVector<QPointF>({{-20,0}, {-5,0}}));
    QCOMPARE(diode->primitives[1].points, QVector<QPointF>({{5,0}, {20,0}}));
    QCOMPARE(diode->primitives[3].points, QVector<QPointF>({{5,-5}, {5,5}}));
    QCOMPARE(diode->primitives[2].points,
             QVector<QPointF>({QPointF(-5, -5), QPointF(-5, 5), QPointF(5, 0)}));
}

void TstSymbolLibrary::categoryPathSplitsIntoSegments()
{
    QCOMPARE(libraryCategoryPath(QStringLiteral("Passives")), QStringList({QStringLiteral("Passives")}));
    QCOMPARE(libraryCategoryPath(QStringLiteral("Semiconductors/Diodes")),
             QStringList({QStringLiteral("Semiconductors"), QStringLiteral("Diodes")}));
}

void TstSymbolLibrary::firstUnusedReferenceUsesSmallestFreeNumber()
{
    QCOMPARE(firstUnusedReference(QStringLiteral("R"), {}), QStringLiteral("R1"));
    QCOMPARE(firstUnusedReference(QStringLiteral("R"), {QStringLiteral("R1"), QStringLiteral("R2")}),
             QStringLiteral("R3"));
    // A freed number is reused, and other prefixes are ignored.
    QCOMPARE(firstUnusedReference(QStringLiteral("R"), {QStringLiteral("R2"), QStringLiteral("C1")}),
             QStringLiteral("R1"));
    QCOMPARE(firstUnusedReference(QStringLiteral("GND"), {QStringLiteral("GND1")}), QStringLiteral("GND2"));
}

void TstSymbolLibrary::findResolvesKnownIdsOnly()
{
    const SymbolLibrary &library = builtinSymbolLibrary();
    for (const LibrarySymbol &symbol : library.symbols()) {
        const LibrarySymbol *found = library.find(symbol.id);
        QVERIFY(found);
        QCOMPARE(found->id, symbol.id);
    }
    QVERIFY(!library.find(SymbolId(QStringLiteral("NotARealSymbol"))));
    QVERIFY(!library.find(SymbolId()));
    // Ids are case-sensitive, like serialized kind names always were.
    QVERIFY(!library.find(SymbolId(QStringLiteral("resistor"))));
}

void TstSymbolLibrary::validationRejectsBadDefinitions()
{
    QVERIFY(validateLibrarySymbol(makeValid()).isEmpty());

    const auto rejects = [](const std::function<void(LibrarySymbol &)> &mutate) {
        LibrarySymbol s = makeValid();
        mutate(s);
        return !validateLibrarySymbol(s).isEmpty();
    };
    const qreal inf = std::numeric_limits<qreal>::infinity();

    QVERIFY(rejects([](LibrarySymbol &s) { s.id = SymbolId(); }));
    QVERIFY(rejects([](LibrarySymbol &s) { s.id = SymbolId(QStringLiteral(" Test")); }));
    QVERIFY(rejects([](LibrarySymbol &s) { s.displayName = QStringLiteral("  "); }));
    QVERIFY(rejects([](LibrarySymbol &s) { s.category.clear(); }));
    QVERIFY(rejects([](LibrarySymbol &s) { s.referencePrefix.clear(); }));
    QVERIFY(rejects([](LibrarySymbol &s) { s.category = QStringLiteral("/Passives"); }));
    QVERIFY(rejects([](LibrarySymbol &s) { s.category = QStringLiteral("Passives/"); }));
    QVERIFY(rejects([](LibrarySymbol &s) { s.category = QStringLiteral("Semiconductors//Diodes"); }));
    QVERIFY(rejects([](LibrarySymbol &s) { s.category = QStringLiteral("Semiconductors/ Diodes"); }));
    QVERIFY(rejects([](LibrarySymbol &s) { s.category = QStringLiteral("/"); }));
    QVERIFY(rejects([](LibrarySymbol &s) { s.primitives.clear(); }));
    QVERIFY(rejects([](LibrarySymbol &s) { s.terminals.clear(); }));
    QVERIFY(rejects([](LibrarySymbol &s) { s.primitives[0].points.append({0, 5}); })); // 3-point Line
    QVERIFY(rejects([](LibrarySymbol &s) { s.primitives[0].kind = PrimitiveKind::Polygon; })); // 2-point Polygon
    QVERIFY(rejects([](LibrarySymbol &s) { s.primitives[0].kind = PrimitiveKind::Polyline;
                                            s.primitives[0].points.resize(1); }));
    QVERIFY(rejects([](LibrarySymbol &s) { s.primitives[0] = {PrimitiveKind::Circle, {{0, 0}}, 0.0}; }));
    QVERIFY(rejects([](LibrarySymbol &s) { s.primitives[0] = {PrimitiveKind::Circle, {{0, 0}}, -3.0}; }));
    QVERIFY(rejects([](LibrarySymbol &s) { s.primitives[0] = {PrimitiveKind::Circle, {{0, 0}, {1, 1}}, 5.0}; }));
    QVERIFY(rejects([inf](LibrarySymbol &s) { s.primitives[0].points[0] = QPointF(inf, 0); }));
    QVERIFY(rejects([](LibrarySymbol &s) { s.terminals[1].name = QStringLiteral("a"); })); // duplicate name
    QVERIFY(rejects([](LibrarySymbol &s) { s.terminals[1].name.clear(); }));
    QVERIFY(rejects([](LibrarySymbol &s) { s.terminals[1].offset = s.terminals[0].offset; }));
    QVERIFY(rejects([inf](LibrarySymbol &s) { s.terminals[0].offset = QPointF(0, inf); }));

    // Nested category paths are valid.
    LibrarySymbol nested = makeValid();
    nested.category = QStringLiteral("Semiconductors/Transistors/MOSFET");
    QVERIFY(validateLibrarySymbol(nested).isEmpty());

    // A valid circle passes.
    LibrarySymbol circle = makeValid();
    circle.primitives[0] = {PrimitiveKind::Circle, {{0, 0}}, 4.0};
    QVERIFY(validateLibrarySymbol(circle).isEmpty());
}

void TstSymbolLibrary::pathPrimitiveValidatesAndBoundsCurvesExactly()
{
    SymbolPrimitive path;
    path.kind = PrimitiveKind::Path;
    path.segments = {
        {PathSegmentKind::MoveTo, {{0, 0}}},
        {PathSegmentKind::CubicTo, {{0, 100}, {100, 100}, {100, 0}}},
        {PathSegmentKind::Close, {}},
    };

    LibrarySymbol symbol = makeValid();
    symbol.primitives = {path};
    QVERIFY2(validateLibrarySymbol(symbol).isEmpty(),
             qPrintable(validateLibrarySymbol(symbol).join(QLatin1Char('\n'))));
    QCOMPARE(symbolBounds(symbol), QRectF(0, 0, 100, 75));

    path.segments[1] = {PathSegmentKind::CubicTo,
                        {{-100, -100}, {-100, -100}, {0, 0}}};
    symbol.primitives = {path};
    QCOMPARE(symbolBounds(symbol), QRectF(-75, -75, 75, 75));

    const auto rejectsPath = [&path](const std::function<void(SymbolPrimitive &)> &mutate) {
        LibrarySymbol candidate = makeValid();
        SymbolPrimitive invalid = path;
        mutate(invalid);
        candidate.primitives = {invalid};
        return !validateLibrarySymbol(candidate).isEmpty();
    };
    QVERIFY(rejectsPath([](SymbolPrimitive &p) { p.segments.clear(); }));
    QVERIFY(rejectsPath([](SymbolPrimitive &p) {
        p.segments[0] = {PathSegmentKind::LineTo, {{0, 0}}};
    }));
    QVERIFY(rejectsPath([](SymbolPrimitive &p) {
        p.segments[1].points.resize(2);
    }));
    QVERIFY(rejectsPath([](SymbolPrimitive &p) {
        p.segments[1].points[0] = QPointF(0, std::numeric_limits<qreal>::infinity());
    }));
    QVERIFY(rejectsPath([](SymbolPrimitive &p) {
        p.segments[1].kind = static_cast<PathSegmentKind>(99);
    }));
    QVERIFY(rejectsPath([](SymbolPrimitive &p) {
        p.fillRule = static_cast<PathFillRule>(99);
    }));
}

void TstSymbolLibrary::addRejectsDuplicateIds()
{
    SymbolLibrary library;
    QStringList errors;
    QVERIFY(library.add(makeValid(), &errors));
    QVERIFY(errors.isEmpty());
    QCOMPARE(library.size(), 1);

    LibrarySymbol other = makeValid();
    other.displayName = QStringLiteral("Different name, same id");
    QVERIFY(!library.add(other, &errors));
    QCOMPARE(errors.size(), 1);
    QVERIFY2(errors.first().contains(QStringLiteral("duplicate symbol id")), qPrintable(errors.first()));
    QCOMPARE(library.size(), 1);
    QCOMPARE(library.find(SymbolId(QStringLiteral("Test")))->displayName, QStringLiteral("Test part"));

    errors.clear();
    LibrarySymbol invalid = makeValid();
    invalid.id = SymbolId(QStringLiteral("Other"));
    invalid.terminals.clear();
    QVERIFY(!library.add(invalid, &errors));
    QVERIFY(!errors.isEmpty());
    QVERIFY(!library.find(SymbolId(QStringLiteral("Other"))));
}

void TstSymbolLibrary::parseSymbolValidatesJsonShape()
{
    QStringList errors;
    const auto parsed = SymbolLibrary::parseSymbol(validJson(), &errors);
    QVERIFY2(parsed, qPrintable(errors.join(QLatin1Char('\n'))));
    QCOMPARE(parsed->id, SymbolId(QStringLiteral("Test")));
    QCOMPARE(parsed->terminals.size(), 2);
    QVERIFY(parsed->showsReference);

    const QJsonObject pathEntry{
        {QStringLiteral("kind"), QStringLiteral("Path")},
        {QStringLiteral("filled"), true},
        {QStringLiteral("fillRule"), QStringLiteral("evenodd")},
        {QStringLiteral("segments"), QJsonArray{
            QJsonArray{QStringLiteral("M"), 0, 0},
            QJsonArray{QStringLiteral("C"), 0, 100, 100, 100, 100, 0},
            QJsonArray{QStringLiteral("Z")}}}};
    QJsonObject pathJson = validJson();
    pathJson.insert(QStringLiteral("primitives"), QJsonArray{pathEntry});
    QStringList pathErrors;
    const auto parsedPath = SymbolLibrary::parseSymbol(pathJson, &pathErrors);
    QVERIFY2(parsedPath, qPrintable(pathErrors.join(QLatin1Char('\n'))));
    QCOMPARE(parsedPath->primitives[0].kind, PrimitiveKind::Path);
    QCOMPARE(parsedPath->primitives[0].segments.size(), 3);
    QVERIFY(parsedPath->primitives[0].filled.has_value());
    QCOMPARE(*parsedPath->primitives[0].filled, true);
    QCOMPARE(parsedPath->primitives[0].fillRule, PathFillRule::EvenOdd);

    QJsonObject defaultPathEntry = pathEntry;
    defaultPathEntry.remove(QStringLiteral("filled"));
    defaultPathEntry.remove(QStringLiteral("fillRule"));
    pathJson.insert(QStringLiteral("primitives"), QJsonArray{defaultPathEntry});
    pathErrors.clear();
    const auto parsedDefaultPath = SymbolLibrary::parseSymbol(pathJson, &pathErrors);
    QVERIFY2(parsedDefaultPath, qPrintable(pathErrors.join(QLatin1Char('\n'))));
    QVERIFY(!parsedDefaultPath->primitives[0].filled.has_value());
    QCOMPARE(parsedDefaultPath->primitives[0].fillRule, PathFillRule::NonZero);

    const auto rejectsPathEntry = [](const QJsonObject &replacement) {
        QJsonObject object = validJson();
        object.insert(QStringLiteral("primitives"), QJsonArray{replacement});
        QStringList errs;
        return !SymbolLibrary::parseSymbol(object, &errs) && !errs.isEmpty();
    };
    QVERIFY(rejectsPathEntry(QJsonObject{
        {QStringLiteral("kind"), QStringLiteral("Path")},
        {QStringLiteral("segments"), QJsonArray{}}}));
    QVERIFY(rejectsPathEntry(QJsonObject{
        {QStringLiteral("kind"), QStringLiteral("Path")},
        {QStringLiteral("segments"), QJsonArray{QJsonArray{QStringLiteral("L"), 0, 0}}}}));
    QVERIFY(rejectsPathEntry(QJsonObject{
        {QStringLiteral("kind"), QStringLiteral("Path")},
        {QStringLiteral("segments"), QJsonArray{
            QJsonArray{QStringLiteral("M"), 0, 0},
            QJsonArray{QStringLiteral("C"), 1, 2, 3, 4, 5}}}}));
    QVERIFY(rejectsPathEntry(QJsonObject{
        {QStringLiteral("kind"), QStringLiteral("Path")},
        {QStringLiteral("fillRule"), QStringLiteral("winding")},
        {QStringLiteral("segments"), QJsonArray{QJsonArray{QStringLiteral("M"), 0, 0}}}}));
    QVERIFY(rejectsPathEntry(QJsonObject{
        {QStringLiteral("kind"), QStringLiteral("Path")},
        {QStringLiteral("segments"), QJsonArray{QJsonArray{QStringLiteral("X"), 0, 0}}}}));
    QVERIFY(rejectsPathEntry(QJsonObject{
        {QStringLiteral("kind"), QStringLiteral("Path")},
        {QStringLiteral("filled"), QStringLiteral("yes")},
        {QStringLiteral("segments"), QJsonArray{QJsonArray{QStringLiteral("M"), 0, 0}}}}));

    const auto rejects = [](const QString &key, const QJsonValue &value) {
        QJsonObject object = validJson();
        if (value.isUndefined()) object.remove(key); else object.insert(key, value);
        QStringList errs;
        return !SymbolLibrary::parseSymbol(object, &errs) && !errs.isEmpty();
    };
    QVERIFY(rejects(QStringLiteral("id"), QJsonValue::Undefined));
    QVERIFY(rejects(QStringLiteral("id"), 5));
    QVERIFY(rejects(QStringLiteral("name"), QJsonValue::Undefined));
    QVERIFY(rejects(QStringLiteral("showsReference"), QStringLiteral("yes")));
    QVERIFY(rejects(QStringLiteral("primitives"), QStringLiteral("nope")));
    QVERIFY(rejects(QStringLiteral("terminals"), QJsonValue::Undefined));
    QVERIFY(rejects(QStringLiteral("primitives"), QJsonArray{QJsonObject{
        {QStringLiteral("kind"), QStringLiteral("Spline")}, {QStringLiteral("points"), QJsonArray{}}}}));
    QVERIFY(rejects(QStringLiteral("primitives"), QJsonArray{QJsonObject{
        {QStringLiteral("kind"), QStringLiteral("Line")},
        {QStringLiteral("points"), QJsonArray{QJsonArray{0, 0}, QJsonArray{1}}}}})); // bad pair
    QVERIFY(rejects(QStringLiteral("primitives"), QJsonArray{QJsonObject{
        {QStringLiteral("kind"), QStringLiteral("Line")},
        {QStringLiteral("points"), QJsonArray{QJsonArray{0, 0}, QJsonArray{5, 0}}},
        {QStringLiteral("radius"), 3}}})); // radius only valid for Circle
    QVERIFY(rejects(QStringLiteral("primitives"), QJsonArray{QJsonObject{
        {QStringLiteral("kind"), QStringLiteral("Circle")},
        {QStringLiteral("points"), QJsonArray{QJsonArray{0, 0}}}}})); // missing radius
    QVERIFY(rejects(QStringLiteral("terminals"), QJsonArray{QJsonObject{
        {QStringLiteral("name"), QStringLiteral("a")}, {QStringLiteral("x"), QStringLiteral("0")},
        {QStringLiteral("y"), 0}}}));
}

void TstSymbolLibrary::loadFromDirectoryIsAllOrNothing()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString base = dir.path();
    const auto jsonBytes = [](const QJsonObject &o) { return QJsonDocument(o).toJson(); };
    const auto manifest = [](const QStringList &files) {
        QJsonArray array;
        for (const QString &f : files) array.append(f);
        return QJsonDocument(QJsonObject{{QStringLiteral("symbols"), array}}).toJson();
    };

    QJsonObject second = validJson();
    second.insert(QStringLiteral("id"), QStringLiteral("Test2"));
    QVERIFY(writeFile(base + QStringLiteral("/a.json"), jsonBytes(validJson())));
    QVERIFY(writeFile(base + QStringLiteral("/b.json"), jsonBytes(second)));

    // Valid: two symbols, manifest order preserved.
    QVERIFY(writeFile(base + QStringLiteral("/manifest.json"),
                      manifest({QStringLiteral("b.json"), QStringLiteral("a.json")})));
    QStringList errors;
    SymbolLibrary loaded = SymbolLibrary::loadFromDirectory(base, &errors);
    QVERIFY2(errors.isEmpty(), qPrintable(errors.join(QLatin1Char('\n'))));
    QCOMPARE(loaded.size(), 2);
    QCOMPARE(loaded.symbols()[0].id, SymbolId(QStringLiteral("Test2")));
    QCOMPARE(loaded.symbols()[1].id, SymbolId(QStringLiteral("Test")));

    // A listed file that does not exist fails the whole load.
    QVERIFY(writeFile(base + QStringLiteral("/manifest.json"),
                      manifest({QStringLiteral("a.json"), QStringLiteral("missing.json")})));
    errors.clear();
    loaded = SymbolLibrary::loadFromDirectory(base, &errors);
    QVERIFY(!errors.isEmpty());
    QCOMPARE(loaded.size(), 0);

    // The same id in two files fails the whole load.
    QVERIFY(writeFile(base + QStringLiteral("/dup.json"), jsonBytes(validJson())));
    QVERIFY(writeFile(base + QStringLiteral("/manifest.json"),
                      manifest({QStringLiteral("a.json"), QStringLiteral("dup.json")})));
    errors.clear();
    loaded = SymbolLibrary::loadFromDirectory(base, &errors);
    QVERIFY(!errors.isEmpty());
    QVERIFY2(errors.join(QLatin1Char('\n')).contains(QStringLiteral("duplicate symbol id")),
             qPrintable(errors.join(QLatin1Char('\n'))));
    QCOMPARE(loaded.size(), 0);

    // An invalid entry fails the whole load.
    QJsonObject bad = validJson();
    bad.insert(QStringLiteral("terminals"), QJsonArray{});
    QVERIFY(writeFile(base + QStringLiteral("/bad.json"), jsonBytes(bad)));
    QVERIFY(writeFile(base + QStringLiteral("/manifest.json"),
                      manifest({QStringLiteral("b.json"), QStringLiteral("bad.json")})));
    errors.clear();
    loaded = SymbolLibrary::loadFromDirectory(base, &errors);
    QVERIFY(!errors.isEmpty());
    QCOMPARE(loaded.size(), 0);

    // A directory with no manifest fails cleanly.
    errors.clear();
    loaded = SymbolLibrary::loadFromDirectory(base + QStringLiteral("/nonexistent"), &errors);
    QVERIFY(!errors.isEmpty());
    QCOMPARE(loaded.size(), 0);
}

void TstSymbolLibrary::symbolSvgImportHandlesShapesTransformsAndWarnings()
{
    const QByteArray sidecar=QJsonDocument(QJsonObject{
        {QStringLiteral("id"),QStringLiteral("SvgTest")},
        {QStringLiteral("name"),QStringLiteral("SVG Test")},
        {QStringLiteral("category"),QStringLiteral("Tests")},
        {QStringLiteral("referencePrefix"),QStringLiteral("X")},
        {QStringLiteral("showsReference"),true},
        {QStringLiteral("svg"),QStringLiteral("shape.svg")},
        {QStringLiteral("origin"),QJsonObject{{QStringLiteral("x"),1},{QStringLiteral("y"),2}}},
        {QStringLiteral("terminals"),QJsonArray{QJsonObject{{QStringLiteral("x"),11},{QStringLiteral("y"),22}}}}
    }).toJson();
    const QByteArray svg=R"SVG(<svg xmlns="http://www.w3.org/2000/svg" transform="translate(10 20)">
      <g transform="scale(2)" style="fill:none; fill-rule:evenodd">
        <line x1="0" y1="0" x2="2" y2="0" stroke="red"/>
        <polygon points="0,0 2,0 1,2" fill="black"/>
        <path fill="black" d="M0 0 l2 0 h1 v1 c1 0 1 1 2 1 s1 1 2 0 q1 -1 2 0 t2 0 a1 1 0 0 1 2 2 z"/>
        <circle cx="1" cy="1" r="1"/>
        <text>ignored</text>
        <path d="this is invalid but hidden" display="none"/>
      </g>
    </svg>)SVG";
    const SymbolSvgImportResult result=SymbolSvgImporter::importSymbol(sidecar,svg,QStringLiteral("/symbols"));
    QVERIFY2(result.symbol.has_value(),qPrintable(result.errors.join(QLatin1Char('\n'))));
    QCOMPARE(result.symbol->primitives.size(),4);
    QCOMPARE(result.symbol->primitives[0].kind,PrimitiveKind::Line);
    QCOMPARE(result.symbol->primitives[0].points[0],QPointF(9,18));
    QCOMPARE(result.symbol->primitives[0].points[1],QPointF(13,18));
    QCOMPARE(result.symbol->terminals[0].name,QStringLiteral("1"));
    QCOMPARE(result.symbol->terminals[0].offset,QPointF(10,20));
    QVERIFY(result.symbol->primitives[1].filled.value_or(false));
    QCOMPARE(result.symbol->primitives[2].kind,PrimitiveKind::Path);
    QCOMPARE(result.symbol->primitives[2].fillRule,PathFillRule::EvenOdd);
    QVERIFY(result.symbol->primitives[2].segments.size()>=10);
    QCOMPARE(result.symbol->primitives[3].kind,PrimitiveKind::Circle);
    QVERIFY(!result.symbol->primitives[3].filled.value_or(true));
    QVERIFY(std::any_of(result.warnings.cbegin(),result.warnings.cend(),[](const QString &w){return w.contains(QStringLiteral("<text>"));}));
    QVERIFY(std::any_of(result.warnings.cbegin(),result.warnings.cend(),[](const QString &w){return w.contains(QStringLiteral("style or attribute"));}));
    QVERIFY(validateLibrarySymbol(*result.symbol).isEmpty());
}

void TstSymbolLibrary::symbolSvgImportRejectsMalformedInputs()
{
    const QJsonObject base{{QStringLiteral("id"),QStringLiteral("SvgTest")},
        {QStringLiteral("name"),QStringLiteral("SVG Test")},{QStringLiteral("category"),QStringLiteral("Tests")},
        {QStringLiteral("referencePrefix"),QStringLiteral("X")},{QStringLiteral("showsReference"),true},
        {QStringLiteral("svg"),QStringLiteral("shape.svg")},
        {QStringLiteral("terminals"),QJsonArray{QJsonObject{{QStringLiteral("x"),0},{QStringLiteral("y"),0}}}}};
    auto run=[&](const QJsonObject &sidecar,const QByteArray &svg) {
        return SymbolSvgImporter::importSymbol(QJsonDocument(sidecar).toJson(),svg,QStringLiteral("/symbols"));
    };
    auto bad=run(base,R"(<svg><path d="M0 0 L nope 1"/></svg>)");
    QVERIFY(!bad.symbol); QVERIFY(!bad.errors.isEmpty());
    bad=run(base,R"(<svg><path d="M0 0 A1 1 0 2 0 2 2"/></svg>)");
    QVERIFY(!bad.symbol); QVERIFY(!bad.errors.isEmpty());
    bad=run(base,R"(<svg><path d="M0 0"/></svg>)");
    QVERIFY(!bad.symbol); QVERIFY(!bad.errors.isEmpty());
    bad=run(base,R"(<svg><unknown/></svg>)");
    QVERIFY(!bad.symbol); QVERIFY(!bad.errors.isEmpty());
    QJsonObject traversal=base; traversal.insert(QStringLiteral("svg"),QStringLiteral("../outside.svg"));
    bad=run(traversal,R"(<svg><line x2="1"/></svg>)");
    QVERIFY(!bad.symbol); QVERIFY(!bad.errors.isEmpty());
    QJsonObject badId=base; badId.insert(QStringLiteral("id"),QStringLiteral("../bad"));
    bad=run(badId,R"(<svg><line x2="1"/></svg>)");
    QVERIFY(!bad.symbol); QVERIFY(!bad.errors.isEmpty());
    bad=run(base,QByteArrayLiteral("<svg><line></svg>"));
    QVERIFY(!bad.symbol); QVERIFY(!bad.errors.isEmpty());
}

void TstSymbolLibrary::symbolSvgImportAppliesTransformsAndConvertsEllipses()
{
    const QByteArray sidecar=QJsonDocument(QJsonObject{
        {QStringLiteral("id"),QStringLiteral("TransformTest")},
        {QStringLiteral("name"),QStringLiteral("Transform Test")},
        {QStringLiteral("category"),QStringLiteral("Tests")},
        {QStringLiteral("referencePrefix"),QStringLiteral("X")},
        {QStringLiteral("showsReference"),false},
        {QStringLiteral("svg"),QStringLiteral("transform.svg")},
        {QStringLiteral("terminals"),QJsonArray{QJsonObject{{QStringLiteral("x"),0},{QStringLiteral("y"),0}}}}
    }).toJson();
    const QByteArray svg=R"SVG(<svg xmlns="http://www.w3.org/2000/svg">
      <g transform="translate(10 20) scale(2)"><line x1="1" y1="1" x2="2" y2="1"/></g>
      <g transform="rotate(90 1 1)"><line x1="2" y1="1" x2="3" y2="1"/></g>
      <g transform="skewX(45)"><line x1="0" y1="1" x2="1" y2="1"/></g>
      <g transform="skewY(45)"><line x1="1" y1="0" x2="1" y2="1"/></g>
      <g transform="matrix(1 2 3 4 5 6)"><line x1="1" y1="1" x2="2" y2="1"/></g>
      <circle cx="1" cy="2" r="3"/>
      <ellipse cx="0" cy="0" rx="1000" ry="500"/>
      <rect x="0" y="0" width="20" height="10" rx="3" ry="2"/>
      <g transform="scale(2 1)"><circle cx="0" cy="0" r="4"/></g>
      <path d="M1000 0 A1000 1000 0 0 1 -1000 0"/>
    </svg>)SVG";
    const auto result=SymbolSvgImporter::importSymbol(sidecar,svg,QStringLiteral("/symbols"));
    QVERIFY2(result.symbol.has_value(),qPrintable(result.errors.join(QLatin1Char('\n'))));
    const auto &primitives=result.symbol->primitives;
    QCOMPARE(primitives.size(),10);
    QCOMPARE(primitives[0].points,QVector<QPointF>({QPointF(12,22),QPointF(14,22)}));
    QCOMPARE(primitives[1].points,QVector<QPointF>({QPointF(1,2),QPointF(1,3)}));
    QCOMPARE(primitives[2].points,QVector<QPointF>({QPointF(1,1),QPointF(2,1)}));
    QCOMPARE(primitives[3].points,QVector<QPointF>({QPointF(1,1),QPointF(1,2)}));
    QCOMPARE(primitives[4].points,QVector<QPointF>({QPointF(9,12),QPointF(10,14)}));
    QCOMPARE(primitives[5].kind,PrimitiveKind::Circle);
    QCOMPARE(primitives[5].points.first(),QPointF(1,2));
    QCOMPARE(primitives[5].radius,3.0);
    QCOMPARE(primitives[6].kind,PrimitiveKind::Path);
    QCOMPARE(primitives[7].kind,PrimitiveKind::Path);
    QCOMPARE(primitives[8].kind,PrimitiveKind::Path);

    const SymbolPrimitive &arc=primitives[9];
    QPointF arcCurrent=arc.segments.first().points.first();
    qreal arcMaximumDeviation=0;
    for (const PathSegment &segment:arc.segments) {
        if (segment.kind!=PathSegmentKind::CubicTo) continue;
        const QPointF p0=arcCurrent,p1=segment.points[0],p2=segment.points[1],p3=segment.points[2];
        for (int sample=1;sample<100;++sample) {
            const qreal t=sample/100.0,u=1-t;
            const QPointF p=p0*(u*u*u)+p1*(3*u*u*t)+p2*(3*u*t*t)+p3*(t*t*t);
            arcMaximumDeviation=std::max(arcMaximumDeviation,std::abs(std::hypot(p.x(),p.y())-1000));
        }
        arcCurrent=p3;
    }
    QVERIFY2(arcMaximumDeviation<=0.1,qPrintable(QString::number(arcMaximumDeviation,'g',12)));

    // Check the ellipse's cubic approximation against its exact equation.
    const SymbolPrimitive &ellipse=primitives[6];
    QPointF current=ellipse.segments.first().points.first();
    qreal maximumDeviation=0;
    for (const PathSegment &segment:ellipse.segments) {
        if (segment.kind==PathSegmentKind::CubicTo) {
            const QPointF p0=current,p1=segment.points[0],p2=segment.points[1],p3=segment.points[2];
            for (int sample=1;sample<100;++sample) {
                const qreal t=sample/100.0,u=1-t;
                const QPointF p=p0*(u*u*u)+p1*(3*u*u*t)+p2*(3*u*t*t)+p3*(t*t*t);
                const qreal radius=std::sqrt((p.x()*p.x())/(1000*1000)+(p.y()*p.y())/(500*500));
                maximumDeviation=std::max(maximumDeviation,std::abs(radius-1)*500);
            }
            current=p3;
        }
    }
    QVERIFY2(maximumDeviation<=0.1,qPrintable(QString::number(maximumDeviation,'g',12)));
}

void TstSymbolLibrary::symbolSvgRoundTripsBuiltinLibrary()
{
    for (const LibrarySymbol &original:builtinSymbolLibrary().symbols()) {
        const SymbolSvgExportResult exported=SymbolSvgExporter::exportSymbol(original);
        QVERIFY2(exported.errors.isEmpty(),qPrintable(exported.errors.join(QLatin1Char('\n'))));
        QCOMPARE(exported.svgFileName,original.id.value()+QStringLiteral(".svg"));
        const SymbolSvgImportResult imported=SymbolSvgImporter::importSymbol(
            exported.sidecar,exported.svg,QStringLiteral("/symbols"));
        QVERIFY2(imported.symbol.has_value(),qPrintable(imported.errors.join(QLatin1Char('\n'))));
        QCOMPARE(imported.symbol->id,original.id);
        QCOMPARE(imported.symbol->displayName,original.displayName);
        QCOMPARE(imported.symbol->category,original.category);
        QCOMPARE(imported.symbol->referencePrefix,original.referencePrefix);
        QCOMPARE(imported.symbol->showsReference,original.showsReference);
        QCOMPARE(imported.symbol->terminals.size(),original.terminals.size());
        for (int i=0;i<original.terminals.size();++i) {
            QCOMPARE(imported.symbol->terminals[i].name,original.terminals[i].name);
            QCOMPARE(imported.symbol->terminals[i].offset,original.terminals[i].offset);
        }
        QCOMPARE(imported.symbol->primitives.size(),original.primitives.size());
        for (int i=0;i<original.primitives.size();++i) {
            const auto &a=original.primitives[i]; const auto &b=imported.symbol->primitives[i];
            QCOMPARE(b.kind,a.kind);
            QCOMPARE(b.points,a.points);
            QCOMPARE(b.radius,a.radius);
            QCOMPARE(b.filled.value_or(b.kind==PrimitiveKind::Polygon),
                     a.filled.value_or(a.kind==PrimitiveKind::Polygon));
            QCOMPARE(b.fillColor, a.fillColor);
            QCOMPARE(b.fillRule,a.fillRule);
            QCOMPARE(b.segments.size(),a.segments.size());
            for (int j=0;j<a.segments.size();++j) {
                QCOMPARE(b.segments[j].kind,a.segments[j].kind);
                QCOMPARE(b.segments[j].points,a.segments[j].points);
            }
        }
    }

    LibrarySymbol unsafe=makeValid();
    unsafe.id=SymbolId(QStringLiteral("../unsafe"));
    QVERIFY(!SymbolSvgExporter::exportSymbol(unsafe).errors.isEmpty());
}

void TstSymbolLibrary::monochromeFillJsonAndLegacyDefaults()
{
    auto symbol = makeValid();
    SymbolPrimitive polygon;
    polygon.kind = PrimitiveKind::Polygon;
    polygon.points = {{-10,-10}, {10,-10}, {10,10}, {-10,10}};
    symbol.primitives = {polygon};
    const auto legacy = SymbolLibrary::symbolToJson(symbol);
    QStringList errors;
    auto parsed = SymbolLibrary::parseSymbol(legacy, &errors);
    QVERIFY(parsed);
    QVERIFY(primitiveIsFilled(parsed->primitives.first()));
    QCOMPARE(parsed->primitives.first().fillColor, PrimitiveFillColor::White);
    auto explicitWhite = legacy;
    auto primitives = explicitWhite["primitives"].toArray();
    auto entry = primitives[0].toObject(); entry["fillColor"] = "white";
    primitives[0] = entry; explicitWhite["primitives"] = primitives;
    parsed = SymbolLibrary::parseSymbol(explicitWhite, &errors);
    QVERIFY(parsed);
    QCOMPARE(SymbolLibrary::symbolToJson(*parsed), legacy);
    symbol.primitives[0].fillColor = PrimitiveFillColor::Black;
    const auto black = SymbolLibrary::symbolToJson(symbol);
    parsed = SymbolLibrary::parseSymbol(black, &errors);
    QVERIFY(parsed);
    QCOMPARE(parsed->primitives.first().fillColor, PrimitiveFillColor::Black);
    QCOMPARE(SymbolLibrary::symbolToJson(*parsed), black);
    for (const QJsonValue bad : {QJsonValue("red"), QJsonValue(true), QJsonValue(42), QJsonValue()}) {
        auto invalid = black;
        auto items = invalid["primitives"].toArray();
        auto item = items[0].toObject(); item["fillColor"] = bad; items[0] = item;
        invalid["primitives"] = items;
        errors.clear(); QVERIFY(!SymbolLibrary::parseSymbol(invalid, &errors)); QVERIFY(!errors.isEmpty());
    }
    symbol.primitives[0].filled = false;
    QVERIFY(!validateLibrarySymbol(symbol).isEmpty());
    symbol.primitives[0].filled = true;
    symbol.primitives[0].fillColor = static_cast<PrimitiveFillColor>(99);
    QVERIFY(!validateLibrarySymbol(symbol).isEmpty());
    // Preserve previously accepted open-primitive white flags, but not black.
    symbol = makeValid(); symbol.primitives[0].filled = true;
    QVERIFY(validateLibrarySymbol(symbol).isEmpty());
    symbol.primitives[0].fillColor = PrimitiveFillColor::Black;
    QVERIFY(!validateLibrarySymbol(symbol).isEmpty());
}

void TstSymbolLibrary::svgMonochromeFillsAndInheritance()
{
    const auto artwork = SymbolSvgImporter::importArtwork(QByteArrayLiteral(
        "<svg><g fill='white'><rect width='20' height='20'/><g fill='black'>"
        "<circle r='5'/><polygon fill='none' points='0,0 10,0 0,10'/>"
        "<path fill='white' d='M0 0H20V20H0Z'/></g></g><rect width='4' height='4'/></svg>"));
    QVERIFY2(artwork.errors.isEmpty(), qPrintable(artwork.errors.join('\n')));
    QCOMPARE(artwork.primitives.size(), 5);
    QCOMPARE(artwork.primitives[0].fillColor, PrimitiveFillColor::White);
    QCOMPARE(artwork.primitives[1].fillColor, PrimitiveFillColor::Black);
    QVERIFY(!primitiveIsFilled(artwork.primitives[2]));
    QCOMPARE(artwork.primitives[3].fillColor, PrimitiveFillColor::White);
    QCOMPARE(artwork.primitives[4].fillColor, PrimitiveFillColor::Black); // SVG default
    for (const auto &color : QStringList{"BLACK", "#000", "#000000", "rgb(0, 0, 0)",
                                       "White", "#FFF", "#ffffff", "rgb(255, 255, 255)"}) {
        const auto imported = SymbolSvgImporter::importArtwork(
            QString("<svg><rect style='fill:%1' width='20' height='20'/></svg>").arg(color).toUtf8());
        QVERIFY(imported.errors.isEmpty());
        auto symbol = makeValid(); symbol.primitives = imported.primitives;
        const auto exported = SymbolSvgExporter::exportSymbol(symbol);
        QVERIFY(exported.errors.isEmpty());
        const auto again = SymbolSvgImporter::importSymbol(exported.sidecar, exported.svg, "/symbols");
        QVERIFY(again.symbol);
        QCOMPARE(SymbolLibrary::symbolToJson(*again.symbol), SymbolLibrary::symbolToJson(symbol));
    }
    for (const auto &color : QStringList{"red", "currentColor", "url(#paint)", "transparent", "#0008"}) {
        const auto imported = SymbolSvgImporter::importArtwork(
            QString("<svg><rect fill='%1' width='20' height='20'/></svg>").arg(color).toUtf8());
        QVERIFY(!imported.errors.isEmpty());
        QVERIFY(imported.errors.join(' ').contains("unsupported fill"));
    }
}

void TstSymbolLibrary::symbolJsonSerializationAndArtworkOnlyImportRoundTrip()
{
    for (const LibrarySymbol &original : builtinSymbolLibrary().symbols()) {
        QStringList errors;
        const auto parsed = SymbolLibrary::parseSymbol(SymbolLibrary::symbolToJson(original), &errors);
        QVERIFY2(parsed.has_value(), qPrintable(errors.join(QLatin1Char('\n'))));
        QCOMPARE(parsed->id, original.id);
        QCOMPARE(parsed->primitives.size(), original.primitives.size());
        QCOMPARE(parsed->terminals.size(), original.terminals.size());
    }

    const QByteArray svg = R"SVG(<svg xmlns="http://www.w3.org/2000/svg">
      <path d="M10 20 L30 20" fill="none"/>
    </svg>)SVG";
    const SymbolSvgArtworkResult artwork = SymbolSvgImporter::importArtwork(svg, QPointF(10, 20));
    QVERIFY2(artwork.errors.isEmpty(), qPrintable(artwork.errors.join(QLatin1Char('\n'))));
    QCOMPARE(artwork.primitives.size(), 1);
    QCOMPARE(artwork.primitives[0].segments[0].points[0], QPointF(0, 0));
    QCOMPARE(artwork.primitives[0].segments[1].points[0], QPointF(20, 0));

    QString resolved;
    QStringList errors;
    const QByteArray sidecar = R"({"svg":"artwork.svg"})";
    QVERIFY(SymbolSvgImporter::resolveSidecarSvgPath(sidecar, QStringLiteral("/symbols"),
                                                       &resolved, &errors));
    QVERIFY(resolved.endsWith(QStringLiteral("artwork.svg")));
    QVERIFY(!SymbolSvgImporter::resolveSidecarSvgPath(
        QByteArrayLiteral("{\"svg\":\"../secret.svg\"}"), QStringLiteral("/symbols"),
        &resolved, &errors));
    QVERIFY(!errors.isEmpty());
}
