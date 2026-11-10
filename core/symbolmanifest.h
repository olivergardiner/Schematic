#ifndef SYMBOLMANIFEST_H
#define SYMBOLMANIFEST_H

#include <QJsonObject>
#include <QStringList>
#include <QVector>
#include <optional>

struct ManifestCategory {
    QString name;
    QStringList symbols; // Filenames, in tile order within this category.
    QVector<ManifestCategory> children;
};
struct ManifestSymbolEntry { QString file; QString category; };

// Collection placement is authoritative here. Individual definitions retain
// their category path for standalone import/export; loaders verify agreement.
struct SymbolManifest {
    QVector<ManifestCategory> categories;
    QStringList overrides;
    bool legacy = false;
    QVector<ManifestSymbolEntry> legacyEntries;

    static std::optional<SymbolManifest> fromJson(const QJsonObject &json, QStringList *errors);
    QJsonObject toJson() const;
    QVector<ManifestSymbolEntry> entries() const;
    QStringList categoryPaths() const;
    // Preserve an existing slot, or move/append the file when category changes.
    // Callers validate the filename and category through the shared validators.
    void placeSymbol(const QString &file, const QString &category);
};

#endif
