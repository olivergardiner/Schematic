#include "symbolsvg.h"

#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include <algorithm>
#include <cmath>
#include <limits>

namespace {

constexpr qreal kCurveTolerance = 0.1;
constexpr qreal kPi = 3.1415926535897932384626433832795;

struct Affine
{
    qreal a = 1, b = 0, c = 0, d = 1, e = 0, f = 0;
    QPointF map(const QPointF &p) const
    { return QPointF(a * p.x() + c * p.y() + e, b * p.x() + d * p.y() + f); }
};

Affine multiply(const Affine &l, const Affine &r)
{
    return {l.a*r.a + l.c*r.b, l.b*r.a + l.d*r.b,
            l.a*r.c + l.c*r.d, l.b*r.c + l.d*r.d,
            l.a*r.e + l.c*r.f + l.e, l.b*r.e + l.d*r.f + l.f};
}

bool finite(qreal value) { return std::isfinite(value); }
bool finitePoint(const QPointF &p) { return finite(p.x()) && finite(p.y()); }

QString localName(const QXmlStreamReader &xml) { return xml.name().toString(); }

struct PaintState
{
    QString fill = QStringLiteral("black");
    QString fillRule = QStringLiteral("nonzero");
    bool displayed = true;
};

struct ImportContext
{
    LibrarySymbol symbol;
    QPointF origin;
    QStringList warnings;
    QStringList errors;
    bool styleWarning = false;
};

bool readNumberList(const QString &source, QVector<qreal> *values)
{
    static const QRegularExpression token(
        QStringLiteral(R"([+-]?(?:(?:\d+\.?\d*)|(?:\.\d+))(?:[eE][+-]?\d+)?)"));
    int position = 0;
    auto isSeparator = [](QChar c) { return c.isSpace() || c == QLatin1Char(','); };
    while (position < source.size()) {
        while (position < source.size() && isSeparator(source[position])) ++position;
        if (position == source.size()) break;
        const auto match = token.match(source, position);
        if (!match.hasMatch() || match.capturedStart() != position) return false;
        bool ok = false;
        const qreal value = match.captured().toDouble(&ok);
        if (!ok || !finite(value)) return false;
        values->append(value);
        position = match.capturedEnd();
    }
    return true;
}

bool parseTransform(const QString &text, Affine *out)
{
    static const QRegularExpression expression(
        QStringLiteral(R"(([A-Za-z]+)\s*\(([^)]*)\))"));
    Affine result;
    int position = 0;
    auto onlySeparators = [](const QString &s) {
        for (QChar c : s) if (!c.isSpace() && c != QLatin1Char(',')) return false;
        return true;
    };
    while (position < text.size()) {
        while (position < text.size() && (text[position].isSpace() || text[position] == QLatin1Char(','))) ++position;
        if (position == text.size()) break;
        const auto match = expression.match(text, position);
        if (!match.hasMatch() || match.capturedStart() != position) return false;
        QVector<qreal> v;
        if (!readNumberList(match.captured(2), &v)) return false;
        const QString name = match.captured(1);
        Affine op;
        if (name == QLatin1String("matrix") && v.size() == 6)
            op = {v[0], v[1], v[2], v[3], v[4], v[5]};
        else if (name == QLatin1String("translate") && (v.size() == 1 || v.size() == 2)) {
            op.e = v[0]; op.f = v.size() == 2 ? v[1] : 0;
        } else if (name == QLatin1String("scale") && (v.size() == 1 || v.size() == 2)) {
            op.a = v[0]; op.d = v.size() == 2 ? v[1] : v[0];
        } else if (name == QLatin1String("rotate") && (v.size() == 1 || v.size() == 3)) {
            const qreal angle = v[0] * kPi / 180.0;
            const qreal cs = std::cos(angle), sn = std::sin(angle);
            op.a = cs; op.b = sn; op.c = -sn; op.d = cs;
            if (v.size() == 3) {
                op.e = v[1] - cs*v[1] + sn*v[2];
                op.f = v[2] - sn*v[1] - cs*v[2];
            }
        } else if (name == QLatin1String("skewX") && v.size() == 1) {
            op.c = std::tan(v[0] * kPi / 180.0);
        } else if (name == QLatin1String("skewY") && v.size() == 1) {
            op.b = std::tan(v[0] * kPi / 180.0);
        } else return false;
        // SVG transform lists compose in written order: later operations act
        // on the already transformed coordinate system.
        result = multiply(result, op);
        position = match.capturedEnd();
    }
    if (!onlySeparators(text.mid(position))) return false;
    *out = result;
    return finite(result.a) && finite(result.b) && finite(result.c)
        && finite(result.d) && finite(result.e) && finite(result.f);
}

struct PathToken { bool command = false; QChar letter; qreal number = 0; };

bool tokenizePath(const QString &data, QVector<PathToken> *tokens)
{
    static const QRegularExpression token(
        QStringLiteral(R"([A-Za-z]|[+-]?(?:(?:\d+\.?\d*)|(?:\.\d+))(?:[eE][+-]?\d+)?)"));
    int position = 0;
    while (position < data.size()) {
        if (data[position].isSpace() || data[position] == QLatin1Char(',')) { ++position; continue; }
        const auto match = token.match(data, position);
        if (!match.hasMatch() || match.capturedStart() != position) return false;
        const QString part = match.captured();
        if (part.size() == 1 && part[0].isLetter()) tokens->append({true, part[0], 0});
        else {
            bool ok = false;
            const qreal n = part.toDouble(&ok);
            if (!ok || !finite(n)) return false;
            tokens->append({false, {}, n});
        }
        position = match.capturedEnd();
    }
    return true;
}

QPointF add(const QPointF &a, const QPointF &b) { return {a.x()+b.x(), a.y()+b.y()}; }
QPointF sub(const QPointF &a, const QPointF &b) { return {a.x()-b.x(), a.y()-b.y()}; }
QPointF scale(const QPointF &a, qreal s) { return {a.x()*s, a.y()*s}; }

void appendLine(SymbolPrimitive *p, const QPointF &point)
{ p->segments.append({PathSegmentKind::LineTo, {point}}); }
void appendCubic(SymbolPrimitive *p, const QPointF &c1, const QPointF &c2, const QPointF &end)
{ p->segments.append({PathSegmentKind::CubicTo, {c1, c2, end}}); }

bool appendArc(SymbolPrimitive *path, QPointF from, qreal rx, qreal ry, qreal rotation,
               bool largeArc, bool sweep, QPointF to, const Affine &transform)
{
    rx = std::abs(rx); ry = std::abs(ry);
    if (rx == 0 || ry == 0) { appendLine(path, to); return true; }
    if (from == to) return true;
    const qreal phi = std::fmod(rotation, 360.0) * kPi / 180.0;
    const qreal cs = std::cos(phi), sn = std::sin(phi);
    const qreal dx = (from.x() - to.x()) / 2.0;
    const qreal dy = (from.y() - to.y()) / 2.0;
    const qreal xp = cs*dx + sn*dy;
    const qreal yp = -sn*dx + cs*dy;
    const qreal lambda = xp*xp/(rx*rx) + yp*yp/(ry*ry);
    if (lambda > 1) { const qreal adjust = std::sqrt(lambda); rx *= adjust; ry *= adjust; }
    const qreal numerator = std::max(qreal(0), rx*rx*ry*ry - rx*rx*yp*yp - ry*ry*xp*xp);
    const qreal denominator = rx*rx*yp*yp + ry*ry*xp*xp;
    if (denominator == 0) { appendLine(path, to); return true; }
    const qreal sign = (largeArc == sweep) ? -1.0 : 1.0;
    const qreal factor = sign * std::sqrt(numerator / denominator);
    const qreal cxp = factor * rx * yp / ry;
    const qreal cyp = factor * -ry * xp / rx;
    const QPointF center(cs*cxp - sn*cyp + (from.x()+to.x())/2.0,
                         sn*cxp + cs*cyp + (from.y()+to.y())/2.0);
    auto angle = [](qreal ux, qreal uy, qreal vx, qreal vy) {
        return std::atan2(ux*vy - uy*vx, ux*vx + uy*vy);
    };
    const qreal ux = (xp - cxp) / rx, uy = (yp - cyp) / ry;
    const qreal vx = (-xp - cxp) / rx, vy = (-yp - cyp) / ry;
    qreal start = std::atan2(uy, ux);
    qreal delta = angle(ux, uy, vx, vy);
    if (!sweep && delta > 0) delta -= 2*kPi;
    if (sweep && delta < 0) delta += 2*kPi;
    // Bound approximation error in the transformed space by limiting each
    // arc piece using the largest singular value of the affine linear part.
    const qreal trace = transform.a*transform.a + transform.b*transform.b
        + transform.c*transform.c + transform.d*transform.d;
    const qreal det = transform.a*transform.d - transform.b*transform.c;
    const qreal maxScale = std::sqrt(std::max(qreal(0),
        (trace + std::sqrt(std::max(qreal(0), trace*trace - 4*det*det))) / 2));
    const qreal maxRadius = std::max(rx, ry) * maxScale;
    const qreal allowedAngle = maxRadius <= kCurveTolerance ? kPi/2
        : 2 * std::acos(std::max(qreal(-1), 1 - kCurveTolerance/maxRadius));
    const qreal stepLimit = std::max(qreal(0.01), std::min(kPi/2, allowedAngle));
    const int count = std::max(1, static_cast<int>(std::ceil(std::abs(delta) / stepLimit)));
    const qreal step = delta / count;
    auto point = [&](qreal theta) {
        return QPointF(center.x() + rx*cs*std::cos(theta) - ry*sn*std::sin(theta),
                       center.y() + rx*sn*std::cos(theta) + ry*cs*std::sin(theta));
    };
    auto derivative = [&](qreal theta) {
        return QPointF(-rx*cs*std::sin(theta) - ry*sn*std::cos(theta),
                       -rx*sn*std::sin(theta) + ry*cs*std::cos(theta));
    };
    for (int i=0; i<count; ++i) {
        const qreal a0 = start, a1 = start + step;
        const qreal alpha = 4.0/3.0 * std::tan((a1-a0)/4.0);
        appendCubic(path, transform.map(add(point(a0), scale(derivative(a0), alpha))),
                    transform.map(sub(point(a1), scale(derivative(a1), alpha))),
                    transform.map(point(a1)));
        start = a1;
    }
    if (!path->segments.isEmpty()) path->segments.last().points.last() = transform.map(to);
    return true;
}

bool parsePathData(const QString &data, const Affine &tx, SymbolPrimitive *primitive)
{
    QVector<PathToken> tokens;
    if (!tokenizePath(data, &tokens) || tokens.isEmpty()) return false;
    int i = 0;
    QChar command;
    QPointF current, start, lastCubic, lastQuadratic;
    bool hasCubic = false, hasQuadratic = false;
    auto isCommand = [&] { return i < tokens.size() && tokens[i].command; };
    auto take = [&](qreal *v) {
        if (i >= tokens.size() || tokens[i].command) return false;
        *v = tokens[i++].number; return true;
    };
    auto point = [&](bool relative, QPointF *p) {
        qreal x, y;
        if (!take(&x) || !take(&y)) return false;
        *p = QPointF(x,y); if (relative) *p = add(*p,current); return finitePoint(*p);
    };
    auto mapped = [&](const QPointF &p) { return tx.map(p); };
    while (i < tokens.size()) {
        if (isCommand()) command = tokens[i++].letter;
        else if (command.isNull()) return false;
        const bool relative = command.isLower();
        const QChar upper = command.toUpper();
        if (upper == QLatin1Char('Z')) {
            primitive->segments.append({PathSegmentKind::Close, {}});
            current = start; hasCubic = hasQuadratic = false; command = {};
            continue;
        }
        if (upper == QLatin1Char('M') || upper == QLatin1Char('L')
            || upper == QLatin1Char('T')) {
            QPointF p;
            if (!point(relative, &p)) return false;
            if (upper == QLatin1Char('M')) {
                current = start = p;
                primitive->segments.append({PathSegmentKind::MoveTo, {mapped(p)}});
                command = relative ? QLatin1Char('l') : QLatin1Char('L');
            } else if (upper == QLatin1Char('T')) {
                const QPointF control = hasQuadratic ? sub(scale(current,2), lastQuadratic) : current;
                const QPointF c1 = add(current, scale(sub(control,current), 2.0/3.0));
                const QPointF c2 = add(p, scale(sub(control,p), 2.0/3.0));
                appendCubic(primitive, mapped(c1), mapped(c2), mapped(p));
                current = p; lastQuadratic = control; hasQuadratic = true;
                hasCubic = false;
            } else { appendLine(primitive, mapped(p)); current = p; hasCubic = hasQuadratic = false; }
            continue;
        }
        if (upper == QLatin1Char('H') || upper == QLatin1Char('V')) {
            qreal v; if (!take(&v)) return false;
            QPointF p = current;
            if (upper == QLatin1Char('H')) p.setX(relative ? current.x()+v : v);
            else p.setY(relative ? current.y()+v : v);
            appendLine(primitive, mapped(p)); current = p; hasCubic = hasQuadratic = false;
            continue;
        }
        if (upper == QLatin1Char('C') || upper == QLatin1Char('S')) {
            QPointF c1, c2, end;
            if (upper == QLatin1Char('C')) { if (!point(relative,&c1) || !point(relative,&c2) || !point(relative,&end)) return false; }
            else {
                c1 = hasCubic ? sub(scale(current,2), lastCubic) : current;
                if (!point(relative,&c2) || !point(relative,&end)) return false;
            }
            appendCubic(primitive, mapped(c1), mapped(c2), mapped(end));
            current = end; lastCubic = c2; hasCubic = true; hasQuadratic = false;
            continue;
        }
        if (upper == QLatin1Char('Q')) {
            QPointF control, end; if (!point(relative,&control) || !point(relative,&end)) return false;
            const QPointF c1 = add(current, scale(sub(control,current), 2.0/3.0));
            const QPointF c2 = add(end, scale(sub(control,end), 2.0/3.0));
            appendCubic(primitive, mapped(c1), mapped(c2), mapped(end));
            current = end; lastQuadratic = control; hasQuadratic = true; hasCubic = false;
            continue;
        }
        if (upper == QLatin1Char('A')) {
            qreal rx, ry, rotation, large, sweep;
            QPointF end;
            if (!take(&rx) || !take(&ry) || !take(&rotation) || !take(&large) || !take(&sweep)
                || (large != 0 && large != 1) || (sweep != 0 && sweep != 1)
                || !point(relative, &end)) return false;
            appendArc(primitive, current, rx, ry, rotation, large != 0, sweep != 0, end, tx);
            current = end; hasCubic = hasQuadratic = false;
            continue;
        }
        return false;
    }
    for (const PathSegment &segment : primitive->segments)
        for (const QPointF &p : segment.points) if (!finitePoint(p)) return false;
    const bool hasDrawing = std::any_of(primitive->segments.cbegin(), primitive->segments.cend(),
        [](const PathSegment &s) { return s.kind == PathSegmentKind::LineTo
            || s.kind == PathSegmentKind::CubicTo; });
    return !primitive->segments.isEmpty()
        && primitive->segments.first().kind == PathSegmentKind::MoveTo && hasDrawing;
}

QString attribute(const QXmlStreamAttributes &attrs, const QString &name)
{ return attrs.value(name).toString(); }

PaintState applyPaint(const QXmlStreamAttributes &attrs, PaintState state, ImportContext *ctx)
{
    QHash<QString, QString> style;
    for (const QString &key : {QStringLiteral("fill"), QStringLiteral("fill-rule"), QStringLiteral("display")}) {
        if (attrs.hasAttribute(key)) style.insert(key, attrs.value(key).toString());
    }
    const QString inlineStyle = attribute(attrs, QStringLiteral("style"));
    if (!inlineStyle.isEmpty()) {
        const auto parts = inlineStyle.split(QLatin1Char(';'), Qt::SkipEmptyParts);
        for (const QString &part : parts) {
            const int colon = part.indexOf(QLatin1Char(':'));
            if (colon < 0) { ctx->styleWarning = true; continue; }
            const QString key = part.left(colon).trimmed();
            const QString value = part.mid(colon+1).trimmed();
            if (key == QLatin1String("fill") || key == QLatin1String("fill-rule") || key == QLatin1String("display"))
                style.insert(key,value);
            else ctx->styleWarning = true;
        }
    }
    if (style.contains(QStringLiteral("fill"))) state.fill = style.value(QStringLiteral("fill")).trimmed().toLower();
    if (style.contains(QStringLiteral("fill-rule"))) state.fillRule = style.value(QStringLiteral("fill-rule")).trimmed().toLower();
    if (style.value(QStringLiteral("display")).trimmed().toLower() == QLatin1String("none")) state.displayed = false;
    for (const auto &a : attrs) {
        const QString name = a.name().toString();
        if (name != QLatin1String("id") && name != QLatin1String("transform")
            && name != QLatin1String("fill") && name != QLatin1String("fill-rule")
            && name != QLatin1String("display") && name != QLatin1String("style")
            && name != QLatin1String("xmlns") && name != QLatin1String("x")
            && name != QLatin1String("y") && name != QLatin1String("x1")
            && name != QLatin1String("y1") && name != QLatin1String("x2")
            && name != QLatin1String("y2") && name != QLatin1String("points")
            && name != QLatin1String("d") && name != QLatin1String("cx")
            && name != QLatin1String("cy") && name != QLatin1String("r")
            && name != QLatin1String("rx") && name != QLatin1String("ry")
            && name != QLatin1String("width") && name != QLatin1String("height")
            && name != QLatin1String("viewBox") && name != QLatin1String("version"))
            ctx->styleWarning = true;
    }
    return state;
}

QVector<QPointF> readPoints(const QString &text, bool *ok)
{
    QVector<qreal> n;
    *ok = readNumberList(text, &n) && n.size() >= 4 && n.size() % 2 == 0;
    QVector<QPointF> points;
    if (*ok) for (int i=0;i<n.size();i+=2) points.append({n[i],n[i+1]});
    return points;
}

bool addElement(const QString &name, const QXmlStreamAttributes &attrs, const Affine &tx,
                const PaintState &paint, ImportContext *ctx)
{
    if (!paint.displayed) return true;
    SymbolPrimitive p;
    bool closed = false;
    const auto number = [&](const QString &key, qreal *out, bool required = true) {
        const QString s = attribute(attrs,key);
        if (s.isEmpty()) return !required;
        bool ok = false; *out = s.toDouble(&ok); return ok && finite(*out);
    };
    if (name == QLatin1String("line")) {
        qreal x1=0,y1=0,x2=0,y2=0;
        if (!number(QStringLiteral("x1"),&x1,false) || !number(QStringLiteral("y1"),&y1,false)
            || !number(QStringLiteral("x2"),&x2,false) || !number(QStringLiteral("y2"),&y2,false)) return false;
        p.kind = PrimitiveKind::Line; p.points = {tx.map({x1,y1}),tx.map({x2,y2})};
    } else if (name == QLatin1String("polyline") || name == QLatin1String("polygon")) {
        bool ok; p.points = readPoints(attribute(attrs,QStringLiteral("points")),&ok); if (!ok) return false;
        for (QPointF &point : p.points) point = tx.map(point);
        closed = name == QLatin1String("polygon"); p.kind = closed ? PrimitiveKind::Polygon : PrimitiveKind::Polyline;
    } else if (name == QLatin1String("rect")) {
        qreal x=0,y=0,w=0,h=0,rx=0,ry=0;
        if (!number(QStringLiteral("x"),&x,false) || !number(QStringLiteral("y"),&y,false)
            || !number(QStringLiteral("width"),&w) || !number(QStringLiteral("height"),&h)
            || !number(QStringLiteral("rx"),&rx,false) || !number(QStringLiteral("ry"),&ry,false)
            || w <= 0 || h <= 0) return false;
        if (rx < 0 || ry < 0) return false;
        const bool hasRx=attrs.hasAttribute(QStringLiteral("rx"));
        const bool hasRy=attrs.hasAttribute(QStringLiteral("ry"));
        if (hasRx && !hasRy) ry=rx;
        else if (!hasRx && hasRy) rx=ry;
        rx = std::min(rx,w/2); ry=std::min(ry,h/2);
        if (rx == 0 || ry == 0) {
            p.kind=PrimitiveKind::Polygon;
            p.points={tx.map({x,y}),tx.map({x+w,y}),tx.map({x+w,y+h}),tx.map({x,y+h})};
        } else {
            p.kind=PrimitiveKind::Path;
            p.segments={{PathSegmentKind::MoveTo,{tx.map({x+rx,y})}},
                        {PathSegmentKind::LineTo,{tx.map({x+w-rx,y})}}};
            appendArc(&p,{x+w-rx,y},rx,ry,0,false,true,{x+w,y+ry},tx);
            appendLine(&p,tx.map({x+w,y+h-ry}));
            appendArc(&p,{x+w,y+h-ry},rx,ry,0,false,true,{x+w-rx,y+h},tx);
            appendLine(&p,tx.map({x+rx,y+h}));
            appendArc(&p,{x+rx,y+h},rx,ry,0,false,true,{x,y+h-ry},tx);
            appendLine(&p,tx.map({x,y+ry}));
            appendArc(&p,{x,y+ry},rx,ry,0,false,true,{x+rx,y},tx);
            p.segments.append({PathSegmentKind::Close,{}});
        }
        closed=true;
    } else if (name == QLatin1String("circle") || name == QLatin1String("ellipse")) {
        qreal cx=0,cy=0,rx=0,ry=0;
        if (!number(QStringLiteral("cx"),&cx,false) || !number(QStringLiteral("cy"),&cy,false)) return false;
        if (name == QLatin1String("circle")) { if (!number(QStringLiteral("r"),&rx) || rx<=0) return false; ry=rx; }
        else if (!number(QStringLiteral("rx"),&rx) || !number(QStringLiteral("ry"),&ry) || rx<=0 || ry<=0) return false;
        const qreal dot=tx.a*tx.c+tx.b*tx.d;
        const qreal sx=std::hypot(tx.a,tx.b), sy=std::hypot(tx.c,tx.d);
        if (name == QLatin1String("circle") && std::abs(dot)<1e-10 && std::abs(sx-sy)<1e-10) {
            p.kind=PrimitiveKind::Circle; p.points={tx.map({cx,cy})}; p.radius=rx*sx;
        } else {
            p.kind=PrimitiveKind::Path;
            p.segments={{PathSegmentKind::MoveTo, {tx.map({cx+rx,cy})}}};
            appendArc(&p,{cx+rx,cy},rx,ry,0,false,true,{cx,cy+ry},tx);
            appendArc(&p,{cx,cy+ry},rx,ry,0,false,true,{cx-rx,cy},tx);
            appendArc(&p,{cx-rx,cy},rx,ry,0,false,true,{cx,cy-ry},tx);
            appendArc(&p,{cx,cy-ry},rx,ry,0,false,true,{cx+rx,cy},tx);
            p.segments.append({PathSegmentKind::Close,{}});
        }
        closed=true;
    } else if (name == QLatin1String("path")) {
        p.kind=PrimitiveKind::Path;
        if (!parsePathData(attribute(attrs,QStringLiteral("d")),tx,&p)) return false;
        closed = true; // SVG fill implicitly closes each open subpath.
    } else return false;
    if (closed) p.filled = paint.fill != QLatin1String("none");
    if (p.kind == PrimitiveKind::Path) {
        if (paint.fillRule == QLatin1String("evenodd")) p.fillRule=PathFillRule::EvenOdd;
        else if (paint.fillRule != QLatin1String("nonzero")) return false;
    }
    for (const QPointF &point : p.points) if (!finitePoint(point)) return false;
    if (p.kind==PrimitiveKind::Path) for (const auto &s:p.segments) for (const auto &point:s.points) if(!finitePoint(point)) return false;
    ctx->symbol.primitives.append(std::move(p));
    return true;
}

void skipElement(QXmlStreamReader &xml)
{
    int depth=1;
    while (!xml.atEnd() && depth) {
        xml.readNext();
        if (xml.isStartElement()) ++depth;
        else if (xml.isEndElement()) --depth;
    }
}

void parseChildren(QXmlStreamReader &xml, const Affine &parentTx,
                   const PaintState &parentPaint, ImportContext *ctx)
{
    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isEndElement()) return;
        if (!xml.isStartElement()) continue;
        const QString name=localName(xml);
        const auto attrs=xml.attributes();
        Affine own;
        if (attrs.hasAttribute(QStringLiteral("transform"))
            && !parseTransform(attribute(attrs,QStringLiteral("transform")),&own)) {
            ctx->errors << QStringLiteral("element <%1> has malformed transform").arg(name); skipElement(xml); continue;
        }
        const Affine tx=multiply(parentTx,own);
        const PaintState paint=applyPaint(attrs,parentPaint,ctx);
        if (!paint.displayed) {
            skipElement(xml);
            continue;
        }
        if (name==QLatin1String("g")) {
            parseChildren(xml,tx,paint,ctx);
        } else if (name==QLatin1String("line") || name==QLatin1String("polyline")
                   || name==QLatin1String("polygon") || name==QLatin1String("rect")
                   || name==QLatin1String("circle") || name==QLatin1String("ellipse")
                   || name==QLatin1String("path")) {
            if (!addElement(name,attrs,tx,paint,ctx))
                ctx->errors << QStringLiteral("element <%1> has malformed or unsupported geometry").arg(name);
            skipElement(xml);
        } else {
            ctx->warnings << QStringLiteral("ignored unsupported SVG element <%1>").arg(name);
            skipElement(xml);
        }
    }
}

