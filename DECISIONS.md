# Design decisions

This file records choices that affect the document model, file compatibility, or core editing behavior. Revisit them deliberately; do not treat implementation details or unresolved questions as settled decisions.

## Document model and connectivity

- The document model is independent of Qt rendering items and input handling. `Document` is the source of truth; the scene and view render it and route user edits through its API.
- Electrical connectivity comes from endpoint identity, never coordinate coincidence. Endpoints refer to component terminals or explicit document nodes. Nets and junction positions are derived; crossings do not connect unless the user explicitly creates a branch.
- A committed wire has resolved endpoints and an orthogonal route. A diagonal pointer move gets a deterministic horizontal-first corner. Grid alignment is an editing convenience, not a validity requirement for saved routes.
- Component positions, symbol geometry, and terminal coordinates use the same world/scene units. Grid spacing affects snapping and grid-line spacing; it does not scale symbols. Terminals are not required to land on grid intersections.
- Component movement updates attached route endpoints while preserving endpoint identities and validating candidate routes before commit.
- Removing a component removes wires that can no longer resolve their terminal endpoint. Shared branch nodes remain when other routes still reference them.
- Persistent net IDs and separately serialized junction lists are not part of the version 1 format.

## File format

- Project documents use versioned JSON. Version 1 stores grid spacing, components, explicit nodes, wire vertices, and endpoint references. Nets and junctions are derived.
- On load, wire endpoint coordinates are recalculated from terminal or node identity. A stale serialized endpoint is a warning; invalid topology or route geometry rejects the whole document.
- Symbol kind names are part of the serialization contract. Built-in terminal geometry also affects loading because endpoint positions are derived from current symbol definitions. Changes to that geometry require a compatibility decision before release.

## Symbols and labels

- Rotation is limited to 0, 90, 180, and 270 degrees. Mirroring and arbitrary angles are out of scope.
- The built-in symbol set for the current milestone is resistor, capacitor, diode, ground, op-amp, potentiometer, and input/output jack.
- Reference labels are trimmed and must be non-empty and unique among other components when edited. Values are trimmed, may be empty, and need not be unique. The loader should enforce the same reference invariant; see the unresolved items in [ROADMAP.md](ROADMAP.md).
- Double-click opens the reference/value dialog. F2 renames the reference of exactly one selected component.

## Milestone boundaries

- The first usable milestone focuses on schematic presentation and editing. PCB layout, circuit simulation, automatic routing, and user-authored symbols are out of scope.
- Undo/redo, wire-corner reshaping, net naming, arbitrary rotation/mirroring, and a symbol editor are deferred unless explicitly brought into scope.
- Grid and terminal placement should follow the world-coordinate contract above; do not move terminals onto a grid solely to make snapping simpler.
