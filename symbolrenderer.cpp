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
    }
    return path;
}

} // namespace

QVector<RenderedSymbolPrimitive> SymbolRenderer::primitives(const LibrarySymbol &symbol)
{
    QVector<RenderedSymbolPrimitive> result;
    result.reserve(symbol.primitives.size());
    for (const SymbolPrimitive &primitive : symbol.primitives) {
        QPainterPath path = primitivePath(primitive);
        if (!path.isEmpty())
            result.append({std::move(path), primitive.kind == PrimitiveKind::Polygon});
    }
    return result;
}

QRectF SymbolRenderer::bounds(const LibrarySymbol &symbol)
{
    QRectF result;
    bool hasBounds = false;
    for (const RenderedSymbolPrimitive &primitive : primitives(symbol)) {
        const QRectF primitiveBounds = primitive.path.boundingRect();
        result = hasBounds ? result.united(primitiveBounds) : primitiveBounds;
        hasBounds = true;
    }
    return hasBounds ? result : QRectF{};
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
    const qreal scale = std::min(available.width() / width, available.height() / height);
    if (!std::isfinite(scale) || scale <= 0.0)
        return;

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
    const QTransform transform = QTransform::fromTranslate(target.center().x(),
                                                           target.center().y())
        * QTransform::fromScale(scale, scale)
        * QTransform::fromTranslate(-localBounds.center().x(), -localBounds.center().y());
    painter->setTransform(transform, true);
    painter->setPen(QPen(Qt::black, kSymbolLineWidth));
    for (const RenderedSymbolPrimitive &primitive : artwork) {
        painter->setBrush(primitive.filled ? QBrush(Qt::white) : QBrush(Qt::NoBrush));
        painter->drawPath(primitive.path);
    }
    painter->restore();
}
