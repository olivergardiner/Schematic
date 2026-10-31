#ifndef SYMBOLRENDERER_H
#define SYMBOLRENDERER_H

#include "symbollibrary.h"

#include <QPainterPath>
#include <QRectF>
#include <QVector>

class QPainter;

struct RenderedSymbolPrimitive
{
    QPainterPath path;
    bool filled = false;
};

// Shared application-layer conversion and preview painting for library
// artwork. Paths remain in symbol-local world units; only the preview painter
// scales them to fit its target rectangle.
class SymbolRenderer
{
public:
    static QVector<RenderedSymbolPrimitive> primitives(const LibrarySymbol &symbol);
    static QRectF bounds(const LibrarySymbol &symbol);
    static void paintPreview(QPainter *painter, const LibrarySymbol &symbol,
                             const QRectF &target);
};

#endif // SYMBOLRENDERER_H
