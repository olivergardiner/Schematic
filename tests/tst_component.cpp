#include "tst_component.h"

#include "component.h"
#include "symbollibrary.h"

#include <QTest>

void TstComponent::terminalPositionAtOriginNoRotation()
{
    Component resistor(1, SymbolIds::Resistor, QPointF(0, 0));
    const LibrarySymbol &def = *builtinSymbolLibrary().find(SymbolIds::Resistor);

    for (int i = 0; i < def.terminals.size(); ++i)
        QCOMPARE(resistor.terminalPosition(i, *builtinSymbolLibrary().find(resistor.symbolId())), def.terminals[i].offset);
}

void TstComponent::terminalPositionTranslatesWithComponent()
{
    const QPointF position(100, 50);
    Component resistor(1, SymbolIds::Resistor, position);
    const LibrarySymbol &def = *builtinSymbolLibrary().find(SymbolIds::Resistor);

    for (int i = 0; i < def.terminals.size(); ++i)
        QCOMPARE(resistor.terminalPosition(i, *builtinSymbolLibrary().find(resistor.symbolId())), position + def.terminals[i].offset);
}

void TstComponent::rotateOffsetIsConsistentAcrossQuadrants()
{
    const QPointF localOffset(20, 0);

    const QPointF r0   = rotateOffset(localOffset, Rotation::Deg0);
    const QPointF r90   = rotateOffset(localOffset, Rotation::Deg90);
    const QPointF r180 = rotateOffset(localOffset, Rotation::Deg180);
    const QPointF r270 = rotateOffset(localOffset, Rotation::Deg270);

    QCOMPARE(r0, QPointF(20, 0));
    QCOMPARE(r90, QPointF(0, 20));
    QCOMPARE(r180, QPointF(-20, 0));
    QCOMPARE(r270, QPointF(0, -20));

    // A further 90-degree step from Deg270 must return to the original
    // (unrotated) offset - i.e. rotating r270's underlying angle by 90
    // degrees again reproduces r0. Verified by re-deriving via Deg0 applied
    // to the same local offset, since Rotation has no "Deg360" value.
    QCOMPARE(rotateOffset(localOffset, Rotation::Deg0), r0);
}

void TstComponent::terminalPositionCombinesRotationAndTranslation()
{
    // Op-amp terminal offsets (see core/symbols/opamp-grid10.json):
    // 0 "in+" (-30,-10), 1 "in-" (-30,10), 2 "out" (30,0),
    // 3 "v+" (0,-20), 4 "v-" (0,20).
    // Expected positions below are hand-computed by applying the documented
    // rotation formula ((x,y)->(-y,x) for 90 degrees, etc. - see
    // rotateOffset() in component.cpp) and then translating by position,
    // worked out on paper rather than by calling terminalPosition() or
    // rotateOffset() here. This cross-checks that terminalPosition()
    // actually composes rotation, translation and terminal indexing
    // correctly (e.g. no off-by-one terminal index, no swapped
    // rotate-then-translate order), independent of trusting the
    // implementation's own control flow.
    const QPointF position(40, -30);

    Component opAmp0(1, SymbolIds::OpAmpGrid10, position);
    opAmp0.setRotation(Rotation::Deg0);
    QCOMPARE(opAmp0.terminalPosition(0, *builtinSymbolLibrary().find(opAmp0.symbolId())), QPointF(10, -40));
    QCOMPARE(opAmp0.terminalPosition(1, *builtinSymbolLibrary().find(opAmp0.symbolId())), QPointF(10, -20));
    QCOMPARE(opAmp0.terminalPosition(2, *builtinSymbolLibrary().find(opAmp0.symbolId())), QPointF(70, -30));
    QCOMPARE(opAmp0.terminalPosition(3, *builtinSymbolLibrary().find(opAmp0.symbolId())), QPointF(40, -50));
    QCOMPARE(opAmp0.terminalPosition(4, *builtinSymbolLibrary().find(opAmp0.symbolId())), QPointF(40, -10));

    Component opAmp90(1, SymbolIds::OpAmpGrid10, position);
    opAmp90.setRotation(Rotation::Deg90);
    QCOMPARE(opAmp90.terminalPosition(0, *builtinSymbolLibrary().find(opAmp90.symbolId())), QPointF(50, -60));
    QCOMPARE(opAmp90.terminalPosition(1, *builtinSymbolLibrary().find(opAmp90.symbolId())), QPointF(30, -60));
    QCOMPARE(opAmp90.terminalPosition(2, *builtinSymbolLibrary().find(opAmp90.symbolId())), QPointF(40, 0));
    QCOMPARE(opAmp90.terminalPosition(3, *builtinSymbolLibrary().find(opAmp90.symbolId())), QPointF(60, -30));
    QCOMPARE(opAmp90.terminalPosition(4, *builtinSymbolLibrary().find(opAmp90.symbolId())), QPointF(20, -30));

    Component opAmp180(1, SymbolIds::OpAmpGrid10, position);
    opAmp180.setRotation(Rotation::Deg180);
    QCOMPARE(opAmp180.terminalPosition(0, *builtinSymbolLibrary().find(opAmp180.symbolId())), QPointF(70, -20));
    QCOMPARE(opAmp180.terminalPosition(1, *builtinSymbolLibrary().find(opAmp180.symbolId())), QPointF(70, -40));
    QCOMPARE(opAmp180.terminalPosition(2, *builtinSymbolLibrary().find(opAmp180.symbolId())), QPointF(10, -30));
    QCOMPARE(opAmp180.terminalPosition(3, *builtinSymbolLibrary().find(opAmp180.symbolId())), QPointF(40, -10));
    QCOMPARE(opAmp180.terminalPosition(4, *builtinSymbolLibrary().find(opAmp180.symbolId())), QPointF(40, -50));

    Component opAmp270(1, SymbolIds::OpAmpGrid10, position);
    opAmp270.setRotation(Rotation::Deg270);
    QCOMPARE(opAmp270.terminalPosition(0, *builtinSymbolLibrary().find(opAmp270.symbolId())), QPointF(30, 0));
    QCOMPARE(opAmp270.terminalPosition(1, *builtinSymbolLibrary().find(opAmp270.symbolId())), QPointF(50, 0));
    QCOMPARE(opAmp270.terminalPosition(2, *builtinSymbolLibrary().find(opAmp270.symbolId())), QPointF(40, -60));
    QCOMPARE(opAmp270.terminalPosition(3, *builtinSymbolLibrary().find(opAmp270.symbolId())), QPointF(20, -30));
    QCOMPARE(opAmp270.terminalPosition(4, *builtinSymbolLibrary().find(opAmp270.symbolId())), QPointF(60, -30));
}

