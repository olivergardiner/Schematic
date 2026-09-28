#include "document.h"

#include <algorithm>
#include <cmath>
#include <limits>

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

ComponentId Document::addComponent(SymbolKind kind, QPointF position, Rotation rotation)
{
    if (m_nextComponentId == kInvalidComponentId)
        return kInvalidComponentId;
    const ComponentId id = m_nextComponentId++;
    Component item(id, kind, position);
    item.setRotation(rotation);
    item.setReference(QStringLiteral("%1%2").arg(symbolKindName(kind)).arg(id));
    m_components.append(item);
    return id;
}

bool Document::moveComponent(ComponentId id, QPointF position)
{
    auto it = std::find_if(m_components.begin(), m_components.end(),
                           [id](const Component &item) { return item.id() == id; });
    if (it == m_components.end() || !std::isfinite(position.x()) || !std::isfinite(position.y()))
        return false;

    Component moved = *it;
    moved.setPosition(position);
    QVector<WireRoute> candidateWires = m_wires;
    for (WireRoute &route : candidateWires) {
        if (isTerminal(route.start) && std::get<TerminalRef>(route.start).component == id) {
            const TerminalRef ref = std::get<TerminalRef>(route.start);
            if (!moveRouteEndpoint(&route, true, moved.terminalPosition(ref.terminal)))
                return false;
        }
        if (isTerminal(route.end) && std::get<TerminalRef>(route.end).component == id) {
            const TerminalRef ref = std::get<TerminalRef>(route.end);
            if (!moveRouteEndpoint(&route, false, moved.terminalPosition(ref.terminal)))
                return false;
        }
    }

    *it = moved;
    m_wires = std::move(candidateWires);
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
    const auto it = std::find_if(m_wires.begin(), m_wires.end(),
                                 [id](const WireRoute &route) { return route.id == id; });
    if (it == m_wires.end() || !std::isfinite(point.x()) || !std::isfinite(point.y()))
        return std::nullopt;
    const int wireIndex = static_cast<int>(std::distance(m_wires.begin(), it));
    const WireRoute original = *it;
    if (samePoint(point, original.vertices.first()) || samePoint(point, original.vertices.last()))
        return std::nullopt;

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
        return std::nullopt;

    QVector<QPointF> leftPoints;
    QVector<QPointF> rightPoints;
    for (int i = 0; i <= segment; ++i)
        leftPoints.append(original.vertices[i]);
    leftPoints.append(point);
    rightPoints.append(point);
    const int rightStart = atVertex ? segment + 2 : segment + 1;
    for (int i = rightStart; i < original.vertices.size(); ++i)
        rightPoints.append(original.vertices[i]);
    if (validateRouteGeometry(leftPoints) != RouteGeometryError::None
        || validateRouteGeometry(rightPoints) != RouteGeometryError::None)
        return std::nullopt;
    if (m_nextNodeId == kInvalidNodeId)
        return std::nullopt;

    const NodeId node = m_nextNodeId++;
    const WireEndpoint nodeEndpoint = makeNodeEndpoint(node);
    const WireId leftId = allocateWireId();
    const WireId rightId = allocateWireId();
    if (leftId == kInvalidWireId || rightId == kInvalidWireId)
        return std::nullopt;
    WireRoute left{leftId, leftPoints, original.start, nodeEndpoint};
    WireRoute right{rightId, rightPoints, nodeEndpoint, original.end};
    m_nodes.insert(node, point);
    m_wires.removeAt(wireIndex);
    m_wires.insert(wireIndex, right);
    m_wires.insert(wireIndex, left);
    return node;
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
        for (TerminalId terminal = 0; terminal < item.terminalCount(); ++terminal)
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
        if (!item || ref.terminal < 0 || ref.terminal >= item->terminalCount())
            return false;
        *position = item->terminalPosition(ref.terminal);
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
