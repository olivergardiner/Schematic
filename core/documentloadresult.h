#ifndef DOCUMENTLOADRESULT_H
#define DOCUMENTLOADRESULT_H

#include "document.h"

#include <QStringList>

enum class SymbolConflictKind { Presentation, Terminals, Missing };

struct SymbolConflict
{
    LibrarySymbol embedded;
    std::optional<LibrarySymbol> active;
    SymbolConflictKind kind;
};

struct DocumentLoadResult
{
    std::optional<Document> document;
    QStringList errors;
    QStringList warnings;
    QVector<SymbolConflict> conflicts;
};

#endif // DOCUMENTLOADRESULT_H
