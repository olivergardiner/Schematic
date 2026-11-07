#include "symbolrenderer.h"

#include <QPainter>
#include <QPen>
#include <QTransform>

#include <algorithm>
#include <cmath>
#include <utility>

namespace {

constexpr qreal kSymbolLineWidth = 1.0;
constexpr qreal kPreviewMargin = 4.0;

QPainterPath primitivePath(const SymbolPrimitive &primitive)
{
    QPainterPath path;
    switch (primitive.kind) {
    case PrimitiveKind::Line:
        if (primitive.points.size() == 2) {
            path.moveTo(primitive.points[0]);
            path.lineTo(primitive.points[1]);
        }
        break;
    case PrimitiveKind::Polyline:
        if (!primitive.points.isEmpty()) {
            path.moveTo(primitive.points.first());
            for (int i = 1; i < primitive.points.size(); ++i)
                path.lineTo(primitive.points[i]);
        }
        break;
    case PrimitiveKind::Polygon:
        if (!primitive.points.isEmpty()) {
            path.moveTo(primitive.points.first());
            for (int i = 1; i < primitive.points.size(); ++i)
                path.lineTo(primitive.points[i]);
            path.closeSubpath();
        }
        break;
    case PrimitiveKind::Circle:
        if (!primitive.points.isEmpty()) {
            const QPointF centre = primitive.points.first();
            path.addEllipse(centre, primitive.radius, primitive.radius);
        }
        break;
    case PrimitiveKind::Path:
        path.setFillRule(primitive.fillRule == PathFillRule::EvenOdd
                             ? Qt::OddEvenFill : Qt::WindingFill);
        for (const PathSegment &segment : primitive.segments) {
            switch (segment.kind) {
            case PathSegmentKind::MoveTo:
                if (segment.points.size() == 1)
                    path.moveTo(segment.points.first());
                break;
            case PathSegmentKind::LineTo:
                if (segment.points.size() == 1)
                    path.lineTo(segment.points.first());
                break;
            case PathSegmentKind::CubicTo:
                if (segment.points.size() == 3)
                    path.cubicTo(segment.points[0], segment.points[1], segment.points[2]);
                break;
            case PathSegmentKind::Close:
                if (segment.points.isEmpty())
                    path.closeSubpath();
                break;
            }
        }
        break;
    }
    return path;
}

} // namespace

QBrush SymbolRenderer::fillBrush(const RenderedSymbolPrimitive &primitive)
{
    return !primitive.filled ? QBrush(Qt::NoBrush)
        : QBrush(primitive.fillColor == PrimitiveFillColor::Black ? Qt::black : Qt::white);
}

QVector<RenderedSymbolPrimitive> SymbolRenderer::primitives(const LibrarySymbol &symbol)
{
    QVector<RenderedSymbolPrimitive> result;
    result.reserve(symbol.primitives.size());
    for (const SymbolPrimitive &primitive : symbol.primitives) {
        QPainterPath path = primitivePath(primitive);
        if (!path.isEmpty())
            result.append({std::move(path),
                           primitiveIsFilled(primitive), primitive.fillColor});
    }
    return result;
}

QRectF SymbolRenderer::bounds(const LibrarySymbol &symbol)
{
    return symbolBounds(symbol);
}

void SymbolRenderer::paintPreview(QPainter *painter, const LibrarySymbol &symbol,
                                  const QRectF &target)
{
    if (!painter || !painter->isActive() || target.isEmpty())
        return;

    const QVector<RenderedSymbolPrimitive> artwork = primitives(symbol);
    const QRectF localBounds = bounds(symbol);
    const QRectF available = target.adjusted(kPreviewMargin, kPreviewMargin,
                                             -kPreviewMargin, -kPreviewMargin);
    if (artwork.isEmpty() || localBounds.isEmpty() || available.isEmpty())
        return;

    const qreal width = std::max(localBounds.width(), qreal(1.0));
    const qreal height = std::max(localBounds.height(), qreal(1.0));
    // One logical pixel per scene unit matches the canvas at default zoom.
    // Shrink large symbols to fit, but never enlarge smaller glyphs.
    const qreal scale = std::min({qreal(1.0), available.width() / width,
                                 available.height() / height});
    if (!std::isfinite(scale) || scale <= 0.0)
        return;

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
    QTransform transform;
    transform.translate(target.center().x(), target.center().y());
    transform.scale(scale, scale);
    transform.translate(-localBounds.center().x(), -localBounds.center().y());
    painter->setTransform(transform, true);
    painter->setPen(QPen(Qt::black, kSymbolLineWidth));
    for (const RenderedSymbolPrimitive &primitive : artwork) {
        painter->setBrush(fillBrush(primitive));
        painter->drawPath(primitive.path);
    }
    painter->restore();
}
