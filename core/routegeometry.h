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
};

constexpr qreal kRouteGeometryEpsilon = 1e-9;

RouteGeometryError validateRouteGeometry(const QVector<QPointF> &vertices);

#endif // ROUTEGEOMETRY_H
