#ifndef IDENTIFIERS_H
#define IDENTIFIERS_H

#include <QtGlobal>

// Document-scoped identity for a placed Component. Assigned by Document
// when a component is added; never reused after the component is removed.
using ComponentId = quint32;

// Index of a terminal within a single component's symbol definition (0-based,
// contiguous - see SymbolDefinition::terminal()). Not unique across
// components; always paired with a ComponentId when referring to a specific
// connection point (see TerminalRef, added in a later step).
using TerminalId = int;

// Document-scoped identity for a bare wire-to-wire connection point (a
// branch) that is not itself a component terminal. Allocated only by an
// explicit branch operation - never inferred from coincident coordinates.
using NodeId = quint32;

// Document-scoped identity for a persisted wire route.
using WireId = quint32;

constexpr ComponentId kInvalidComponentId = 0;
constexpr TerminalId   kInvalidTerminalId  = -1;
constexpr NodeId       kInvalidNodeId      = 0;
constexpr WireId       kInvalidWireId      = 0;

#endif // IDENTIFIERS_H
