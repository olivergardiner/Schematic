#ifndef DOCUMENT_H
#define DOCUMENT_H

#include "component.h"
#include "pendingwireendpoint.h"
#include "routegeometry.h"
#include "wireroute.h"

#include <QHash>
#include <QByteArray>
#include <QJsonObject>
#include <QPointF>
#include <QVector>

#include <optional>

struct DocumentLoadResult;

struct Net
{
    QVector<TerminalRef> terminals;
};

class Document
{
public:
    qreal gridSpacing() const { return m_gridSpacing; }
    bool setGridSpacing(qreal spacing);

    const QVector<Component> &components() const { return m_components; }
    const QVector<WireRoute> &wires() const { return m_wires; }
    const QHash<NodeId, QPointF> &nodes() const { return m_nodes; }

    const Component *component(ComponentId id) const;
    const WireRoute *wire(WireId id) const;

    ComponentId addComponent(SymbolKind kind, QPointF position,
                             Rotation rotation = Rotation::Deg0);
    bool moveComponent(ComponentId id, QPointF position);
    bool removeComponent(ComponentId id);
    // Transactional: rejects (returns false, document unchanged) if id is
    // unknown or any attached route would become invalid.
    bool rotateComponent(ComponentId id, Rotation rotation);
    bool setComponentMirrored(ComponentId id, bool mirrored);


    // Renames/re-values an already-placed component. Both reference and
    // value are trimmed before validation and storage (so "R1" and " R1 "
    // are treated as the same reference, and an all-whitespace reference is
    // treated as empty). Rejected (returning false, leaving the document
    // completely unchanged) if id does not exist, the trimmed reference is
    // empty, or the trimmed reference collides with another component's
    // trimmed reference (case-sensitive; renaming a component to its own
    // current reference is allowed). value has no uniqueness constraint and
    // may be empty after trimming - see DECISIONS.md "Symbols and labels".
    bool setComponentLabels(ComponentId id, const QString &reference, const QString &value);

    std::optional<WireId> addWire(const WireEndpoint &start,
                                  const QVector<QPointF> &interiorVertices,
                                  const WireEndpoint &end);
    bool removeWire(WireId id);
    std::optional<NodeId> branchWireAt(WireId id, QPointF point);

    // Atomic wire completion: resolves each endpoint (materializing any
    // BranchSite's split only if the whole operation succeeds), validates
    // the full candidate route, and only then mutates the document - see
    // DECISIONS.md "Document model and connectivity". Rejects (returning
    // std::nullopt with the document completely unchanged, including no ID
    // allocation) if either endpoint fails to resolve, both endpoints are
    // BranchSites on the same WireId (currently unsupported - draw two
    // wires instead), a BranchSite's point does not lie on its wire, or the
    // resulting route/split geometry is invalid.
    std::optional<WireId> addWireBranching(const PendingWireEndpoint &start,
                                           const QVector<QPointF> &interiorVertices,
                                           const PendingWireEndpoint &end);

    QVector<Net> computeNets() const;
    QVector<QPointF> junctionPoints() const;

    // Version-1 JSON stores grid, components, nodes, and route points with
    // explicit terminal/node endpoint identities. Nets and junctions are
    // derived and are never serialized.
    QJsonObject toJson() const;
    QByteArray toJsonBytes() const;
    static DocumentLoadResult fromJson(const QJsonObject &root);
    static DocumentLoadResult fromJsonBytes(const QByteArray &data);

private:
    bool endpointPosition(const WireEndpoint &endpoint, QPointF *position) const;
    bool endpointExists(const WireEndpoint &endpoint) const;
    void removeOrphanNodes();
    WireId allocateWireId();
    bool commitComponentChange(ComponentId id, const Component &changed);

    // Pure (non-mutating) computation of a branch split: finds the segment
    // of wire id containing point and returns the two resulting vertex
    // lists and the original route's endpoints, without allocating any ID
    // or touching m_wires/m_nodes. Shared by branchWireAt() and
    // addWireBranching() so both use identical split geometry rules.
    bool computeBranchSplit(WireId id, QPointF point, QVector<QPointF> *leftPoints,
                            QVector<QPointF> *rightPoints, WireEndpoint *originalStart,
                            WireEndpoint *originalEnd) const;
    // Mutates m_wires/m_nodes to actually apply a previously computed split.
    // Callers must have already reserved node/leftId/rightId (e.g. via
    // idsAvailable()) - this function performs no validation and cannot
    // fail, which is what lets addWireBranching() apply two splits and a
    // new wire as one all-or-nothing sequence.
    void commitBranchSplit(WireId id, QPointF point, const QVector<QPointF> &leftPoints,
                           const QVector<QPointF> &rightPoints, const WireEndpoint &originalStart,
                           const WireEndpoint &originalEnd, NodeId node, WireId leftId,
                           WireId rightId);
    // True if count sequential IDs starting at nextId can be allocated
    // without hitting the invalid-ID sentinel (0) or wrapping past
    // quint32's range. Used to pre-flight every ID an addWireBranching()
    // call might need before any mutation begins.
    static bool idsAvailable(quint32 nextId, int count);

    qreal m_gridSpacing = 10.0;
    ComponentId m_nextComponentId = 1;
    WireId m_nextWireId = 1;
    NodeId m_nextNodeId = 1;
    QVector<Component> m_components;
    QVector<WireRoute> m_wires;
    QHash<NodeId, QPointF> m_nodes;
};

#endif // DOCUMENT_H
