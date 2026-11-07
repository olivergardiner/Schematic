# Roadmap

This document describes open work and possible future directions. It is not a fixed schedule or a promise of delivery. Settled behavior and architecture belong in [DECISIONS.md](DECISIONS.md); project goals and collaboration guidance belong in [AGENTS.md](AGENTS.md).

## Suggested implementation order

**Implemented: movable property blocks.** The [property-block placement plan](docs/design/property-block-placement.md) unifies Reference, Value, and custom rows into one compact movable block, adds saved instance placement and reset, and defaults the Properties dock beneath Symbols on the left. It uses format v6 and preserves saved application dock layouts. Release and native Windows verification is recorded in the design note.

This is the default starting point for a coding request, not an authorization to implement multiple items at once. Confirm the requested scope before each substantial change, following [AGENTS.md](AGENTS.md). Keep the design and acceptance checks for the selected item short and specific.

**Implemented: instance properties and a right-hand editor.** The [properties design](docs/design/instance-properties.md) covers Reference/Value plus custom properties, per-instance canvas visibility, mixed-value and partial-presence multi-selection editing, atomic undoable changes, label layout, and the v5 document representation. Release tests and native Windows inspector checks pass.

**Directional group selection is implemented** per the approved [box-selection plan](docs/design/group-selection.md): left-to-right enclosure and right-to-left crossing selection for components, labels, and wire routes. Automated checks pass offscreen and on native Windows; captured visuals have been inspected, with a full manual application walkthrough outstanding. The remaining work is listed below.

**Group move, rotate, and flip are implemented** per the approved [group-transform plan](docs/design/group-transforms.md): atomic mixed-selection transforms, geometry-based pivots, shared snapping, internal and boundary wiring, live move previews, and component/wire selection restoration through undo/redo. Automated model and UI checks pass; a full manual application walkthrough remains outstanding.

1. **Symbol library authoring.** J1 (`Path`), J2 (headless SVG import/export), J3 (the import dialog and writable library), and J4 (document symbol snapshots and conflict handling) from the approved [symbol import design](docs/design/symbol-import.md) are implemented. The writable library foundation was included in J3 so imported symbols can be used immediately. The [J4 implementation plan](docs/design/symbol-import-j4-plan.md) records the approved document embedding and conflict implementation; automated verification passes, with the manual visual walkthrough outstanding. J5 symbol content remains later work.
2. **Symbol library growth.** Add symbol groups through the same library format, authoring process, validation, and serialization path.
3. **Reliability and usability polish.** Address the open grid, wire recovery, route retracing, save, warning, and title-bar issues below as focused follow-up work.

The order can change when priorities change or a coding request targets another item. The detailed ordering belongs here; only durable model, file-format, and cross-cutting editing choices belong in `DECISIONS.md`.

Undo/redo is implemented per [its approved design](docs/design/undo-redo.md). The remaining items below are open; each substantial change still needs a scoped plan and approval.

### Editing workflow

- **Implemented:** [Preferences dialog](docs/design/preferences-dialog.md): Edit > Preferences… opens a tabbed application dialog with an initially empty General tab and a Close button. Release and native Windows lifecycle checks pass. Actual preference controls and persistence remain separate follow-up work.

- **Implemented:** [Edit menu groups and clipboard](docs/design/edit-menu-clipboard.md): grouped Cut/Copy/Paste/Delete, rotate and flip actions with shortcuts, text-focus handling, and a topology-preserving schematic clipboard. Cut/Copy/Paste now work on schematic selections and focused text fields; native menu and shortcut checks pass.

- **Implemented:** [Movable property blocks](docs/design/property-block-placement.md), including tighter line spacing, block-only dragging, automatic-position reset, transform behavior, legacy relayout, and a left/below default Properties dock.

- **Implemented:** [Instance properties and selection inspector](docs/design/instance-properties.md), including right-hand property editing, visibility checkboxes, `[Multiple values]` feedback, and atomic bulk edits.

- **Implemented:** [Group move, rotate, and flip](docs/design/group-transforms.md), including internal wiring, anchored external connections, atomic validation, live move previews, and mixed-selection undo restoration.

- **Implemented:** [Directional box selection](docs/design/group-selection.md), including whole-component label handling, wire-route selection, live feedback, additive Ctrl-drag selection, and Escape cancellation.

