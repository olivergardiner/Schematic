#include "symbollibrary.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QPair>
#include <QSet>

#include <cmath>

namespace {

std::optional<PrimitiveKind> primitiveKindFromName(const QString &name)
{
    if (name == QLatin1String("Line"))     return PrimitiveKind::Line;
    if (name == QLatin1String("Polyline")) return PrimitiveKind::Polyline;
    if (name == QLatin1String("Polygon"))  return PrimitiveKind::Polygon;
    if (name == QLatin1String("Circle"))   return PrimitiveKind::Circle;
    return std::nullopt;
}

bool readPoint(const QJsonValue &value, QPointF *out)
{
    if (!value.isArray())
        return false;
    const QJsonArray pair = value.toArray();
    if (pair.size() != 2 || !pair[0].isDouble() || !pair[1].isDouble())
        return false;
    *out = QPointF(pair[0].toDouble(), pair[1].toDouble());
    return true;
}

bool finitePoint(const QPointF &p)
{
    return std::isfinite(p.x()) && std::isfinite(p.y());
}

QString label(const SymbolId &id)
{
    return id.isEmpty() ? QStringLiteral("symbol")
                        : QStringLiteral("symbol \"%1\"").arg(id.value());
}

void appendAll(QStringList *errors, const QString &prefix, const QStringList &messages)
{
    if (!errors)
        return;
    for (const QString &message : messages)
        errors->append(prefix + message);
}

} // namespace

QStringList libraryCategoryPath(const QString &category)
{
    return category.split(QLatin1Char('/'));
}

QString firstUnusedReference(const QString &prefix, const QSet<QString> &used)
{
    for (int n = 1;; ++n) {
        const QString candidate = prefix + QString::number(n);
        if (!used.contains(candidate))
            return candidate;
    }
}

QStringList validateLibrarySymbol(const LibrarySymbol &symbol)
{
    QStringList errors;
    if (symbol.id.isEmpty())
        errors << QStringLiteral("id must not be empty");
    else if (symbol.id.value() != symbol.id.value().trimmed())
        errors << QStringLiteral("id must not have leading or trailing whitespace");
    if (symbol.displayName.trimmed().isEmpty())
        errors << QStringLiteral("name must not be empty");
    if (symbol.category.isEmpty()) {
        errors << QStringLiteral("category must not be empty");
    } else {
        for (const QString &segment : libraryCategoryPath(symbol.category)) {
            if (segment.isEmpty() || segment != segment.trimmed()) {
                errors << QStringLiteral("category \"%1\" must be '/'-separated non-empty "
                                         "segments without leading or trailing whitespace")
                              .arg(symbol.category);
                break;
            }
        }
    }
    if (symbol.referencePrefix.trimmed().isEmpty())
        errors << QStringLiteral("referencePrefix must not be empty");

    if (symbol.primitives.isEmpty())
        errors << QStringLiteral("must have at least one primitive");
    for (int i = 0; i < symbol.primitives.size(); ++i) {
        const SymbolPrimitive &primitive = symbol.primitives[i];
        const QString where = QStringLiteral("primitives[%1] ").arg(i);
        const int count = primitive.points.size();
        switch (primitive.kind) {
        case PrimitiveKind::Line:
            if (count != 2) errors << where + QStringLiteral("Line needs exactly 2 points");
            break;
        case PrimitiveKind::Polyline:
            if (count < 2) errors << where + QStringLiteral("Polyline needs at least 2 points");
            break;
        case PrimitiveKind::Polygon:
            if (count < 3) errors << where + QStringLiteral("Polygon needs at least 3 points");
            break;
        case PrimitiveKind::Circle:
            if (count != 1) errors << where + QStringLiteral("Circle needs exactly 1 point (its centre)");
            if (!std::isfinite(primitive.radius) || primitive.radius <= 0.0)
                errors << where + QStringLiteral("Circle radius must be a finite positive number");
            break;
        }
        for (const QPointF &point : primitive.points) {
            if (!finitePoint(point)) {
                errors << where + QStringLiteral("coordinates must be finite");
                break;
            }
        }
    }

    if (symbol.terminals.isEmpty())
        errors << QStringLiteral("must have at least one terminal");
    QSet<QString> names;
    QSet<QPair<qreal, qreal>> offsets;
    for (int i = 0; i < symbol.terminals.size(); ++i) {
        const SymbolTerminal &terminal = symbol.terminals[i];
        const QString where = QStringLiteral("terminals[%1] ").arg(i);
        if (terminal.name.trimmed().isEmpty())
            errors << where + QStringLiteral("name must not be empty");
        else if (names.contains(terminal.name))
            errors << where + QStringLiteral("duplicate terminal name \"%1\"").arg(terminal.name);
        names.insert(terminal.name);
        if (!finitePoint(terminal.offset)) {
            errors << where + QStringLiteral("offset must be finite");
            continue;
        }
        const QPair<qreal, qreal> key(terminal.offset.x(), terminal.offset.y());
        if (offsets.contains(key))
            errors << where + QStringLiteral("duplicate terminal offset");
        offsets.insert(key);
    }
    return errors;
}

