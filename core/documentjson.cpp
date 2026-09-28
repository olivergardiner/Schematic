#include "document.h"
#include "documentloadresult.h"

#include "symboldefinition.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QSet>

#include <cmath>
#include <limits>

namespace {

bool number(const QJsonValue &value, qreal *out)
{
    if (!value.isDouble()) return false;
    const double v = value.toDouble();
    if (!std::isfinite(v)) return false;
    *out = v;
    return true;
}

bool idValue(const QJsonValue &value, quint32 *out)
{
    qreal n = 0;
    if (!number(value, &n) || n < 1 || n > std::numeric_limits<quint32>::max()
        || std::floor(n) != n) return false;
    *out = static_cast<quint32>(n);
    return true;
}

QJsonObject endpointJson(const WireEndpoint &endpoint)
{
    if (isTerminal(endpoint)) {
        const TerminalRef ref = std::get<TerminalRef>(endpoint);
        return {{QStringLiteral("component"), static_cast<double>(ref.component)},
                {QStringLiteral("terminal"), ref.terminal}};
    }
    return {{QStringLiteral("node"), static_cast<double>(std::get<NodeId>(endpoint))}};
}

} // namespace

QJsonObject Document::toJson() const
{
    QJsonObject root;
    root.insert(QStringLiteral("formatVersion"), 1);
    root.insert(QStringLiteral("grid"), QJsonObject{{QStringLiteral("spacing"), m_gridSpacing}});
    QJsonArray components;
    for (const Component &item : m_components) {
        components.append(QJsonObject{{QStringLiteral("id"), static_cast<double>(item.id())},
            {QStringLiteral("kind"), symbolKindName(item.kind())},
            {QStringLiteral("x"), item.position().x()}, {QStringLiteral("y"), item.position().y()},
            {QStringLiteral("rotation"), static_cast<int>(item.rotation())},
            {QStringLiteral("reference"), item.reference()}, {QStringLiteral("value"), item.value()}});
    }
    root.insert(QStringLiteral("components"), components);
    QJsonArray nodes;
    for (auto it = m_nodes.cbegin(); it != m_nodes.cend(); ++it)
        nodes.append(QJsonObject{{QStringLiteral("id"), static_cast<double>(it.key())},
            {QStringLiteral("x"), it.value().x()}, {QStringLiteral("y"), it.value().y()}});
    root.insert(QStringLiteral("nodes"), nodes);
    QJsonArray wires;
    for (const WireRoute &route : m_wires) {
        QJsonArray points;
        for (const QPointF &point : route.vertices)
            points.append(QJsonArray{point.x(), point.y()});
        wires.append(QJsonObject{{QStringLiteral("id"), static_cast<double>(route.id)},
            {QStringLiteral("points"), points}, {QStringLiteral("start"), endpointJson(route.start)},
            {QStringLiteral("end"), endpointJson(route.end)}});
    }
    root.insert(QStringLiteral("wires"), wires);
    return root;
}

QByteArray Document::toJsonBytes() const
{
    return QJsonDocument(toJson()).toJson(QJsonDocument::Indented);
}

DocumentLoadResult Document::fromJsonBytes(const QByteArray &data)
{
    QJsonParseError parseError;
    const QJsonDocument parsed = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError || !parsed.isObject())
        return {{}, {QStringLiteral("Invalid JSON document: %1").arg(parseError.errorString())}, {}};
    return fromJson(parsed.object());
}

