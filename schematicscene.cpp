#include "schematicscene.h"

#include "component.h"
#include "document.h"
#include "symboldefinition.h"
#include "wireroute.h"

#include <QGraphicsEllipseItem>
#include <QGraphicsItemGroup>
#include <QGraphicsPathItem>
#include <QGraphicsSimpleTextItem>
#include <QPainterPath>
#include <QPen>

namespace {
// A generous default canvas in scene units (treated as mm at 1 unit == 1 mm)
// so a typical A4/A3-ish schematic sheet fits comfortably without the user
// needing to resize anything up front.
constexpr qreal kDefaultWidth  = 2000.0;
constexpr qreal kDefaultHeight = 1500.0;

// Explicit stacking order (see AGENTS.md/CLAUDE.md step 4 plan): wires sit
// behind everything, junction dots sit above wires (so a dot is never hidden
// under a wire it marks), symbol artwork sits above both, and reference/
// value labels sit above everything so text is never occluded by artwork.
constexpr qreal kWireZ     = 0.0;
constexpr qreal kJunctionZ = 1.0;
constexpr qreal kSymbolZ   = 2.0;
constexpr qreal kLabelZ    = 3.0;

// Real (non-cosmetic) scene-unit pen widths and a scene-unit font size, so
// line weight and text both scale with the schematic exactly like symbol
// artwork does when the view is zoomed - see the step 4 plan's decision that
// this is a presentation/export tool, not a screen-fixed-size UI.
constexpr qreal kLineWidth      = 1.0;
constexpr qreal kJunctionRadius = 2.5;
constexpr qreal kLabelPixelSize = 8.0;
constexpr qreal kLabelGap       = 4.0; // Gap between a component's local
                                       // bounding box and its labels.

QPainterPath primitivePath(const SymbolPrimitive &primitive)
{
    QPainterPath path;
    switch (primitive.kind) {
    case PrimitiveKind::Line:
        if (primitive.points.size() == 2) {
            path.moveTo(primitive.points[0]);
            path.lineTo(primitive.points[1]);
        }
        break;
    case PrimitiveKind::Polyline:
        if (!primitive.points.isEmpty()) {
            path.moveTo(primitive.points.first());
            for (int i = 1; i < primitive.points.size(); ++i)
                path.lineTo(primitive.points[i]);
        }
        break;
    case PrimitiveKind::Polygon:
        if (!primitive.points.isEmpty()) {
            path.moveTo(primitive.points.first());
            for (int i = 1; i < primitive.points.size(); ++i)
                path.lineTo(primitive.points[i]);
            path.closeSubpath();
        }
        break;
    case PrimitiveKind::Circle:
        if (!primitive.points.isEmpty()) {
            const QPointF centre = primitive.points.first();
            path.addEllipse(centre, primitive.radius, primitive.radius);
        }
        break;
    }
    return path;
}

} // namespace

SchematicScene::SchematicScene(QObject *parent)
    : QGraphicsScene(parent)
{
    setSceneRect(-kDefaultWidth / 2.0, -kDefaultHeight / 2.0, kDefaultWidth, kDefaultHeight);
    setBackgroundBrush(Qt::white);
}

void SchematicScene::setDocument(const Document &document)
{
    clear();

    for (const WireRoute &route : document.wires())
        addWireItems(route);
    for (const QPointF &point : document.junctionPoints())
        addJunctionItem(point);
    for (const Component &component : document.components())
        addComponentItems(component);
}

void SchematicScene::addWireItems(const WireRoute &route)
{
    QPainterPath path;
    path.moveTo(route.vertices.first());
    for (int i = 1; i < route.vertices.size(); ++i)
        path.lineTo(route.vertices[i]);

    auto *item = addPath(path, QPen(Qt::black, kLineWidth));
    item->setZValue(kWireZ);
}

void SchematicScene::addJunctionItem(QPointF point)
{
    auto *item = addEllipse(point.x() - kJunctionRadius, point.y() - kJunctionRadius,
                             kJunctionRadius * 2.0, kJunctionRadius * 2.0,
                             QPen(Qt::NoPen), QBrush(Qt::black));
    item->setZValue(kJunctionZ);
}

void SchematicScene::addComponentItems(const Component &component)
{
    const SymbolDefinition &def = symbolDefinition(component.kind());
    const qreal degrees = static_cast<qreal>(static_cast<int>(component.rotation()));

    // Symbol artwork is one rotated/translated group - a single Qt transform
    // handles rotating every primitive together, matching Component's own
    // rotate-then-translate contract (see component.h/component.cpp
    // rotateOffset()).
    auto *artwork = new QGraphicsItemGroup;
    for (const SymbolPrimitive &primitive : def.primitives) {
        const QPainterPath path = primitivePath(primitive);
        if (path.isEmpty())
            continue;
        auto *primitiveItem = new QGraphicsPathItem(path);
        primitiveItem->setPen(QPen(Qt::black, kLineWidth));
        if (primitive.kind == PrimitiveKind::Polygon)
            primitiveItem->setBrush(Qt::white);
        artwork->addToGroup(primitiveItem);
    }
    artwork->setPos(component.position());
    artwork->setRotation(degrees);
    artwork->setZValue(kSymbolZ);
    addItem(artwork);

    // Labels are separate, unrotated top-level items positioned directly in
    // world coordinates (not parented under the rotated artwork group), so
    // component rotation never rotates the text - see the step 4 plan's
    // decision on label placement. A fixed offset below/above the symbol's
    // own *unrotated* local bounding box keeps label position independent of
    // rotation as well, so labels don't jump around the symbol as it's
    // rotated.
    QRectF localBounds;
    for (const SymbolPrimitive &primitive : def.primitives)
        localBounds = localBounds.united(primitivePath(primitive).boundingRect());

    auto *reference = new QGraphicsSimpleTextItem(component.reference());
    QFont font = reference->font();
    font.setPixelSize(static_cast<int>(kLabelPixelSize));
    reference->setFont(font);
    reference->setPos(component.position()
                       + QPointF(localBounds.left(),
                                 localBounds.top() - kLabelGap - kLabelPixelSize));
    reference->setZValue(kLabelZ);
    addItem(reference);

    if (!component.value().isEmpty()) {
        auto *value = new QGraphicsSimpleTextItem(component.value());
        value->setFont(font);
        value->setPos(component.position()
                      + QPointF(localBounds.left(), localBounds.bottom() + kLabelGap));
        value->setZValue(kLabelZ);
        addItem(value);
    }
}