std::optional<LibrarySymbol> SymbolLibrary::parseSymbol(const QJsonObject &object,
                                                        QStringList *errors)
{
    QStringList problems;
    LibrarySymbol symbol;

    auto readString = [&](const char *key, QString *out) {
        const QJsonValue value = object.value(QString::fromLatin1(key));
        if (!value.isString())
            problems << QStringLiteral("\"%1\" must be a string").arg(QString::fromLatin1(key));
        else
            *out = value.toString();
    };
    QString id;
    readString("id", &id);
    symbol.id = SymbolId(id);
    readString("name", &symbol.displayName);
    readString("category", &symbol.category);
    readString("referencePrefix", &symbol.referencePrefix);

    const QJsonValue shows = object.value(QStringLiteral("showsReference"));
    if (!shows.isBool())
        problems << QStringLiteral("\"showsReference\" must be a boolean");
    else
        symbol.showsReference = shows.toBool();

    const QJsonValue primitives = object.value(QStringLiteral("primitives"));
    if (!primitives.isArray()) {
        problems << QStringLiteral("\"primitives\" must be an array");
    } else {
        int index = 0;
        for (const QJsonValue &value : primitives.toArray()) {
            const QString where = QStringLiteral("primitives[%1] ").arg(index++);
            if (!value.isObject()) { problems << where + QStringLiteral("must be an object"); continue; }
            const QJsonObject entry = value.toObject();
            const auto kind = primitiveKindFromName(entry.value(QStringLiteral("kind")).toString());
            if (!kind) { problems << where + QStringLiteral("has an unknown kind"); continue; }
            SymbolPrimitive primitive;
            primitive.kind = *kind;
            const QJsonValue points = entry.value(QStringLiteral("points"));
            bool pointsOk = points.isArray();
            if (pointsOk) {
                for (const QJsonValue &pointValue : points.toArray()) {
                    QPointF point;
                    if (!readPoint(pointValue, &point)) { pointsOk = false; break; }
                    primitive.points.append(point);
                }
            }
            if (!pointsOk) { problems << where + QStringLiteral("points must be an array of [x, y] pairs"); continue; }
            const bool hasRadius = entry.contains(QStringLiteral("radius"));
            if (*kind == PrimitiveKind::Circle) {
                if (!entry.value(QStringLiteral("radius")).isDouble()) {
                    problems << where + QStringLiteral("Circle needs a numeric radius");
                    continue;
                }
                primitive.radius = entry.value(QStringLiteral("radius")).toDouble();
            } else if (hasRadius) {
                problems << where + QStringLiteral("radius is only valid for Circle");
                continue;
            }
            symbol.primitives.append(primitive);
        }
    }

    const QJsonValue terminals = object.value(QStringLiteral("terminals"));
    if (!terminals.isArray()) {
        problems << QStringLiteral("\"terminals\" must be an array");
    } else {
        int index = 0;
        for (const QJsonValue &value : terminals.toArray()) {
            const QString where = QStringLiteral("terminals[%1] ").arg(index++);
            const QJsonObject entry = value.toObject();
            if (!value.isObject() || !entry.value(QStringLiteral("name")).isString()
                || !entry.value(QStringLiteral("x")).isDouble()
                || !entry.value(QStringLiteral("y")).isDouble()) {
                problems << where + QStringLiteral("needs a string name and numeric x and y");
                continue;
            }
            symbol.terminals.append({entry.value(QStringLiteral("name")).toString(),
                                     QPointF(entry.value(QStringLiteral("x")).toDouble(),
                                             entry.value(QStringLiteral("y")).toDouble())});
        }
    }

    if (problems.isEmpty())
        problems = validateLibrarySymbol(symbol);
    if (!problems.isEmpty()) {
        appendAll(errors, label(symbol.id) + QStringLiteral(": "), problems);
        return std::nullopt;
    }
    return symbol;
}