QJsonObject terminalJson(const SymbolTerminal &terminal)
{
    return {{QStringLiteral("name"),terminal.name},{QStringLiteral("x"),terminal.offset.x()},
            {QStringLiteral("y"),terminal.offset.y()}};
}

void writePath(QXmlStreamWriter &xml, const SymbolPrimitive &p)
{
    QString d;
    auto pair=[&](const QPointF &v){d+=QString::number(v.x(),'g',17)+QLatin1Char(' ')+QString::number(v.y(),'g',17);};
    for (const PathSegment &s:p.segments) {
        if (s.kind==PathSegmentKind::MoveTo) { d+=QLatin1Char('M'); pair(s.points[0]); }
        else if (s.kind==PathSegmentKind::LineTo) { d+=QLatin1Char('L'); pair(s.points[0]); }
        else if (s.kind==PathSegmentKind::CubicTo) { d+=QLatin1Char('C'); pair(s.points[0]); d+=QLatin1Char(' '); pair(s.points[1]); d+=QLatin1Char(' '); pair(s.points[2]); }
        else d+=QLatin1Char('Z');
        d+=QLatin1Char(' ');
    }
    xml.writeAttribute(QStringLiteral("d"),d.trimmed());
}

} // namespace

bool SymbolSvgImporter::resolveSidecarSvgPath(const QByteArray &sidecarJson,
                                              const QString &sidecarDirectory,
                                              QString *absoluteSvgPath,
                                              QStringList *errors)
{
    if (absoluteSvgPath)
        absoluteSvgPath->clear();
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(sidecarJson, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (errors)
            errors->append(QStringLiteral("sidecar is not a valid JSON object: %1")
                               .arg(parseError.errorString()));
        return false;
    }
    const QJsonValue value = document.object().value(QStringLiteral("svg"));
    if (!value.isString()) {
        if (errors)
            errors->append(QStringLiteral("sidecar \"svg\" must be a string"));
        return false;
    }
    const QString svgName = value.toString();
    const QString clean = QDir::cleanPath(svgName);
    const QString absBase = QDir::cleanPath(QDir(sidecarDirectory).absolutePath());
    const QString absSvg = QDir::cleanPath(QDir(sidecarDirectory).absoluteFilePath(svgName));
    const QString basePrefix = absBase.endsWith(QLatin1Char('/'))
        ? absBase : absBase + QLatin1Char('/');
    if (svgName.isEmpty() || QDir::isAbsolutePath(svgName) || svgName.contains(QLatin1Char(':'))
        || clean == QStringLiteral("..") || clean.startsWith(QStringLiteral("../"))
        || !(absSvg == absBase || absSvg.startsWith(basePrefix))) {
        if (errors)
            errors->append(QStringLiteral("sidecar svg must be a local relative path within its directory"));
        return false;
    }
    if (absoluteSvgPath)
        *absoluteSvgPath = absSvg;
    return true;
}

