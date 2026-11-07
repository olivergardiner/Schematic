#include "../symbolpackagestore.h"

#include <QFile>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest>

namespace {

LibrarySymbol packageSymbol()
{
    LibrarySymbol symbol;
    symbol.id = SymbolId(QStringLiteral("PackageStoreTest"));
    symbol.displayName = QStringLiteral("Package Store Test");
    symbol.category = QStringLiteral("Tests/Storage");
    symbol.referencePrefix = QStringLiteral("X");
    symbol.showsReference = true;
    symbol.primitives = {{PrimitiveKind::Line, {{-10, 0}, {10, 0}}, 0.0}};
    symbol.terminals = {{QStringLiteral("1"), {-10, 0}}, {QStringLiteral("2"), {10, 0}}};
    return symbol;
}

QByteArray readFile(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

bool writeFile(const QString &path, const QByteArray &data)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(data) == data.size();
}

} // namespace

class SymbolPackageStoreTests : public QObject
{
    Q_OBJECT

private slots:
    void savesReloadsAndProtectsTerminalSignature();
    void embeddedBatchOverridesAndRollsBack();
    void rejectsUnsafeManifestAndRollsBackWriteFailure();
};

void SymbolPackageStoreTests::savesReloadsAndProtectsTerminalSignature()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    SymbolPackageStore store(directory.path());
    const QByteArray svg = QByteArrayLiteral("<svg xmlns=\"http://www.w3.org/2000/svg\"><line x2=\"1\"/></svg>");
    const QByteArray sidecar = QByteArrayLiteral("{\"id\":\"PackageStoreTest\"}");
    LibrarySymbol symbol = packageSymbol();
    QStringList errors;
    QVERIFY2(store.save(symbol, svg, sidecar, &errors), qPrintable(errors.join(QLatin1Char('\n'))));
    QVERIFY(store.contains(symbol.id));
    QCOMPARE(applicationSymbolLibrary().find(symbol.id)->displayName, symbol.displayName);
    QCOMPARE(readFile(directory.filePath(QStringLiteral("sources/PackageStoreTest.svg"))), svg);
    QCOMPARE(readFile(directory.filePath(QStringLiteral("sources/PackageStoreTest.json"))), sidecar);

    const QString convertedPath = directory.filePath(QStringLiteral("PackageStoreTest.json"));
    const QByteArray originalConverted = readFile(convertedPath);
    LibrarySymbol incompatible = symbol;
    incompatible.terminals[1].offset.setX(12);
    errors.clear();
    QVERIFY(!store.save(incompatible, svg, sidecar, &errors));
    QVERIFY(!errors.isEmpty());
    QCOMPARE(readFile(convertedPath), originalConverted);

    LibrarySymbol reimported = symbol;
    reimported.displayName = QStringLiteral("Refined Artwork Metadata");
    SymbolPrimitive plate;
    plate.kind = PrimitiveKind::Polygon; plate.points = {{-3,-10}, {3,-10}, {3,10}, {-3,10}};
    plate.fillColor = PrimitiveFillColor::Black;
    reimported.primitives.append(plate);
    errors.clear();
    QVERIFY2(store.save(reimported, svg, sidecar, &errors), qPrintable(errors.join(QLatin1Char('\n'))));
    QCOMPARE(applicationSymbolLibrary().find(symbol.id)->displayName, reimported.displayName);

    SymbolLibrary loaded;
    errors = store.loadInto(&loaded);
    QVERIFY2(errors.isEmpty(), qPrintable(errors.join(QLatin1Char('\n'))));
    QCOMPARE(loaded.size(), 1);
    QCOMPARE(loaded.find(symbol.id)->displayName, reimported.displayName);
    QCOMPARE(loaded.find(symbol.id)->primitives.last().fillColor, PrimitiveFillColor::Black);
}

void SymbolPackageStoreTests::rejectsUnsafeManifestAndRollsBackWriteFailure()
{
    QTemporaryDir badDirectory;
    QVERIFY(badDirectory.isValid());
    QVERIFY(writeFile(badDirectory.filePath(QStringLiteral("manifest.json")),
                      QJsonDocument(QJsonObject{{QStringLiteral("symbols"),
                          QJsonArray{QStringLiteral("../outside.json")}}}).toJson()));
    SymbolPackageStore badStore(badDirectory.path());
    SymbolLibrary loaded;
    QStringList errors = badStore.loadInto(&loaded);
    QVERIFY(!errors.isEmpty());
    QCOMPARE(loaded.size(), 0);
    errors.clear();
    QVERIFY(!badStore.save(packageSymbol(), QByteArrayLiteral("<svg/>"),
                           QByteArrayLiteral("{}"), &errors));
    QVERIFY(!errors.isEmpty());

    QTemporaryDir blockedDirectory;
    QVERIFY(blockedDirectory.isValid());
    const QString sourcesPath = blockedDirectory.filePath(QStringLiteral("sources"));
    QVERIFY(writeFile(sourcesPath, QByteArrayLiteral("keep me")));
    SymbolPackageStore blockedStore(blockedDirectory.path());
    errors.clear();
    QVERIFY(!blockedStore.save(packageSymbol(), QByteArrayLiteral("<svg/>"),
                               QByteArrayLiteral("{}"), &errors));
    QVERIFY(!errors.isEmpty());
    QCOMPARE(readFile(sourcesPath), QByteArrayLiteral("keep me"));
    QVERIFY(!QFile::exists(blockedDirectory.filePath(QStringLiteral("manifest.json"))));
}

