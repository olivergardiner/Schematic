#ifndef SYMBOLRENDERER_H
#define SYMBOLRENDERER_H

#include "symbollibrary.h"

#include <QPainterPath>
#include <QBrush>
#include <QRectF>
#include <QVector>

class QPainter;

struct RenderedSymbolPrimitive
{
    QPainterPath path;
    bool filled = false;
    PrimitiveFillColor fillColor = PrimitiveFillColor::White;
    QPainterPath secondaryPath;
};

// Shared application-layer conversion and preview painting for library
// artwork. Paths remain in symbol-local world units; only the preview painter
// shrinks oversized glyphs to fit its target rectangle without enlarging them
// beyond one logical pixel per scene unit (the canvas's default zoom).
class SymbolRenderer
{
public:
    // Presentation only: retains original path anchors and fill rule.
    static QPainterPath handDrawnPath(const QPainterPath &path, quint32 seed, qreal looseness = 1.0);
    static QVector<RenderedSymbolPrimitive> handDrawnPrimitives(const LibrarySymbol &symbol,
        quint32 instanceId, bool consistent, int strokes, qreal looseness = 1.0);
    static QPainterPath handDrawnWire(const QPainterPath &path, quint32 seed, int strokes, qreal looseness = 1.0);
    static QBrush fillBrush(const RenderedSymbolPrimitive &primitive);
    static QVector<RenderedSymbolPrimitive> primitives(const LibrarySymbol &symbol);
    static QRectF bounds(const LibrarySymbol &symbol);
    static void paintPreview(QPainter *painter, const LibrarySymbol &symbol,
                             const QRectF &target);
};

#endif // SYMBOLRENDERER_H
