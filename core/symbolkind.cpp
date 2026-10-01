#include "symbolkind.h"

QString symbolKindName(SymbolKind kind)
{
    switch (kind) {
    case SymbolKind::Resistor:      return QStringLiteral("Resistor");
    case SymbolKind::Capacitor:     return QStringLiteral("Capacitor");
    case SymbolKind::Diode:         return QStringLiteral("Diode");
    case SymbolKind::Ground:        return QStringLiteral("Ground");
    case SymbolKind::OpAmp:         return QStringLiteral("OpAmp");
    case SymbolKind::Potentiometer: return QStringLiteral("Potentiometer");
    case SymbolKind::Jack:          return QStringLiteral("Jack");
    }
    return QString();
}

std::optional<SymbolKind> symbolKindFromName(const QString &name)
{
    if (name == QStringLiteral("Resistor"))      return SymbolKind::Resistor;
    if (name == QStringLiteral("Capacitor"))     return SymbolKind::Capacitor;
    if (name == QStringLiteral("Diode"))         return SymbolKind::Diode;
    if (name == QStringLiteral("Ground"))        return SymbolKind::Ground;
    if (name == QStringLiteral("OpAmp"))         return SymbolKind::OpAmp;
    if (name == QStringLiteral("Potentiometer")) return SymbolKind::Potentiometer;
    if (name == QStringLiteral("Jack"))          return SymbolKind::Jack;
    return std::nullopt;
}

QString symbolKindReferencePrefix(SymbolKind kind)
{
    switch (kind) {
    case SymbolKind::Resistor:      return QStringLiteral("R");
    case SymbolKind::Capacitor:     return QStringLiteral("C");
    case SymbolKind::Diode:         return QStringLiteral("D");
    case SymbolKind::Ground:        return QStringLiteral("GND");
    case SymbolKind::OpAmp:         return QStringLiteral("U");
    case SymbolKind::Potentiometer: return QStringLiteral("RV");
    case SymbolKind::Jack:          return QStringLiteral("J");
    }
    return QStringLiteral("X");
}
