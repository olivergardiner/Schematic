#include "tst_symbollibrary.h"

#include "symbollibrary.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QTest>

#include <functional>
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
        {SymbolIds::OpAmp,         "Op-Amp",            "ICs",        "U",   true},
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
