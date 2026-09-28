#ifndef DOCUMENT_H
#define DOCUMENT_H

#include "component.h"
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

    std::optional<WireId> addWire(const WireEndpoint &start,
                                  const QVector<QPointF> &interiorVertices,
                                  const WireEndpoint &end);
    bool removeWire(WireId id);
    std::optional<NodeId> branchWireAt(WireId id, QPointF point);

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

    qreal m_gridSpacing = 10.0;
    ComponentId m_nextComponentId = 1;
    WireId m_nextWireId = 1;
    NodeId m_nextNodeId = 1;
    QVector<Component> m_components;
    QVector<WireRoute> m_wires;
    QHash<NodeId, QPointF> m_nodes;
};

#endif // DOCUMENT_H
