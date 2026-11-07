#include "symbollibrary.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QPair>
#include <QSet>

#include <algorithm>
#include <cmath>
#include <limits>

namespace {

std::optional<PrimitiveKind> primitiveKindFromName(const QString &name)
{
    if (name == QLatin1String("Line"))     return PrimitiveKind::Line;
    if (name == QLatin1String("Polyline")) return PrimitiveKind::Polyline;
    if (name == QLatin1String("Polygon"))  return PrimitiveKind::Polygon;
    if (name == QLatin1String("Circle"))   return PrimitiveKind::Circle;
    if (name == QLatin1String("Path"))     return PrimitiveKind::Path;
    return std::nullopt;
}

QString primitiveKindName(PrimitiveKind kind)
{
    switch (kind) {
    case PrimitiveKind::Line: return QStringLiteral("Line");
    case PrimitiveKind::Polyline: return QStringLiteral("Polyline");
    case PrimitiveKind::Polygon: return QStringLiteral("Polygon");
    case PrimitiveKind::Circle: return QStringLiteral("Circle");
    case PrimitiveKind::Path: return QStringLiteral("Path");
    }
    return {};
}

std::optional<PathSegmentKind> pathSegmentKindFromCode(const QString &code)
{
    if (code == QLatin1String("M")) return PathSegmentKind::MoveTo;
    if (code == QLatin1String("L")) return PathSegmentKind::LineTo;
    if (code == QLatin1String("C")) return PathSegmentKind::CubicTo;
    if (code == QLatin1String("Z")) return PathSegmentKind::Close;
    return std::nullopt;
}

int pathSegmentPointCount(PathSegmentKind kind)
{
    switch (kind) {
    case PathSegmentKind::MoveTo:
    case PathSegmentKind::LineTo:
        return 1;
    case PathSegmentKind::CubicTo:
        return 3;
    case PathSegmentKind::Close:
        return 0;
    }
    return -1;
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

void includePoint(const QPointF &point, bool *hasBounds,
                  qreal *minX, qreal *minY, qreal *maxX, qreal *maxY)
{
    if (!finitePoint(point))
        return;
    if (!*hasBounds) {
        *minX = *maxX = point.x();
        *minY = *maxY = point.y();
        *hasBounds = true;
        return;
    }
    *minX = std::min(*minX, point.x());
    *minY = std::min(*minY, point.y());
    *maxX = std::max(*maxX, point.x());
    *maxY = std::max(*maxY, point.y());
}

qreal cubicCoordinate(qreal p0, qreal p1, qreal p2, qreal p3, qreal t)
{
    const qreal u = 1.0 - t;
    return u * u * u * p0 + 3.0 * u * u * t * p1
        + 3.0 * u * t * t * p2 + t * t * t * p3;
}

void appendCubicExtremaRoots(qreal p0, qreal p1, qreal p2, qreal p3,
                             QVector<qreal> *roots)
{
    // B'(t) / 3 = a*t^2 + b*t + c.
    const qreal a = -p0 + 3.0 * p1 - 3.0 * p2 + p3;
    const qreal b = 2.0 * (p0 - 2.0 * p1 + p2);
    const qreal c = p1 - p0;
    const qreal scale = std::max({qreal(1.0), std::abs(p0), std::abs(p1),
                                  std::abs(p2), std::abs(p3)});
    const qreal epsilon = std::numeric_limits<qreal>::epsilon() * scale * 16.0;

    if (std::abs(a) <= epsilon) {
        if (std::abs(b) > epsilon) {
            const qreal t = -c / b;
            if (t > 0.0 && t < 1.0)
                roots->append(t);
        }
        return;
    }

    const qreal discriminant = b * b - 4.0 * a * c;
    if (discriminant < 0.0)
        return;
    const qreal sqrtDiscriminant = std::sqrt(discriminant);
    const qreal denominator = 2.0 * a;
    const qreal first = (-b + sqrtDiscriminant) / denominator;
    const qreal second = (-b - sqrtDiscriminant) / denominator;
    if (first > 0.0 && first < 1.0)
        roots->append(first);
    if (second > 0.0 && second < 1.0 && second != first)
        roots->append(second);
}

void includeCubicBounds(const QPointF &p0, const QPointF &p1,
                         const QPointF &p2, const QPointF &p3, bool *hasBounds,
                         qreal *minX, qreal *minY, qreal *maxX, qreal *maxY)
{
    includePoint(p0, hasBounds, minX, minY, maxX, maxY);
    includePoint(p3, hasBounds, minX, minY, maxX, maxY);
    QVector<qreal> roots;
    appendCubicExtremaRoots(p0.x(), p1.x(), p2.x(), p3.x(), &roots);
    appendCubicExtremaRoots(p0.y(), p1.y(), p2.y(), p3.y(), &roots);
    for (qreal t : roots) {
        includePoint(QPointF(cubicCoordinate(p0.x(), p1.x(), p2.x(), p3.x(), t),
                             cubicCoordinate(p0.y(), p1.y(), p2.y(), p3.y(), t)),
                     hasBounds, minX, minY, maxX, maxY);
    }
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
        if (primitive.fillColor != PrimitiveFillColor::White
            && primitive.fillColor != PrimitiveFillColor::Black)
            errors << where + QStringLiteral("has an unknown fill colour");
        if (primitive.fillColor == PrimitiveFillColor::Black
            && (!primitiveIsFilled(primitive) || primitive.kind == PrimitiveKind::Line
                || primitive.kind == PrimitiveKind::Polyline))
            errors << where + QStringLiteral("black fill requires a filled Polygon, Circle or Path");
        // Legacy white fill flags on open primitives remain accepted.
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
        case PrimitiveKind::Path: {
            if (primitive.segments.isEmpty()) {
                errors << where + QStringLiteral("Path needs at least one segment");
                break;
            }
            if (primitive.fillRule != PathFillRule::NonZero
                && primitive.fillRule != PathFillRule::EvenOdd) {
                errors << where + QStringLiteral("Path has an unknown fill rule");
            }
            if (primitive.segments.first().kind != PathSegmentKind::MoveTo)
                errors << where + QStringLiteral("Path must start with MoveTo");
            for (int j = 0; j < primitive.segments.size(); ++j) {
                const PathSegment &segment = primitive.segments[j];
                const QString segmentWhere = where + QStringLiteral("segments[%1] ").arg(j);
                const int expected = pathSegmentPointCount(segment.kind);
                if (expected < 0) {
                    errors << segmentWhere + QStringLiteral("has an unknown segment kind");
                    continue;
                }
                if (segment.points.size() != expected) {
                    errors << segmentWhere + QStringLiteral("has the wrong number of points");
                    continue;
                }
                for (const QPointF &point : segment.points) {
                    if (!finitePoint(point)) {
                        errors << segmentWhere + QStringLiteral("coordinates must be finite");
                        break;
                    }
                }
            }
            break;
        }
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

QRectF symbolBounds(const LibrarySymbol &symbol)
{
    bool hasBounds = false;
    qreal minX = 0.0;
    qreal minY = 0.0;
    qreal maxX = 0.0;
    qreal maxY = 0.0;

    for (const SymbolPrimitive &primitive : symbol.primitives) {
        if (primitive.kind == PrimitiveKind::Circle && primitive.points.size() == 1
            && std::isfinite(primitive.radius) && primitive.radius > 0.0) {
            const QPointF centre = primitive.points.first();
            includePoint(QPointF(centre.x() - primitive.radius,
                                 centre.y() - primitive.radius),
                         &hasBounds, &minX, &minY, &maxX, &maxY);
            includePoint(QPointF(centre.x() + primitive.radius,
                                 centre.y() + primitive.radius),
                         &hasBounds, &minX, &minY, &maxX, &maxY);
            continue;
        }

        if (primitive.kind != PrimitiveKind::Path) {
            for (const QPointF &point : primitive.points)
                includePoint(point, &hasBounds, &minX, &minY, &maxX, &maxY);
            continue;
        }

        QPointF current;
        QPointF subpathStart;
        bool hasCurrent = false;
        for (const PathSegment &segment : primitive.segments) {
            switch (segment.kind) {
            case PathSegmentKind::MoveTo:
                if (segment.points.size() != 1)
                    break;
                current = subpathStart = segment.points.first();
                hasCurrent = true;
                includePoint(current, &hasBounds, &minX, &minY, &maxX, &maxY);
                break;
            case PathSegmentKind::LineTo:
                if (!hasCurrent || segment.points.size() != 1)
                    break;
                current = segment.points.first();
                includePoint(current, &hasBounds, &minX, &minY, &maxX, &maxY);
                break;
            case PathSegmentKind::CubicTo:
                if (!hasCurrent || segment.points.size() != 3)
                    break;
                includeCubicBounds(current, segment.points[0], segment.points[1],
                                   segment.points[2], &hasBounds, &minX, &minY,
                                   &maxX, &maxY);
                current = segment.points[2];
                break;
            case PathSegmentKind::Close:
                if (!hasCurrent || !segment.points.isEmpty())
                    break;
                current = subpathStart;
                includePoint(current, &hasBounds, &minX, &minY, &maxX, &maxY);
                break;
            }
        }
    }

    return hasBounds ? QRectF(QPointF(minX, minY), QPointF(maxX, maxY)) : QRectF{};
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

            if (entry.contains(QStringLiteral("fillColor"))) {
                const auto color = entry.value(QStringLiteral("fillColor"));
                if (!color.isString() || (color.toString() != QLatin1String("white")
                                         && color.toString() != QLatin1String("black")))
                    problems << where + QStringLiteral("fillColor must be white or black");
                else if (color.toString() == QLatin1String("black"))
                    primitive.fillColor = PrimitiveFillColor::Black;
            }

            if (entry.contains(QStringLiteral("filled"))) {
                const QJsonValue filled = entry.value(QStringLiteral("filled"));
                if (!filled.isBool())
                    problems << where + QStringLiteral("filled must be a boolean");
                else
                    primitive.filled = filled.toBool();
            }

            const bool hasRadius = entry.contains(QStringLiteral("radius"));
            if (*kind == PrimitiveKind::Path) {
                if (entry.contains(QStringLiteral("points")))
                    problems << where + QStringLiteral("points are not valid for Path");
                if (hasRadius)
                    problems << where + QStringLiteral("radius is only valid for Circle");

                if (entry.contains(QStringLiteral("fillRule"))) {
                    const QJsonValue fillRule = entry.value(QStringLiteral("fillRule"));
                    if (!fillRule.isString()) {
                        problems << where + QStringLiteral("fillRule must be a string");
                    } else if (fillRule.toString() == QLatin1String("nonzero")) {
                        primitive.fillRule = PathFillRule::NonZero;
                    } else if (fillRule.toString() == QLatin1String("evenodd")) {
                        primitive.fillRule = PathFillRule::EvenOdd;
                    } else {
                        problems << where + QStringLiteral("fillRule must be nonzero or evenodd");
                    }
                }

                const QJsonValue segments = entry.value(QStringLiteral("segments"));
                if (!segments.isArray()) {
                    problems << where + QStringLiteral("segments must be an array");
                } else {
                    int segmentIndex = 0;
                    for (const QJsonValue &segmentValue : segments.toArray()) {
                        const QString segmentWhere = where
                            + QStringLiteral("segments[%1] ").arg(segmentIndex++);
                        if (!segmentValue.isArray()) {
                            problems << segmentWhere + QStringLiteral("must be an array");
                            continue;
                        }
                        const QJsonArray array = segmentValue.toArray();
                        if (array.isEmpty() || !array.first().isString()) {
                            problems << segmentWhere + QStringLiteral("needs an operation code");
                            continue;
                        }
                        const auto segmentKind = pathSegmentKindFromCode(array.first().toString());
                        if (!segmentKind) {
                            problems << segmentWhere + QStringLiteral("has an unknown operation code");
                            continue;
                        }
                        const int pointCount = pathSegmentPointCount(*segmentKind);
                        if (array.size() != 1 + pointCount * 2) {
                            problems << segmentWhere + QStringLiteral("has the wrong number of coordinates");
                            continue;
                        }
                        PathSegment segment;
                        segment.kind = *segmentKind;
                        bool coordinatesOk = true;
                        for (int i = 1; i < array.size(); i += 2) {
                            if (!array[i].isDouble() || !array[i + 1].isDouble()) {
                                coordinatesOk = false;
                                break;
                            }
                            segment.points.append(QPointF(array[i].toDouble(),
                                                          array[i + 1].toDouble()));
                        }
                        if (!coordinatesOk) {
                            problems << segmentWhere + QStringLiteral("coordinates must be numeric pairs");
                            continue;
                        }
                        primitive.segments.append(std::move(segment));
                    }
                }
            } else {
                if (entry.contains(QStringLiteral("segments")))
                    problems << where + QStringLiteral("segments are only valid for Path");
                if (entry.contains(QStringLiteral("fillRule")))
                    problems << where + QStringLiteral("fillRule is only valid for Path");

                const QJsonValue points = entry.value(QStringLiteral("points"));
                bool pointsOk = points.isArray();
                if (pointsOk) {
                    for (const QJsonValue &pointValue : points.toArray()) {
                        QPointF point;
                        if (!readPoint(pointValue, &point)) { pointsOk = false; break; }
                        primitive.points.append(point);
                    }
                }
                if (!pointsOk)
                    problems << where + QStringLiteral("points must be an array of [x, y] pairs");
            }

            if (*kind == PrimitiveKind::Circle) {
                if (!entry.value(QStringLiteral("radius")).isDouble()) {
                    problems << where + QStringLiteral("Circle needs a numeric radius");
                    continue;
                }
                primitive.radius = entry.value(QStringLiteral("radius")).toDouble();
            } else if (*kind != PrimitiveKind::Path && hasRadius) {
                problems << where + QStringLiteral("radius is only valid for Circle");
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

QJsonObject SymbolLibrary::symbolToJson(const LibrarySymbol &symbol)
{
    QJsonArray primitives;
    for (const SymbolPrimitive &primitive : symbol.primitives) {
        QJsonObject entry{{QStringLiteral("kind"), primitiveKindName(primitive.kind)}};
        if (primitive.kind == PrimitiveKind::Path) {
            QJsonArray segments;
            for (const PathSegment &segment : primitive.segments) {
                QString code;
                switch (segment.kind) {
                case PathSegmentKind::MoveTo: code = QStringLiteral("M"); break;
                case PathSegmentKind::LineTo: code = QStringLiteral("L"); break;
                case PathSegmentKind::CubicTo: code = QStringLiteral("C"); break;
                case PathSegmentKind::Close: code = QStringLiteral("Z"); break;
                }
                QJsonArray coordinates{code};
                for (const QPointF &point : segment.points) {
                    coordinates.append(point.x());
                    coordinates.append(point.y());
                }
                segments.append(coordinates);
            }
            entry.insert(QStringLiteral("segments"), segments);
            entry.insert(QStringLiteral("fillRule"), primitive.fillRule == PathFillRule::EvenOdd
                               ? QStringLiteral("evenodd") : QStringLiteral("nonzero"));
        } else {
            QJsonArray points;
            for (const QPointF &point : primitive.points)
                points.append(QJsonArray{point.x(), point.y()});
            entry.insert(QStringLiteral("points"), points);
            if (primitive.kind == PrimitiveKind::Circle)
                entry.insert(QStringLiteral("radius"), primitive.radius);
        }
        if (primitive.filled.has_value())
            entry.insert(QStringLiteral("filled"), *primitive.filled);
        if (primitive.fillColor == PrimitiveFillColor::Black)
            entry.insert(QStringLiteral("fillColor"), QStringLiteral("black"));
        primitives.append(entry);
    }
    QJsonArray terminals;
    for (const SymbolTerminal &terminal : symbol.terminals) {
        terminals.append(QJsonObject{{QStringLiteral("name"), terminal.name},
                                     {QStringLiteral("x"), terminal.offset.x()},
                                     {QStringLiteral("y"), terminal.offset.y()}});
    }
    return {{QStringLiteral("id"), symbol.id.value()},
            {QStringLiteral("name"), symbol.displayName},
            {QStringLiteral("category"), symbol.category},
            {QStringLiteral("referencePrefix"), symbol.referencePrefix},
            {QStringLiteral("showsReference"), symbol.showsReference},
            {QStringLiteral("primitives"), primitives},
            {QStringLiteral("terminals"), terminals}};
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

bool SymbolLibrary::replace(const LibrarySymbol &symbol, QStringList *errors)
{
    QStringList problems = validateLibrarySymbol(symbol);
    const auto it = m_index.constFind(symbol.id);
    if (problems.isEmpty() && it == m_index.cend())
        problems << QStringLiteral("symbol id does not exist");
    if (!problems.isEmpty()) {
        appendAll(errors, label(symbol.id) + QStringLiteral(": "), problems);
        return false;
    }
    m_symbols[it.value()] = symbol;
    return true;
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

SymbolLibrary &applicationSymbolLibrary()
{
    static SymbolLibrary library = builtinSymbolLibrary();
    return library;
}

bool sameTerminalSignature(const LibrarySymbol &a, const LibrarySymbol &b)
{
    if (a.terminals.size() != b.terminals.size()) return false;
    for (int i = 0; i < a.terminals.size(); ++i)
        if (a.terminals[i].name != b.terminals[i].name || a.terminals[i].offset != b.terminals[i].offset) return false;
    return true;
}
