#include "schematicscene.h"
#include "schematicitems.h"
#include "symbolrenderer.h"

#include "component.h"
#include "document.h"
#include "routegeometry.h"
#include "symbollibrary.h"
#include "wireendpoint.h"
#include "wireroute.h"

#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSceneContextMenuEvent>
#include <QFontMetricsF>
#include <QMenu>
#include <QGraphicsView>
#include <QKeyEvent>
#include <QLineF>
#include <QPainterPath>
#include <QPainter>
#include <QPen>
#include <QSet>
#include <QApplication>
#include <algorithm>

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

} // namespace

SchematicScene::SchematicScene(QObject *parent)
    : QGraphicsScene(parent)
{
    setSceneRect(-kDefaultWidth / 2.0, -kDefaultHeight / 2.0, kDefaultWidth, kDefaultHeight);
    setBackgroundBrush(Qt::white);
}

void SchematicScene::bindDocument(Document *document)
{
    cancelMove();
    emit selectionGestureReset();
    cancelPendingWire();
    m_dragging = false;
    // Component IDs are only meaningful within one Document, so a newly bound
    // (or unbound) document must never inherit the previous selection.
    clearSelection();
    m_document = document;
    rebuild();
}

void SchematicScene::setEditMode(EditMode mode)
{
    if (mode == m_mode)
        return;
    cancelMove();
    emit selectionGestureReset();
    cancelPendingWire();
    clearSelection();
    m_mode = mode;
    update();
    emit editModeChanged(m_mode);
}

bool SchematicScene::placeComponentAt(const SymbolId &id, QPointF scenePos)
{
    if (!m_document)
        return false;
    const LibrarySymbol *symbol = m_document->symbol(id);
    if (!symbol) symbol = applicationSymbolLibrary().find(id);
    if (!symbol)
        return false;

    const QPointF snapped = snapToGrid(scenePos);
    ComponentId added = kInvalidComponentId;
    return commitEdit(tr("Place %1").arg(symbol->displayName),
                      [this, id, snapped, &added]() {
        if (!m_document)
            return false;
        added = m_document->addComponent(id, snapped);
        return added != kInvalidComponentId;
    }, [&added]() {
        return added == kInvalidComponentId ? Selection{} : Selection{{added}, {}};
    });
}

void SchematicScene::cancelPendingWire()
{
    const bool hadCandidate = m_hasWireHoverCandidate;
    m_hasWireHoverCandidate = false;
    if (!m_wireActive) {
        if (hadCandidate)
            update();
        return;
    }
    m_wireActive = false;
    m_pendingVertices.clear();
    if (m_wirePreviewItem) {
        removeItem(m_wirePreviewItem);
        delete m_wirePreviewItem;
        m_wirePreviewItem = nullptr;
    }
    update();
}

void SchematicScene::setWireHoverCandidate(bool visible, QPointF point)
{
    if (m_hasWireHoverCandidate == visible
        && (!visible || closeEnough(m_wireHoverCandidate, point)))
        return;
    m_hasWireHoverCandidate = visible;
    m_wireHoverCandidate = point;
    update();
}

