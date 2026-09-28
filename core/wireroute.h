#ifndef WIREROUTE_H
#define WIREROUTE_H

#include "wireendpoint.h"

#include <QPointF>
#include <QVector>

struct WireRoute
{
    WireId id = kInvalidWireId;
    QVector<QPointF> vertices;
    WireEndpoint start;
    WireEndpoint end;

    bool referencesNode(NodeId node) const
    {
        return (isNode(start) && std::get<NodeId>(start) == node)
            || (isNode(end) && std::get<NodeId>(end) == node);
    }
};

#endif // WIREROUTE_H
