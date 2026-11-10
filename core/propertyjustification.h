#ifndef PROPERTYJUSTIFICATION_H
#define PROPERTYJUSTIFICATION_H

#include <QString>
#include <optional>

enum class PropertyJustification { Left, Middle, Right };

inline QString propertyJustificationName(PropertyJustification value)
{
    switch (value) {
    case PropertyJustification::Left: return QStringLiteral("left");
    case PropertyJustification::Middle: return QStringLiteral("middle");
    case PropertyJustification::Right: return QStringLiteral("right");
    }
    return {};
}

inline std::optional<PropertyJustification> propertyJustificationFromName(const QString &value)
{
    if (value == QLatin1String("left")) return PropertyJustification::Left;
    if (value == QLatin1String("middle")) return PropertyJustification::Middle;
    if (value == QLatin1String("right")) return PropertyJustification::Right;
    return std::nullopt;
}

#endif
