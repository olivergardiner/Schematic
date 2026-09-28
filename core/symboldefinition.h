#ifndef SYMBOLDEFINITION_H
#define SYMBOLDEFINITION_H

#include "symbolkind.h"

#include <QPointF>
#include <QString>
#include <QVector>

// A single drawing primitive of a symbol's static artwork, expressed in
// local symbol-space world/scene units at rotation 0, centred on the
// origin. These are the same units as Component::position() and
// Document::gridSpacing (added in a later step) - see the coordinate
// contract note in component.h. Document::gridSpacing controls snapping and
// background grid-line spacing only; it never scales symbol artwork or
// terminal offsets, so changing it does not resize placed components.
enum class PrimitiveKind {
    Line,     // exactly 2 points: start, end
    Polyline, // 2+ points, drawn open (not closed/filled)
    Polygon,  // 3+ points, drawn closed (outline; may be filled by the renderer)
    Circle,   // exactly 1 point (centre); radius is set separately
};

struct SymbolPrimitive
{
    PrimitiveKind kind = PrimitiveKind::Line;
    QVector<QPointF> points;
    qreal radius = 0.0; // only meaningful when kind == Circle
};

// A single named connection point on a symbol, in the same local world/scene
// unit space as SymbolPrimitive geometry (no scaling - see above). TerminalId
// is this terminal's index within SymbolDefinition::terminals.
struct SymbolTerminal
{
    QString name;
    QPointF offset;
};

// Static, built-in description of one SymbolKind's artwork and terminals.
// One instance exists per SymbolKind for the lifetime of the application;
// see symbolDefinition(). Symbol artwork is presentation-only and may be
// refined later without affecting terminal identity or electrical meaning.
struct SymbolDefinition
{
    SymbolKind kind;
    QString displayName;
    QVector<SymbolPrimitive> primitives;
    QVector<SymbolTerminal> terminals;
};

// Returns the static definition for the given symbol kind. The returned
// reference is valid for the lifetime of the application.
const SymbolDefinition &symbolDefinition(SymbolKind kind);

// Convenience accessor: symbolDefinition(kind).terminals.size().
int symbolTerminalCount(SymbolKind kind);

#endif // SYMBOLDEFINITION_H