void SymbolPackageStoreTests::embeddedBatchOverridesAndRollsBack()
{
    struct Restore { SymbolLibrary saved; ~Restore() { applicationSymbolLibrary() = saved; } } restore{applicationSymbolLibrary()};
    QTemporaryDir directory;
    SymbolPackageStore store(directory.path());
    auto packaged = *builtinSymbolLibrary().find(SymbolIds::Resistor);
    packaged.displayName = QStringLiteral("Document resistor");
    auto added = packageSymbol();
    added.id = SymbolId(QStringLiteral("EmbeddedBatchNew"));
    QStringList errors;
    QVERIFY2(store.saveEmbeddedBatch({packaged, added}, &errors), qPrintable(errors.join('\n')));
    SymbolLibrary restarted = builtinSymbolLibrary();
    errors = store.loadInto(&restarted);
    QVERIFY2(errors.isEmpty(), qPrintable(errors.join('\n')));
    QCOMPARE(restarted.find(packaged.id)->displayName, packaged.displayName);
    QVERIFY(restarted.find(added.id));
    const auto oldManifest = readFile(directory.filePath(QStringLiteral("manifest.json")));
    auto invalid = added;
    invalid.terminals[0].offset.setX(-99);
    errors.clear();
    QVERIFY(!store.saveEmbeddedBatch({packaged, invalid}, &errors));
    QCOMPARE(readFile(directory.filePath(QStringLiteral("manifest.json"))), oldManifest);
    const auto savedDefinition = readFile(directory.filePath(QStringLiteral("Resistor.json")));
    auto cancelled = packaged;
    cancelled.displayName = QStringLiteral("Cancelled replacement");
    errors.clear();
    QVERIFY(!store.saveEmbeddedBatch({cancelled}, &errors, [] { return false; }));
    QVERIFY(errors.isEmpty());
    QCOMPARE(readFile(directory.filePath(QStringLiteral("Resistor.json"))), savedDefinition);
    QCOMPARE(applicationSymbolLibrary().find(packaged.id)->displayName, packaged.displayName);
    // Force a later backup to fail after existing package files were moved.
    auto collision = added;
    collision.id = SymbolId(QStringLiteral("Collision"));
    const QString obstruction = directory.filePath(QStringLiteral("sources/Collision.svg"));
    QVERIFY(QDir().mkpath(obstruction));
    QVERIFY(writeFile(QDir(obstruction).filePath(QStringLiteral("keep")), QByteArrayLiteral("keep")));
    const auto oldDefinition = readFile(directory.filePath(QStringLiteral("Resistor.json")));
    auto refined = packaged;
    refined.displayName = QStringLiteral("Must not commit");
    errors.clear();
    QVERIFY(!store.saveEmbeddedBatch({refined, collision}, &errors));
    QCOMPARE(readFile(directory.filePath(QStringLiteral("Resistor.json"))), oldDefinition);
    QCOMPARE(readFile(directory.filePath(QStringLiteral("manifest.json"))), oldManifest);
    QCOMPARE(applicationSymbolLibrary().find(packaged.id)->displayName, packaged.displayName);
    QTemporaryDir blocked;
    QVERIFY(writeFile(blocked.filePath(QStringLiteral("sources")), QByteArrayLiteral("blocked")));
    SymbolPackageStore blockedStore(blocked.path());
    auto other = added;
    other.id = SymbolId(QStringLiteral("MustRollback"));
    const auto before = applicationSymbolLibrary().size();
    errors.clear();
    QVERIFY(!blockedStore.saveEmbeddedBatch({packaged, other}, &errors));
    QVERIFY(!errors.isEmpty());
    QVERIFY(!QFile::exists(blocked.filePath(QStringLiteral("Resistor.json"))));
    QVERIFY(!QFile::exists(blocked.filePath(QStringLiteral("MustRollback.json"))));
    QVERIFY(!QFile::exists(blocked.filePath(QStringLiteral("manifest.json"))));
    QCOMPARE(applicationSymbolLibrary().size(), before);
    QVERIFY(!applicationSymbolLibrary().find(other.id));
    QCOMPARE(readFile(blocked.filePath(QStringLiteral("sources"))), QByteArrayLiteral("blocked"));
}

QTEST_GUILESS_MAIN(SymbolPackageStoreTests)
#include "tst_symbolpackagestore.moc"
