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
    Document candidate = *this;
    const bool stored = candidate.m_symbols.find(definition.id)
        ? candidate.m_symbols.replace(definition, errors) : candidate.m_symbols.add(definition, errors);
    if (!stored || !candidate.ensureLayoutClosure(definition.id, errors)) return false;
    for (const auto &item : candidate.m_components)
        if (!candidate.ensureLayoutClosure(item.symbolId(), errors)) return false;
    m_symbols = std::move(candidate.m_symbols);
    return true;
}

bool Document::ensureLayoutClosure(const SymbolId &id, QStringList *errors)
{
    QStringList pending{id.value()};
    QSet<QString> visited;
    while (!pending.isEmpty()) {
        const auto key = pending.takeLast();
        if (visited.contains(key)) continue;
        visited.insert(key);
        if (!m_symbols.find(SymbolId(key))) {
            const auto *installed = applicationSymbolLibrary().find(SymbolId(key));
            if (!installed || !m_symbols.add(*installed, errors)) {
                if (errors && !installed) errors->append("Install the missing layout alternative: " + key);
                return false;
            }
        }
        const auto controls = m_symbols.find(SymbolId(key))->layoutControls;
        for (const auto &control : controls) for (const auto &choice : control.choices)
            pending.append(choice.symbolId.value());
    }
    QStringList problems;
    m_symbols.layoutClosure({id.value()}, &problems);
    if (errors) errors->append(problems);
    return problems.isEmpty();
}

bool Document::canSetSymbolLayout(ComponentId id, const QString &controlKey, const QString &choiceKey) const
{
    const auto *item = component(id);
    const auto *source = item ? symbol(item->symbolId()) : nullptr;
    const auto *control = source ? symbolLayoutControl(*source, controlKey) : nullptr;
    if (!control) return false;
    for (const auto &choice : control->choices) if (choice.key == choiceKey) {
        const auto *target = symbol(choice.symbolId);
        if (!target) target = applicationSymbolLibrary().find(choice.symbolId);
        if (!target) return false;
        QStringList errors;
        layoutTerminalMapping(*source, *control, choice, *target, &errors);
        return errors.isEmpty();
    }
    return false;
}

bool Document::setSymbolLayout(ComponentId id, const QString &controlKey, const QString &choiceKey, QStringList *errors)
{
    const auto fail = [&](const QString &message) { if (errors) errors->append(message); return false; };
    const auto *item = component(id);
    const auto *source = item ? symbol(item->symbolId()) : nullptr;
    const auto *control = source ? symbolLayoutControl(*source, controlKey) : nullptr;
    if (!control) return fail("This component has no such layout control.");
    const SymbolLayoutChoice *choice = nullptr;
    for (const auto &option : control->choices) if (option.key == choiceKey) choice = &option;
    if (!choice) return fail("Unknown layout choice.");
    if (control->current == choiceKey) return true;
    Document candidate = *this;
    if (!candidate.ensureLayoutClosure(source->id, errors)) return false;
    const auto *target = candidate.symbol(choice->symbolId);
    if (!target) return fail("The layout alternative is not available.");
    const auto mapping = layoutTerminalMapping(*source, *control, *choice, *target, errors);
    if (mapping.isEmpty()) return false;
    Component changed = *item;
    changed.m_symbolId = target->id;
    for (auto &route : candidate.m_wires) {
        for (auto *endpoint : {&route.start, &route.end}) {
            if (isTerminal(*endpoint)) {
                auto &ref = std::get<TerminalRef>(*endpoint);
                if (ref.component == id) ref.terminal = mapping[ref.terminal];
            }
        }
    }
    if (!candidate.commitComponentChange(id, changed))
        return fail("The connected wire route could not be repaired. Adjust the wire and try again.");
    *this = std::move(candidate);
    return true;
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
    if (m_nextComponentId == kInvalidComponentId) return kInvalidComponentId;
    const auto before = m_symbols;
    QStringList errors;
    if (!ensureLayoutClosure(symbolId, &errors)) { m_symbols = before; return kInvalidComponentId; }
    const auto *symbol = m_symbols.find(symbolId);
    const ComponentId id = m_nextComponentId++;
    Component item(id, symbolId, position);
    item.setRotation(rotation);
    QSet<QString> used;
    for (const Component &existing : m_components)
        used.insert(existing.reference());
    item.setReference(firstUnusedReference(symbol->referencePrefix, used));
    auto properties = item.properties();
    properties[0].visible = symbol->showsReference;
    properties[1].value = symbol->defaultValue;
    item.setProperties(properties);
    item.setPropertyBlockJustification(symbol->defaultPropertyJustification);
    m_components.append(item);
    return id;
}