void SchematicScene::drawForeground(QPainter *painter, const QRectF &rect)
{
    QGraphicsScene::drawForeground(painter, rect);
    if (m_mode != EditMode::DrawWire || !m_document || !painter)
        return;

    const qreal viewScale = std::hypot(painter->worldTransform().m11(),
                                       painter->worldTransform().m12());
    if (viewScale <= 0.0)
        return;
    const qreal terminalRadius = 4.0 / viewScale;
    const qreal candidateRadius = 7.0 / viewScale;

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    QPen terminalPen(QColor(35, 105, 155));
    terminalPen.setCosmetic(true);
    terminalPen.setWidth(1);
    painter->setPen(terminalPen);
    painter->setBrush(QColor(225, 242, 252));
    for (const Component &component : m_document->components()) {
        for (TerminalId terminal = 0; terminal < m_document->terminalCount(component); ++terminal) {
            const QPointF point = m_document->terminalPosition(component, terminal);
            if (rect.adjusted(-terminalRadius, -terminalRadius,
                              terminalRadius, terminalRadius).contains(point))
                painter->drawEllipse(point, terminalRadius, terminalRadius);
        }
    }

    if (m_wireActive && m_hasWireHoverCandidate) {
        QPen candidatePen(QColor(190, 95, 0));
        candidatePen.setCosmetic(true);
        candidatePen.setWidth(2);
        painter->setPen(candidatePen);
        painter->setBrush(QColor(255, 218, 130));
        QPolygonF diamond;
        diamond << QPointF(m_wireHoverCandidate.x(), m_wireHoverCandidate.y() - candidateRadius)
                << QPointF(m_wireHoverCandidate.x() + candidateRadius, m_wireHoverCandidate.y())
                << QPointF(m_wireHoverCandidate.x(), m_wireHoverCandidate.y() + candidateRadius)
                << QPointF(m_wireHoverCandidate.x() - candidateRadius, m_wireHoverCandidate.y());
        painter->drawPolygon(diamond);
    }
    painter->restore();
}

void SchematicScene::rebuild(const Selection *selectionOverride)
{
    cancelMove();
    emit selectionGestureReset();
    const Selection retained = selectionOverride ? *selectionOverride : selection();
    if (m_document)
        renderDocument(*m_document, retained);
    else
        clear();
}

