#include "document.h"

#include "symbollibrary.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <QSet>
#include <QRegularExpression>

namespace {

bool near(qreal a, qreal b)
{
    return std::abs(a - b) <= kRouteGeometryEpsilon;
}

bool samePoint(const QPointF &a, const QPointF &b)
{
    return near(a.x(), b.x()) && near(a.y(), b.y());
}

bool pointOnSegment(const QPointF &p, const QPointF &a, const QPointF &b)
{
    if (near(a.x(), b.x()))
        return near(p.x(), a.x()) && p.y() >= std::min(a.y(), b.y()) - kRouteGeometryEpsilon
            && p.y() <= std::max(a.y(), b.y()) + kRouteGeometryEpsilon;
    if (near(a.y(), b.y()))
        return near(p.y(), a.y()) && p.x() >= std::min(a.x(), b.x()) - kRouteGeometryEpsilon
            && p.x() <= std::max(a.x(), b.x()) + kRouteGeometryEpsilon;
    return false;
}

bool moveRouteEndpoint(WireRoute *route, bool start, QPointF position)
{
    QVector<QPointF> &points = route->vertices;
    if (start) {
        points[0] = position;
        while (points.size() >= 2 && samePoint(points[0], points[1])) {
            if (points.size() == 2)
                return false;
            points.removeAt(1);
        }
        if (!near(points[0].x(), points[1].x()) && !near(points[0].y(), points[1].y()))
            points.insert(1, QPointF(points[0].x(), points[1].y()));
    } else {
        points.last() = position;
        while (points.size() >= 2 && samePoint(points.last(), points[points.size() - 2])) {
            if (points.size() == 2)
                return false;
            points.removeAt(points.size() - 2);
        }
        const QPointF endpoint = points.last();
        const QPointF adjacent = points[points.size() - 2];
        if (!near(endpoint.x(), adjacent.x()) && !near(endpoint.y(), adjacent.y()))
            points.insert(points.size() - 1, QPointF(endpoint.x(), adjacent.y()));
    }
    return validateRouteGeometry(points) == RouteGeometryError::None;
}

} // namespace

int Document::terminalCount(const Component &item) const
{
    const auto *definition = symbol(item.symbolId());
    return definition ? item.terminalCount(*definition) : 0;
}

QPointF Document::terminalPosition(const Component &item, TerminalId terminal) const
{
    const auto *definition = symbol(item.symbolId());
    Q_ASSERT(definition);
    return definition ? item.terminalPosition(terminal, *definition) : item.position();
}

bool Document::useSymbol(const LibrarySymbol &definition, QStringList *errors)
{
    const auto *current = symbol(definition.id);
    if (current && !sameTerminalSignature(*current, definition)) {
        if (errors) errors->append(QStringLiteral("Document symbol terminal signature cannot change"));
        return false;
    }
    return m_symbols.find(definition.id) ? m_symbols.replace(definition, errors)
                                         : m_symbols.add(definition, errors);
}

bool Document::setGridSpacing(qreal spacing)
{
    if (!std::isfinite(spacing) || spacing <= 0.0)
        return false;
    m_gridSpacing = spacing;
    return true;
}

const Component *Document::component(ComponentId id) const
{
    for (const Component &item : m_components) {
        if (item.id() == id)
            return &item;
    }
    return nullptr;
}

const WireRoute *Document::wire(WireId id) const
{
    for (const WireRoute &item : m_wires) {
        if (item.id == id)
            return &item;
    }
    return nullptr;
}

ComponentId Document::addComponent(const SymbolId &symbolId, QPointF position, Rotation rotation)
{
    const LibrarySymbol *symbol = m_symbols.find(symbolId);
    if (!symbol && m_nextComponentId != kInvalidComponentId) {
        symbol = applicationSymbolLibrary().find(symbolId);
        if (symbol) { QStringList errors; m_symbols.add(*symbol, &errors); symbol = m_symbols.find(symbolId); }
    }
    if (!symbol || m_nextComponentId == kInvalidComponentId)
        return kInvalidComponentId;
    const ComponentId id = m_nextComponentId++;
    Component item(id, symbolId, position);
    item.setRotation(rotation);
    QSet<QString> used;
    for (const Component &existing : m_components)
        used.insert(existing.reference());
    item.setReference(firstUnusedReference(symbol->referencePrefix, used));
    auto properties = item.properties();
    properties[0].visible = symbol->showsReference;
    item.setProperties(properties);
    m_components.append(item);
    return id;
}