SymbolSvgArtworkResult SymbolSvgImporter::importArtwork(const QByteArray &svgBytes,
                                                        const QPointF &origin)
{
    SymbolSvgArtworkResult result;
    const QJsonObject terminal{{QStringLiteral("name"), QStringLiteral("preview")},
                               {QStringLiteral("x"), origin.x()},
                               {QStringLiteral("y"), origin.y()}};
    const QJsonObject sidecar{{QStringLiteral("id"), QStringLiteral("Preview")},
        {QStringLiteral("name"), QStringLiteral("Preview")},
        {QStringLiteral("category"), QStringLiteral("Preview")},
        {QStringLiteral("referencePrefix"), QStringLiteral("P")},
        {QStringLiteral("showsReference"), false},
        {QStringLiteral("svg"), QStringLiteral("preview.svg")},
        {QStringLiteral("origin"), QJsonObject{{QStringLiteral("x"), origin.x()},
                                               {QStringLiteral("y"), origin.y()}}},
        {QStringLiteral("terminals"), QJsonArray{terminal}}};
    const SymbolSvgImportResult imported = importSymbol(
        QJsonDocument(sidecar).toJson(QJsonDocument::Compact), svgBytes, QStringLiteral("/"));
    result.warnings = imported.warnings;
    result.errors = imported.errors;
    if (imported.symbol)
        result.primitives = imported.symbol->primitives;
    return result;
}