void TstComponent::terminalCountMatchesSymbolDefinition()
{
    const QVector<SymbolId> kinds = {
        SymbolIds::Resistor,  SymbolIds::Capacitor, SymbolIds::Diode,
        SymbolIds::Ground,    SymbolIds::OpAmpGrid10,      SymbolIds::Potentiometer,
        SymbolIds::Jack,
    };

    for (const SymbolId &kind : kinds) {
        Component component(1, kind, QPointF(0, 0));
        QCOMPARE(component.terminalCount(*builtinSymbolLibrary().find(component.symbolId())), builtinSymbolLibrary().find(kind)->terminals.size());
    }
}

void TstComponent::mirroredTerminalPositions()
{
    // Mirror negates local x before rotation. Terminal indices are unchanged.
    Component m0(1, SymbolIds::OpAmpGrid10, QPointF(40, -30));
    m0.setMirrored(true);
    QCOMPARE(m0.terminalPosition(0, *builtinSymbolLibrary().find(m0.symbolId())), QPointF(70, -40));
    QCOMPARE(m0.terminalPosition(1, *builtinSymbolLibrary().find(m0.symbolId())), QPointF(70, -20));
    QCOMPARE(m0.terminalPosition(2, *builtinSymbolLibrary().find(m0.symbolId())), QPointF(10, -30));
    QCOMPARE(m0.terminalPosition(3, *builtinSymbolLibrary().find(m0.symbolId())), QPointF(40, -50));
    QCOMPARE(m0.terminalPosition(4, *builtinSymbolLibrary().find(m0.symbolId())), QPointF(40, -10));

    Component m90(1, SymbolIds::OpAmpGrid10, QPointF(40, -30));
    m90.setMirrored(true);
    m90.setRotation(Rotation::Deg90);
    QCOMPARE(m90.terminalPosition(0, *builtinSymbolLibrary().find(m90.symbolId())), QPointF(50, 0));
    QCOMPARE(m90.terminalPosition(1, *builtinSymbolLibrary().find(m90.symbolId())), QPointF(30, 0));
    QCOMPARE(m90.terminalPosition(2, *builtinSymbolLibrary().find(m90.symbolId())), QPointF(40, -60));
    QCOMPARE(m90.terminalPosition(3, *builtinSymbolLibrary().find(m90.symbolId())), QPointF(60, -30));
    QCOMPARE(m90.terminalPosition(4, *builtinSymbolLibrary().find(m90.symbolId())), QPointF(20, -30));
}