bool Document::moveComponent(ComponentId id, QPointF position)
{
    const Component *current = component(id);
    if (!current || !std::isfinite(position.x()) || !std::isfinite(position.y()))
        return false;
    Component moved = *current;
    moved.setPosition(position);
    return commitComponentChange(id, moved);
}

bool Document::rotateComponent(ComponentId id, Rotation rotation)
{
    const auto *current = component(id);
    return current && setComponentTransform(id, rotation, current->mirrored());
}

bool Document::setComponentMirrored(ComponentId id, bool mirrored)
{
    const auto *current = component(id);
    return current && setComponentTransform(id, current->rotation(), mirrored);
}

bool Document::setComponentTransform(ComponentId id, Rotation rotation, bool mirrored)
{
    const auto *current = component(id);
    const int degrees = static_cast<int>(rotation);
    if (!current || (degrees != 0 && degrees != 90 && degrees != 180 && degrees != 270))
        return false;
    Component changed = *current;
    changed.setRotation(rotation);
    changed.setMirrored(mirrored);
    return commitComponentChange(id, changed);
}

bool Document::flipComponent(ComponentId id, FlipAxis axis)
{
    const auto *current = component(id);
    if (!current) return false;
    // Canvas H * R(theta) = R(-theta) * local H; canvas V adds 180 degrees.
    const int degrees = (360 + (axis == FlipAxis::Vertical ? 180 : 0)
                         - static_cast<int>(current->rotation())) % 360;
    return setComponentTransform(id, static_cast<Rotation>(degrees), !current->mirrored());
}

bool Document::commitComponentChange(ComponentId id, const Component &changed)
{
    auto it = std::find_if(m_components.begin(), m_components.end(),
                           [id](const Component &item) { return item.id() == id; });
    if (it == m_components.end())
        return false;
    QVector<WireRoute> candidateWires = m_wires;
    for (WireRoute &route : candidateWires) {
        if (isTerminal(route.start) && std::get<TerminalRef>(route.start).component == id) {
            const TerminalRef ref = std::get<TerminalRef>(route.start);
            if (!moveRouteEndpoint(&route, true, terminalPosition(changed, ref.terminal)))
                return false;
        }
        if (isTerminal(route.end) && std::get<TerminalRef>(route.end).component == id) {
            const TerminalRef ref = std::get<TerminalRef>(route.end);
            if (!moveRouteEndpoint(&route, false, terminalPosition(changed, ref.terminal)))
                return false;
        }
    }
    *it = changed;
    m_wires = std::move(candidateWires);
    return true;
}

