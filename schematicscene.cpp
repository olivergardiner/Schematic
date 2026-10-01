#include "schematicscene.h"
#include "schematicitems.h"

#include "component.h"
#include "document.h"
#include "routegeometry.h"
#include "symboldefinition.h"
#include "wireendpoint.h"
#include "wireroute.h"

#include <QGraphicsSceneMouseEvent>
#include <QGraphicsView>
#include <QKeyEvent>
#include <QLineF>
#include <QPainterPath>
#include <QPen>

#include <cmath>

namespace {
// A generous default canvas in scene units (treated as mm at 1 unit == 1 mm)
// so a typical A4/A3-ish schematic sheet fits comfortably without the user
// needing to resize anything up front.
constexpr qreal kDefaultWidth  = 2000.0;
constexpr qreal kDefaultHeight = 1500.0;

// Explicit stacking order: wires sit behind everything, junction dots sit
// above wires (so a dot is never hidden under a wire it marks), symbol
// artwork sits above both, and reference/value labels sit above everything
// so text is never occluded by artwork. The in-progress wire preview sits
// above all rendered content so it's always visible while drawing.
constexpr qreal kWireZ        = 0.0;
constexpr qreal kJunctionZ    = 1.0;
constexpr qreal kSymbolZ      = 2.0;
constexpr qreal kLabelZ       = 3.0;
constexpr qreal kWirePreviewZ = 4.0;

// Real (non-cosmetic) scene-unit pen widths and a scene-unit font size, so
// line weight and text both scale with the schematic exactly like symbol
// artwork does when the view is zoomed. This is a presentation/export tool,
// not a screen-fixed-size UI.
constexpr qreal kLineWidth      = 1.0;
constexpr qreal kJunctionRadius = 2.5;
constexpr qreal kLabelPixelSize = 8.0;
constexpr qreal kLabelGap       = 4.0; // Gap between a component's local
                                       // bounding box and its labels.

// Fixed on-screen hit tolerance for terminal/node/wire-segment picking,
// converted to scene units via the view's current zoom (see
// SchematicScene::hitToleranceInSceneUnits()) so the click target stays a
// constant, comfortable screen size regardless of zoom level.
constexpr qreal kHitToleranceScreenPixels = 8.0;

bool closeEnough(qreal a, qreal b)
{
    return std::abs(a - b) <= kRouteGeometryEpsilon;
}

bool closeEnough(QPointF a, QPointF b)
{
    return closeEnough(a.x(), b.x()) && closeEnough(a.y(), b.y());
}

// Closest point to p lying exactly on the axis-aligned segment [a, b].
// Routes are always orthogonal (validateRouteGeometry() enforces this), so
// this only needs the horizontal/vertical cases.
QPointF closestPointOnSegment(QPointF p, QPointF a, QPointF b)
{
    if (closeEnough(a.y(), b.y())) {
        const qreal lo = std::min(a.x(), b.x());
        const qreal hi = std::max(a.x(), b.x());
        return QPointF(qBound(lo, p.x(), hi), a.y());
    }
    if (closeEnough(a.x(), b.x())) {
        const qreal lo = std::min(a.y(), b.y());
        const qreal hi = std::max(a.y(), b.y());
        return QPointF(a.x(), qBound(lo, p.y(), hi));
    }
    return a; // Unreachable for a validated orthogonal route.
}

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

void SchematicScene::bindDocument(Document *document)
{
    cancelPendingWire();
    m_dragging = false;
    m_document = document;
    rebuild();
}

void SchematicScene::setEditMode(EditMode mode)
{
    if (mode == m_mode)
        return;
    cancelPendingWire();
    clearSelection();
    m_mode = mode;
}

void SchematicScene::cancelPendingWire()
{
    if (!m_wireActive)
        return;
    m_wireActive = false;
    m_pendingVertices.clear();
    if (m_wirePreviewItem) {
        removeItem(m_wirePreviewItem);
        delete m_wirePreviewItem;
        m_wirePreviewItem = nullptr;
    }
}

void SchematicScene::rebuild()
{
    clear();
    m_wirePreviewItem = nullptr; // clear() already deleted the item itself.
    if (!m_document)
        return;

    for (const WireRoute &route : m_document->wires())
        addWireItems(route);
    for (const QPointF &point : m_document->junctionPoints())
        addJunctionItem(point);
    for (const Component &component : m_document->components())
        addComponentItems(component);
}

void SchematicScene::addWireItems(const WireRoute &route)
{
    QPainterPath path;
    path.moveTo(route.vertices.first());
    for (int i = 1; i < route.vertices.size(); ++i)
        path.lineTo(route.vertices[i]);

    auto *item = new WireItem(route.id);
    item->setPath(path);
    item->setPen(QPen(Qt::black, kLineWidth));
    item->setFlag(QGraphicsItem::ItemIsSelectable, true);
    item->setZValue(kWireZ);
    addItem(item);
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
    auto *artwork = new ComponentItem(component.id());
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
    artwork->setFlag(QGraphicsItem::ItemIsSelectable, true);
    addItem(artwork);

    // Labels are separate, unrotated top-level items positioned directly in
    // world coordinates (not parented under the rotated artwork group), so
    // component rotation never rotates the text (see DECISIONS.md
    // "Symbols and labels"). Each is tagged with the same ComponentId (see
    // schematicitems.h) so a click on a label maps back to this component for
    // selection/dragging/deletion - see componentItemAt().
    QRectF localBounds;
    for (const SymbolPrimitive &primitive : def.primitives)
        localBounds = localBounds.united(primitivePath(primitive).boundingRect());

    auto *reference = new ComponentLabelItem(component.reference(), component.id());
    QFont font = reference->font();
    font.setPixelSize(static_cast<int>(kLabelPixelSize));
    reference->setFont(font);
    reference->setPos(component.position()
                       + QPointF(localBounds.left(),
                                 localBounds.top() - kLabelGap - kLabelPixelSize));
    reference->setZValue(kLabelZ);
    addItem(reference);

    if (!component.value().isEmpty()) {
        auto *value = new ComponentLabelItem(component.value(), component.id());
        value->setFont(font);
        value->setPos(component.position()
                      + QPointF(localBounds.left(), localBounds.bottom() + kLabelGap));
        value->setZValue(kLabelZ);
        addItem(value);
    }
}

QPointF SchematicScene::snapToGrid(QPointF point) const
{
    const qreal spacing = m_document ? m_document->gridSpacing() : 10.0;
    return QPointF(std::round(point.x() / spacing) * spacing,
                   std::round(point.y() / spacing) * spacing);
}

qreal SchematicScene::hitToleranceInSceneUnits() const
{
    // Convert a fixed on-screen tolerance to scene units via the attached
    // view's current zoom, so the click target stays a constant, comfortable
    // screen size regardless of zoom level. Falls back to a plain scene-unit
    // value if the scene isn't attached to a view yet.
    const QList<QGraphicsView *> attachedViews = views();
    if (attachedViews.isEmpty())
        return kHitToleranceScreenPixels;
    const QGraphicsView *view = attachedViews.first();
    const qreal scale = view->transform().m11();
    if (scale <= 0.0)
        return kHitToleranceScreenPixels;
    return kHitToleranceScreenPixels / scale;
}

SchematicScene::HitResult SchematicScene::hitTestConnection(QPointF scenePos) const
{
    HitResult best;
    if (!m_document)
        return best;
    const qreal tolerance = hitToleranceInSceneUnits();
    qreal bestDistance = tolerance;

    // Terminals take priority over nodes and wire segments: a terminal is
    // always the most specific, most useful thing to attach to at a given
    // point.
    for (const Component &component : m_document->components()) {
        for (TerminalId t = 0; t < component.terminalCount(); ++t) {
            const QPointF terminalPos = component.terminalPosition(t);
            const qreal distance = QLineF(scenePos, terminalPos).length();
            if (distance <= bestDistance) {
                bestDistance = distance;
                best.kind = HitKind::Terminal;
                best.terminal = TerminalRef{component.id(), t};
                best.point = terminalPos;
            }
        }
    }
    if (best.kind == HitKind::Terminal)
        return best;

    // Nodes must be tested in full (document->nodes()), not just
    // junctionPoints() - a node referenced by exactly 2 wires is a
    // legitimate, clickable pass-through attachment identity even though it
    // isn't drawn as a visible dot (only 3+-reference nodes are).
    const QHash<NodeId, QPointF> &nodes = m_document->nodes();
    for (auto it = nodes.cbegin(); it != nodes.cend(); ++it) {
        const qreal distance = QLineF(scenePos, it.value()).length();
        if (distance <= bestDistance) {
            bestDistance = distance;
            best.kind = HitKind::Node;
            best.node = it.key();
            best.point = it.value();
        }
    }
    if (best.kind == HitKind::Node)
        return best;

    // Wire segments (candidate BranchSite locations). The exact snapped
    // point is recomputed against the model's stricter segment-membership
    // rule when the wire is actually completed (see
    // Document::addWireBranching()); this is a looser UI-only pick.
    for (const WireRoute &route : m_document->wires()) {
        for (int i = 0; i + 1 < route.vertices.size(); ++i) {
            const QPointF closest = closestPointOnSegment(scenePos, route.vertices[i],
                                                           route.vertices[i + 1]);
            const qreal distance = QLineF(scenePos, closest).length();
            if (distance <= bestDistance) {
                bestDistance = distance;
                best.kind = HitKind::WireSegment;
                best.wire = route.id;
                best.point = closest;
            }
        }
    }
    return best;
}

ComponentItem *SchematicScene::componentItemAt(QPointF scenePos) const
{
    const QList<QGraphicsItem *> hits = items(scenePos);
    for (QGraphicsItem *hit : hits) {
        if (auto *component = qgraphicsitem_cast<ComponentItem *>(hit))
            return component;
        if (auto *label = qgraphicsitem_cast<ComponentLabelItem *>(hit)) {
            if (!m_document)
                continue;
            // Map the label back to its owning ComponentItem so both drive
            // identical select/drag/delete behaviour - see schematicitems.h.
            for (QGraphicsItem *sibling : items()) {
                if (auto *candidate = qgraphicsitem_cast<ComponentItem *>(sibling)) {
                    if (candidate->componentId() == label->componentId())
                        return candidate;
                }
            }
        }
    }
    return nullptr;
}

void SchematicScene::beginDrag(ComponentItem *item, QPointF scenePos)
{
    if (!m_document)
        return;
    const Component *component = m_document->component(item->componentId());
    if (!component)
        return;
    m_dragging = true;
    m_dragComponentId = item->componentId();
    m_dragOriginalPosition = component->position();
    m_dragPressScenePos = scenePos;
    m_dragCurrentSnappedPosition = m_dragOriginalPosition;
    m_dragItems.clear();
    m_dragItemOrigins.clear();
    m_dragItems.append(item);
    m_dragItemOrigins.append(item->pos());
    for (QGraphicsItem *candidate : items()) {
        if (auto *label = qgraphicsitem_cast<ComponentLabelItem *>(candidate)) {
            if (label->componentId() == m_dragComponentId) {
                m_dragItems.append(label);
                m_dragItemOrigins.append(label->pos());
            }
        }
    }
}

void SchematicScene::updateDrag(QPointF scenePos)
{
    if (!m_dragging)
        return;
    const QPointF delta = snapToGrid(m_dragOriginalPosition + (scenePos - m_dragPressScenePos))
                          - m_dragOriginalPosition;
    m_dragCurrentSnappedPosition = m_dragOriginalPosition + delta;
    for (int i = 0; i < m_dragItems.size(); ++i)
        m_dragItems[i]->setPos(m_dragItemOrigins[i] + delta);
}

void SchematicScene::endDrag()
{
    if (!m_dragging)
        return;
    m_dragging = false;
    m_dragItems.clear();
    m_dragItemOrigins.clear();
    // A plain click (mouse pressed and released with no movement in
    // between) never calls updateDrag(), so m_dragCurrentSnappedPosition
    // stays equal to m_dragOriginalPosition here - that case must not
    // rebuild(), or it would wipe out the native selection state that
    // mousePressEvent() just set up for a plain select-click.
    if (m_dragCurrentSnappedPosition == m_dragOriginalPosition)
        return;
    if (m_document) {
        if (m_document->moveComponent(m_dragComponentId, m_dragCurrentSnappedPosition))
            emit documentEdited();
        else
            emit statusMessage(tr("That move would break an existing wire connection."));
    }
    rebuild(); // Whether the move committed or was rejected, the scene must
               // reflect the document's actual current state.
}

void SchematicScene::extendPendingWire(QPointF scenePos)
{
    if (!m_document)
        return;
    const HitResult hit = hitTestConnection(scenePos);

    if (!m_wireActive) {
        // A wire can only ever *start* from a terminal, node, or existing
        // wire segment - never from empty space: both ends of a committed
        // wire must resolve to something real (see DECISIONS.md "Document
        // model and connectivity").
        if (hit.kind == HitKind::None) {
            emit statusMessage(tr("Start a wire from a terminal, node, or existing wire."));
            return;
        }
        m_wireActive = true;
        m_pendingVertices.clear();
        m_pendingVertices.append(hit.point);
        switch (hit.kind) {
        case HitKind::Terminal:
            m_pendingStart = makeTerminalEndpoint(hit.terminal.component, hit.terminal.terminal);
            break;
        case HitKind::Node:
            m_pendingStart = makeNodeEndpoint(hit.node);
            break;
        case HitKind::WireSegment:
            m_pendingStart = BranchSite{hit.wire, hit.point};
            break;
        case HitKind::None:
            break;
        }
        return;
    }

    // A click on empty space extends the pending route with a grid-snapped
    // corner and never calls into Document - only a click resolving to a
    // terminal, node, or wire segment can complete the wire.
    if (hit.kind == HitKind::None) {
        const QPointF snapped = snapToGrid(scenePos);
        const QPointF last = m_pendingVertices.last();
        // Insert a horizontal-first deterministic corner for a diagonal
        // click (see DECISIONS.md "Document model and connectivity").
        if (!closeEnough(last.x(), snapped.x()) && !closeEnough(last.y(), snapped.y()))
            m_pendingVertices.append(QPointF(snapped.x(), last.y()));
        m_pendingVertices.append(snapped);
        return;
    }

    // Completion: build the interior-vertex list (everything after the
    // resolved start point) and commit via the atomic model API.
    QVector<QPointF> interior = m_pendingVertices.mid(1);
    const QPointF last = interior.isEmpty() ? m_pendingVertices.first() : interior.last();
    if (!closeEnough(last.x(), hit.point.x()) && !closeEnough(last.y(), hit.point.y()))
        interior.append(QPointF(hit.point.x(), last.y()));

    PendingWireEndpoint end;
    switch (hit.kind) {
    case HitKind::Terminal:
        end = makeTerminalEndpoint(hit.terminal.component, hit.terminal.terminal);
        break;
    case HitKind::Node:
        end = makeNodeEndpoint(hit.node);
        break;
    case HitKind::WireSegment:
        end = BranchSite{hit.wire, hit.point};
        break;
    case HitKind::None:
        Q_UNREACHABLE();
    }

    const auto result = m_document->addWireBranching(m_pendingStart, interior, end);
    cancelPendingWire();
    if (result) {
        emit documentEdited();
        rebuild();
    } else {
        emit statusMessage(tr("That wire could not be completed there."));
    }
}

void SchematicScene::deleteSelection()
{
    if (!m_document)
        return;
    bool changed = false;
    const QList<QGraphicsItem *> selected = selectedItems();
    for (QGraphicsItem *item : selected) {
        if (auto *component = qgraphicsitem_cast<ComponentItem *>(item)) {
            if (m_document->removeComponent(component->componentId()))
                changed = true;
        } else if (auto *wire = qgraphicsitem_cast<WireItem *>(item)) {
            if (m_document->removeWire(wire->wireId()))
                changed = true;
        }
    }
    if (changed) {
        emit documentEdited();
        rebuild();
    }
}

void SchematicScene::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (!m_document) {
        QGraphicsScene::mousePressEvent(event);
        return;
    }

