#ifndef SYMBOLMIMEDATA_H
#define SYMBOLMIMEDATA_H

#include "symbollibrary.h"

#include <QMimeData>

#include <optional>

namespace SymbolMimeData {

inline constexpr char kSymbolIdMimeType[] = "application/vnd.schematic.symbol-id-v1";

// The MIME type carries the payload version; the payload itself contains
// only the stable library ID.
inline QMimeData *create(const SymbolId &id)
{
    auto *mimeData = new QMimeData;
    mimeData->setData(QString::fromLatin1(kSymbolIdMimeType), id.value().toUtf8());
    return mimeData;
}

inline std::optional<SymbolId> read(const QMimeData *mimeData)
{
    const QString mimeType = QString::fromLatin1(kSymbolIdMimeType);
    if (!mimeData || !mimeData->hasFormat(mimeType))
        return std::nullopt;

    const QByteArray payload = mimeData->data(mimeType);
    if (payload.isEmpty())
        return std::nullopt;

    const SymbolId id(QString::fromUtf8(payload));
    return applicationSymbolLibrary().find(id) ? std::optional<SymbolId>(id) : std::nullopt;
}

} // namespace SymbolMimeData

#endif // SYMBOLMIMEDATA_H