DocumentLoadResult Document::fromJson(const QJsonObject &root)
{
    DocumentLoadResult result;
    if (!root.value(QStringLiteral("formatVersion")).isDouble()
        || root.value(QStringLiteral("formatVersion")).toInt(-1) != 1) {
        result.errors.append(QStringLiteral("Unsupported or missing formatVersion (expected 1)"));
        return result;
    }
    auto error = [&result](const QString &message) { result.errors.append(message); };
    const QJsonValue gridValue = root.value(QStringLiteral("grid"));
    qreal spacing = 0;
    if (!gridValue.isObject() || !number(gridValue.toObject().value(QStringLiteral("spacing")), &spacing)
        || spacing <= 0)
        error(QStringLiteral("grid.spacing must be a finite positive number"));

    struct ParsedComponent { ComponentId id; SymbolKind kind; QPointF position; Rotation rotation; QString reference; QString value; };
    QVector<ParsedComponent> parsedComponents;
    QHash<ComponentId, SymbolKind> kinds;
    QSet<ComponentId> componentIds;
    const QJsonValue componentValue = root.value(QStringLiteral("components"));
    if (!componentValue.isArray()) error(QStringLiteral("components must be an array"));
    else {
        int index = 0;
        for (const QJsonValue &value : componentValue.toArray()) {
            const QString path = QStringLiteral("components[%1]").arg(index++);
            if (!value.isObject()) { error(path + QStringLiteral(" must be an object")); continue; }
            const QJsonObject obj = value.toObject();
            quint32 id = 0; qreal x = 0, y = 0;
            const auto kind = symbolKindFromName(obj.value(QStringLiteral("kind")).toString());
            const int rot = obj.value(QStringLiteral("rotation")).toInt(-1);
            bool valid = true;
            if (!idValue(obj.value(QStringLiteral("id")), &id) || componentIds.contains(id)) { error(path + QStringLiteral(" has an invalid or duplicate id")); valid = false; }
            if (!kind) { error(path + QStringLiteral(" has an unknown kind")); valid = false; }
            if (!number(obj.value(QStringLiteral("x")), &x) || !number(obj.value(QStringLiteral("y")), &y)) { error(path + QStringLiteral(" coordinates must be finite numbers")); valid = false; }
            if (rot != 0 && rot != 90 && rot != 180 && rot != 270) { error(path + QStringLiteral(" has an invalid rotation")); valid = false; }
            const QJsonValue reference = obj.value(QStringLiteral("reference"));
            const QJsonValue valueField = obj.value(QStringLiteral("value"));
            if ((obj.contains(QStringLiteral("reference")) && !reference.isString())
                || (obj.contains(QStringLiteral("value")) && !valueField.isString())) {
                error(path + QStringLiteral(" reference and value must be strings when present")); valid = false;
            }
            if (!valid) continue;
            componentIds.insert(id); kinds.insert(id, *kind);
            parsedComponents.append({id, *kind, {x, y}, static_cast<Rotation>(rot), reference.toString(), valueField.toString()});
        }
    }

    struct ParsedNode { NodeId id; QPointF point; };
    QVector<ParsedNode> parsedNodes;
    QHash<NodeId, QPointF> nodePositions;
    QSet<NodeId> nodeIds;
    const QJsonValue nodeValue = root.value(QStringLiteral("nodes"));
    if (!nodeValue.isArray()) error(QStringLiteral("nodes must be an array"));
    else {
        int index = 0;
        for (const QJsonValue &value : nodeValue.toArray()) {
            const QString path = QStringLiteral("nodes[%1]").arg(index++);
            if (!value.isObject()) { error(path + QStringLiteral(" must be an object")); continue; }
            const QJsonObject obj = value.toObject(); quint32 id = 0; qreal x = 0, y = 0;
            if (!idValue(obj.value(QStringLiteral("id")), &id) || nodeIds.contains(id)) { error(path + QStringLiteral(" has an invalid or duplicate id")); continue; }
            if (!number(obj.value(QStringLiteral("x")), &x) || !number(obj.value(QStringLiteral("y")), &y)) { error(path + QStringLiteral(" coordinates must be finite numbers")); continue; }
            nodeIds.insert(id); nodePositions.insert(id, {x, y}); parsedNodes.append({id, {x, y}});
        }
    }

    struct ParsedWire { WireId id; QVector<QPointF> points; WireEndpoint start; WireEndpoint end; };
    QVector<ParsedWire> parsedWires;
    QSet<WireId> wireIds;
    const QJsonValue wireValue = root.value(QStringLiteral("wires"));
    if (!wireValue.isArray()) error(QStringLiteral("wires must be an array"));
    else {
        int index = 0;
        for (const QJsonValue &value : wireValue.toArray()) {
            const QString path = QStringLiteral("wires[%1]").arg(index++);
            if (!value.isObject()) { error(path + QStringLiteral(" must be an object")); continue; }
            const QJsonObject obj = value.toObject(); quint32 id = 0;
            bool valid = true;
            if (!idValue(obj.value(QStringLiteral("id")), &id) || wireIds.contains(id)) { error(path + QStringLiteral(" has an invalid or duplicate id")); valid = false; }
            QVector<QPointF> points;
            const QJsonValue pointsValue = obj.value(QStringLiteral("points"));
            if (!pointsValue.isArray() || pointsValue.toArray().size() < 2) { error(path + QStringLiteral(" points must contain at least two coordinate pairs")); valid = false; }
            else for (const QJsonValue &pointValue : pointsValue.toArray()) {
                qreal x = 0, y = 0;
                if (!pointValue.isArray() || pointValue.toArray().size() != 2 || !number(pointValue.toArray()[0], &x) || !number(pointValue.toArray()[1], &y)) { error(path + QStringLiteral(" contains an invalid coordinate pair")); valid = false; break; }
                points.append({x, y});
            }
            auto parseEndpoint = [&](const QJsonValue &endpointValue, WireEndpoint *endpoint) {
                if (!endpointValue.isObject()) return false;
                const QJsonObject ep = endpointValue.toObject();
                const bool hasComponent = ep.contains(QStringLiteral("component"));
                const bool hasTerminal = ep.contains(QStringLiteral("terminal"));
                const bool hasNode = ep.contains(QStringLiteral("node"));
                if (hasNode && !hasComponent && !hasTerminal) {
                    quint32 node = 0;
                    if (idValue(ep.value(QStringLiteral("node")), &node) && nodeIds.contains(node)) { *endpoint = makeNodeEndpoint(node); return true; }
                } else if (hasComponent && hasTerminal && !hasNode) {
                    quint32 component = 0; qreal terminalNumber = 0;
                    if (idValue(ep.value(QStringLiteral("component")), &component) && number(ep.value(QStringLiteral("terminal")), &terminalNumber)
                        && std::floor(terminalNumber) == terminalNumber && terminalNumber <= std::numeric_limits<int>::max()
                        && kinds.contains(component) && terminalNumber >= 0 && terminalNumber < symbolTerminalCount(kinds.value(component))) {
                        *endpoint = makeTerminalEndpoint(component, static_cast<int>(terminalNumber)); return true;
                    }
                }
                return false;
            };
            WireEndpoint start, end;
            if (!parseEndpoint(obj.value(QStringLiteral("start")), &start)) { error(path + QStringLiteral(" has an invalid or unresolved start endpoint")); valid = false; }
            if (!parseEndpoint(obj.value(QStringLiteral("end")), &end)) { error(path + QStringLiteral(" has an invalid or unresolved end endpoint")); valid = false; }
            if (valid) { wireIds.insert(id); parsedWires.append({id, points, start, end}); }
        }
    }
    if (!result.errors.isEmpty()) return result;

    Document doc;
    doc.m_gridSpacing = spacing;
    quint32 maxComponent = 0, maxNode = 0, maxWire = 0;
    for (const ParsedComponent &item : parsedComponents) {
        Component component(item.id, item.kind, item.position);
        component.setRotation(item.rotation); component.setReference(item.reference); component.setValue(item.value);
        doc.m_components.append(component); maxComponent = qMax(maxComponent, item.id);
    }
    for (const ParsedNode &node : parsedNodes) { doc.m_nodes.insert(node.id, node.point); maxNode = qMax(maxNode, node.id); }
    auto endpointPosition = [&doc](const WireEndpoint &endpoint) {
        QPointF point;
        if (isTerminal(endpoint)) {
            const TerminalRef ref = std::get<TerminalRef>(endpoint);
            point = doc.component(ref.component)->terminalPosition(ref.terminal);
        } else point = doc.m_nodes.value(std::get<NodeId>(endpoint));
        return point;
    };
    for (ParsedWire &wire : parsedWires) {
        const QPointF start = endpointPosition(wire.start), end = endpointPosition(wire.end);
        auto differs = [](QPointF a, QPointF b) { return std::abs(a.x()-b.x()) > kRouteGeometryEpsilon || std::abs(a.y()-b.y()) > kRouteGeometryEpsilon; };
        if (differs(wire.points.first(), start)) result.warnings.append(QStringLiteral("Wire %1's start point was recalculated from its endpoint identity").arg(wire.id));
        if (differs(wire.points.last(), end)) result.warnings.append(QStringLiteral("Wire %1's end point was recalculated from its endpoint identity").arg(wire.id));
        wire.points.first() = start; wire.points.last() = end;
        if (validateRouteGeometry(wire.points) != RouteGeometryError::None) {
            error(QStringLiteral("Wire %1 has invalid route geometry after endpoint resolution").arg(wire.id));
            continue;
        }
        doc.m_wires.append({wire.id, wire.points, wire.start, wire.end}); maxWire = qMax(maxWire, wire.id);
    }
    if (!result.errors.isEmpty()) return result;
    QSet<NodeId> referenced;
    for (const WireRoute &wire : doc.m_wires) {
        if (isNode(wire.start)) referenced.insert(std::get<NodeId>(wire.start));
        if (isNode(wire.end)) referenced.insert(std::get<NodeId>(wire.end));
    }
    for (const ParsedNode &node : parsedNodes) if (!referenced.contains(node.id)) {
        doc.m_nodes.remove(node.id);
        result.warnings.append(QStringLiteral("Unreferenced node %1 was discarded").arg(node.id));
    }
    doc.m_nextComponentId = maxComponent == std::numeric_limits<quint32>::max() ? kInvalidComponentId : maxComponent + 1;
    doc.m_nextNodeId = maxNode == std::numeric_limits<quint32>::max() ? kInvalidNodeId : maxNode + 1;
    doc.m_nextWireId = maxWire == std::numeric_limits<quint32>::max() ? kInvalidWireId : maxWire + 1;
    result.document = std::move(doc);
    return result;
}