void SchematicScene::renderDocument(const Document &document, const Selection &retained)
{
    clear();
    m_wirePreviewItem = nullptr; // clear() already deleted the item itself.
    for (const WireRoute &route : document.wires())
        addWireItems(route);
    for (const QPointF &point : document.junctionPoints())
        addJunctionItem(point);
    for (const Component &component : document.components())
        addComponentItems(component, document);
    restoreSelection(retained);
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

void SchematicScene::addComponentItems(const Component &component, const Document &document)
{
    const LibrarySymbol *symbol = document.symbol(component.symbolId());
    if (!symbol)
        return; // Unreachable for components created through Document's API.
    const LibrarySymbol &def = *symbol;
    const qreal degrees = static_cast<qreal>(static_cast<int>(component.rotation()));

    // Symbol artwork is one rotated/translated group - a single Qt transform
    // handles rotating every primitive together, matching Component's own
    // rotate-then-translate contract (see component.h/component.cpp
    // rotateOffset()).
    auto *artwork = new ComponentItem(component.id());
    for (const RenderedSymbolPrimitive &primitive : SymbolRenderer::primitives(def)) {
        auto *primitiveItem = new QGraphicsPathItem(primitive.path);
        primitiveItem->setPen(QPen(Qt::black, kLineWidth));
        primitiveItem->setBrush(SymbolRenderer::fillBrush(primitive));
        artwork->addToGroup(primitiveItem);
    }
    artwork->setPos(component.position());
    artwork->setTransform(QTransform::fromScale(component.mirrored() ? -1.0 : 1.0, 1.0));
    artwork->setRotation(degrees);
    artwork->setZValue(kSymbolZ);
    artwork->setFlag(QGraphicsItem::ItemIsSelectable, true);
    addItem(artwork);

    // The upright block is independent of the artwork transform. Its rows
    // retain property/owner IDs for selection and inspector navigation.
    const QRectF bounds = artwork->sceneBoundingRect();

    QFont font;
    font.setPixelSize(static_cast<int>(kLabelPixelSize));

    auto *block = new PropertyBlockItem(component.id());
    block->setZValue(kLabelZ); addItem(block);
    block->setData(0, m_dragging && m_propertyDragging && m_dragComponentId == component.id());
    block->setPos(component.propertyBlockOffset()
        ? component.position() + *component.propertyBlockOffset()
        : QPointF(bounds.left(), bounds.bottom() + kLabelGap));
    qreal below = 0;
    for (const auto &property : component.properties()) {
        if (!property.visible || property.value.isEmpty()) continue;
        const bool reference = property.id == "reference";
        const QString text = reference || property.id == "value" ? property.value
                            : property.name + ": " + property.value;
        auto *label = new ComponentLabelItem(text, component.id(), property.id);
        label->setFont(font);
        block->addToGroup(label);
        label->setPos(0, below);
        below += qMax(QFontMetricsF(font).lineSpacing(), label->boundingRect().height());
        label->setZValue(kLabelZ);
        // The block owns the row.
    }
    if (block->childItems().isEmpty()) delete block;
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
        for (TerminalId t = 0; t < m_document->terminalCount(component); ++t) {
            const QPointF terminalPos = m_document->terminalPosition(component, t);
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
        if (auto *block = qgraphicsitem_cast<PropertyBlockItem *>(hit)) {
            for (auto *candidate : items())
                if (auto *body = qgraphicsitem_cast<ComponentItem *>(candidate))
                    if (body->componentId() == block->componentId()) return body;
        }
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

SchematicScene::Selection SchematicScene::selection() const
{
    Selection result;
    for (QGraphicsItem *item : selectedItems()) {
        if (auto *component = qgraphicsitem_cast<ComponentItem *>(item))
            result.components.append(component->componentId());
        if (auto *wire = qgraphicsitem_cast<WireItem *>(item))
            result.wires.append(wire->wireId());
    }
    std::sort(result.components.begin(), result.components.end());
    std::sort(result.wires.begin(), result.wires.end());
    return result;
}

void SchematicScene::restoreSelection(const Selection &selection)
{
    for (QGraphicsItem *item : items()) {
        if (auto *component = qgraphicsitem_cast<ComponentItem *>(item))
            component->setSelected(selection.components.contains(component->componentId()));
        if (auto *wire = qgraphicsitem_cast<WireItem *>(item))
            wire->setSelected(selection.wires.contains(wire->wireId()));
    }
}

bool SchematicScene::canStartSelectionBox(QPointF position) const
{
    if (!m_document || m_mode != EditMode::Select || componentItemAt(position))
        return false;
    for (QGraphicsItem *item : items(position)) {
        if (qgraphicsitem_cast<WireItem *>(item))
            return false;
    }
    return true;
}

void SchematicScene::selectInBox(const QRectF &rect, bool crossing,
                                 const Selection &baseline)
{
    // Group::shape() is its bounding rectangle. Gather primitive shapes
    // individually to avoid selecting empty space between artwork parts.
    QHash<ComponentId, QPainterPath> shapes;
    const auto sceneItems = items();
    for (QGraphicsItem *item : sceneItems) {
        if (auto *component = qgraphicsitem_cast<ComponentItem *>(item)) {
            for (QGraphicsItem *child : component->childItems())
                shapes[component->componentId()] = shapes[component->componentId()].united(
                    child->mapToScene(child->shape()));
        } else if (auto *label = qgraphicsitem_cast<ComponentLabelItem *>(item)) {
            shapes[label->componentId()] = shapes[label->componentId()].united(
                label->mapToScene(label->shape()));
        }
    }
    const QRectF box = rect.normalized();
    // Tiny numerical tolerance includes exact boundary contact without
    // introducing a zoom-dependent picking margin.
    const QRectF inclusiveBox = box.adjusted(-1e-8, -1e-8, 1e-8, 1e-8);
    const auto matches = [&](const QPainterPath &shape) {
        return !shape.isEmpty() && (crossing ? shape.intersects(inclusiveBox)
                                            : inclusiveBox.contains(shape.boundingRect()));
    };
    for (QGraphicsItem *item : sceneItems) {
        if (auto *component = qgraphicsitem_cast<ComponentItem *>(item))
            component->setSelected(baseline.components.contains(component->componentId())
                                   || matches(shapes.value(component->componentId())));
        if (auto *wire = qgraphicsitem_cast<WireItem *>(item))
            wire->setSelected(baseline.wires.contains(wire->wireId())
                              || matches(wire->mapToScene(wire->shape())));
    }
}

void SchematicScene::beginDrag(ComponentItem *item, QPointF scenePos)
{
    if (!m_document)
        return;
    m_propertyDragging = false;
    m_dragging = true;
    m_dragThresholdPassed = false;
    m_dragCandidateValid = true;
    m_dragComponentId = item ? item->componentId() : kInvalidComponentId;
    m_dragOriginalPosition = item ? m_document->component(item->componentId())->position() : QPointF{};
    m_dragPressScenePos = scenePos;
    m_dragDisplacement = {};
    m_dragSelection = selection();
    m_dragOriginal = *m_document;
    emit moveStateChanged();
}

void SchematicScene::updateDrag(QPointF scenePos)
{
    if (!m_dragging)
        return;
    const QPointF pointerDelta = scenePos - m_dragPressScenePos;
    QPointF screenDelta = pointerDelta;
    if (!views().isEmpty())
        screenDelta = views().first()->transform().map(pointerDelta)
                      - views().first()->transform().map(QPointF{});
    if (!m_dragThresholdPassed && std::abs(screenDelta.x()) + std::abs(screenDelta.y())
                                     < QApplication::startDragDistance())
        return;
    m_dragThresholdPassed = true;
    if (m_propertyDragging) {
        m_dragDisplacement = pointerDelta;
        Document candidate = *m_dragOriginal;
        candidate.setPropertyBlockOffset({m_dragComponentId}, m_propertyOriginalOffset + pointerDelta);
        renderDocument(candidate, m_dragSelection);
        return;
    }
    m_dragDisplacement = pointerDelta.isNull() ? QPointF{}
        : (m_dragComponentId != kInvalidComponentId
           ? snapToGrid(m_dragOriginalPosition + pointerDelta) - m_dragOriginalPosition
           : snapToGrid(pointerDelta));
    Document candidate = *m_dragOriginal;
    m_dragCandidateValid = candidate.transformSelection(m_dragSelection.components, m_dragSelection.wires,
                              {SelectionTransform::Kind::Translate, m_dragDisplacement});
    if (m_dragCandidateValid) {
        renderDocument(candidate, m_dragSelection);
        emit statusMessage(QString{});
    } else {
        emit statusMessage(tr("That move would create an invalid wire route. Choose another position or press Escape."));
    }
}

void SchematicScene::endDrag()
{
    if (!m_dragging)
        return;
    const bool shouldCommit = m_dragThresholdPassed && m_dragCandidateValid;
    const auto retained = m_dragSelection;
    const QPointF delta = m_dragDisplacement;
    m_dragging = false;
    m_dragOriginal.reset();
    if (shouldCommit && !delta.isNull() && m_propertyDragging) {
        const auto id = m_dragComponentId; const auto offset = m_propertyOriginalOffset + delta;
        commitEdit(tr("Move property block"), [this, id, offset] {
            return m_document->setPropertyBlockOffset({id}, offset);
        });
    } else if (shouldCommit && !delta.isNull())
        commitEdit(tr("Move selection"), [this, retained, delta]() {
            return m_document && m_document->transformSelection(retained.components, retained.wires,
                                                {SelectionTransform::Kind::Translate, delta});
        });
    // Also restore a rejected/no-op preview when no command rebuild occurs.
    if (m_document) renderDocument(*m_document, retained);
    emit moveStateChanged();
}

void SchematicScene::cancelMove()
{
    if (!m_dragging) return;
    m_dragging = false;
    m_dragOriginal.reset();
    if (m_document) renderDocument(*m_document, m_dragSelection);
    emit statusMessage(QString{});
    emit moveStateChanged();
}

bool SchematicScene::transformSelected(SelectionTransform::Kind kind, int quarterTurns)
{
    if (!m_document || m_mode != EditMode::Select || m_dragging) return false;
    const auto targets = selection();
    if (targets.isEmpty()) return false;
    QPointF pivot;
    if (targets.components.size() == 1 && targets.wires.isEmpty()) {
        pivot = m_document->component(targets.components.first())->position();
    } else {
        bool initialized = false;
        qreal left = 0, right = 0, top = 0, bottom = 0;
        const auto include = [&](QPointF point) {
            if (!initialized) { left = right = point.x(); top = bottom = point.y(); initialized = true; }
            else { left = qMin(left, point.x()); right = qMax(right, point.x());
                   top = qMin(top, point.y()); bottom = qMax(bottom, point.y()); }
        };
        for (ComponentId id : targets.components) {
            const auto *component = m_document->component(id);
            const auto *definition = m_document->symbol(component->symbolId());
            QTransform pose;
            pose.translate(component->position().x(), component->position().y());
            pose.rotate(static_cast<int>(component->rotation()));
            pose.scale(component->mirrored() ? -1 : 1, 1);
            for (const auto &primitive : SymbolRenderer::primitives(*definition)) {
                const QRectF bounds = pose.map(primitive.path).boundingRect();
                include(bounds.topLeft()); include(bounds.bottomRight());
            }
        }
        for (WireId id : targets.wires)
            for (QPointF point : m_document->wire(id)->vertices) include(point);
        pivot = QPointF((left + right) / 2, (top + bottom) / 2);
    }
    QString description;
    if (kind == SelectionTransform::Kind::Rotate)
        description = quarterTurns > 0 ? tr("Rotate selection right") : tr("Rotate selection left");
    else if (kind == SelectionTransform::Kind::FlipHorizontal)
        description = tr("Flip selection horizontally");
    else if (kind == SelectionTransform::Kind::FlipVertical)
        description = tr("Flip selection vertically");
    else return false;
    const bool accepted = commitEdit(description, [this, targets, kind, quarterTurns, pivot]() {
        return m_document->transformSelection(targets.components, targets.wires, {kind, {}, pivot, quarterTurns});
    });
    if (!accepted)
        emit statusMessage(tr("The selection was unchanged or the transform would create an invalid wire route."));
    return accepted;
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
        setWireHoverCandidate(true, hit.point);
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
        // A repeat click on the current last point would add a zero-length
        // segment that the route validator then rejects at completion.
        if (closeEnough(last, snapped))
            return;
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

    std::optional<WireId> result;
    const bool committed = commitEdit(tr("Draw wire"), [this, &interior, &end, &result]() {
        if (!m_document)
            return false;
        result = m_document->addWireBranching(m_pendingStart, interior, end);
        if (!result)
            return false;
        cancelPendingWire();
        return true;
    });
    if (!committed) {
        // The Document is unchanged on rejection (addWireBranching() is
        // atomic) and m_pendingStart/m_pendingVertices were not modified
        // above, so the draft and its preview stay available: the user can
        // click another corner or target, or press Escape to discard it.
        emit statusMessage(tr("That wire could not be completed there. Click to adjust the "
                              "route or press Escape to cancel."));
    }
}

void SchematicScene::deleteSelection()
{
    if (!m_document)
        return;
    const QList<QGraphicsItem *> selected = selectedItems();
    const bool committed = commitEdit(tr("Delete selection"), [this, selected]() {
        bool changed = false;
        for (QGraphicsItem *item : selected) {
            if (auto *component = qgraphicsitem_cast<ComponentItem *>(item)) {
                if (m_document->removeComponent(component->componentId()))
                    changed = true;
            } else if (auto *wire = qgraphicsitem_cast<WireItem *>(item)) {
                if (m_document->removeWire(wire->wireId()))
                    changed = true;
            }
        }
        return changed;
    });
    Q_UNUSED(committed);
}

bool SchematicScene::commitEdit(const QString &description,
                                const std::function<bool()> &operation,
                                std::function<Selection()> afterSelection)
{
    if (!m_editCommitter)
        return false;
    return m_editCommitter(description, operation, std::move(afterSelection));
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
            emit selectionInteractionStarted();
            PropertyBlockItem *blockHit = nullptr;
            for (auto *item : items(event->scenePos())) {
                for (auto *ancestor = item; ancestor; ancestor = ancestor->parentItem())
                    if (auto *block = qgraphicsitem_cast<PropertyBlockItem *>(ancestor)) { blockHit = block; break; }
                if (blockHit) break;
            }
            const QPointF anchor = blockHit ? blockHit->scenePos() : QPointF{};
            ComponentItem *hit = componentItemAt(event->scenePos());
            QGraphicsItem *target = hit;
            if (!target) {
                for (auto *item : items(event->scenePos())) {
                    if (qgraphicsitem_cast<WireItem *>(item)) { target = item; break; }
                }
            }
            if (target) {
                if (event->modifiers() != Qt::NoModifier) {
                    if (event->modifiers().testFlag(Qt::ControlModifier))
                        target->setSelected(!target->isSelected());
                    event->accept();
                    return;
                }
                if (!target->isSelected()) { clearSelection(); target->setSelected(true); }
                beginDrag(hit, event->scenePos());
                if (blockHit && hit) {
                    m_propertyDragging = true;
                    m_propertyOriginalOffset = anchor - m_document->component(hit->componentId())->position();
                }
                event->accept();
                return;
            }
            break;
        }
        case EditMode::PlaceComponent: {
            placeComponentAt(m_placeKind, event->scenePos());
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
        event->accept();
        return;
    }
    if (m_mode == EditMode::DrawWire && m_wireActive) {
        // Live preview only - no Document call. Mirrors the same
        // hit-test/corner-insertion rule extendPendingWire() uses on click,
        // so the preview always matches what a click there would commit.
        const HitResult hit = hitTestConnection(event->scenePos());
        setWireHoverCandidate(hit.kind != HitKind::None, hit.point);
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
    if (m_hasWireHoverCandidate)
        setWireHoverCandidate(false);
    QGraphicsScene::mouseMoveEvent(event);
}

void SchematicScene::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_dragging && event->button() == Qt::LeftButton) {
        updateDrag(event->scenePos());
        endDrag();
        event->accept();
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

QVector<ComponentId> SchematicScene::selectedComponentIds() const
{
    QVector<ComponentId> selected;
    for (QGraphicsItem *item : selectedItems()) {
        if (auto *component = qgraphicsitem_cast<ComponentItem *>(item))
            selected.append(component->componentId());
    }
    return selected;
}

void SchematicScene::restoreAfterDocumentChange(const Selection &selection)
{
    cancelPendingWire();
    rebuild(&selection);
    emit selectionChanged();
}

void SchematicScene::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        cancelMove();
        setEditMode(EditMode::Select);
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
        cancelMove();
        deleteSelection();
        event->accept();
        return;
    }
    QGraphicsScene::keyPressEvent(event);
}

bool SchematicScene::resetPropertyPositions(const QVector<ComponentId> &ids)
{
    if (!m_document || m_dragging || ids.isEmpty()) return false;
    return commitEdit(tr("Reset property position"), [this, ids] {
        return m_document->setPropertyBlockOffset(ids, std::nullopt);
    });
}

void SchematicScene::contextMenuEvent(QGraphicsSceneContextMenuEvent *event)
{
    if (m_mode != EditMode::Select || m_dragging || !m_document) return;
    for (auto *item : items(event->scenePos())) {
        for (auto *ancestor = item; ancestor; ancestor = ancestor->parentItem()) {
            if (auto *block = qgraphicsitem_cast<PropertyBlockItem *>(ancestor)) {
                const auto id = block->componentId();
                QMenu menu; auto *reset = menu.addAction(tr("Reset property position"));
                reset->setEnabled(m_document->component(id)->propertyBlockOffset().has_value());
                if (menu.exec(event->screenPos()) == reset) resetPropertyPositions({id});
                event->accept(); return;
            }
        }
    }
    QGraphicsScene::contextMenuEvent(event);
}
