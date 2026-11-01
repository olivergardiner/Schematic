#ifndef SYMBOLSVG_H
#define SYMBOLSVG_H

#include "symbollibrary.h"

#include <QByteArray>

#include <optional>

struct SymbolSvgImportResult
{
    std::optional<LibrarySymbol> symbol;
    QStringList warnings;
    QStringList errors;
};

struct SymbolSvgExportResult
{
    QByteArray svg;
    QByteArray sidecar;
    QString svgFileName;
    QString sidecarFileName;
    QStringList errors;
};

struct SymbolSvgArtworkResult
{
    QVector<SymbolPrimitive> primitives;
    QStringList warnings;
    QStringList errors;
};

// Headless conversion for the restricted SVG profile in symbol-import.md.
// The caller supplies the sidecar and referenced SVG contents; no files are
// read or written here, so callers can stage imports and exports safely.
class SymbolSvgImporter
{
public:
    static SymbolSvgArtworkResult importArtwork(const QByteArray &svgBytes,
                                                const QPointF &origin = {});
    static bool resolveSidecarSvgPath(const QByteArray &sidecarJson,
                                      const QString &sidecarDirectory,
                                      QString *absoluteSvgPath,
                                      QStringList *errors);
    static SymbolSvgImportResult importSymbol(const QByteArray &sidecarJson,
                                              const QByteArray &svgBytes,
                                              const QString &sidecarDirectory);
};

class SymbolSvgExporter
{
public:
    static SymbolSvgExportResult exportSymbol(const LibrarySymbol &symbol);
};

#endif // SYMBOLSVG_H