    if (event->button() == Qt::LeftButton) {
        switch (m_mode) {
        case EditMode::Select: {
            ComponentItem *hit = componentItemAt(event->scenePos());
            if (hit) {
                QGraphicsScene::mousePressEvent(event); // Native selection.
                beginDrag(hit, event->scenePos());
                return;
            }
            break;
        }
        case EditMode::PlaceComponent: {
            const QPointF snapped = snapToGrid(event->scenePos());
            m_document->addComponent(m_placeKind, snapped);
            emit documentEdited();
            rebuild();
            return;
        }
        case EditMode::DrawWire:
            extendPendingWire(event->scenePos());
            return;
        }
    }

    QGraphicsScene::mousePressEvent(event);
}

void SchematicScene::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_dragging) {
        updateDrag(event->scenePos());
        return;
    }
    if (m_mode == EditMode::DrawWire && m_wireActive) {
        // Live preview only - no Document call. Mirrors the same
        // hit-test/corner-insertion rule extendPendingWire() uses on click,
        // so the preview always matches what a click there would commit.
        const HitResult hit = hitTestConnection(event->scenePos());
        const QPointF target = hit.kind != HitKind::None ? hit.point
                                                          : snapToGrid(event->scenePos());
        QVector<QPointF> preview = m_pendingVertices;
        const QPointF last = preview.last();
        if (!closeEnough(last.x(), target.x()) && !closeEnough(last.y(), target.y()))
            preview.append(QPointF(target.x(), last.y()));
        preview.append(target);

        QPainterPath path;
        path.moveTo(preview.first());
        for (int i = 1; i < preview.size(); ++i)
            path.lineTo(preview[i]);
        if (!m_wirePreviewItem) {
            m_wirePreviewItem = new QGraphicsPathItem;
            QPen pen(Qt::blue, kLineWidth);
            pen.setStyle(Qt::DashLine);
            m_wirePreviewItem->setPen(pen);
            m_wirePreviewItem->setZValue(kWirePreviewZ);
            addItem(m_wirePreviewItem);
        }
        m_wirePreviewItem->setPath(path);
        return;
    }
    QGraphicsScene::mouseMoveEvent(event);
}

void SchematicScene::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_dragging) {
        endDrag();
        return;
    }
    QGraphicsScene::mouseReleaseEvent(event);
}

void SchematicScene::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_document && m_mode == EditMode::Select && event->button() == Qt::LeftButton) {
        if (ComponentItem *hit = componentItemAt(event->scenePos())) {
            emit componentDoubleClicked(hit->componentId());
            return;
        }
    }
    QGraphicsScene::mouseDoubleClickEvent(event);
}

void SchematicScene::refreshAfterExternalEdit()
{
    if (!m_document)
        return;
    rebuild();
    emit documentEdited();
}

std::optional<ComponentId> SchematicScene::singleSelectedComponent() const
{
    std::optional<ComponentId> found;
    for (QGraphicsItem *item : selectedItems()) {
        if (auto *component = qgraphicsitem_cast<ComponentItem *>(item)) {
            if (found)
                return std::nullopt; // More than one component selected.
            found = component->componentId();
        }
    }
    return found;
}

void SchematicScene::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        cancelPendingWire();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
        deleteSelection();
        event->accept();
        return;
    }
    QGraphicsScene::keyPressEvent(event);
}
