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

QPainterPath SymbolRenderer::handDrawnPath(const QPainterPath &path, quint32 seed, qreal looseness)
{
    if (looseness == 0) return path;
    QPainterPath result;
    result.setFillRule(path.fillRule());
    // Fixed integer mixing avoids platform-dependent random generators.
    const auto variation = [&seed]() {
        seed = seed * 1664525u + 1013904223u;
        return qreal(seed & 0xffffu) / 65535.0 * 2.0 - 1.0;
    };
    QPointF current, start;
    constexpr qreal pi = 3.14159265358979323846;
    for (int i = 0; i < path.elementCount(); ++i) {
        const auto element = path.elementAt(i);
        const QPointF end(element.x, element.y);
        if (element.isMoveTo()) {
            result.moveTo(end); current = start = end;
        } else if (element.isLineTo()) {
            const QPointF delta = end - current;
            const qreal length = std::hypot(delta.x(), delta.y());
            if (length > 0) {
                const QPointF normal(-delta.y()/length, delta.x()/length);
                const qreal amplitude = std::min(qreal(0.65), length * 0.035) * looseness;
                const qreal phase = variation() * pi;
                const int count = int(std::clamp(std::ceil(length / 16.0), 1.0, 256.0));
                const auto offset = [&](qreal t) { return amplitude * std::sin(pi*t) * std::sin(2*pi*t+phase); };
                const auto tangent = [&](qreal t) {
                    return delta + normal * (amplitude*pi*(std::cos(pi*t)*std::sin(2*pi*t+phase)
                                             + 2*std::sin(pi*t)*std::cos(2*pi*t+phase)));
                };
                for (int j=0; j<count; ++j) {
                    const qreal a=qreal(j)/count, b=qreal(j+1)/count, step=(b-a)/3;
                    const QPointF from = j == 0 ? current : current + delta*a + normal*offset(a);
                    const QPointF to = j+1 == count ? end : current + delta*b + normal*offset(b);
                    result.cubicTo(from+tangent(a)*step, to-tangent(b)*step, to);
                }
            }
            current = end;
            if (end == start) result.closeSubpath();
        } else if (element.type == QPainterPath::CurveToElement && i+2 < path.elementCount()) {
            const auto c2 = path.elementAt(i+1), last = path.elementAt(i+2);
            // Perturb controls, retaining the original curve anchors and closure.
            const qreal dx = variation()*0.45*looseness;
            const qreal dy = variation()*0.45*looseness;
            const QPointF shift(dx, dy);
            result.cubicTo(end+shift, QPointF(c2.x,c2.y)+shift, QPointF(last.x,last.y));
            current = QPointF(last.x,last.y); i += 2;
            if (current == start) result.closeSubpath();
        }
    }
    return result;
}

QVector<RenderedSymbolPrimitive> SymbolRenderer::handDrawnPrimitives(
    const LibrarySymbol &symbol, quint32 instanceId, bool consistent, int strokes, qreal looseness)
{
    quint32 seed = instanceId * 2654435761u;
    if (consistent) {
        // FNV-1a over UTF-8: stable across processes and Qt hash randomization.
        seed = 2166136261u;
        for (const auto byte : symbol.id.value().toUtf8()) {
            seed ^= static_cast<unsigned char>(byte);
            seed *= 16777619u;
        }
    }
    auto result = primitives(symbol);
    for (auto &primitive : result) {
        const auto original = primitive.path;
        primitive.path = handDrawnPath(original, ++seed, looseness);
        if (strokes == 2) primitive.secondaryPath = handDrawnPath(original, seed ^ 0x9e3779b9u, looseness);
    }
    return result;
}

QPainterPath SymbolRenderer::handDrawnWire(const QPainterPath &path, quint32 seed, int strokes, qreal looseness)
{
    auto result = handDrawnPath(path, seed, looseness);
    if (strokes == 2) result.addPath(handDrawnPath(path, seed ^ 0x9e3779b9u, looseness));
    return result;
}
