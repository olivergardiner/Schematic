#include "symbolpackagestore.h"
#include "core/symbolsvg.h"

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

bool safeEntryName(const QString &name)
{
    static const QRegularExpression pattern(QStringLiteral("^[A-Za-z0-9][A-Za-z0-9._-]*\\.json$"));
    return pattern.match(name).hasMatch();
}

bool readManifest(const QString &directory, QJsonArray *entries, QStringList *errors, QJsonArray *overrides = nullptr)
{
    QFile file(QDir(directory).filePath(QStringLiteral("manifest.json")));
    if (!file.exists()) {
        *entries = {};
        return true;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        errors->append(QStringLiteral("%1: %2").arg(file.fileName(), file.errorString()));
        return false;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()
        || !document.object().value(QStringLiteral("symbols")).isArray()) {
        errors->append(QStringLiteral("%1: invalid symbol manifest (%2)")
                           .arg(file.fileName(), parseError.errorString()));
        return false;
    }
    *entries = document.object().value(QStringLiteral("symbols")).toArray();
    QSet<QString> names;
    for (const QJsonValue &entry : *entries) {
        if (!entry.isString() || !safeEntryName(entry.toString()) || names.contains(entry.toString())) {
            errors->append(QStringLiteral("%1: manifest contains an unsafe symbol filename")
                               .arg(file.fileName()));
            return false;
        }
        names.insert(entry.toString());
    }
    const auto overrideValue = document.object().value(QStringLiteral("overrides"));
    if (!overrideValue.isUndefined() && !overrideValue.isArray()) {
        errors->append(QStringLiteral("manifest overrides must be an array"));
        return false;
    }
    QSet<QString> overrideIds;
    for (const auto &value : overrideValue.toArray()) {
        if (!value.isString() || overrideIds.contains(value.toString())
            || !names.contains(value.toString() + QStringLiteral(".json"))) {
            errors->append(QStringLiteral("Invalid or duplicate manifest override"));
            return false;
        }
        overrideIds.insert(value.toString());
    }
    if (overrides) *overrides = overrideValue.toArray();
    return true;
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
    QJsonArray entries, overrides;
    if (!readManifest(m_directory, &entries, &errors, &overrides))
        return errors;
    for (const QJsonValue &value : entries) {
        const QString path = QDir(m_directory).filePath(value.toString());
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            errors << QStringLiteral("%1: could not open (%2)").arg(path, file.errorString());
            continue;
        }
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            errors << QStringLiteral("%1: invalid symbol JSON (%2)").arg(path, parseError.errorString());
            continue;
        }
        QStringList entryErrors;
        auto symbol = SymbolLibrary::parseSymbol(document.object(), &entryErrors);
        if (symbol) {
            if (overrides.contains(symbol->id.value())) {
                const auto *existing = library->find(symbol->id);
                if (!existing || !sameTerminalSignature(*existing, *symbol))
                    entryErrors << QStringLiteral("Invalid override for %1").arg(symbol->id.value());
                else library->replace(*symbol, &entryErrors);
            } else library->add(*symbol, &entryErrors);
        }
        errors.append(entryErrors);
    }
    return errors;
}

bool SymbolPackageStore::contains(const SymbolId &id) const
{
    QJsonArray entries;
    QStringList errors;
    if (!readManifest(m_directory, &entries, &errors))
        return false;
    const QString filename = id.value() + QStringLiteral(".json");
    for (const QJsonValue &entry : entries)
        if (entry.toString() == filename)
            return true;
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
    QJsonArray entries, overrides;
    if (!readManifest(m_directory, &entries, &localErrors, &overrides)) {
        if (errors) errors->append(localErrors);
        return false;
    }
    for (const auto &package : packages) {
        const QString filename = package.symbol.id.value() + QStringLiteral(".json");
        if (!entries.contains(filename)) entries.append(filename);
        if (allowOverrides && builtinSymbolLibrary().find(package.symbol.id)
            && !overrides.contains(package.symbol.id.value())) overrides.append(package.symbol.id.value());
    }

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
    const QByteArray manifest = QJsonDocument(QJsonObject{{QStringLiteral("symbols"), entries},
        {QStringLiteral("overrides"), overrides}}).toJson();
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