bool Document::transformSelection(const QVector<ComponentId> &componentIds,
                                  const QVector<WireId> &wireIds,
                                  const SelectionTransform &transform)
{
    const auto finite = [](QPointF p) { return std::isfinite(p.x()) && std::isfinite(p.y()); };
    if ((componentIds.isEmpty() && wireIds.isEmpty())
        || !finite(transform.displacement) || !finite(transform.pivot))
        return false;
    const int turns = (transform.quarterTurns % 4 + 4) % 4;
    switch (transform.kind) {
    case SelectionTransform::Kind::Translate:
    case SelectionTransform::Kind::Rotate:
    case SelectionTransform::Kind::FlipHorizontal:
    case SelectionTransform::Kind::FlipVertical: break;
    default: return false;
    }
    const auto map = [&](QPointF p) {
        if (transform.kind == SelectionTransform::Kind::Translate)
            return p + transform.displacement;
        QPointF offset = p - transform.pivot;
        if (transform.kind == SelectionTransform::Kind::Rotate)
            offset = rotateOffset(offset, static_cast<Rotation>(turns * 90));
        else if (transform.kind == SelectionTransform::Kind::FlipHorizontal)
            offset.setX(-offset.x());
        else
            offset.setY(-offset.y());
        return transform.pivot + offset;
    };
    QSet<ComponentId> selectedComponents;
    QSet<WireId> movingRoutes;
    for (ComponentId id : componentIds) {
        if (!component(id)) return false;
        selectedComponents.insert(id);
    }
    for (WireId id : wireIds) {
        if (!wire(id)) return false;
        movingRoutes.insert(id);
    }
    for (const WireRoute &route : m_wires) {
        if (isTerminal(route.start) && isTerminal(route.end)
            && selectedComponents.contains(std::get<TerminalRef>(route.start).component)
            && selectedComponents.contains(std::get<TerminalRef>(route.end).component))
            movingRoutes.insert(route.id);
    }

    Document candidate = *this;
    for (Component &item : candidate.m_components) {
        if (!selectedComponents.contains(item.id())) continue;
        item.setPosition(map(item.position()));
        int degrees = static_cast<int>(item.rotation());
        if (transform.kind == SelectionTransform::Kind::Rotate)
            degrees = (degrees + turns * 90) % 360;
        else if (transform.kind != SelectionTransform::Kind::Translate) {
            degrees = (360 + (transform.kind == SelectionTransform::Kind::FlipVertical ? 180 : 0)
                       - degrees) % 360;
            item.setMirrored(!item.mirrored());
        }
        item.setRotation(static_cast<Rotation>(degrees));
        if (!finite(item.position())) return false;
    }
    for (auto node = candidate.m_nodes.begin(); node != candidate.m_nodes.end(); ++node) {
        bool incident = false;
        bool allMoving = true;
        for (const WireRoute &route : m_wires) {
            if ((isNode(route.start) && std::get<NodeId>(route.start) == node.key())
                || (isNode(route.end) && std::get<NodeId>(route.end) == node.key())) {
                incident = true;
                allMoving &= movingRoutes.contains(route.id);
            }
        }
        if (incident && allMoving) node.value() = map(node.value());
        if (!finite(node.value())) return false;
    }
    for (WireRoute &route : candidate.m_wires) {
        if (movingRoutes.contains(route.id)) {
            for (QPointF &point : route.vertices) point = map(point);
        }
        QPointF start, end;
        if (!candidate.endpointPosition(route.start, &start)
            || !candidate.endpointPosition(route.end, &end)) return false;
        const bool startChanged = !samePoint(route.vertices.first(), start);
        const bool endChanged = !samePoint(route.vertices.last(), end);
        // Stage BOTH identity-derived endpoints before corner insertion or
        // validation. Intermediate single-end states need not be valid.
        auto &points = route.vertices;
        points.first() = start;
        points.last() = end;
        if (startChanged) {
            while (points.size() > 2 && samePoint(points[0], points[1])) points.removeAt(1);
        }
        if (endChanged) {
            while (points.size() > 2 && samePoint(points.last(), points[points.size() - 2]))
                points.removeAt(points.size() - 2);
        }
        if (startChanged && !near(points[0].x(), points[1].x())
            && !near(points[0].y(), points[1].y()))
            points.insert(1, QPointF(points[0].x(), points[1].y()));
        if (endChanged && !near(points.last().x(), points[points.size() - 2].x())
            && !near(points.last().y(), points[points.size() - 2].y()))
            points.insert(points.size() - 1, QPointF(points.last().x(), points[points.size() - 2].y()));
        if (validateRouteGeometry(points) != RouteGeometryError::None) return false;
    }
    m_components = std::move(candidate.m_components);
    m_nodes = std::move(candidate.m_nodes);
    m_wires = std::move(candidate.m_wires);
    return true;
}

bool Document::setComponentLabels(ComponentId id, const QString &reference, const QString &value)
{
    return editProperties({id}, {{PropertyEdit::Kind::SetValue, "reference", {}, reference},
                                 {PropertyEdit::Kind::SetValue, "value", {}, value}});
}

