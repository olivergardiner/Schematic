#ifndef TST_SYMBOLDEFINITION_H
#define TST_SYMBOLDEFINITION_H

#include <QObject>

class TstSymbolDefinition : public QObject
{
    Q_OBJECT

private slots:
    // Every built-in symbol kind must resolve to a definition with a
    // non-empty display name, at least one terminal, and at least one
    // drawing primitive - a symbol with none of these would be unusable in
    // the palette/canvas even though it compiles.
    void allKindsHaveUsableDefinitions();

    // Terminal offsets must be unique within a symbol - two terminals
    // sharing one local offset would make them visually and electrically
    // indistinguishable when a wire snaps to that point.
    void terminalOffsetsAreUniquePerSymbol();

    // symbolKindName()/symbolKindFromName() must round-trip for every kind,
    // since this pairing is the serialization contract used by the JSON
    // file format (added in a later step).
    void nameRoundTripsForEveryKind();

    // An unrecognised name (e.g. from a newer file format) must not resolve
    // to any kind.
    void unknownNameReturnsNullopt();
};

#endif // TST_SYMBOLDEFINITION_H