ComponentId Document::placeComponent(const SymbolId &symbolId, QPointF position,
                                      Rotation rotation, QStringList *warnings)
{
    if (!std::isfinite(position.x()) || !std::isfinite(position.y()))
        return kInvalidComponentId;
    Document candidate = *this;
    const ComponentId id = candidate.addComponent(symbolId, position, rotation);
    if (id == kInvalidComponentId) return id;
    const Component placed = *candidate.component(id);
    QStringList skipped;
    for (TerminalId t = 0; t < candidate.terminalCount(placed); ++t) {
        const QPointF point = candidate.terminalPosition(placed, t);
        const WireEndpoint terminal = makeTerminalEndpoint(id, t);
        QVector<WireId> hits;
        std::optional<WireEndpoint> shared;
        bool ambiguous = false;
        for (const auto &route : candidate.m_wires) {
            bool touches = false;
            for (int i = 0; i + 1 < route.vertices.size(); ++i) {
                if (pointOnSegment(point, route.vertices[i], route.vertices[i + 1])) {
                    touches = true;
                    break;
                }
            }
            if (!touches) continue;
            std::optional<WireEndpoint> endpoint;
            if (samePoint(point, route.vertices.first())) endpoint = route.start;
            else if (samePoint(point, route.vertices.last())) endpoint = route.end;
            // Several legs are one site only when they already share the
            // same endpoint identity. Coordinate crossings remain separate.
            if (hits.isEmpty()) shared = endpoint;
            else if (!shared || !endpoint || *shared != *endpoint) ambiguous = true;
            hits.append(route.id);
        }
        if (hits.isEmpty()) continue;
        if (ambiguous || (shared && isTerminal(*shared))) {
            skipped.append(QStringLiteral("Terminal %1 was left unconnected: %2.")
                .arg(candidate.symbol(symbolId)->terminals[t].name,
                     ambiguous ? QStringLiteral("more than one wire connection touches it")
                               : QStringLiteral("the point is occupied by another component terminal")));
            continue;
        }
        if (shared) {
            // Replace the bare node with the placed terminal identity. All
            // its legs retain their routes, IDs and electrical connectivity.
            const NodeId node = std::get<NodeId>(*shared);
            for (auto &route : candidate.m_wires) {
                if (route.start == *shared) { route.start = terminal; route.vertices.first() = point; }
                if (route.end == *shared) { route.end = terminal; route.vertices.last() = point; }
            }
            candidate.m_nodes.remove(node);
        } else {
            QVector<QPointF> left, right;
            WireEndpoint start, end;
            if (!candidate.computeBranchSplit(hits.first(), point, &left, &right, &start, &end)
                || !idsAvailable(candidate.m_nextWireId, 2))
                return kInvalidComponentId;
            const WireId leftId = candidate.allocateWireId();
            const WireId rightId = candidate.allocateWireId();
            const auto it = std::find_if(candidate.m_wires.begin(), candidate.m_wires.end(),
                [&](const WireRoute &route) { return route.id == hits.first(); });
            const int index = static_cast<int>(std::distance(candidate.m_wires.begin(), it));
            candidate.m_wires.removeAt(index);
            candidate.m_wires.insert(index, WireRoute{rightId, right, terminal, end});
            candidate.m_wires.insert(index, WireRoute{leftId, left, start, terminal});
        }
    }
    for (const auto &route : candidate.m_wires)
        if (validateRouteGeometry(route.vertices) != RouteGeometryError::None)
            return kInvalidComponentId;
    *this = std::move(candidate);
    if (warnings) warnings->append(skipped);
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
                                  const SelectionTransform &transform, const QVector<NoteId> &noteIds)
{
    const auto finite = [](QPointF p) { return std::isfinite(p.x()) && std::isfinite(p.y()); };
    if ((componentIds.isEmpty() && wireIds.isEmpty() && noteIds.isEmpty())
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
    for (NoteId id : noteIds) if (!note(id)) return false;
    for (auto &item : candidate.m_notes) {
        if (!noteIds.contains(item.id)) continue;
        item.position = map(item.position);
        if (!finite(item.position) || !std::isfinite(item.position.x() + item.width)) return false;
    }
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
    m_notes = std::move(candidate.m_notes);
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
    QSet<TerminalId> used;
    for (const auto &route : m_wires)
        for (const auto &endpoint : {route.start, route.end})
            if (isTerminal(endpoint) && std::get<TerminalRef>(endpoint).component == id)
                used.insert(std::get<TerminalRef>(endpoint).terminal);
    if (!idsAvailable(m_nextNodeId, used.size())) return false;
    QHash<TerminalId, NodeId> detached;
    // Allocate in terminal order, independently of route traversal order.
    for (TerminalId t = 0; t < terminalCount(*componentIt); ++t) {
        if (!used.contains(t)) continue;
        const NodeId node = m_nextNodeId++;
        detached.insert(t, node);
        m_nodes.insert(node, terminalPosition(*componentIt, t));
    }
    for (auto &route : m_wires) {
        for (auto *endpoint : {&route.start, &route.end})
            if (isTerminal(*endpoint) && std::get<TerminalRef>(*endpoint).component == id)
                *endpoint = makeNodeEndpoint(detached.value(std::get<TerminalRef>(*endpoint).terminal));
    }
    m_components.erase(componentIt);
    removeOrphanNodes();
    return true;
}

bool Document::removeSelection(const QVector<ComponentId> &components, const QVector<WireId> &wires,
                                const QVector<NoteId> &notes)
{
    if (components.isEmpty() && wires.isEmpty() && notes.isEmpty()) return false;
    Document candidate = *this;
    for (WireId id : wires) {
        if (!wire(id)) return false;
        if (candidate.wire(id) && !candidate.removeWire(id)) return false;
    }
    for (ComponentId id : components) {
        if (!component(id)) return false;
        if (candidate.component(id) && !candidate.removeComponent(id)) return false;
    }
    for (NoteId id : notes) {
        if (!note(id)) return false;
        if (candidate.note(id) && !candidate.removeNote(id)) return false;
    }
    *this = std::move(candidate);
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

bool Document::moveWireSegment(WireId id, int segment, qreal coordinate)
{
    auto it = std::find_if(m_wires.begin(), m_wires.end(),
                          [id](const WireRoute &route) { return route.id == id; });
    if (it == m_wires.end() || segment < 0 || segment >= it->vertices.size() - 1
        || !std::isfinite(coordinate))
        return false;

    const auto &original = it->vertices;
    const bool horizontal = near(original[segment].y(), original[segment + 1].y());
    if (near(coordinate, horizontal ? original[segment].y() : original[segment].x()))
        return false;
    // Saved routes may contain redundant vertices along one straight run.
    // Treat that run as one segment rather than creating diagonal neighbours.
    int first = segment, last = segment + 1;
    const auto onAxis = [horizontal](QPointF a, QPointF b) {
        return horizontal ? near(a.y(), b.y()) : near(a.x(), b.x());
    };
    while (first > 0 && onAxis(original[first - 1], original[first])) --first;
    while (last + 1 < original.size() && onAxis(original[last], original[last + 1])) ++last;
    QPointF a = original[first], b = original[last];
    if (horizontal) { a.setY(coordinate); b.setY(coordinate); }
    else { a.setX(coordinate); b.setX(coordinate); }

    // Interior corners move with the segment. Only fixed route endpoints
    // need an extra perpendicular connector at the start/end.
    QVector<QPointF> points;
    for (int i = 0; i < first; ++i) points.append(original[i]);
    if (first == 0) points.append(original.first());
    points.append(a);
    points.append(b);
    if (last == original.size() - 1) points.append(original.last());
    for (int i = last + 1; i < original.size(); ++i) points.append(original[i]);

    // Collapse vanishing neighbours and same-direction collinear corners.
    // Never normalize a reversal into a different path: reject it instead.
    QVector<QPointF> cleaned;
    for (const QPointF &point : points) {
        if (!cleaned.isEmpty() && samePoint(cleaned.last(), point)) continue;
        while (cleaned.size() >= 2) {
            const QPointF u = cleaned.last() - cleaned[cleaned.size() - 2];
            const QPointF v = point - cleaned.last();
            const bool collinear = (near(u.x(), 0) && near(v.x(), 0))
                                || (near(u.y(), 0) && near(v.y(), 0));
            if (!collinear || QPointF::dotProduct(u, v) <= 0) break;
            cleaned.removeLast();
        }
        cleaned.append(point);
    }
    if (validateRouteGeometry(cleaned) != RouteGeometryError::None
        || !samePoint(cleaned.first(), original.first())
        || !samePoint(cleaned.last(), original.last()))
        return false;
    it->vertices = std::move(cleaned);
    return true;
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

std::optional<WireId> Document::addWireToPoint(const PendingWireEndpoint &start,
                                              const QVector<QPointF> &interiorVertices, QPointF end)
{
    if (!std::isfinite(end.x()) || !std::isfinite(end.y()) || !idsAvailable(m_nextNodeId, 1))
        return std::nullopt;
    Document candidate = *this;
    const NodeId node = candidate.m_nextNodeId++;
    candidate.m_nodes.insert(node, end);
    const auto result = candidate.addWireBranching(start, interiorVertices, makeNodeEndpoint(node));
    if (!result) return std::nullopt;
    *this = std::move(candidate);
    return result;
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
        // A component terminal contributes its own symbol lead as well as
        // the incident route ends. Bare nodes need three wire legs.
        if (counts[i] + (isTerminal(endpoints[i]) ? 1 : 0) < 3)
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
bool Document::setPropertyBlockJustification(const QVector<ComponentId> &ids, PropertyJustification value)
{
    if (ids.isEmpty() || propertyJustificationName(value).isEmpty()) return false;
    for (auto id : ids) if (!component(id)) return false;
    bool changed = false;
    for (auto &item : m_components) if (ids.contains(item.id()) && item.propertyBlockJustification() != value) {
        item.setPropertyBlockJustification(value); changed = true;
    }
    return changed;
}

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
