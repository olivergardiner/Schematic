#ifndef TST_SYMBOLLIBRARY_H
#define TST_SYMBOLLIBRARY_H

#include <QObject>

class TstSymbolLibrary : public QObject
{
    Q_OBJECT

private slots:
    // The built-in library holds exactly the seven shipped symbols, in
    // palette order, with their hand-written display name, category,
    // reference prefix and reference visibility.
    void builtinLibraryHasSevenSymbolsWithExpectedMetadata();

    // Released terminal order, names and positions are frozen: saved wires
    // refer to terminals by index and derive endpoint coordinates from
    // these offsets. Expected values are written out by hand here.
    void builtinTerminalsAreFrozen();

    // Primitive kinds per symbol (and the jack's circle) match the artwork
    // the app has always drawn.
    void builtinPrimitiveShapes();

    // A category is a '/'-separated path; libraryCategoryPath() splits it.
    void categoryPathSplitsIntoSegments();

    // find() resolves every built-in id and returns nullptr otherwise.
    void findResolvesKnownIdsOnly();

    // validateLibrarySymbol() reports each class of bad definition.
    void validationRejectsBadDefinitions();

    // A valid symbol passes, and add() refuses duplicate ids without
    // changing the library.
    void addRejectsDuplicateIds();

    // parseSymbol() rejects malformed JSON entries and accepts a valid one.
    void parseSymbolValidatesJsonShape();

    // Directory loading works from disk and is all-or-nothing.
    void loadFromDirectoryIsAllOrNothing();
};

#endif // TST_SYMBOLLIBRARY_H
