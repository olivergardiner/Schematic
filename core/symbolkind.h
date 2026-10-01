#ifndef SYMBOLKIND_H
#define SYMBOLKIND_H

#include <QString>
#include <optional>

// The fixed set of built-in symbols for the milestone-1 editor. This list is
// intentionally closed (no user-authored symbols in scope) - see AGENTS.md.
enum class SymbolKind {
    Resistor,
    Capacitor,
    Diode,
    Ground,
    OpAmp,
    Potentiometer,
    Jack,
};

// Stable, serialization-facing name for a SymbolKind (used by the JSON file
// format - see Document::toJson/fromJson in a later step).
QString symbolKindName(SymbolKind kind);

// Inverse of symbolKindName(); returns std::nullopt for any unrecognised
// name, e.g. one written by a newer file format version.
std::optional<SymbolKind> symbolKindFromName(const QString &name);

// Conventional reference-designator prefix for a kind (e.g. "R" for a
// resistor). Non-empty for every kind. Used to generate references for
// components that are loaded without one; the same metadata is intended
// for default references on newly placed components.
QString symbolKindReferencePrefix(SymbolKind kind);

#endif // SYMBOLKIND_H