bool Document::editProperties(const QVector<ComponentId> &ids, const QVector<PropertyEdit> &edits,
                              QStringList *errors)
{
    const auto fail = [&](const QString &message) { if (errors) errors->append(message); return false; };
    if (ids.isEmpty() || edits.isEmpty()) return fail(QStringLiteral("No properties selected."));
    const QRegularExpression key("^[a-z][a-z0-9_]*$");
    for (const auto &edit : edits)
        if (!key.match(edit.id).hasMatch()) return fail(QStringLiteral("Invalid property ID."));
    QSet<ComponentId> targets;
    for (auto id : ids) {
        if (!component(id)) return fail(QStringLiteral("A selected component no longer exists."));
        targets.insert(id);
    }
    auto candidate = m_components;
    for (auto &item : candidate) {
        if (!targets.contains(item.id())) continue;
        auto properties = item.properties();
        for (const auto &edit : edits) {
            auto found = std::find_if(properties.begin(), properties.end(), [&](const InstanceProperty &p) { return p.id == edit.id; });
            const bool reserved = edit.id == "reference" || edit.id == "value";
            switch (edit.kind) {
            case PropertyEdit::Kind::Add:
                if (reserved || found != properties.end()) return fail(QStringLiteral("That property already exists or is reserved."));
                properties.append({edit.id, edit.name.trimmed(), edit.value.trimmed(), edit.visible});
                break;
            case PropertyEdit::Kind::Remove:
                if (reserved) return fail(QStringLiteral("Reference and Value cannot be removed."));
                if (found != properties.end()) properties.erase(found);
                break;
            case PropertyEdit::Kind::Rename:
                if (reserved || found == properties.end()) return fail(QStringLiteral("Only an existing custom property can be renamed."));
                found->name = edit.name.trimmed();
                break;
            case PropertyEdit::Kind::SetValue:
            case PropertyEdit::Kind::SetVisible:
                if (found == properties.end()) {
                    if (reserved) return fail(QStringLiteral("Missing reserved property."));
                    properties.append({edit.id, edit.name.isEmpty() ? edit.id : edit.name.trimmed(), {}, false});
                    found = properties.end() - 1;
                }
                if (edit.kind == PropertyEdit::Kind::SetValue) found->value = edit.value.trimmed();
                else found->visible = edit.visible;
                break;
            default: return fail(QStringLiteral("Unknown property operation."));
            }
        }
        const auto problems = validateInstanceProperties(properties);
        if (!problems.isEmpty()) return fail(problems.join('\n'));
        item.setProperties(properties);
    }
    QSet<QString> references;
    for (const auto &item : candidate) {
        if (references.contains(item.reference())) return fail(QStringLiteral("Reference must be unique among components."));
        references.insert(item.reference());
    }
    m_components = std::move(candidate);
    return true;
}

bool Document::removeComponent(ComponentId id)
{
    const auto componentIt = std::find_if(m_components.begin(), m_components.end(),
                                          [id](const Component &item) { return item.id() == id; });
    if (componentIt == m_components.end())
        return false;
    m_wires.erase(std::remove_if(m_wires.begin(), m_wires.end(), [id](const WireRoute &route) {
        return (isTerminal(route.start) && std::get<TerminalRef>(route.start).component == id)
            || (isTerminal(route.end) && std::get<TerminalRef>(route.end).component == id);
    }), m_wires.end());
    m_components.erase(componentIt);
    removeOrphanNodes();
    return true;
}

std::optional<WireId> Document::addWire(const WireEndpoint &start,
                                        const QVector<QPointF> &interiorVertices,
                                        const WireEndpoint &end)
{
    QPointF startPoint;
    QPointF endPoint;
    if (!endpointPosition(start, &startPoint) || !endpointPosition(end, &endPoint))
        return std::nullopt;

    QVector<QPointF> vertices;
    vertices.reserve(interiorVertices.size() + 2);
    vertices.append(startPoint);
    vertices += interiorVertices;
    vertices.append(endPoint);
    if (validateRouteGeometry(vertices) != RouteGeometryError::None)
        return std::nullopt;

    const WireId id = allocateWireId();
    if (id == kInvalidWireId)
        return std::nullopt;
    m_wires.append(WireRoute{id, vertices, start, end});
    return id;
}

bool Document::removeWire(WireId id)
{
    const auto it = std::find_if(m_wires.begin(), m_wires.end(),
                                 [id](const WireRoute &route) { return route.id == id; });
    if (it == m_wires.end())
        return false;
    m_wires.erase(it);
    removeOrphanNodes();
    return true;
}

