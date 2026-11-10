#include "document.h"
#include "documentloadresult.h"
#include <QJsonDocument>
#include <QSet>
#include <algorithm>
#include <cmath>

QByteArray Document::copyFragment(const QVector<ComponentId> &components,
                                  const QVector<WireId> &wires, QStringList *errors, const QVector<NoteId> &notes) const
{
    const auto fail = [&](const QString &message) { if (errors) errors->append(message); return QByteArray{}; };
    if (components.isEmpty() && wires.isEmpty() && notes.isEmpty()) return fail("Select objects to copy.");
    for (auto id : notes) if (!note(id)) return fail("A selected note no longer exists.");
    for (auto id : components) if (!component(id)) return fail("A selected component no longer exists.");
    for (auto id : wires) if (!wire(id)) return fail("A selected wire no longer exists.");
    Document fragment;
    fragment.m_gridSpacing = m_gridSpacing;
    for (const auto &item : m_notes) if (notes.contains(item.id)) fragment.m_notes.append(item);
    // Keep the source document's alternatives; never substitute installed
    // definitions while copying an embedded layout family.
    fragment.m_symbols = m_symbols;
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
                             QVector<ComponentId> *components, QVector<WireId> *wires, QStringList *errors,
                             QVector<NoteId> *notes)
{
    return pasteFragmentImpl(bytes, displacement, false, components, wires, errors, notes);
}

bool Document::pasteFragmentAt(const QByteArray &bytes, QPointF target,
                               QVector<ComponentId> *components, QVector<WireId> *wires, QStringList *errors,
                               QVector<NoteId> *notes)
{
    return pasteFragmentImpl(bytes, target, true, components, wires, errors, notes);
}

bool Document::pasteFragmentImpl(const QByteArray &bytes, QPointF displacement, bool atTarget,
                                 QVector<ComponentId> *components, QVector<WireId> *wires, QStringList *errors,
                                 QVector<NoteId> *notes)
{
    const auto fail = [&](const QString &message) { if (errors) errors->append(message); return false; };
    if (!std::isfinite(displacement.x()) || !std::isfinite(displacement.y())) return fail("Invalid paste displacement.");
    QJsonParseError parseError;
    const auto json = QJsonDocument::fromJson(bytes, &parseError);
    const auto root = json.object();
    if (parseError.error != QJsonParseError::NoError || !json.isObject()
        || root.value("clipboardVersion").toDouble(-1) != 1 || !root.value("document").isObject()
        || (root.value("document").toObject().value("formatVersion").toDouble(-1) != 6
            && root.value("document").toObject().value("formatVersion").toDouble(-1) != 7
            && root.value("document").toObject().value("formatVersion").toDouble(-1) != 8
            && root.value("document").toObject().value("formatVersion").toDouble(-1) != 9
            && root.value("document").toObject().value("formatVersion").toDouble(-1) != 10))
        return fail("Unsupported or malformed schematic clipboard data.");
    auto loaded = fromJson(root.value("document").toObject());
    if (!loaded.document || !loaded.warnings.isEmpty())
        return fail((loaded.errors + loaded.warnings).join('\n'));
    const auto &fragment = *loaded.document;
    if (fragment.components().isEmpty() && fragment.wires().isEmpty() && fragment.notes().isEmpty()) return fail("The clipboard fragment is empty.");
    if (atTarget) {
        bool first = true;
        QPointF minimum, maximum;
        const auto include = [&](QPointF point) {
            if (first) { minimum = maximum = point; first = false; }
            else {
                minimum.setX(std::min(minimum.x(), point.x())); minimum.setY(std::min(minimum.y(), point.y()));
                maximum.setX(std::max(maximum.x(), point.x())); maximum.setY(std::max(maximum.y(), point.y()));
            }
        };
        for (const auto &item : fragment.components()) include(item.position());
        for (const auto &item : fragment.notes()) include(item.position);
        for (const auto &route : fragment.wires())
            for (QPointF point : route.vertices) include(point);
        const QPointF centre = minimum / 2.0 + maximum / 2.0;
        displacement -= centre;
        if (!std::isfinite(displacement.x()) || !std::isfinite(displacement.y()))
            return fail("Invalid paste displacement.");
    }
    if (!idsAvailable(m_nextComponentId, fragment.components().size())
        || !idsAvailable(m_nextWireId, fragment.wires().size()) || !idsAvailable(m_nextNodeId, fragment.nodes().size())
        || !idsAvailable(m_nextNoteId, fragment.notes().size()))
        return fail("There are not enough available document identities for this paste.");
    Document candidate = *this;
    for (const auto &entry : fragment.m_symbols.symbols()) {
        const auto *definition = &entry;
        const auto *existing = candidate.symbol(definition->id);
        if (existing && SymbolLibrary::symbolToJson(*existing) != SymbolLibrary::symbolToJson(*definition))
            return fail(QString("Cannot paste: symbol %1 has a different definition in this document.").arg(definition->id.value()));
        if (!existing) {
            QStringList problems;
            if (!candidate.m_symbols.add(*definition, &problems)) return fail(problems.join('\n'));
        }
    }
    QHash<ComponentId, ComponentId> componentMap;
    QHash<NodeId, NodeId> nodeMap;
    QVector<ComponentId> addedComponents;
    QVector<WireId> addedWires;
    QVector<NoteId> addedNotes;
    for (const auto &item : fragment.notes()) {
        const auto id = candidate.addNote(item.position + displacement, item.text, item.width);
        if (!id) return fail("Pasted note geometry is invalid.");
        auto &copy = candidate.m_notes.last();
        copy.justification = item.justification; copy.fillColor = item.fillColor; copy.outlineColor = item.outlineColor;
        addedNotes.append(id);
    }
    for (const auto &item : fragment.components()) {
        const auto position = item.position() + displacement;
        if (!std::isfinite(position.x()) || !std::isfinite(position.y())) return fail("Pasted position is outside the supported range.");
        const auto id = candidate.addComponent(item.symbolId(), position, item.rotation());
        if (!id) return fail("Could not allocate a pasted component.");
        auto &copy = candidate.m_components.last();
        auto properties = item.properties(); properties[0].value = copy.reference();
        copy.setProperties(properties); copy.setMirrored(item.mirrored());
        copy.setPropertyBlockOffset(item.propertyBlockOffset());
        copy.setPropertyBlockJustification(item.propertyBlockJustification());
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
    if (notes) *notes = addedNotes;
    return true;
}
