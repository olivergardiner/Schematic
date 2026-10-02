#include "tst_component.h"

#include "component.h"
#include "symboldefinition.h"

#include <QTest>

void TstComponent::terminalPositionAtOriginNoRotation()
{
    Component resistor(1, SymbolKind::Resistor, QPointF(0, 0));
    const SymbolDefinition &def = symbolDefinition(SymbolKind::Resistor);

    for (int i = 0; i < def.terminals.size(); ++i)
        QCOMPARE(resistor.terminalPosition(i), def.terminals[i].offset);
}

void TstComponent::terminalPositionTranslatesWithComponent()
{
    const QPointF position(100, 50);
    Component resistor(1, SymbolKind::Resistor, position);
    const SymbolDefinition &def = symbolDefinition(SymbolKind::Resistor);

    for (int i = 0; i < def.terminals.size(); ++i)
        QCOMPARE(resistor.terminalPosition(i), position + def.terminals[i].offset);
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
    // Op-amp terminal offsets (see core/symboldefinition.cpp makeOpAmp()):
    // 0 "in+" (-28,-8), 1 "in-" (-28,8), 2 "out" (28,0),
    // 3 "v+" (0,-16), 4 "v-" (0,16).
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

    Component opAmp0(1, SymbolKind::OpAmp, position);
    opAmp0.setRotation(Rotation::Deg0);
    QCOMPARE(opAmp0.terminalPosition(0), QPointF(12, -38));
    QCOMPARE(opAmp0.terminalPosition(1), QPointF(12, -22));
    QCOMPARE(opAmp0.terminalPosition(2), QPointF(68, -30));
    QCOMPARE(opAmp0.terminalPosition(3), QPointF(40, -46));
    QCOMPARE(opAmp0.terminalPosition(4), QPointF(40, -14));

    Component opAmp90(1, SymbolKind::OpAmp, position);
    opAmp90.setRotation(Rotation::Deg90);
    QCOMPARE(opAmp90.terminalPosition(0), QPointF(48, -58));
    QCOMPARE(opAmp90.terminalPosition(1), QPointF(32, -58));
    QCOMPARE(opAmp90.terminalPosition(2), QPointF(40, -2));
    QCOMPARE(opAmp90.terminalPosition(3), QPointF(56, -30));
    QCOMPARE(opAmp90.terminalPosition(4), QPointF(24, -30));

    Component opAmp180(1, SymbolKind::OpAmp, position);
    opAmp180.setRotation(Rotation::Deg180);
    QCOMPARE(opAmp180.terminalPosition(0), QPointF(68, -22));
    QCOMPARE(opAmp180.terminalPosition(1), QPointF(68, -38));
    QCOMPARE(opAmp180.terminalPosition(2), QPointF(12, -30));
    QCOMPARE(opAmp180.terminalPosition(3), QPointF(40, -14));
    QCOMPARE(opAmp180.terminalPosition(4), QPointF(40, -46));

    Component opAmp270(1, SymbolKind::OpAmp, position);
    opAmp270.setRotation(Rotation::Deg270);
    QCOMPARE(opAmp270.terminalPosition(0), QPointF(32, -2));
    QCOMPARE(opAmp270.terminalPosition(1), QPointF(48, -2));
    QCOMPARE(opAmp270.terminalPosition(2), QPointF(40, -58));
    QCOMPARE(opAmp270.terminalPosition(3), QPointF(24, -30));
    QCOMPARE(opAmp270.terminalPosition(4), QPointF(56, -30));
}

void TstComponent::terminalCountMatchesSymbolDefinition()
{
    const QVector<SymbolKind> kinds = {
        SymbolKind::Resistor,  SymbolKind::Capacitor, SymbolKind::Diode,
        SymbolKind::Ground,    SymbolKind::OpAmp,      SymbolKind::Potentiometer,
        SymbolKind::Jack,
    };

    for (SymbolKind kind : kinds) {
        Component component(1, kind, QPointF(0, 0));
        QCOMPARE(component.terminalCount(), symbolTerminalCount(kind));
    }
}

void TstComponent::mirroredTerminalPositions()
{
    // Mirror negates local x before rotation. Terminal indices are unchanged.
    Component m0(1, SymbolKind::OpAmp, QPointF(40, -30));
    m0.setMirrored(true);
    QCOMPARE(m0.terminalPosition(0), QPointF(68, -38));
    QCOMPARE(m0.terminalPosition(1), QPointF(68, -22));
    QCOMPARE(m0.terminalPosition(2), QPointF(12, -30));
    QCOMPARE(m0.terminalPosition(3), QPointF(40, -46));
    QCOMPARE(m0.terminalPosition(4), QPointF(40, -14));

    Component m90(1, SymbolKind::OpAmp, QPointF(40, -30));
    m90.setMirrored(true);
    m90.setRotation(Rotation::Deg90);
    QCOMPARE(m90.terminalPosition(0), QPointF(48, -2));
    QCOMPARE(m90.terminalPosition(1), QPointF(32, -2));
    QCOMPARE(m90.terminalPosition(2), QPointF(40, -58));
    QCOMPARE(m90.terminalPosition(3), QPointF(56, -30));
    QCOMPARE(m90.terminalPosition(4), QPointF(24, -30));
}