std::optional<NodeId> Document::branchWireAt(WireId id, QPointF point)
{
    QVector<QPointF> leftPoints;
    QVector<QPointF> rightPoints;
    WireEndpoint originalStart;
    WireEndpoint originalEnd;
    if (!computeBranchSplit(id, point, &leftPoints, &rightPoints, &originalStart, &originalEnd))
        return std::nullopt;
    if (!idsAvailable(m_nextNodeId, 1) || !idsAvailable(m_nextWireId, 2))
        return std::nullopt;

    const NodeId node = m_nextNodeId++;
    const WireId leftId = allocateWireId();
    const WireId rightId = allocateWireId();
    commitBranchSplit(id, point, leftPoints, rightPoints, originalStart, originalEnd,
                      node, leftId, rightId);
    return node;
}

bool Document::computeBranchSplit(WireId id, QPointF point, QVector<QPointF> *leftPoints,
                                  QVector<QPointF> *rightPoints, WireEndpoint *originalStart,
                                  WireEndpoint *originalEnd) const
{
    const auto it = std::find_if(m_wires.cbegin(), m_wires.cend(),
                                 [id](const WireRoute &route) { return route.id == id; });
    if (it == m_wires.cend() || !std::isfinite(point.x()) || !std::isfinite(point.y()))
        return false;
    const WireRoute &original = *it;
    if (samePoint(point, original.vertices.first()) || samePoint(point, original.vertices.last()))
        return false;

    int segment = -1;
    bool atVertex = false;
    for (int i = 0; i < original.vertices.size() - 1; ++i) {
        if (pointOnSegment(point, original.vertices[i], original.vertices[i + 1])) {
            segment = i;
            atVertex = samePoint(point, original.vertices[i + 1]);
            break;
        }
    }
    if (segment < 0)
        return false;

    QVector<QPointF> left;
    QVector<QPointF> right;
    for (int i = 0; i <= segment; ++i)
        left.append(original.vertices[i]);
    left.append(point);
    right.append(point);
    const int rightStart = atVertex ? segment + 2 : segment + 1;
    for (int i = rightStart; i < original.vertices.size(); ++i)
        right.append(original.vertices[i]);
    if (validateRouteGeometry(left) != RouteGeometryError::None
        || validateRouteGeometry(right) != RouteGeometryError::None)
        return false;

    *leftPoints = left;
    *rightPoints = right;
    *originalStart = original.start;
    *originalEnd = original.end;
    return true;
}

void Document::commitBranchSplit(WireId id, QPointF point, const QVector<QPointF> &leftPoints,
                                 const QVector<QPointF> &rightPoints,
                                 const WireEndpoint &originalStart, const WireEndpoint &originalEnd,
                                 NodeId node, WireId leftId, WireId rightId)
{
    const auto it = std::find_if(m_wires.begin(), m_wires.end(),
                                 [id](const WireRoute &route) { return route.id == id; });
    Q_ASSERT(it != m_wires.end());
    const int wireIndex = static_cast<int>(std::distance(m_wires.begin(), it));
    const WireEndpoint nodeEndpoint = makeNodeEndpoint(node);
    WireRoute left{leftId, leftPoints, originalStart, nodeEndpoint};
    WireRoute right{rightId, rightPoints, nodeEndpoint, originalEnd};
    m_nodes.insert(node, point);
    m_wires.removeAt(wireIndex);
    m_wires.insert(wireIndex, right);
    m_wires.insert(wireIndex, left);
}

bool Document::idsAvailable(quint32 nextId, int count)
{
    if (count <= 0)
        return true;
    if (nextId == 0)
        return false;
    return nextId <= std::numeric_limits<quint32>::max() - static_cast<quint32>(count - 1);
}