bool SymbolLibrary::add(const LibrarySymbol &symbol, QStringList *errors)
{
    QStringList problems = validateLibrarySymbol(symbol);
    if (problems.isEmpty() && m_index.contains(symbol.id))
        problems << QStringLiteral("duplicate symbol id");
    if (!problems.isEmpty()) {
        appendAll(errors, label(symbol.id) + QStringLiteral(": "), problems);
        return false;
    }
    m_index.insert(symbol.id, m_symbols.size());
    m_symbols.append(symbol);
    return true;
}

bool SymbolLibrary::addFromJson(const QJsonObject &object, QStringList *errors)
{
    const std::optional<LibrarySymbol> symbol = parseSymbol(object, errors);
    return symbol && add(*symbol, errors);
}

SymbolLibrary SymbolLibrary::loadFromDirectory(const QString &directory, QStringList *errors)
{
    QStringList local;
    auto readJson = [&local](const QString &path, QJsonObject *out) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            local << QStringLiteral("%1: could not open (%2)").arg(path, file.errorString());
            return false;
        }
        QJsonParseError parseError;
        const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
            local << QStringLiteral("%1: invalid JSON object (%2)").arg(path, parseError.errorString());
            return false;
        }
        *out = doc.object();
        return true;
    };

    SymbolLibrary library;
    QJsonObject manifest;
    if (readJson(directory + QStringLiteral("/manifest.json"), &manifest)) {
        const QJsonValue listed = manifest.value(QStringLiteral("symbols"));
        if (!listed.isArray()) {
            local << QStringLiteral("%1/manifest.json: \"symbols\" must be an array of file names")
                         .arg(directory);
        } else {
            for (const QJsonValue &entry : listed.toArray()) {
                if (!entry.isString()) {
                    local << QStringLiteral("%1/manifest.json: file names must be strings").arg(directory);
                    continue;
                }
                const QString path = directory + QLatin1Char('/') + entry.toString();
                QJsonObject object;
                if (!readJson(path, &object))
                    continue;
                QStringList entryErrors;
                library.addFromJson(object, &entryErrors);
                appendAll(&local, path + QStringLiteral(": "), entryErrors);
            }
        }
    }

    if (!local.isEmpty()) {
        if (errors)
            errors->append(local);
        return SymbolLibrary();
    }
    return library;
}

const LibrarySymbol *SymbolLibrary::find(const SymbolId &id) const
{
    const auto it = m_index.constFind(id);
    return it == m_index.cend() ? nullptr : &m_symbols[it.value()];
}

const SymbolLibrary &builtinSymbolLibrary()
{
    static const SymbolLibrary library = [] {
        QStringList errors;
        SymbolLibrary loaded = SymbolLibrary::loadFromDirectory(QStringLiteral(":/symbols"), &errors);
        if (!errors.isEmpty())
            qFatal("Built-in symbol library is invalid:\n%s", qPrintable(errors.join(QLatin1Char('\n'))));
        return loaded;
    }();
    return library;
}