- **Implemented:** Extract the shared symbol renderer and replace the flat text list with a category tree and graphical symbol grid.
- **Implemented:** Drag a palette symbol to the snapped canvas position through the shared undoable placement path.
- **Implemented:** Provide separate, movable File, Tools, Edit, and View toolbars; show or hide toolbars and dock panels from View > Toolbars; restore their layout between sessions. Keep zoom in the View menu/Ctrl+wheel.
- **Implemented:** Show terminal markers and a hover cue for candidate wire connections; Escape cancels a draft and returns to Select.

Palette placement and toolbar edits use the established edit-commit path so they are undoable from introduction.

### Reliability and usability

- Investigate the existing Debug `MainWindow` shutdown assertion in `schematicopen_tests` (`transformToolbarActionsAreUndoable`); reproduced on the unchanged revision with Qt 6.11.1 / MinGW 13.1 during [group-selection verification](docs/design/group-selection.md).

- Bound grid-rendering work for very small document grid spacing while preserving exact spacing for snapping.
- Decide how users recover from an invalid vertex in a pending wire (for example, reject it with feedback or provide a remove-last-vertex action).
- Decide whether component moves that retrace a route should be rejected or should collapse the retraced corner.
- Add the `.schematic` suffix on save when a path has no extension.
- Make persistent load warnings discoverable after the temporary status message expires.
- Consider showing a concise document name in the window title instead of the full path.

## Symbol library growth

**Implemented:** the [built-in template refresh](docs/design/builtin-template-refresh.md) replaces compatible resistor/capacitor artwork, adds the electrolytic and grid-aligned op-amp, and retains the original op-amp identity in a Compatibility category. The nine-entry library is checked against the source templates; all seven Release suites pass and palette captures were visually inspected.

The [draft symbol style guide](docs/design/symbol-style-guide.md) proposes shared drawing rules and component-specific conventions. The four reviewed [Grid 10 templates](symbol-templates/grid10/README.md) are now references for the packaged refresh; capacitors use black box plates, with a hollow positive plate for the electrolytic. The broader guide and future component-family conventions remain open; monochrome black-fill rendering support is implemented.

**Implemented:** [Black symbol fills](docs/design/black-symbol-fills.md) supports monochrome fill storage, SVG import/export, shared canvas/palette/import-preview rendering, legacy white-fill compatibility, and version-4 document writing so older builds reject unsupported embedded artwork. All seven Release test suites pass; native preview and captured canvas/fixture rendering were checked. Capacitor template revisions remain separate symbol-content work.

The data-driven library is in place. All entries should use the same format and management path. Add symbol groups with focused validation and serialization coverage; preserve released IDs, terminal order, and terminal positions as described in [DECISIONS.md](DECISIONS.md).

Potential additions, subject to selection and design when work is planned:

- JFET, BJT, and MOSFET variants.
- LED, Zener, and Schottky diodes; switches, inductors, and supply rails.
- Triode and pentode valves, and transconductance amplifiers.
- Input, output, and bidirectional signal ports. Their labels and direction should remain presentation metadata and must not infer electrical connectivity.

Agree the precise set, geometry, categories, and acceptance checks as part of the proposal for each group of symbols.

## Symbol library authoring

The approved [symbol import design](docs/design/symbol-import.md) describes a restricted SVG profile, in-app metadata and terminal editing, conversion to the native symbol definition, library storage, and document embedding. J1–J4 are implemented; the [J4 plan](docs/design/symbol-import-j4-plan.md) records verification and the outstanding manual visual check, and J5 symbol content will need its own scoped plan. Inkscape or another SVG editor remains the drawing tool; the application does not include a general-purpose artwork editor in this scope.

Exporting an individual symbol as SVG and its metadata sidecar supports authoring and round trips. Exporting a complete schematic to SVG is a separate required future capability; it is not part of the symbol-creation feature.

## Other possible directions

- Export complete schematics to SVG (required future capability); PNG and PDF export are also possible later formats.
- Add free-text annotations and callouts, and improve label placement for rotated or crowded symbols.
- Persistent named groups and custom transform pivots remain later possibilities beyond the implemented [selection transforms](docs/design/group-transforms.md).
- Add UI and file-I/O tests where they fit the existing headless model-test setup.

## Open design questions

- Should a junction dot appear where multiple wire routes meet a component terminal? Current rendering derives dots from shared route endpoint identities with three or more wire ends.
- If future symbol features require per-instance metadata or net naming, what document-format changes and migration rules are needed?

## Planning and status

For substantial work, inspect the current code and propose a short, reviewable plan before implementation. Each proposal should state scope, design choices that are costly to change, and acceptance checks. Follow the approval process in [AGENTS.md](AGENTS.md). Update this roadmap when priorities or open questions change; record settled cross-cutting choices in [DECISIONS.md](DECISIONS.md).
