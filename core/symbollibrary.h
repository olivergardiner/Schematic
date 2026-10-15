#ifndef SYMBOLLIBRARY_H
#define SYMBOLLIBRARY_H

#include "symbolid.h"

#include <QHash>
#include <QJsonObject>
#include <QPointF>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>

#include <optional>

// A single drawing primitive of a symbol's static artwork, expressed in
// local symbol-space world/scene units at rotation 0, centred on the origin.
// These are the same units as Component::position() and
// Document::gridSpacing - see the coordinate contract note in component.h.
// Grid spacing controls snapping and grid-line spacing only; it never scales
// symbol artwork or terminal offsets.
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

// A single named connection point on a symbol, in the same local unit space
// as SymbolPrimitive geometry (no scaling). TerminalId is this terminal's
// index within LibrarySymbol::terminals.
struct SymbolTerminal
{
    QString name;
    QPointF offset;
};

// One symbol as served by a SymbolLibrary. Terminal order is the
// TerminalId that saved wires refer to, so for a released symbol it must
// never change; changed geometry gets a new id.
struct LibrarySymbol
{
    SymbolId id;
    QString displayName;
    // Palette placement as a '/'-separated path, e.g. "Passives" or
    // "Semiconductors/Diodes" (presentation only; see libraryCategoryPath()).
    QString category;
    QString referencePrefix; // e.g. "R"; used for default references.
    bool showsReference = true;
    QVector<SymbolPrimitive> primitives;
    QVector<SymbolTerminal> terminals;
};

// Splits a category path ("Semiconductors/Diodes") into its segments, outermost
// first. A single-level category yields one segment. Does not validate; an
// invalid path (see validateLibrarySymbol()) may yield empty segments.
QStringList libraryCategoryPath(const QString &category);

// Returns one message per problem found; empty means valid. Checks: non-empty
// (and untrimmed-clean) id, non-empty display name and prefix; a category
// path whose '/'-separated segments are all non-empty and have no leading or
// trailing whitespace; at
// least one primitive and one terminal; primitive point counts for their
// kind; finite coordinates; positive circle radius; non-empty, unique
// terminal names; unique terminal offsets.
QStringList validateLibrarySymbol(const LibrarySymbol &symbol);

// Smallest-numbered unused "<prefix><n>" (n >= 1) not present in used. Shared
// by Document::addComponent() and the loader so defaults and repairs agree.
QString firstUnusedReference(const QString &prefix, const QSet<QString> &used);

// An ordered collection of validated symbols with unique ids. Order is the
// order symbols were added (for the built-in library, manifest order).
// Qt Core only.
class SymbolLibrary
{
public:
    // Parses and validates one symbol entry. Problems are appended to
    // *errors (each naming the symbol) and std::nullopt is returned.
    static std::optional<LibrarySymbol> parseSymbol(const QJsonObject &object,
                                                    QStringList *errors);

    // Validates symbol and adds it. Rejected (returning false, library
    // unchanged, reasons appended to *errors) if invalid or if its id is
    // already present.
    bool add(const LibrarySymbol &symbol, QStringList *errors);
    bool addFromJson(const QJsonObject &object, QStringList *errors);

    // Loads <directory>/manifest.json - {"symbols": ["a.json", ...]} - and
    // every listed file, which works for both a Qt resource path (":/symbols")
    // and a disk directory. All-or-nothing: if anything fails, errors are
    // appended to *errors and an empty library is returned.
    static SymbolLibrary loadFromDirectory(const QString &directory, QStringList *errors);

    // The returned pointer is valid until the next add().
    const LibrarySymbol *find(const SymbolId &id) const;
    const QVector<LibrarySymbol> &symbols() const { return m_symbols; }
    int size() const { return m_symbols.size(); }

private:
    QVector<LibrarySymbol> m_symbols;
    QHash<SymbolId, int> m_index;
};

// The library compiled into the application from :/symbols. Loaded and
// validated on first use; an invalid built-in library is a fatal error
// (it is covered by tests, so this indicates a broken build).
const SymbolLibrary &builtinSymbolLibrary();

#endif // SYMBOLLIBRARY_H
