#ifndef COMPONENT_H
#define COMPONENT_H

#include "identifiers.h"
#include "symbolid.h"

#include <QPointF>
#include <QString>
#include <QStringList>
#include <QVector>
#include <optional>

// Rotation is restricted to right-angle quadrants - see DECISIONS.md
// "Symbols and labels". This keeps terminal geometry a
// simple axis swap/negation (rotateOffset() below) rather than a general
// affine transform, and it preserves axis-aligned symbol geometry: any
// horizontal or vertical line in a symbol's local artwork (see
// symbollibrary.h) stays horizontal or vertical after rotation, never
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

// A single placed instance of a library symbol. Component owns no Qt scene
// state (no QGraphicsItem, no signals) - see AGENTS.md "keep the schematic
// document model independent of Qt rendering items and input handling".
//
// Coordinate contract: position() and terminalPosition() are both in
// world/scene units - the same units symbol artwork is authored in (see
// symbollibrary.h) - not pixels. Document::gridSpacing (added in a later
// step) is purely a snap increment and background grid-line spacing; it
// never scales symbol geometry, so changing grid spacing does not change
// component size. The Qt view layer is responsible for any pixel-per-unit
// scaling when painting (via its own zoom transform), independent of this
// model.
struct LibrarySymbol;

struct InstanceProperty
{
    QString id;
    QString name; // Custom display name; reserved properties use fixed UI names.
    QString value;
    bool visible = true;
};

QString propertyIdFromName(const QString &name);
QStringList validateInstanceProperties(const QVector<InstanceProperty> &properties);

class Component
{
public:
    Component() = default;
    Component(ComponentId id, const SymbolId &symbolId, QPointF position);

    ComponentId id() const { return m_id; }
    const SymbolId &symbolId() const { return m_symbolId; }

    QPointF position() const { return m_position; }
    void setPosition(QPointF position) { m_position = position; }

    Rotation rotation() const { return m_rotation; }
    void setRotation(Rotation rotation) { m_rotation = rotation; }

    QString reference() const { return m_properties[0].value; }
    void setReference(const QString &reference) { m_properties[0].value = reference; }

    QString value() const { return m_properties[1].value; }
    void setValue(const QString &value) { m_properties[1].value = value; }
    const QVector<InstanceProperty> &properties() const { return m_properties; }
    const InstanceProperty *property(const QString &id) const;
    void setProperties(const QVector<InstanceProperty> &properties) { m_properties = properties; }

    std::optional<QPointF> propertyBlockOffset() const { return m_propertyBlockOffset; }
    void setPropertyBlockOffset(std::optional<QPointF> offset) { m_propertyBlockOffset = offset; }

    bool mirrored() const { return m_mirrored; }
    void setMirrored(bool mirrored) { m_mirrored = mirrored; }

    // Geometry always uses the caller-provided document symbol definition.
    int terminalCount(const LibrarySymbol &symbol) const;

    // World/scene-space position of the given terminal (see the coordinate
    // contract above - no scaling is applied), computed by rotating the
    // symbol definition's local terminal offset by this component's
    // rotation and then translating by this component's position. Returns
    // m_position unchanged (with a Q_ASSERT in debug builds) if terminal is
    // out of range.
    QPointF terminalPosition(TerminalId terminal, const LibrarySymbol &symbol) const;

private:
    ComponentId m_id = kInvalidComponentId;
    SymbolId m_symbolId;
    QPointF m_position;
    Rotation m_rotation = Rotation::Deg0;
    QVector<InstanceProperty> m_properties{{"reference", {}, {}, true}, {"value", {}, {}, true}};
    bool m_mirrored = false;
    std::optional<QPointF> m_propertyBlockOffset;
};

// Rotates a local symbol-space offset by the given quadrant rotation.
// Exposed separately from Component so it can be unit tested directly and
// reused by future geometry (e.g. rendering symbol artwork at rotation).
QPointF rotateOffset(QPointF localOffset, Rotation rotation);

#endif // COMPONENT_H
