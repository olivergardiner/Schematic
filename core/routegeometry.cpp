#include "routegeometry.h"

#include <cmath>

RouteGeometryError validateRouteGeometry(const QVector<QPointF> &vertices)
{
    if (vertices.size() < 2)
        return RouteGeometryError::TooFewVertices;

    for (const QPointF &point : vertices) {
        if (!std::isfinite(point.x()) || !std::isfinite(point.y()))
            return RouteGeometryError::NonFiniteCoordinate;
    }

    for (int i = 1; i < vertices.size(); ++i) {
        const qreal dx = std::abs(vertices[i].x() - vertices[i - 1].x());
        const qreal dy = std::abs(vertices[i].y() - vertices[i - 1].y());
        if (dx <= kRouteGeometryEpsilon && dy <= kRouteGeometryEpsilon)
            return RouteGeometryError::ZeroLengthSegment;
        if (dx > kRouteGeometryEpsilon && dy > kRouteGeometryEpsilon)
            return RouteGeometryError::DiagonalSegment;
    }

    return RouteGeometryError::None;
}