std::optional<WireId> Document::addWireBranching(const PendingWireEndpoint &start,
                                                  const QVector<QPointF> &interiorVertices,
                                                  const PendingWireEndpoint &end)
{
    // Scope limit: branching twice from the same wire in one call
    // would require compounding-split bookkeeping (the second site's
    // segment index may shift once the first split is applied). Reject
    // rather than handle that edge case - the user can draw two separate
    // wires instead.
    if (isBranchSite(start) && isBranchSite(end)
        && std::get<BranchSite>(start).wire == std::get<BranchSite>(end).wire)
        return std::nullopt;

    struct ResolvedSite
    {
        WireId originalWire = kInvalidWireId;
        QPointF point;
        QVector<QPointF> leftPoints;
        QVector<QPointF> rightPoints;
        WireEndpoint originalStart;
        WireEndpoint originalEnd;
    };

    QPointF startPoint;
    QPointF endPoint;
    std::optional<ResolvedSite> startSite;
    std::optional<ResolvedSite> endSite;

    if (isBranchSite(start)) {
        const BranchSite &site = std::get<BranchSite>(start);
        ResolvedSite resolved;
        resolved.originalWire = site.wire;
        resolved.point = site.point;
        if (!computeBranchSplit(site.wire, site.point, &resolved.leftPoints,
                                &resolved.rightPoints, &resolved.originalStart,
                                &resolved.originalEnd))
            return std::nullopt;
        startPoint = site.point;
        startSite = resolved;
    } else if (!endpointPosition(std::get<WireEndpoint>(start), &startPoint)) {
        return std::nullopt;
    }

    if (isBranchSite(end)) {
        const BranchSite &site = std::get<BranchSite>(end);
        ResolvedSite resolved;
        resolved.originalWire = site.wire;
        resolved.point = site.point;
        if (!computeBranchSplit(site.wire, site.point, &resolved.leftPoints,
                                &resolved.rightPoints, &resolved.originalStart,
                                &resolved.originalEnd))
            return std::nullopt;
        endPoint = site.point;
        endSite = resolved;
    } else if (!endpointPosition(std::get<WireEndpoint>(end), &endPoint)) {
        return std::nullopt;
    }

    QVector<QPointF> vertices;
    vertices.reserve(interiorVertices.size() + 2);
    vertices.append(startPoint);
    vertices += interiorVertices;
    vertices.append(endPoint);
    if (validateRouteGeometry(vertices) != RouteGeometryError::None)
        return std::nullopt;

    // Pre-flight every ID this operation might need before mutating
    // anything - this is what makes the whole operation atomic even though
    // it can allocate up to 1 wire ID per branch site plus 1 for the new
    // wire itself.
    const int branchCount = (startSite ? 1 : 0) + (endSite ? 1 : 0);
    if (!idsAvailable(m_nextWireId, 1 + branchCount * 2) || !idsAvailable(m_nextNodeId, branchCount))
        return std::nullopt;

    NodeId startNode = kInvalidNodeId;
    NodeId endNode = kInvalidNodeId;
    if (startSite) {
        startNode = m_nextNodeId++;
        const WireId leftId = allocateWireId();
        const WireId rightId = allocateWireId();
        commitBranchSplit(startSite->originalWire, startSite->point, startSite->leftPoints,
                          startSite->rightPoints, startSite->originalStart,
                          startSite->originalEnd, startNode, leftId, rightId);
    }
    if (endSite) {
        endNode = m_nextNodeId++;
        const WireId leftId = allocateWireId();
        const WireId rightId = allocateWireId();
        commitBranchSplit(endSite->originalWire, endSite->point, endSite->leftPoints,
                          endSite->rightPoints, endSite->originalStart, endSite->originalEnd,
                          endNode, leftId, rightId);
    }

    const WireEndpoint resolvedStart = startSite ? makeNodeEndpoint(startNode)
                                                  : std::get<WireEndpoint>(start);
    const WireEndpoint resolvedEnd = endSite ? makeNodeEndpoint(endNode)
                                              : std::get<WireEndpoint>(end);
    // allocateWireId() cannot fail here: idsAvailable() already confirmed
    // 1 + branchCount*2 consecutive IDs were available before any were
    // consumed above.
    const WireId id = allocateWireId();
    m_wires.append(WireRoute{id, vertices, resolvedStart, resolvedEnd});
    return id;
}

