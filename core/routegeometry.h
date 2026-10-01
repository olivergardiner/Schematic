#ifndef ROUTEGEOMETRY_H
#define ROUTEGEOMETRY_H

#include <QPointF>
#include <QVector>

enum class RouteGeometryError
{
    None,
    TooFewVertices,
    NonFiniteCoordinate,
    ZeroLengthSegment,
    DiagonalSegment,
    // Two consecutive segments on the same axis that run in opposite
    // directions, e.g. (0,0) -> (50,0) -> (30,0). Collinear segments that
    // keep going the same way are still accepted.
    CollinearBacktrack,
};

constexpr qreal kRouteGeometryEpsilon = 1e-9;

RouteGeometryError validateRouteGeometry(const QVector<QPointF> &vertices);

#endif // ROUTEGEOMETRY_H
