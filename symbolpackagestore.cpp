#include "symbolpackagestore.h"
#include "core/symbolsvg.h"
#include "core/symbolmanifest.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QUuid>

#include <utility>

namespace {

bool readManifest(const QString &directory, SymbolManifest *manifest, QStringList *errors)
{
    QFile file(QDir(directory).filePath(QStringLiteral("manifest.json")));
    if (!file.exists()) { *manifest = {}; return true; }
    if (!file.open(QIODevice::ReadOnly)) {
        errors->append(QStringLiteral("%1: %2").arg(file.fileName(), file.errorString())); return false;
    }
    QJsonParseError parseError;
    const auto json = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !json.isObject()) {
        errors->append(QStringLiteral("%1: invalid symbol manifest (%2)").arg(file.fileName(), parseError.errorString())); return false;
    }
    const auto parsed = SymbolManifest::fromJson(json.object(), errors);
    if (!parsed) return false;
    *manifest = *parsed; return true;
}

bool normaliseLegacyManifest(const QString &directory, SymbolManifest *manifest, QStringList *errors)
{
    if (!manifest->legacy) return true;
    for (const auto &entry : manifest->legacyEntries) {
        QFile file(QDir(directory).filePath(entry.file));
        if (!file.open(QIODevice::ReadOnly)) { errors->append("Could not read symbol for manifest upgrade: " + file.fileName()); return false; }
        QJsonParseError parseError;
        const auto json = QJsonDocument::fromJson(file.readAll(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !json.isObject()) {
            errors->append("Invalid symbol for manifest upgrade: " + file.fileName()); return false;
        }
        const auto symbol = SymbolLibrary::parseSymbol(json.object(), errors);
        if (!symbol) return false;
        manifest->placeSymbol(entry.file, symbol->category);
    }
    manifest->legacy = false; manifest->legacyEntries.clear(); return true;
}

bool writeFile(const QString &path, const QByteArray &bytes, QStringList *errors)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size()) {
        errors->append(QStringLiteral("%1: could not write (%2)").arg(path, file.errorString()));
        return false;
    }
    return true;
}

} // namespace

SymbolPackageStore::SymbolPackageStore(QString directory)
    : m_directory(directory.isEmpty()
          ? QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
                .filePath(QStringLiteral("symbol-library"))
          : std::move(directory))
{
}

QStringList SymbolPackageStore::loadInto(SymbolLibrary *library) const
{
    QStringList errors;
    if (!library)
        return {QStringLiteral("symbol library destination is null")};
    if (!QFileInfo::exists(QDir(m_directory).filePath(QStringLiteral("manifest.json"))))
        return errors;
    SymbolManifest manifest;
    if (!readManifest(m_directory, &manifest, &errors)) return errors;
    const auto collection = SymbolLibrary::loadFromDirectory(m_directory, &errors);
    if (!errors.isEmpty()) return errors;
    SymbolLibrary candidate = *library;
    candidate.declareCategories(collection.categories());
    for (const auto &symbol : collection.symbols()) {
        if (manifest.overrides.contains(symbol.id.value())) {
            const auto *existing = candidate.find(symbol.id);
            if (!existing || !sameTerminalSignature(*existing, symbol))
                errors << QStringLiteral("Invalid override for %1").arg(symbol.id.value());
            else candidate.replace(symbol, &errors);
        } else candidate.add(symbol, &errors);
    }
    if (errors.isEmpty()) *library = std::move(candidate);
    return errors;
}

bool SymbolPackageStore::contains(const SymbolId &id) const
{
    SymbolManifest manifest;
    QStringList errors;
    if (!readManifest(m_directory, &manifest, &errors)) return false;
    const auto filename = id.value() + QStringLiteral(".json");
    for (const auto &entry : manifest.entries()) if (entry.file == filename) return true;
    return false;
}

bool SymbolPackageStore::save(const LibrarySymbol &symbol, const QByteArray &sourceSvg,
                              const QByteArray &sourceSidecar, QStringList *errors)
{
    return saveBatch({{symbol, sourceSvg, sourceSidecar}}, false, errors);
}

bool SymbolPackageStore::saveEmbeddedBatch(const QVector<LibrarySymbol> &symbols, QStringList *errors,
                                           const std::function<bool()> &beforeCommit)
{
    QVector<Package> packages;
    for (const auto &symbol : symbols) {
        const auto exported = SymbolSvgExporter::exportSymbol(symbol);
        if (!exported.errors.isEmpty()) { if (errors) errors->append(exported.errors); return false; }
        packages.append({symbol, exported.svg, exported.sidecar});
    }
    return saveBatch(packages, true, errors, beforeCommit);
}

