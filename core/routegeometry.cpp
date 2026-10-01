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

        // The previous segment was already checked to be non-zero and
        // axis-aligned, so a reversal is a same-axis pair with opposite signs.
        if (i >= 2) {
            const qreal prevDx = vertices[i - 1].x() - vertices[i - 2].x();
            const qreal prevDy = vertices[i - 1].y() - vertices[i - 2].y();
            const qreal curDx = vertices[i].x() - vertices[i - 1].x();
            const qreal curDy = vertices[i].y() - vertices[i - 1].y();
            const bool bothHorizontal = std::abs(prevDy) <= kRouteGeometryEpsilon
                && std::abs(curDy) <= kRouteGeometryEpsilon;
            const bool bothVertical = std::abs(prevDx) <= kRouteGeometryEpsilon
                && std::abs(curDx) <= kRouteGeometryEpsilon;
            if ((bothHorizontal && prevDx * curDx < 0.0)
                || (bothVertical && prevDy * curDy < 0.0))
                return RouteGeometryError::CollinearBacktrack;
        }
    }

    return RouteGeometryError::None;
}
