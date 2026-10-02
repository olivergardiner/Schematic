#ifndef TST_COMPONENT_H
#define TST_COMPONENT_H

#include <QObject>

class TstComponent : public QObject
{
    Q_OBJECT

private slots:
    // At rotation 0 with position at the origin, terminalPosition() must
    // equal the symbol's raw local terminal offset - the base case every
    // other rotation/translation case builds on.
    void terminalPositionAtOriginNoRotation();

    // Translating a component must translate every terminal position by the
    // same delta, independent of rotation.
    void terminalPositionTranslatesWithComponent();

    // Each 90-degree rotation step must rotate terminal offsets consistently
    // (a full 360-degree round trip returns to the original offset), and
    // must not depend on component position.
    void rotateOffsetIsConsistentAcrossQuadrants();

    // terminalPosition() combines rotation and translation together, not
    // just rotateOffset() in isolation - a positioned, rotated component's
    // terminal must equal position() + rotateOffset(localOffset, rotation)
    // at every quadrant. This is the exact computation the wire tool and
    // renderer depend on to find a terminal's on-screen location, so it is
    // tested directly rather than assumed from the two simpler cases above.
    void terminalPositionCombinesRotationAndTranslation();

    // terminalCount() must match the number of terminals defined for the
    // component's symbol kind, for every built-in kind - this is what the
    // wire tool and file-format validator (added in later steps) rely on to
    // detect an out-of-range terminal index.
    void terminalCountMatchesSymbolDefinition();

    // Mirroring negates the local x offset before rotation; terminal indices
    // are unchanged. Expected values are hand-computed for the op-amp.
    void mirroredTerminalPositions();
};

#endif // TST_COMPONENT_H
