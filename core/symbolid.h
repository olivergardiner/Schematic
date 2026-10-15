#ifndef SYMBOLID_H
#define SYMBOLID_H

#include <QHashFunctions>
#include <QString>

// Stable, serialization-facing identity of a symbol in the symbol library
// (see SymbolLibrary). The string is the symbol's library ID and is what
// documents store as a component's "kind", so a released ID must never be
// reused for different geometry - see DECISIONS.md "File format".
//
// A strong type rather than a bare QString so a symbol ID cannot be
// confused with a reference label, display name or category.
class SymbolId
{
public:
    SymbolId() = default;
    explicit SymbolId(const QString &value) : m_value(value) {}

    const QString &value() const { return m_value; }
    bool isEmpty() const { return m_value.isEmpty(); }

    friend bool operator==(const SymbolId &a, const SymbolId &b) { return a.m_value == b.m_value; }
    friend bool operator!=(const SymbolId &a, const SymbolId &b) { return !(a == b); }
    friend size_t qHash(const SymbolId &id, size_t seed = 0) noexcept
    {
        return qHash(id.m_value, seed);
    }

private:
    QString m_value;
};

// IDs of the seven symbols shipped in the built-in library. These strings
// are identical to the pre-library SymbolKind serialization names, which is
// what lets version 1 and 2 documents load unchanged.
namespace SymbolIds {
inline const SymbolId Resistor{QStringLiteral("Resistor")};
inline const SymbolId Capacitor{QStringLiteral("Capacitor")};
inline const SymbolId Diode{QStringLiteral("Diode")};
inline const SymbolId Ground{QStringLiteral("Ground")};
inline const SymbolId OpAmp{QStringLiteral("OpAmp")};
inline const SymbolId Potentiometer{QStringLiteral("Potentiometer")};
inline const SymbolId Jack{QStringLiteral("Jack")};
} // namespace SymbolIds

#endif // SYMBOLID_H
