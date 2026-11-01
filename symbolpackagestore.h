#ifndef SYMBOLPACKAGESTORE_H
#define SYMBOLPACKAGESTORE_H

#include "core/symbollibrary.h"

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <functional>

class SymbolPackageStore
{
public:
    explicit SymbolPackageStore(QString directory = {});

    const QString &directory() const { return m_directory; }
    QStringList loadInto(SymbolLibrary *library) const;
    bool contains(const SymbolId &id) const;
    bool save(const LibrarySymbol &symbol, const QByteArray &sourceSvg,
              const QByteArray &sourceSidecar, QStringList *errors);

    // beforeCommit runs after successful file installation, before publishing the registry.
    // Returning false rolls back the batch (for example, a cancelled unsaved-work prompt).
    bool saveEmbeddedBatch(const QVector<LibrarySymbol> &symbols, QStringList *errors,
                           const std::function<bool()> &beforeCommit = {});

private:
    struct Package { LibrarySymbol symbol; QByteArray svg; QByteArray sidecar; };
    bool saveBatch(const QVector<Package> &packages, bool allowOverrides, QStringList *errors,
                   const std::function<bool()> &beforeCommit = {});
    QString m_directory;
};

#endif // SYMBOLPACKAGESTORE_H