SymbolSvgImportResult SymbolSvgImporter::importSymbol(const QByteArray &sidecarJson,
                                                       const QByteArray &svgBytes,
                                                       const QString &sidecarDirectory)
{
    SymbolSvgImportResult result;
    QJsonParseError jsonError;
    const QJsonDocument doc=QJsonDocument::fromJson(sidecarJson,&jsonError);
    if (jsonError.error!=QJsonParseError::NoError || !doc.isObject()) {
        result.errors << QStringLiteral("sidecar is not a valid JSON object: %1").arg(jsonError.errorString()); return result;
    }
    const QJsonObject object=doc.object();
    if (object.contains(QStringLiteral("primitives"))) {
        result.errors << QStringLiteral("sidecar must use svg instead of inline primitives");
        return result;
    }
    auto stringField=[&](const QString &key, QString *out) {
        if (!object.value(key).isString()) { result.errors << QStringLiteral("sidecar \"%1\" must be a string").arg(key); return false; }
        *out=object.value(key).toString(); return true;
    };
    QString id,name,category,prefix,svgName;
    const bool fields=stringField(QStringLiteral("id"),&id) & stringField(QStringLiteral("name"),&name)
        & stringField(QStringLiteral("category"),&category) & stringField(QStringLiteral("referencePrefix"),&prefix)
        & stringField(QStringLiteral("svg"),&svgName);
    if (!fields) return result;
    static const QRegularExpression validId(QStringLiteral("^[A-Za-z0-9][A-Za-z0-9._-]*$"));
    if (!validId.match(id).hasMatch()) result.errors << QStringLiteral("sidecar id must be filename-safe ASCII");
    QStringList pathErrors;
    resolveSidecarSvgPath(sidecarJson, sidecarDirectory, nullptr, &pathErrors);
    result.errors.append(pathErrors);
    bool shows=false;
    if (!object.value(QStringLiteral("showsReference")).isBool()) result.errors << QStringLiteral("sidecar \"showsReference\" must be a boolean");
    else shows=object.value(QStringLiteral("showsReference")).toBool();
    QPointF origin;
    if (object.contains(QStringLiteral("origin"))) {
        const QJsonValue v=object.value(QStringLiteral("origin"));
        if (!v.isObject() || !v.toObject().value(QStringLiteral("x")).isDouble()
            || !v.toObject().value(QStringLiteral("y")).isDouble()) result.errors << QStringLiteral("sidecar origin needs numeric x and y");
        else origin={v.toObject().value(QStringLiteral("x")).toDouble(),v.toObject().value(QStringLiteral("y")).toDouble()};
    }
    if (!finitePoint(origin)) result.errors << QStringLiteral("sidecar origin must be finite");
    const QJsonValue terminalsValue=object.value(QStringLiteral("terminals"));
    if (!terminalsValue.isArray()) result.errors << QStringLiteral("sidecar terminals must be an array");
    if (!result.errors.isEmpty()) return result;

    ImportContext ctx;
    ctx.origin=origin;
    ctx.symbol.id=SymbolId(id); ctx.symbol.displayName=name; ctx.symbol.category=category;
    ctx.symbol.referencePrefix=prefix; ctx.symbol.showsReference=shows;
    const QJsonArray terminals=terminalsValue.toArray();
    for (int i=0;i<terminals.size();++i) {
        if (!terminals[i].isObject()) { result.errors << QStringLiteral("sidecar terminals[%1] must be an object").arg(i); continue; }
        const QJsonObject t=terminals[i].toObject();
        QString terminalName=QString::number(i+1);
        if (t.contains(QStringLiteral("name"))) {
            if (!t.value(QStringLiteral("name")).isString()) { result.errors << QStringLiteral("sidecar terminals[%1] name must be a string").arg(i); continue; }
            terminalName=t.value(QStringLiteral("name")).toString();
        }
        if (!t.value(QStringLiteral("x")).isDouble() || !t.value(QStringLiteral("y")).isDouble()) {
            result.errors << QStringLiteral("sidecar terminals[%1] need numeric x and y").arg(i); continue;
        }
        const QPointF pos(t.value(QStringLiteral("x")).toDouble()-origin.x(),t.value(QStringLiteral("y")).toDouble()-origin.y());
        ctx.symbol.terminals.append({terminalName,pos});
    }
    if (!result.errors.isEmpty()) return result;

    QXmlStreamReader xml(svgBytes);
    bool foundRoot=false;
    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement()) {
            if (!foundRoot) {
                if (localName(xml)!=QLatin1String("svg")) { result.errors << QStringLiteral("SVG root element must be <svg>"); break; }
                foundRoot=true;
                Affine rootTransform;
                const auto rootAttrs=xml.attributes();
                if (rootAttrs.hasAttribute(QStringLiteral("transform"))
                    && !parseTransform(attribute(rootAttrs,QStringLiteral("transform")),&rootTransform))
                    ctx.errors << QStringLiteral("element <svg> has malformed transform");
                const PaintState rootPaint=applyPaint(rootAttrs,PaintState{},&ctx);
                parseChildren(xml,rootTransform,rootPaint,&ctx);
                while (!xml.atEnd()) {
                    xml.readNext();
                    if (xml.isStartElement()) {
                        ctx.errors << QStringLiteral("SVG must contain exactly one root element");
                        skipElement(xml);
                    }
                }
                break;
            }
        }
    }
    if (!foundRoot) result.errors << QStringLiteral("SVG has no <svg> root element");
    if (xml.hasError()) result.errors << QStringLiteral("malformed SVG XML: %1").arg(xml.errorString());
    if (ctx.styleWarning) ctx.warnings << QStringLiteral("ignored unsupported SVG style or attribute details");
    if (ctx.symbol.primitives.isEmpty() && ctx.errors.isEmpty()) result.errors << QStringLiteral("SVG contains no supported drawable primitives");
    for (SymbolPrimitive &p:ctx.symbol.primitives) {
        auto normalize=[&](QPointF &point){ point-=origin; };
        for (QPointF &point:p.points) normalize(point);
        for (PathSegment &s:p.segments) for (QPointF &point:s.points) normalize(point);
    }
    result.warnings=ctx.warnings;
    result.errors.append(ctx.errors);
    if (result.errors.isEmpty()) {
        const QStringList validation=validateLibrarySymbol(ctx.symbol);
        for (const QString &problem:validation) result.errors << QStringLiteral("converted symbol: ")+problem;
    }
    if (result.errors.isEmpty()) result.symbol=std::move(ctx.symbol);
    return result;
}

