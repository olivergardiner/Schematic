#ifndef COMPONENT_H
#define COMPONENT_H

#include "identifiers.h"
#include "symbolkind.h"

#include <QPointF>
#include <QString>

// Rotation is restricted to right-angle quadrants - see DECISIONS.md
// "Symbols and labels". This keeps terminal geometry a
// simple axis swap/negation (rotateOffset() below) rather than a general
// affine transform, and it preserves axis-aligned symbol geometry: any
// horizontal or vertical line in a symbol's local artwork (see
// symboldefinition.h) stays horizontal or vertical after rotation, never
// becoming diagonal. This does NOT mean terminals land on grid
// intersections - e.g. the op-amp's (-28,-8) terminal offset is off-grid at
// a 10-unit spacing regardless of rotation (a stale version of this comment
// claimed otherwise). Terminal snapping while drawing wires works from each
// terminal's actual computed position via terminalPosition(), not from an
// assumption that terminals sit on grid points; grid-aligned placement is a
// separate, editing-time snapping concern, not a property this type
// provides.
enum class Rotation {
    Deg0   = 0,
    Deg90  = 90,
    Deg180 = 180,
    Deg270 = 270,
};

// A single placed instance of a built-in symbol. Component owns no Qt scene
// state (no QGraphicsItem, no signals) - see AGENTS.md "keep the schematic
// document model independent of Qt rendering items and input handling".
//
// Coordinate contract: position() and terminalPosition() are both in
// world/scene units - the same units symbol artwork is authored in (see
// symboldefinition.h) - not pixels. Document::gridSpacing (added in a later
// step) is purely a snap increment and background grid-line spacing; it
// never scales symbol geometry, so changing grid spacing does not change
// component size. The Qt view layer is responsible for any pixel-per-unit
// scaling when painting (via its own zoom transform), independent of this
// model.
class Component
{
public:
    Component() = default;
    Component(ComponentId id, SymbolKind kind, QPointF position);

    ComponentId id() const { return m_id; }
    SymbolKind kind() const { return m_kind; }

    QPointF position() const { return m_position; }
    void setPosition(QPointF position) { m_position = position; }

    Rotation rotation() const { return m_rotation; }
    void setRotation(Rotation rotation) { m_rotation = rotation; }

    QString reference() const { return m_reference; }
    void setReference(const QString &reference) { m_reference = reference; }

    QString value() const { return m_value; }
    void setValue(const QString &value) { m_value = value; }

    bool mirrored() const { return m_mirrored; }
    void setMirrored(bool mirrored) { m_mirrored = mirrored; }

    // Number of terminals this component's symbol kind defines.
    int terminalCount() const;

    // World/scene-space position of the given terminal (see the coordinate
    // contract above - no scaling is applied), computed by rotating the
    // symbol definition's local terminal offset by this component's
    // rotation and then translating by this component's position. Returns
    // m_position unchanged (with a Q_ASSERT in debug builds) if terminal is
    // out of range.
    QPointF terminalPosition(TerminalId terminal) const;

private:
    ComponentId m_id = kInvalidComponentId;
    SymbolKind m_kind = SymbolKind::Resistor;
    QPointF m_position;
    Rotation m_rotation = Rotation::Deg0;
    QString m_reference;
    QString m_value;
    bool m_mirrored = false;
};

// Rotates a local symbol-space offset by the given quadrant rotation.
// Exposed separately from Component so it can be unit tested directly and
// reused by future geometry (e.g. rendering symbol artwork at rotation).
QPointF rotateOffset(QPointF localOffset, Rotation rotation);

#endif // COMPONENT_H
