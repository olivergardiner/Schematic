#include "document.h"
#include "documentloadresult.h"
#include <QJsonDocument>
#include <QSet>
#include <algorithm>
#include <cmath>

QByteArray Document::copyFragment(const QVector<ComponentId> &components,
                                  const QVector<WireId> &wires, QStringList *errors) const
{
    const auto fail = [&](const QString &message) { if (errors) errors->append(message); return QByteArray{}; };
    if (components.isEmpty() && wires.isEmpty()) return fail("Select components or wires to copy.");
    for (auto id : components) if (!component(id)) return fail("A selected component no longer exists.");
    for (auto id : wires) if (!wire(id)) return fail("A selected wire no longer exists.");
    Document fragment;
    fragment.m_gridSpacing = m_gridSpacing;
    for (const auto &item : m_components) if (components.contains(item.id())) {
        fragment.m_components.append(item);
        if (!fragment.symbol(item.symbolId())) {
            QStringList problems;
            if (!fragment.useSymbol(*symbol(item.symbolId()), &problems)) return fail(problems.join('\n'));
        }
    }
    // Endpoint identity, not coordinates, determines sharing in the fragment.
    QHash<WireEndpoint, NodeId> detached;
    const auto endpoint = [&](const WireEndpoint &source) -> WireEndpoint {
        if (isTerminal(source) && components.contains(std::get<TerminalRef>(source).component)) return source;
        if (!detached.contains(source)) {
            QPointF position;
            endpointPosition(source, &position);
            const auto id = fragment.m_nextNodeId++;
            detached.insert(source, id);
            fragment.m_nodes.insert(id, position);
        }
        return makeNodeEndpoint(detached.value(source));
    };
    for (const auto &route : m_wires) if (wires.contains(route.id)) {
        auto copied = route;
        copied.start = endpoint(route.start); copied.end = endpoint(route.end);
        fragment.m_wires.append(copied);
    }
    return QJsonDocument(QJsonObject{{"clipboardVersion", 1}, {"document", fragment.toJson()}}).toJson(QJsonDocument::Compact);
}

bool Document::pasteFragment(const QByteArray &bytes, QPointF displacement,
                             QVector<ComponentId> *components, QVector<WireId> *wires, QStringList *errors)
{
    const auto fail = [&](const QString &message) { if (errors) errors->append(message); return false; };
    if (!std::isfinite(displacement.x()) || !std::isfinite(displacement.y())) return fail("Invalid paste displacement.");
    QJsonParseError parseError;
    const auto json = QJsonDocument::fromJson(bytes, &parseError);
    const auto root = json.object();
    if (parseError.error != QJsonParseError::NoError || !json.isObject()
        || root.value("clipboardVersion").toDouble(-1) != 1 || !root.value("document").isObject()
        || root.value("document").toObject().value("formatVersion").toDouble(-1) != 6)
        return fail("Unsupported or malformed schematic clipboard data.");
    auto loaded = fromJson(root.value("document").toObject());
    if (!loaded.document || !loaded.warnings.isEmpty())
        return fail((loaded.errors + loaded.warnings).join('\n'));
    const auto &fragment = *loaded.document;
    if (fragment.components().isEmpty() && fragment.wires().isEmpty()) return fail("The clipboard fragment is empty.");
    if (!idsAvailable(m_nextComponentId, fragment.components().size())
        || !idsAvailable(m_nextWireId, fragment.wires().size()) || !idsAvailable(m_nextNodeId, fragment.nodes().size()))
        return fail("There are not enough available document identities for this paste.");
    Document candidate = *this;
    for (const auto &item : fragment.components()) {
        const auto *definition = fragment.symbol(item.symbolId());
        const auto *existing = candidate.symbol(item.symbolId());
        if (existing && SymbolLibrary::symbolToJson(*existing) != SymbolLibrary::symbolToJson(*definition))
            return fail(QString("Cannot paste: symbol %1 has a different definition in this document.").arg(item.symbolId().value()));
        if (!existing) {
            QStringList problems;
            if (!candidate.useSymbol(*definition, &problems)) return fail(problems.join('\n'));
        }
    }
    QHash<ComponentId, ComponentId> componentMap;
    QHash<NodeId, NodeId> nodeMap;
    QVector<ComponentId> addedComponents;
    QVector<WireId> addedWires;
    for (const auto &item : fragment.components()) {
        const auto position = item.position() + displacement;
        if (!std::isfinite(position.x()) || !std::isfinite(position.y())) return fail("Pasted position is outside the supported range.");
        const auto id = candidate.addComponent(item.symbolId(), position, item.rotation());
        if (!id) return fail("Could not allocate a pasted component.");
        auto &copy = candidate.m_components.last();
        auto properties = item.properties(); properties[0].value = copy.reference();
        copy.setProperties(properties); copy.setMirrored(item.mirrored());
        copy.setPropertyBlockOffset(item.propertyBlockOffset());
        componentMap.insert(item.id(), id); addedComponents.append(id);
    }
    auto nodeIds = fragment.nodes().keys(); std::sort(nodeIds.begin(), nodeIds.end());
    for (auto old : nodeIds) {
        const auto point = fragment.nodes().value(old) + displacement;
        if (!std::isfinite(point.x()) || !std::isfinite(point.y())) return fail("Pasted node is outside the supported range.");
        const auto id = candidate.m_nextNodeId++;
        candidate.m_nodes.insert(id, point); nodeMap.insert(old, id);
    }
    const auto endpoint = [&](const WireEndpoint &old) -> WireEndpoint {
        if (isNode(old)) return makeNodeEndpoint(nodeMap.value(std::get<NodeId>(old)));
        const auto ref = std::get<TerminalRef>(old);
        return makeTerminalEndpoint(componentMap.value(ref.component), ref.terminal);
    };
    for (const auto &route : fragment.wires()) {
        QVector<QPointF> interior;
        for (int i = 1; i + 1 < route.vertices.size(); ++i) interior.append(route.vertices[i] + displacement);
        const auto id = candidate.addWire(endpoint(route.start), interior, endpoint(route.end));
        if (!id) return fail("Pasted wire geometry is invalid.");
        addedWires.append(*id);
    }
    *this = std::move(candidate);
    if (components) *components = addedComponents;
    if (wires) *wires = addedWires;
    return true;
}
