# Design decisions

This file records choices that affect the document model, file compatibility, or core editing behavior. Revisit them deliberately; do not treat implementation details or unresolved questions as settled decisions.

## Document model and connectivity

- The document model is independent of Qt rendering items and input handling. `Document` is the source of truth; the scene and view render it and route user edits through its API.
- Electrical connectivity comes from endpoint identity, never coordinate coincidence. Endpoints refer to component terminals or explicit document nodes. Nets and junction positions are derived; crossings do not connect unless the user explicitly creates a branch.
- A committed wire has resolved endpoints and an orthogonal route with no collinear reversal. A diagonal pointer move gets a deterministic horizontal-first corner. Grid alignment is an editing convenience, not a validity requirement for saved routes.
- Component positions, symbol geometry, and terminal coordinates use the same world/scene units. Grid spacing affects snapping and grid-line spacing; it does not scale symbols. Terminals are not required to land on grid intersections.
- Component movement updates attached route endpoints while preserving endpoint identities and validating candidate routes before commit.
- Removing a component removes wires that can no longer resolve their terminal endpoint. Shared branch nodes remain when other routes still reference them.
- Persistent net IDs and separately serialized junction lists are not part of the version 1 format.

## File format

- Project documents use versioned JSON. Version 1 stores grid spacing, components, explicit nodes, wire vertices, and endpoint references. Nets and junctions are derived.
- On load, wire endpoint coordinates are recalculated from terminal or node identity. A stale serialized endpoint is a warning; invalid topology or route geometry rejects the whole document. The loader rejects collinear reversals rather than normalizing saved geometry.
- Symbol kind names are part of the serialization contract. Built-in terminal geometry also affects loading because endpoint positions are derived from current symbol definitions. Changes to that geometry require a compatibility decision before release.
- Milestone 2 adds format version 2 for horizontal mirroring. The loader accepts v1 and v2; v1 components are unmirrored and may not contain a `mirrored` property. V2 requires a boolean `mirrored` property per component. Writers emit v2, so saving a loaded v1 document upgrades it.
- Missing, empty, and whitespace-only references are generated from per-kind prefixes with a warning. Generation avoids collisions deterministically. Duplicate non-empty references are load errors that name both components; they are not silently renamed. References are trimmed and compared case-sensitively. Loaded values are trimmed as well.

## Symbols and labels

- Rotation is limited to 0, 90, 180, and 270 degrees. Horizontal mirroring is out of scope for milestone 1 and in scope for milestone 2 (format v2); arbitrary-angle rotation and vertical mirroring remain out of scope.
- The built-in symbol set for the current milestone is resistor, capacitor, diode, ground, op-amp, potentiometer, and input/output jack.
- Reference labels are trimmed and must be non-empty and unique among other components. Values are trimmed, may be empty, and need not be unique. Load repair/rejection follows the explicit policy above.
- Double-click opens the reference/value dialog. F2 renames the reference of exactly one selected component.
- Component transforms include quadrant rotation and horizontal mirroring. Mirroring is applied to local symbol geometry before rotation; text labels remain readable. Transform operations update connected routes transactionally.

## Milestone boundaries

- The first usable milestone focuses on schematic presentation and editing. PCB layout, circuit simulation, automatic routing, and user-authored symbols are out of scope.
- Undo/redo is included in milestone 2, with its own design note covering clean/dirty state, selection restoration, and command boundaries before implementation. Wire-corner reshaping, net naming, arbitrary-angle rotation, vertical mirroring, and a full symbol editor remain deferred.
- Grid and terminal placement should follow the world-coordinate contract above; do not move terminals onto a grid solely to make snapping simpler.
