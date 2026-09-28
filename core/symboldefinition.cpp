#include "symboldefinition.h"

#include <QHash>

namespace {

// All symbols are authored directly in world/scene units (no separate
// "symbol unit" scale - see the coordinate contract note in
// symboldefinition.h and component.h). Component::terminalPosition()
// rotates and translates these local offsets by the component's placement;
// it never scales them, so Document::gridSpacing (added in a later step)
// only affects snapping and background grid-line spacing, not symbol size.
// Two-terminal parts share a common horizontal layout (terminals at
// x = -20 and x = +20) so their default (unrotated) orientation is visually
// consistent; component placement is expected to snap to a grid whose
// spacing is comparable to these dimensions (e.g. 10-20 units) for a tidy
// default layout, though off-grid placement is not itself invalid.

SymbolDefinition makeResistor()
{
    SymbolDefinition def;
    def.kind = SymbolKind::Resistor;
    def.displayName = QStringLiteral("Resistor");
    def.primitives = {
        { PrimitiveKind::Line,    { {-20, 0}, {-10, 0} }, 0.0 },
        { PrimitiveKind::Line,    { { 10, 0}, { 20, 0} }, 0.0 },
        { PrimitiveKind::Polygon, { {-10,-6}, {10,-6}, {10,6}, {-10,6} }, 0.0 },
    };
    def.terminals = {
        { QStringLiteral("1"), {-20, 0} },
        { QStringLiteral("2"), { 20, 0} },
    };
    return def;
}

SymbolDefinition makeCapacitor()
{
    SymbolDefinition def;
    def.kind = SymbolKind::Capacitor;
    def.displayName = QStringLiteral("Capacitor");
    def.primitives = {
        { PrimitiveKind::Line, { {-20, 0}, {-4, 0} }, 0.0 },
        { PrimitiveKind::Line, { {  4, 0}, {20, 0} }, 0.0 },
        { PrimitiveKind::Line, { { -4,-10}, {-4,10} }, 0.0 },
        { PrimitiveKind::Line, { {  4,-10}, { 4,10} }, 0.0 },
    };
    def.terminals = {
        { QStringLiteral("1"), {-20, 0} },
        { QStringLiteral("2"), { 20, 0} },
    };
    return def;
}

SymbolDefinition makeDiode()
{
    SymbolDefinition def;
    def.kind = SymbolKind::Diode;
    def.displayName = QStringLiteral("Diode");
    def.primitives = {
        { PrimitiveKind::Line,    { {-20, 0}, {-8, 0} }, 0.0 },
        { PrimitiveKind::Line,    { {  8, 0}, {20, 0} }, 0.0 },
        { PrimitiveKind::Polygon, { {-8,-8}, {-8,8}, {8,0} }, 0.0 },
        { PrimitiveKind::Line,    { {  8,-8}, { 8,8} }, 0.0 },
    };
    def.terminals = {
        { QStringLiteral("anode"),   {-20, 0} },
        { QStringLiteral("cathode"), { 20, 0} },
    };
    return def;
}

SymbolDefinition makeGround()
{
    SymbolDefinition def;
    def.kind = SymbolKind::Ground;
    def.displayName = QStringLiteral("Ground");
    def.primitives = {
        { PrimitiveKind::Line, { {0,-20}, {0, 0} }, 0.0 },
        { PrimitiveKind::Line, { {-16, 0}, {16, 0} }, 0.0 },
        { PrimitiveKind::Line, { {-10, 6}, {10, 6} }, 0.0 },
        { PrimitiveKind::Line, { { -4,12}, { 4,12} }, 0.0 },
    };
    def.terminals = {
        { QStringLiteral("1"), {0, -20} },
    };
    return def;
}

SymbolDefinition makeOpAmp()
{
    SymbolDefinition def;
    def.kind = SymbolKind::OpAmp;
    def.displayName = QStringLiteral("Op-Amp");
    def.primitives = {
        { PrimitiveKind::Polygon, { {-20,-16}, {-20,16}, {20,0} }, 0.0 },
        { PrimitiveKind::Line,    { {-28,-8}, {-20,-8} }, 0.0 },
        { PrimitiveKind::Line,    { {-28, 8}, {-20, 8} }, 0.0 },
        { PrimitiveKind::Line,    { { 20, 0}, { 28, 0} }, 0.0 },
        { PrimitiveKind::Line,    { {  0,-8}, {  0,-16} }, 0.0 },
        { PrimitiveKind::Line,    { {  0, 8}, {  0, 16} }, 0.0 },
    };
    def.terminals = {
        { QStringLiteral("in+"), {-28, -8} },
        { QStringLiteral("in-"), {-28,  8} },
        { QStringLiteral("out"), { 28,  0} },
        { QStringLiteral("v+"),  {  0,-16} },
        { QStringLiteral("v-"),  {  0, 16} },
    };
    return def;
}

SymbolDefinition makePotentiometer()
{
    SymbolDefinition def;
    def.kind = SymbolKind::Potentiometer;
    def.displayName = QStringLiteral("Potentiometer");
    def.primitives = {
        { PrimitiveKind::Line,     { {-20, 0}, {-10, 0} }, 0.0 },
        { PrimitiveKind::Line,     { { 10, 0}, { 20, 0} }, 0.0 },
        { PrimitiveKind::Polygon,  { {-10,-6}, {10,-6}, {10,6}, {-10,6} }, 0.0 },
        { PrimitiveKind::Line,     { {  0,-20}, {  0,-6} }, 0.0 },
        { PrimitiveKind::Polyline, { { -4,-14}, {  0,-6}, {  4,-14} }, 0.0 },
    };
    def.terminals = {
        { QStringLiteral("1"),     {-20,  0} },
        { QStringLiteral("2"),     { 20,  0} },
        { QStringLiteral("wiper"), {  0,-20} },
    };
    return def;
}

SymbolDefinition makeJack()
{
    SymbolDefinition def;
    def.kind = SymbolKind::Jack;
    def.displayName = QStringLiteral("Input/Output Jack");
    def.primitives = {
        { PrimitiveKind::Circle, { {0, 0} }, 10.0 },
        { PrimitiveKind::Line,   { {-20, 0}, {-10, 0} }, 0.0 },
        { PrimitiveKind::Line,   { { 10, 0}, { 20, 0} }, 0.0 },
    };
    def.terminals = {
        { QStringLiteral("tip"),    {-20, 0} },
        { QStringLiteral("sleeve"), { 20, 0} },
    };
    return def;
}

const QHash<SymbolKind, SymbolDefinition> &symbolTable()
{
    static const QHash<SymbolKind, SymbolDefinition> table = {
        { SymbolKind::Resistor,      makeResistor() },
        { SymbolKind::Capacitor,     makeCapacitor() },
        { SymbolKind::Diode,         makeDiode() },
        { SymbolKind::Ground,        makeGround() },
        { SymbolKind::OpAmp,         makeOpAmp() },
        { SymbolKind::Potentiometer, makePotentiometer() },
        { SymbolKind::Jack,          makeJack() },
    };
    return table;
}

} // namespace

const SymbolDefinition &symbolDefinition(SymbolKind kind)
{
    // QHash::operator[] on a const QHash returns by value (it cannot return
    // a reference to a possibly-absent key), so it must not be used here -
    // doing so would return a reference to a destroyed temporary. All seven
    // built-in kinds are always present in the static table, so
    // constFind()->value() safely yields a reference into the container
    // itself, which is valid for the lifetime of the application.
    const QHash<SymbolKind, SymbolDefinition> &table = symbolTable();
    auto it = table.constFind(kind);
    Q_ASSERT(it != table.constEnd());
    return it.value();
}

int symbolTerminalCount(SymbolKind kind)
{
    return symbolDefinition(kind).terminals.size();
}