SymbolSvgExportResult SymbolSvgExporter::exportSymbol(const LibrarySymbol &symbol)
{
    SymbolSvgExportResult result;
    const QStringList validation=validateLibrarySymbol(symbol);
    if (!validation.isEmpty()) { result.errors=validation; return result; }
    static const QRegularExpression validId(QStringLiteral("^[A-Za-z0-9][A-Za-z0-9._-]*$"));
    if (!validId.match(symbol.id.value()).hasMatch()) {
        result.errors << QStringLiteral("id must be filename-safe ASCII for SVG export");
        return result;
    }
    result.svgFileName=symbol.id.value()+QStringLiteral(".svg");
    result.sidecarFileName=symbol.id.value()+QStringLiteral(".json");
    QByteArray svg;
    QXmlStreamWriter xml(&svg);
    xml.setAutoFormatting(true);
    xml.writeStartDocument();
    xml.writeStartElement(QStringLiteral("svg"));
    xml.writeDefaultNamespace(QStringLiteral("http://www.w3.org/2000/svg"));
    const QRectF bounds=symbolBounds(symbol).adjusted(-2,-2,2,2);
    xml.writeAttribute(QStringLiteral("viewBox"),QStringLiteral("%1 %2 %3 %4")
        .arg(bounds.x(),0,'g',17).arg(bounds.y(),0,'g',17)
        .arg(std::max(bounds.width(),qreal(1)),0,'g',17)
        .arg(std::max(bounds.height(),qreal(1)),0,'g',17));
    for (const SymbolPrimitive &p:symbol.primitives) {
        const bool filled=p.filled.value_or(p.kind==PrimitiveKind::Polygon);
        const QString element=p.kind==PrimitiveKind::Line?QStringLiteral("line")
            : p.kind==PrimitiveKind::Polyline?QStringLiteral("polyline")
            : p.kind==PrimitiveKind::Polygon?QStringLiteral("polygon")
            : p.kind==PrimitiveKind::Circle?QStringLiteral("circle")
            : QStringLiteral("path");
        xml.writeStartElement(element);
        xml.writeAttribute(QStringLiteral("stroke"),QStringLiteral("black"));
        xml.writeAttribute(QStringLiteral("stroke-width"),QStringLiteral("1"));
        xml.writeAttribute(QStringLiteral("fill"),filled?QStringLiteral("black"):QStringLiteral("none"));
        xml.writeAttribute(QStringLiteral("fill-rule"),p.fillRule==PathFillRule::EvenOdd?QStringLiteral("evenodd"):QStringLiteral("nonzero"));
        if (p.kind==PrimitiveKind::Line) {
            xml.writeAttribute(QStringLiteral("x1"),QString::number(p.points[0].x(),'g',17));
            xml.writeAttribute(QStringLiteral("y1"),QString::number(p.points[0].y(),'g',17));
            xml.writeAttribute(QStringLiteral("x2"),QString::number(p.points[1].x(),'g',17));
            xml.writeAttribute(QStringLiteral("y2"),QString::number(p.points[1].y(),'g',17));
        } else if (p.kind==PrimitiveKind::Polyline || p.kind==PrimitiveKind::Polygon) {
            QString points;
            for (const QPointF &v:p.points) points+=QString::number(v.x(),'g',17)+QLatin1Char(',')+QString::number(v.y(),'g',17)+QLatin1Char(' ');
            xml.writeAttribute(QStringLiteral("points"),points.trimmed());
        } else if (p.kind==PrimitiveKind::Circle) {
            const QPointF c=p.points.first();
            xml.writeAttribute(QStringLiteral("cx"),QString::number(c.x(),'g',17));
            xml.writeAttribute(QStringLiteral("cy"),QString::number(c.y(),'g',17));
            xml.writeAttribute(QStringLiteral("r"),QString::number(p.radius,'g',17));
        } else writePath(xml,p);
        xml.writeEndElement();
    }
    xml.writeEndElement(); xml.writeEndDocument();
    result.svg=svg;

    QJsonArray terminals;
    for (const SymbolTerminal &t:symbol.terminals) terminals.append(terminalJson(t));
    const QJsonObject sidecar{{QStringLiteral("id"),symbol.id.value()},
        {QStringLiteral("name"),symbol.displayName},{QStringLiteral("category"),symbol.category},
        {QStringLiteral("referencePrefix"),symbol.referencePrefix},
        {QStringLiteral("showsReference"),symbol.showsReference},
        {QStringLiteral("svg"),result.svgFileName},
        {QStringLiteral("origin"),QJsonObject{{QStringLiteral("x"),0},{QStringLiteral("y"),0}}},
        {QStringLiteral("terminals"),terminals}};
    result.sidecar=QJsonDocument(sidecar).toJson(QJsonDocument::Indented);
    return result;
}

