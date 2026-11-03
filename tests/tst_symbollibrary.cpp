#include "tst_symbollibrary.h"

#include "symbollibrary.h"
#include "symbolsvg.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QTest>

#include <functional>
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

void TstSymbolLibrary::builtinLibraryHasSevenSymbolsWithExpectedMetadata()
{
    struct Expected { SymbolId id; const char *name; const char *category; const char *prefix; bool shows; };
    const QVector<Expected> expected = {
        {SymbolIds::Resistor,      "Resistor",          "Passives",   "R",   true},
        {SymbolIds::Capacitor,     "Capacitor",         "Passives",   "C",   true},
        {SymbolIds::Diode,         "Diode",             "Semiconductors/Diodes", "D", true},
        {SymbolIds::Ground,        "Ground",            "Signals",    "GND", false},
        {SymbolIds::OpAmp,         "Op-Amp",            "Semiconductors/ICs", "U", true},
        {SymbolIds::Potentiometer, "Potentiometer",     "Passives",   "VR",  true},
        {SymbolIds::Jack,          "Input/Output Jack", "Connectors", "J",   true},
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

void TstSymbolLibrary::builtinTerminalsAreFrozen()
{
    checkTerminals(SymbolIds::Resistor, {{"1", -20, 0}, {"2", 20, 0}});
    if (QTest::currentTestFailed()) return;
    checkTerminals(SymbolIds::Capacitor, {{"1", -20, 0}, {"2", 20, 0}});
    if (QTest::currentTestFailed()) return;
    checkTerminals(SymbolIds::Diode, {{"anode", -20, 0}, {"cathode", 20, 0}});
    if (QTest::currentTestFailed()) return;
    checkTerminals(SymbolIds::Ground, {{"1", 0, -20}});
    if (QTest::currentTestFailed()) return;
    checkTerminals(SymbolIds::OpAmp, {{"in+", -28, -8}, {"in-", -28, 8}, {"out", 28, 0},
                                      {"v+", 0, -16}, {"v-", 0, 16}});
    if (QTest::currentTestFailed()) return;
    checkTerminals(SymbolIds::Potentiometer, {{"1", -20, 0}, {"2", 20, 0}, {"wiper", 0, -20}});
    if (QTest::currentTestFailed()) return;
    checkTerminals(SymbolIds::Jack, {{"tip", -20, 0}, {"sleeve", 20, 0}});
}

void TstSymbolLibrary::builtinPrimitiveShapes()
{
    using K = PrimitiveKind;
    checkKinds(SymbolIds::Resistor, {K::Line, K::Line, K::Polygon});
    if (QTest::currentTestFailed()) return;
    checkKinds(SymbolIds::Capacitor, {K::Line, K::Line, K::Line, K::Line});
    if (QTest::currentTestFailed()) return;
    checkKinds(SymbolIds::Diode, {K::Line, K::Line, K::Polygon, K::Line});
    if (QTest::currentTestFailed()) return;
    checkKinds(SymbolIds::Ground, {K::Line, K::Line, K::Line, K::Line});
    if (QTest::currentTestFailed()) return;
    checkKinds(SymbolIds::OpAmp, {K::Polygon, K::Line, K::Line, K::Line, K::Line, K::Line});
    if (QTest::currentTestFailed()) return;
    checkKinds(SymbolIds::Potentiometer, {K::Line, K::Line, K::Polygon, K::Line, K::Polyline});
    if (QTest::currentTestFailed()) return;
    checkKinds(SymbolIds::Jack, {K::Circle, K::Line, K::Line});
    if (QTest::currentTestFailed()) return;

    const LibrarySymbol *jack = builtinSymbolLibrary().find(SymbolIds::Jack);
    QCOMPARE(jack->primitives[0].points, QVector<QPointF>({QPointF(0, 0)}));
    QCOMPARE(jack->primitives[0].radius, 10.0);
    const LibrarySymbol *diode = builtinSymbolLibrary().find(SymbolIds::Diode);
    QCOMPARE(diode->primitives[2].points,
             QVector<QPointF>({QPointF(-8, -8), QPointF(-8, 8), QPointF(8, 0)}));
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
