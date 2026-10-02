#include "tst_symboldefinition.h"

#include "symboldefinition.h"

#include <QSet>
#include <QTest>

namespace {
// The full set of built-in symbol kinds, kept here (rather than iterating
// an enum range) so this list is an explicit, reviewable statement of what
// the milestone-1 symbol set contains.
const QVector<SymbolKind> kAllKinds = {
    SymbolKind::Resistor,
    SymbolKind::Capacitor,
    SymbolKind::Diode,
    SymbolKind::Ground,
    SymbolKind::OpAmp,
    SymbolKind::Potentiometer,
    SymbolKind::Jack,
};
} // namespace

void TstSymbolDefinition::allKindsHaveUsableDefinitions()
{
    for (SymbolKind kind : kAllKinds) {
        const SymbolDefinition &def = symbolDefinition(kind);
        QVERIFY(!def.displayName.isEmpty());
        QVERIFY(!def.terminals.isEmpty());
        QVERIFY(!def.primitives.isEmpty());
        QCOMPARE(def.kind, kind);
        QCOMPARE(symbolTerminalCount(kind), def.terminals.size());
    }
}

void TstSymbolDefinition::terminalOffsetsAreUniquePerSymbol()
{
    for (SymbolKind kind : kAllKinds) {
        const SymbolDefinition &def = symbolDefinition(kind);
        QSet<QPair<qreal, qreal>> seen;
        for (const SymbolTerminal &terminal : def.terminals) {
            const QPair<qreal, qreal> key(terminal.offset.x(), terminal.offset.y());
            QVERIFY2(!seen.contains(key),
                      qPrintable(QStringLiteral("duplicate terminal offset in %1").arg(def.displayName)));
            seen.insert(key);
        }
    }
}

void TstSymbolDefinition::nameRoundTripsForEveryKind()
{
    for (SymbolKind kind : kAllKinds) {
        const QString name = symbolKindName(kind);
        QVERIFY(!name.isEmpty());
        const std::optional<SymbolKind> parsed = symbolKindFromName(name);
        QVERIFY(parsed.has_value());
        QCOMPARE(*parsed, kind);
    }
}

void TstSymbolDefinition::everyKindHasReferencePrefix()
{
    for (SymbolKind kind : kAllKinds)
        QVERIFY2(!symbolKindReferencePrefix(kind).trimmed().isEmpty(),
                 qPrintable(symbolKindName(kind)));
    QCOMPARE(symbolKindReferencePrefix(SymbolKind::Resistor), QStringLiteral("R"));
    QCOMPARE(symbolKindReferencePrefix(SymbolKind::Capacitor), QStringLiteral("C"));
    QCOMPARE(symbolKindReferencePrefix(SymbolKind::Diode), QStringLiteral("D"));
}

void TstSymbolDefinition::unknownNameReturnsNullopt()
{
    QVERIFY(!symbolKindFromName(QStringLiteral("NotARealSymbol")).has_value());
    QVERIFY(!symbolKindFromName(QString()).has_value());
}

void TstSymbolDefinition::groundIsTheOnlyKindThatHidesItsReference()
{
    for (SymbolKind kind : kAllKinds)
        QCOMPARE(symbolKindShowsReference(kind), kind != SymbolKind::Ground);
}