QVector<Net> Document::computeNets() const
{
    QVector<WireEndpoint> identities;
    QVector<int> parent;
    auto ensure = [&identities, &parent](const WireEndpoint &endpoint) {
        for (int i = 0; i < identities.size(); ++i) {
            if (identities[i] == endpoint)
                return i;
        }
        identities.append(endpoint);
        parent.append(static_cast<int>(parent.size()));
        return static_cast<int>(parent.size() - 1);
    };
    auto root = [&parent](int index) {
        int result = index;
        while (parent[result] != result)
            result = parent[result];
        while (parent[index] != index) {
            const int next = parent[index];
            parent[index] = result;
            index = next;
        }
        return result;
    };

    for (const Component &item : m_components) {
        for (TerminalId terminal = 0; terminal < terminalCount(item); ++terminal)
            ensure(makeTerminalEndpoint(item.id(), terminal));
    }
    for (const WireRoute &route : m_wires) {
        const int a = ensure(route.start);
        const int b = ensure(route.end);
        const int rootA = root(a);
        const int rootB = root(b);
        parent[rootB] = rootA;
    }

    QVector<Net> nets;
    QVector<int> roots;
    for (int i = 0; i < identities.size(); ++i) {
        const int r = root(i);
        int netIndex = roots.indexOf(r);
        if (netIndex < 0) {
            roots.append(r);
            nets.append(Net{});
            netIndex = nets.size() - 1;
        }
        if (isTerminal(identities[i]))
            nets[netIndex].terminals.append(std::get<TerminalRef>(identities[i]));
    }
    return nets;
}

QVector<QPointF> Document::junctionPoints() const
{
    QVector<WireEndpoint> endpoints;
    QVector<int> counts;
    for (const WireRoute &route : m_wires) {
        for (const WireEndpoint *endpoint : {&route.start, &route.end}) {
            int index = 0;
            while (index < endpoints.size() && endpoints[index] != *endpoint)
                ++index;
            if (index == endpoints.size()) {
                endpoints.append(*endpoint);
                counts.append(0);
            }
            ++counts[index];
        }
    }
    QVector<QPointF> points;
    for (int i = 0; i < endpoints.size(); ++i) {
        if (counts[i] < 3)
            continue;
        QPointF position;
        if (endpointPosition(endpoints[i], &position))
            points.append(position);
    }
    return points;
}

bool Document::endpointPosition(const WireEndpoint &endpoint, QPointF *position) const
{
    if (isTerminal(endpoint)) {
        const TerminalRef ref = std::get<TerminalRef>(endpoint);
        const Component *item = component(ref.component);
        if (!item || ref.terminal < 0 || ref.terminal >= terminalCount(*item))
            return false;
        *position = terminalPosition(*item, ref.terminal);
        return true;
    }
    const auto it = m_nodes.constFind(std::get<NodeId>(endpoint));
    if (it == m_nodes.cend())
        return false;
    *position = it.value();
    return true;
}

bool Document::endpointExists(const WireEndpoint &endpoint) const
{
    QPointF ignored;
    return endpointPosition(endpoint, &ignored);
}

void Document::removeOrphanNodes()
{
    QHash<NodeId, int> references;
    for (const WireRoute &route : m_wires) {
        if (isNode(route.start)) ++references[std::get<NodeId>(route.start)];
        if (isNode(route.end)) ++references[std::get<NodeId>(route.end)];
    }
    for (auto it = m_nodes.begin(); it != m_nodes.end();) {
        if (references.value(it.key()) == 0)
            it = m_nodes.erase(it);
        else
            ++it;
    }
}

WireId Document::allocateWireId()
{
    if (m_nextWireId == kInvalidWireId)
        return kInvalidWireId;
    return m_nextWireId++;
}

// Presentation-only edit: validate all targets before changing any instance.
bool Document::setPropertyBlockOffset(const QVector<ComponentId> &ids, std::optional<QPointF> offset)
{
    if (ids.isEmpty() || (offset && (!std::isfinite(offset->x()) || !std::isfinite(offset->y())))) return false;
    for (auto id : ids) if (!component(id)) return false;
    bool changed = false;
    for (auto &item : m_components) if (ids.contains(item.id()) && item.propertyBlockOffset() != offset) {
        item.setPropertyBlockOffset(offset); changed = true;
    }
    return changed;
}
