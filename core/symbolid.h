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

// Packaged symbol identities. Original IDs retain the pre-library names
// and terminal interfaces so version 1/2 documents still load unchanged.
namespace SymbolIds {
inline const SymbolId Resistor{QStringLiteral("Resistor")};
inline const SymbolId Capacitor{QStringLiteral("Capacitor")};
inline const SymbolId ElectrolyticCapacitor{QStringLiteral("ElectrolyticCapacitor")};
inline const SymbolId SchottkyDiode{QStringLiteral("SchottkyDiode")};
inline const SymbolId ZenerDiode{QStringLiteral("ZenerDiode")};
inline const SymbolId LED{QStringLiteral("LED")};
inline const SymbolId PNPCompact30{QStringLiteral("PNP.Compact30")};
inline const SymbolId NPNCompact30{QStringLiteral("NPN.Compact30")};
inline const SymbolId NChannelJFETCompact30{QStringLiteral("NChannelJFET.Compact30")};
inline const SymbolId PChannelJFETCompact30{QStringLiteral("PChannelJFET.Compact30")};
inline const SymbolId NChannelMOSFETCompact30{QStringLiteral("NChannelMOSFET.Compact30")};
inline const SymbolId PChannelMOSFETCompact30{QStringLiteral("PChannelMOSFET.Compact30")};
inline const SymbolId Diode{QStringLiteral("Diode")};
inline const SymbolId RailVCC{QStringLiteral("Rail.VCC")};
inline const SymbolId GroundChassis{QStringLiteral("Ground.Chassis")};
inline const SymbolId GroundSignal{QStringLiteral("Ground.Signal")};
inline const SymbolId PortInput{QStringLiteral("Port.Input")};
inline const SymbolId PortOutput{QStringLiteral("Port.Output")};
inline const SymbolId PortBidirectional{QStringLiteral("Port.Bidirectional")};
inline const SymbolId Ground{QStringLiteral("Ground")};
inline const SymbolId OpAmpNoSupplies{QStringLiteral("OpAmp.Grid10.NoSupplies")};
inline const SymbolId OTAWithSuppliesVerticalBias{QStringLiteral("OTA.WithSupplies.OutputSource.VerticalBias")};
inline const SymbolId OTANoSuppliesVerticalBias{QStringLiteral("OTA.NoSupplies.OutputSource.VerticalBias")};
inline const SymbolId DarlingtonBufferLM13700{QStringLiteral("DarlingtonBuffer.LM13700")};
inline const SymbolId OTAWithSuppliesBiasAbove{QStringLiteral("OTA.WithSupplies.OutputSource.BiasAbove")};
inline const SymbolId OTANoSuppliesBiasAbove{QStringLiteral("OTA.NoSupplies.OutputSource.BiasAbove")};
inline const SymbolId OpAmpGrid10{QStringLiteral("OpAmp.Grid10")};
inline const SymbolId PresetResistor{QStringLiteral("PresetResistor")};
inline const SymbolId Potentiometer{QStringLiteral("Potentiometer")};
inline const SymbolId Jack{QStringLiteral("Jack")};
inline const SymbolId BlockHighPass{QStringLiteral("Block.HighPass")};
inline const SymbolId BlockLowPass{QStringLiteral("Block.LowPass")};
inline const SymbolId BlockBandPass{QStringLiteral("Block.BandPass")};
inline const SymbolId BlockBuffer{QStringLiteral("Block.Buffer")};
inline const SymbolId BlockSummingNode{QStringLiteral("Block.SummingNode")};
inline const SymbolId BlockBandStop{QStringLiteral("Block.BandStop")};
inline const SymbolId BlockVariableHighPass{QStringLiteral("Block.VariableHighPass")};
inline const SymbolId BlockVariableLowPass{QStringLiteral("Block.VariableLowPass")};
inline const SymbolId BlockVariableBandPass{QStringLiteral("Block.VariableBandPass")};
inline const SymbolId BlockVariableBandStop{QStringLiteral("Block.VariableBandStop")};
inline const SymbolId ValveTriodeWithHeaters{QStringLiteral("Valve.Triode.WithHeaters")};
inline const SymbolId ValveTriodeNoHeaters{QStringLiteral("Valve.Triode.NoHeaters")};
inline const SymbolId ValvePentodeWithHeaters{QStringLiteral("Valve.Pentode.WithHeaters")};
inline const SymbolId ValvePentodeNoHeaters{QStringLiteral("Valve.Pentode.NoHeaters")};
inline const SymbolId ValvePentodeCathodeSuppressorWithHeaters{QStringLiteral("Valve.PentodeCathodeSuppressor.WithHeaters")};
inline const SymbolId ValvePentodeCathodeSuppressorNoHeaters{QStringLiteral("Valve.PentodeCathodeSuppressor.NoHeaters")};
inline const SymbolId ValveBeamTetrodeWithHeaters{QStringLiteral("Valve.BeamTetrode.WithHeaters")};
inline const SymbolId ValveBeamTetrodeNoHeaters{QStringLiteral("Valve.BeamTetrode.NoHeaters")};
inline const SymbolId ValveDiodeWithHeaters{QStringLiteral("Valve.Diode.WithHeaters")};
inline const SymbolId ValveDiodeNoHeaters{QStringLiteral("Valve.Diode.NoHeaters")};
} // namespace SymbolIds

#endif // SYMBOLID_H
