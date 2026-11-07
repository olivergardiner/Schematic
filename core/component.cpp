#include "component.h"
#include "symbollibrary.h"

#include <QtGlobal>
#include <QRegularExpression>
#include <QSet>

QString propertyIdFromName(const QString &name)
{
    QString id = name.trimmed().toLower();
    id.replace(QRegularExpression("[^a-z0-9]+"), "_");
    id.remove(QRegularExpression("^_+|_+$"));
    if (id.isEmpty()) id = "property";
    if (!id.front().isLetter()) id.prepend("property_");
    return id;
}

QStringList validateInstanceProperties(const QVector<InstanceProperty> &properties)
{
    QStringList errors;
    if (properties.size() < 2 || properties[0].id != "reference" || properties[1].id != "value")
        return {QStringLiteral("Reference and Value must be the first two properties.")};
    QSet<QString> ids, names;
    const QRegularExpression key("^[a-z][a-z0-9_]*$");
    const auto singleLine = [](const QString &s) {
        return !s.contains('\n') && !s.contains('\r') && !s.contains(QChar(0))
            && !s.contains(QChar(0x2028)) && !s.contains(QChar(0x2029));
    };
    for (int i = 0; i < properties.size(); ++i) {
        const auto &p = properties[i];
        if (!key.match(p.id).hasMatch() || ids.contains(p.id))
            errors << QStringLiteral("Property IDs must be valid and unique.");
        ids.insert(p.id);
        const QString name = i < 2 ? (i == 0 ? QStringLiteral("Reference") : QStringLiteral("Value")) : p.name;
        if ((i < 2 && !p.name.isEmpty()) || (i >= 2 && (p.id == "reference" || p.id == "value"))
            || name.trimmed().isEmpty() || name != name.trimmed() || !singleLine(name)
            || names.contains(name.toCaseFolded()))
            errors << QStringLiteral("Property names must be non-empty and unique; reserved rows cannot be renamed.");
        names.insert(name.toCaseFolded());
        if (!singleLine(p.value)) errors << QStringLiteral("Property values must be single-line text.");
    }
    if (properties[0].value.trimmed().isEmpty()) errors << QStringLiteral("Reference must not be empty.");
    return errors;
}

const InstanceProperty *Component::property(const QString &id) const
{
    for (const auto &p : m_properties) if (p.id == id) return &p;
    return nullptr;
}

QPointF rotateOffset(QPointF localOffset, Rotation rotation)
{
    const qreal x = localOffset.x();
    const qreal y = localOffset.y();

    switch (rotation) {
    case Rotation::Deg0:
        return { x, y };
    case Rotation::Deg90:
        // Clockwise quadrant rotation: (x, y) -> (-y, x)
        return { -y, x };
    case Rotation::Deg180:
        return { -x, -y };
    case Rotation::Deg270:
        return { y, -x };
    }
    return localOffset;
}

Component::Component(ComponentId id, const SymbolId &symbolId, QPointF position)
    : m_id(id)
    , m_symbolId(symbolId)
    , m_position(position)
{
}

int Component::terminalCount(const LibrarySymbol &symbol) const
{
    return symbol.terminals.size();
}

QPointF Component::terminalPosition(TerminalId terminal, const LibrarySymbol &definition) const
{
    const LibrarySymbol *symbol = &definition;
    Q_ASSERT(symbol && terminal >= 0 && terminal < symbol->terminals.size());
    if (!symbol || terminal < 0 || terminal >= symbol->terminals.size())
        return m_position;

    // Apply mirroring if necessary - the local offset is mirrored (x negated) first, then rotated, then translated.
    const QPointF local = symbol->terminals[terminal].offset;
    const QPointF mirroredOffset = m_mirrored ? QPointF(-local.x(), local.y()) : local;
    return m_position + rotateOffset(mirroredOffset, m_rotation);
}
