#ifndef DOCUMENTLOADRESULT_H
#define DOCUMENTLOADRESULT_H

#include "document.h"

#include <QStringList>

struct DocumentLoadResult
{
    std::optional<Document> document;
    QStringList errors;
    QStringList warnings;
};

#endif // DOCUMENTLOADRESULT_H