bool SymbolPackageStore::saveBatch(const QVector<Package> &packages, bool allowOverrides, QStringList *errors,
                                   const std::function<bool()> &beforeCommit)
{
    if (packages.isEmpty()) return !beforeCommit || beforeCommit();
    QStringList localErrors;
    SymbolLibrary candidate = applicationSymbolLibrary();
    QSet<SymbolId> ids;
    for (const auto &package : packages) {
        const auto &symbol = package.symbol;
        localErrors.append(validateLibrarySymbol(symbol));
        static const QRegularExpression validId(QStringLiteral("^[A-Za-z0-9][A-Za-z0-9._-]*$"));
        if (!validId.match(symbol.id.value()).hasMatch() || ids.contains(symbol.id))
            localErrors << QStringLiteral("Invalid or duplicate package id: %1").arg(symbol.id.value());
        ids.insert(symbol.id);
        if (package.svg.isEmpty() || package.sidecar.isEmpty())
            localErrors << QStringLiteral("Source SVG and sidecar are required");
        const auto *existing = candidate.find(symbol.id);
        if (existing && !contains(symbol.id) && !allowOverrides)
            localErrors << QStringLiteral("symbol id conflicts with a packaged symbol");
        if (existing && !sameTerminalSignature(*existing, symbol))
            localErrors << QStringLiteral("replacing a symbol requires an unchanged terminal signature");
        if (localErrors.isEmpty()) {
            if (existing) candidate.replace(symbol, &localErrors);
            else candidate.add(symbol, &localErrors);
        }
    }
    if (!localErrors.isEmpty()) { if (errors) errors->append(localErrors); return false; }

    QDir base(m_directory);
    if (!base.exists() && !QDir().mkpath(m_directory)) {
        localErrors << QStringLiteral("could not create symbol library directory: %1").arg(m_directory);
        if (errors) errors->append(localErrors);
        return false;
    }
    SymbolManifest collection;
    if (!readManifest(m_directory, &collection, &localErrors)
        || !normaliseLegacyManifest(m_directory, &collection, &localErrors)) {
        if (errors) errors->append(localErrors); return false;
    }
    for (const auto &package : packages) {
        collection.placeSymbol(package.symbol.id.value() + QStringLiteral(".json"), package.symbol.category);
        if (allowOverrides && builtinSymbolLibrary().find(package.symbol.id)
            && !collection.overrides.contains(package.symbol.id.value())) collection.overrides.append(package.symbol.id.value());
    }
    candidate.declareCategories(collection.categoryPaths());

    const QString token = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString stagePath = base.filePath(QStringLiteral(".stage-%1").arg(token));
    const QString backupPath = base.filePath(QStringLiteral(".backup-%1").arg(token));
    if (!QDir().mkpath(stagePath) || !QDir().mkpath(QDir(stagePath).filePath(QStringLiteral("sources")))) {
        localErrors << QStringLiteral("could not create staging directory for symbol package");
        if (errors) errors->append(localErrors);
        return false;
    }
    QStringList relativeFiles;
    for (const auto &package : packages) {
        const QString filename = package.symbol.id.value() + QStringLiteral(".json");
        const QString svg = QStringLiteral("sources/%1.svg").arg(package.symbol.id.value());
        const QString sidecar = QStringLiteral("sources/%1.json").arg(package.symbol.id.value());
        if (!writeFile(QDir(stagePath).filePath(filename), QJsonDocument(SymbolLibrary::symbolToJson(package.symbol)).toJson(), &localErrors)
            || !writeFile(QDir(stagePath).filePath(svg), package.svg, &localErrors)
            || !writeFile(QDir(stagePath).filePath(sidecar), package.sidecar, &localErrors)) {
            QDir(stagePath).removeRecursively();
            if (errors) errors->append(localErrors);
            return false;
        }
        relativeFiles << filename << svg << sidecar;
    }
    const QByteArray manifest = QJsonDocument(collection.toJson()).toJson();
    if (!writeFile(QDir(stagePath).filePath(QStringLiteral("manifest.json")), manifest, &localErrors)) {
        QDir(stagePath).removeRecursively();
        if (errors) errors->append(localErrors);
        return false;
    }
    relativeFiles << QStringLiteral("manifest.json");
    QDir().mkpath(backupPath);
    QStringList backedUp;
    QStringList installed;
    auto rollback = [&] {
        bool restored = true;
        for (const QString &relative : installed) {
            if (!QFile::remove(base.filePath(relative))) {
                restored = false;
                localErrors << QStringLiteral("Could not remove failed batch file: %1").arg(base.filePath(relative));
            }
        }
        for (const QString &relative : backedUp) {
            const QString backup = QDir(backupPath).filePath(relative);
            QDir().mkpath(QFileInfo(base.filePath(relative)).absolutePath());
            if (!QFile::rename(backup, base.filePath(relative))) {
                restored = false;
                localErrors << QStringLiteral("Could not restore %1; backup retained at %2").arg(base.filePath(relative), backup);
            }
        }
        QDir(stagePath).removeRecursively();
        if (restored) QDir(backupPath).removeRecursively();
    };
    for (const QString &relative : relativeFiles) {
        const QString target = base.filePath(relative);
        if (!QFileInfo::exists(target))
            continue;
        if (!QFileInfo(target).isFile()) {
            localErrors << QStringLiteral("Package target is not a file: %1").arg(target);
            rollback();
            if (errors) errors->append(localErrors);
            return false;
        }
        const QString backup = QDir(backupPath).filePath(relative);
        QDir().mkpath(QFileInfo(backup).absolutePath());
        if (!QFile::rename(target, backup)) {
            localErrors << QStringLiteral("could not stage existing package file: %1").arg(target);
            rollback();
            if (errors) errors->append(localErrors);
            return false;
        }
        backedUp.append(relative);
    }
    // The manifest is installed last, so it never points at a partially
    // written new package. On any failure the old files are restored.
    for (const QString &relative : relativeFiles) {
        const QString staged = QDir(stagePath).filePath(relative);
        const QString target = base.filePath(relative);
        QDir().mkpath(QFileInfo(target).absolutePath());
        if (!QFile::rename(staged, target)) {
            localErrors << QStringLiteral("could not install staged package file: %1").arg(target);
            rollback();
            if (errors) errors->append(localErrors);
            return false;
        }
        installed.append(relative);
    }
    if (beforeCommit && !beforeCommit()) {
        rollback();
        if (errors) errors->append(localErrors);
        return false;
    }
    applicationSymbolLibrary() = std::move(candidate);
    QDir(stagePath).removeRecursively();
    QDir(backupPath).removeRecursively();
    return true;
}
