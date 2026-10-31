# Roadmap

This document describes open work and possible future directions. It is not a fixed schedule or a promise of delivery. Settled behavior and architecture belong in [DECISIONS.md](DECISIONS.md); project goals and collaboration guidance belong in [AGENTS.md](AGENTS.md).

## Suggested implementation order

This is the default starting point for a coding request, not an authorization to implement multiple items at once. Confirm the requested scope before each substantial change, following [AGENTS.md](AGENTS.md). Keep the design and acceptance checks for the selected item short and specific.

1. **Symbol library authoring.** The approved [symbol import design](docs/design/symbol-import.md) is split into J1–J5. Next, prepare a scoped implementation plan for J1 (the `Path` primitive); each step needs its own approval before implementation.
2. **Symbol library growth.** Add symbol groups through the same library format, authoring process, validation, and serialization path.
3. **Reliability and usability polish.** Address the open grid, wire recovery, route retracing, save, warning, and title-bar issues below as focused follow-up work.

The order can change when priorities change or a coding request targets another item. The detailed ordering belongs here; only durable model, file-format, and cross-cutting editing choices belong in `DECISIONS.md`.

Undo/redo is implemented per [its approved design](docs/design/undo-redo.md). The remaining items below are open; each substantial change still needs a scoped plan and approval.

### Editing workflow

- **Implemented:** Extract the shared symbol renderer and replace the flat text list with a category tree and graphical symbol grid.
- **Implemented:** Drag a palette symbol to the snapped canvas position through the shared undoable placement path.
- **Implemented:** Provide separate, movable File, Tools, Edit, and View toolbars; show or hide toolbars and dock panels from View > Toolbars; restore their layout between sessions. Keep zoom in the View menu/Ctrl+wheel.
- **Implemented:** Show terminal markers and a hover cue for candidate wire connections; Escape cancels a draft and returns to Select.

Palette placement and toolbar edits use the established edit-commit path so they are undoable from introduction.

### Reliability and usability

- Bound grid-rendering work for very small document grid spacing while preserving exact spacing for snapping.
- Decide how users recover from an invalid vertex in a pending wire (for example, reject it with feedback or provide a remove-last-vertex action).
- Decide whether component moves that retrace a route should be rejected or should collapse the retraced corner.
- Add the `.schematic` suffix on save when a path has no extension.
- Make persistent load warnings discoverable after the temporary status message expires.
- Consider showing a concise document name in the window title instead of the full path.

## Symbol library growth

The data-driven library is in place. All entries should use the same format and management path. Add symbol groups with focused validation and serialization coverage; preserve released IDs, terminal order, and terminal positions as described in [DECISIONS.md](DECISIONS.md).

Potential additions, subject to selection and design when work is planned:

- JFET, BJT, and MOSFET variants.
- LED, Zener, and Schottky diodes; switches, inductors, and supply rails.
- Triode and pentode valves, and transconductance amplifiers.
- Input, output, and bidirectional signal ports. Their labels and direction should remain presentation metadata and must not infer electrical connectivity.

Agree the precise set, geometry, categories, and acceptance checks as part of the proposal for each group of symbols.

## Symbol library authoring

This is the next major feature area. The approved [symbol import design](docs/design/symbol-import.md) describes a restricted SVG profile, in-app metadata and terminal editing, conversion to the native symbol definition, library storage, and document embedding. Its J1–J5 steps are proposals; approve a scoped plan for each step before implementation. Inkscape or another SVG editor remains the drawing tool; the application does not include a general-purpose artwork editor in this scope.

Exporting an individual symbol as SVG and its metadata sidecar supports authoring and round trips. Exporting a complete schematic to SVG is a separate required future capability; it is not part of the symbol-creation feature.

## Other possible directions

- Export complete schematics to SVG (required future capability); PNG and PDF export are also possible later formats.
- Add free-text annotations and callouts, and improve label placement for rotated or crowded symbols.
- Support moving multiple selected components together.
- Add UI and file-I/O tests where they fit the existing headless model-test setup.

## Open design questions

- Should a junction dot appear where multiple wire routes meet a component terminal? Current rendering derives dots from shared route endpoint identities with three or more wire ends.
- If future symbol features require per-instance metadata or net naming, what document-format changes and migration rules are needed?

## Planning and status

For substantial work, inspect the current code and propose a short, reviewable plan before implementation. Each proposal should state scope, design choices that are costly to change, and acceptance checks. Follow the approval process in [AGENTS.md](AGENTS.md). Update this roadmap when priorities or open questions change; record settled cross-cutting choices in [DECISIONS.md](DECISIONS.md).
