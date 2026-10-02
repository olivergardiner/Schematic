# Roadmap and review findings

This document is a working list, not a promise or a fixed schedule. The first usable milestone has its main model, rendering, editing, and JSON file workflows in place. A code review identified the issues below; each item records whether it remains open or has been addressed.

## Milestone 2 plan

The following M2 scope and decisions have been agreed. Work is divided into chunks A–I (E is split into E0–E3). Chunks A–D are complete. **Each chunk requires the user's approval before implementation begins; completion of one chunk does not authorize the next.** Keep each piece of work limited to that chunk and its stated acceptance checks.

### Settled M2 decisions

- Horizontal mirroring is in M2. Arbitrary-angle rotation and vertical mirroring are not included.
- The project format advances to version 2 in chunk D. The loader accepts versions 1 and 2. Version 1 components are unmirrored; a `mirrored` property is invalid in version 1. Version 2 requires a boolean `mirrored` field on every component. `toJson()` always writes version 2, including `mirrored: false` where appropriate; opening and resaving a v1 file therefore upgrades it to v2.
- On load, a missing, empty, or whitespace-only reference is generated using the component kind's reference prefix and receives a warning. Generation is deterministic and avoids references already used by explicit labels or other generated labels.
- Non-empty duplicate references are rejected with an error identifying both components (kind and component ID) and the conflicting reference. Comparison is case-sensitive after trimming, matching the existing edit rule. This avoids silently renaming user-authored labels.
- Undo/redo is in M2. Its design note must settle clean/dirty state, selection restoration, command boundaries for drags and compound wire operations, and New/Open/save behavior before implementation.
- The potentiometer's reference prefix is `VR`.
- Symbols become library data rather than hard-coded C++ (chunk E0, subject to its design note). JFET is the first new symbol added once that library exists.

### Chunks and acceptance checks

**A — Repository and comment housekeeping** (done)

- Replace code/test comments that cite obsolete numbered planning sections with durable explanations or links to `DECISIONS.md`.
- Keep the mirroring decision clear: out of scope for milestone 1 and in scope for milestone 2 (format v2).
- Add `CMakeUserPresets.json` to `.gitignore`; remove machine-specific paths from checked-in CMake/VS Code configuration and use a user preset for local Qt/compiler paths.
- Done when no obsolete step-plan references remain and a clean configure succeeds using a user preset.

**B — File and document reliability** (done)

- Add the window close prompt using the existing unsaved-change flow.
- Check that `QSaveFile::write()` writes the complete byte array before committing.
- Prevent grid-control signals from mutating a newly loaded/new document during UI synchronization, and preserve the file's supported grid precision in the control/model.
- Serialize nodes in stable ID order.
- Implement and test the settled loader label policy: generate missing/blank references deterministically with warnings, reject duplicate non-empty references naming both components, and trim loaded values consistently with label editing. Add reference-prefix metadata for the existing symbols as needed for deterministic generation; chunk D uses the same metadata for defaults on newly placed components.
- Done when focused tests cover reference repair/duplicate rejection, precise grid round trips and stable node order, and the close prompt is checked manually.

**C — Wire recovery, route reversal, and selection preservation** (done)

- Keep a pending wire available for correction when a completion attempt is rejected; do not append duplicate consecutive vertices.
- Preserve selected component identities across scene rebuilds. Current wire items are not selectable, so this chunk must not silently expand scope to wire selection.
- Reject a collinear route that reverses direction (for example, `(0,0) → (50,0) → (30,0)`) through the shared route validator; ensure endpoint movement cannot commit such a route.
- Done when model tests cover route reversals, a manual check confirms a rejected wire can be corrected, and selection survives edits that rebuild the scene.

**D — Component transforms, references, and format v2** (done)

- Add transactional `Document::rotateComponent()` and horizontal mirroring. Both update terminal-anchored route geometry atomically and reject a transform if any affected route becomes invalid.
- Add a `mirrored` component property and implement the v1/v2 loading and v2 writing contract above. Mirror local symbol geometry horizontally before applying quadrant rotation; keep label text readable and do not mirror it.
- Add per-kind reference prefixes (for example, R, C, D) for default references and a `showsReference` property for symbol kinds that should hide that label while retaining a unique internal reference.
- Done when headless tests cover rotation/mirror terminal positions, transactional rejection, defaults, and v1/v2 round trips including invalid version/property combinations.

**E0 — Data-driven symbol library (design note first)**

- Write and approve a short design note before coding. The long-term goal is a maintainable, extensible library (transistors including MOSFETs, diode variants, valves, transconductance amps and more), so adding a symbol should be a data entry plus a test, not a C++ change.
- Move symbol definitions into library data (JSON, embedded as Qt resources initially). Each entry has a stable string ID, display name, category (used by the palette in F), reference prefix, `showsReference`, primitives, and named terminals. A registry in `schematiccore` serves them (Qt Core only). `SymbolKind` as a closed enum is replaced by an ID-based handle.
- The existing seven symbols keep their current names as IDs, so version 1 and 2 documents load unchanged. No format version bump in this chunk.
- Terminals are referenced by index in saved wires, so a released symbol's terminal order and positions must never change; changed geometry gets a new ID. The note must decide whether to keep index references or move to names.
- The note must also decide how an unknown symbol ID is handled on load, and where user library files could live later (on disk, embedded in documents). Neither is implemented in E0, but E0 must not block them.
- Done when the seven existing symbols load from library data, all existing tests pass apart from renames for the new API, library validation is tested (duplicate IDs, bad terminal definitions, missing prefix), and a pre-E0 file loads identically.

**E1 — Transistors**

- JFET first, then BJT (NPN/PNP) and MOSFET (N/P). Polarity and type variants are separate library entries, not flags.
- Done when each symbol and its JSON round trip are covered by focused tests.

**E2 — Diodes, passives, valves, and other devices**

- LED, Zener and Schottky diodes; switch, inductor, supply rail; triode and pentode valves; transconductance amp (OTA). Order and exact set to be agreed when E2 is approved.
- Done when each symbol and its JSON round trip are covered by focused tests.

**E3 — Signal ports (design note first)**

- Input, output and bidirectional signal ports. Direction and labels are presentation metadata; they do not infer or alter electrical connectivity.
- These likely need per-instance properties and a format change, so they get their own design note.

**H — Undo/redo design and core integration (schedule before F and G)**

- Write and approve a design note before coding. It must define clean-index dirty state, New/Open/save interaction, selection restoration, and transaction boundaries: a component drag is one command and a completed branched wire operation is one command.
- Keep `schematiccore` Qt Core-only. Put any `QUndoStack` or scene/controller integration in the application layer.
- Establish a single edit-commit path and route existing edits plus the M2 model operations through it. This gives the palette drop and toolbar actions in F/G an undoable path from their first implementation.
- Done when undo/redo tests cover model state and clean/dirty transitions, and manual checks confirm selection restoration and one-step undo for a drag and a branched wire.

**F — Shared renderer, grouped palette, and drag-to-place**

- Extract the current primitive path generation into a shared application-layer renderer used by both canvas and palette, so symbol artwork matches. Generate the sections from the library's category field (E0) rather than a hard-coded list.
- Build grouped Passives, ICs, Transistors, Connectors, and Signals palette sections with graphical symbols, accessible names, and tooltips.
- Support dragging a palette symbol onto the canvas and placing it at the snapped drop position through the edit-commit path.
- Done when palette artwork matches the canvas and a drop places one snapped, undoable component.

**G — Frequent-action toolbar and interaction polish**

- Add a graphical toolbar with in-code icons for frequent actions, including rotation and horizontal mirror. Enable transform actions only when one component is selected.
- Move zoom and grid spacing controls to the View menu/secondary controls; Ctrl+mouse-wheel remains the primary zoom interaction.
- Escape returns to Select mode; highlight valid terminals while drawing wires.
- Done when a manual walkthrough covers placement, rotation, mirroring, undo, and wire drawing.

**I - Tidy up M2 comments**

- Make sure that there are no stale references to chunks once M2 is complete
- Remove finished-work notes from this file (completed chunk descriptions that no longer help) and any stale chunk references in code and tests.

## Correctness and reliability follow-up

The following items were confirmed by source review. Prioritize them before adding larger product features:

1. **Protect unsaved work on window close.** Implemented (chunk B): `MainWindow::closeEvent()` calls `maybeSave()`. Manually verified.
2. **Preserve a pending wire after rejected completion.** Fixed (chunk C): a rejected completion keeps the draft available for correction, and duplicate consecutive points are suppressed.
3. **Validate route reversals.** Fixed (chunk C): the shared route validator rejects collinear backtracking, and endpoint movement uses the same validator. Covered by model tests.
4. **Apply label normalization while loading.** Fixed (chunk B), covered by tests: missing/blank references are generated from per-kind prefixes with a warning; duplicate non-empty references are rejected with an error naming both components; references and values are trimmed.
5. **Preserve grid precision on open.** Fixed (chunk B): the control is synchronized with signals blocked, so loading cannot rewrite the document value, and the file value round-trips exactly (tested). The spin box shows 4 decimals and is limited to 0.1-1000, so a valid file value outside that range displays clamped but is kept unchanged in the model.
6. **Check the full save write.** Fixed (chunk B): a short or failed `QSaveFile::write()` cancels the save instead of committing. Not covered by an automated test (no practical way to force a short write headlessly).
7. **Make serialized node order stable.** Fixed (chunk B): `toJson()` writes nodes in ascending ID order (tested).
8. **Bound grid-rendering work for very small spacing.** The loader accepts every finite positive spacing, while `SchematicView::drawBackground()` currently emits one line per scene-space interval. A file with spacing such as `0.001` can therefore cause an enormous number of grid lines per repaint. Adapt grid drawing to zoom (skip minor lines or use a coarser visual grid below a screen-space threshold) while retaining the document's exact spacing for snapping.
9. **Recover from a bad vertex in a pending wire.** While drawing, a click that would reverse direction on the last segment is accepted into the draft, and nothing removes it. Either reject such a click at click time with a status message (reusing the route validator), or add a remove-last-vertex action. Backspace is already bound to delete-selection, so that action needs another key or a context check.
10. **Component moves that retrace a route.** Moving a component along a wire's axis past the route's next vertex is now rejected. Decide whether to leave this or collapse the retraced vertex automatically (as the existing redundant-corner collapse does), and cover the decision with a model test.

Smaller file/UI follow-ups from the review:

- Add the `.schematic` suffix when saving if the chosen path has no extension.
- Consider showing a shorter document name rather than the full path in the title bar.
- Make persistent load warnings discoverable after the temporary status-bar message expires (for example, retain them in a log or document notice).

## Decisions to settle before changing the model or file contract

- **Symbol geometry compatibility:** version 1 derives route endpoints from current built-in terminal positions. Moving a terminal offset can make an existing saved route invalid on load. Freeze geometry before public files exist, or design explicit symbol-definition versioning/migration before changing it. To be settled in the E0 design note.
- **Junction dots at component terminals:** decide whether a terminal shared by multiple wire routes should display a dot. Current derived dots are based on shared route endpoint identities with three or more wire ends.
- **Off-grid terminals:** op-amp terminal offsets are not all grid multiples. This follows the world-coordinate design decision; changing offsets for grid alignment would alter symbol geometry and saved-wire compatibility, so do not do it as a cosmetic fix.
- **Document compatibility for future symbols and transforms:** custom symbol definitions, changed terminal positions, and mirroring affect how documents reopen. Choose how definitions are versioned or stored with documents before shipping these capabilities. To be settled in the E0 design note.

## Later presentation and editing features

These are product ideas from review, not confirmed requirements for the first milestone:

- SVG, PNG, or PDF export.
- Free-text annotations and callouts; improved label placement for rotated or crowded symbols.
- Escape-to-Select behavior and moving multiple selected components together.
- Automated UI/file-I/O tests where they can be added without undermining the existing headless model-test setup.

## Editing and symbol-library direction

The following are user-requested directions for the next product iteration. Details can be planned before implementation:

- **Maintainable symbol libraries:** the symbol set should grow without code changes (see E0). Categories drive the palette, variants such as N/P-channel or LED versus Zener are separate entries, and user-maintained library files are a later step once the library format and compatibility rules are settled.
- **Grouped symbol palette:** organize symbols into simple categories such as Passives, ICs, Transistors, Connectors, and Signals. Signals can include ground and supply symbols plus labelled input, output, and bidirectional signal connectors. Signal labels and direction are presentation metadata; they must not infer electrical connectivity.
- **Drag-to-place palette:** users should be able to drag a symbol from the palette onto the canvas to place it, rather than selecting a text item and clicking to place. Show the actual graphical symbol in the palette instead of a text-only name; retain accessible names and tooltips.
- **Frequent-action toolbar:** replace text-heavy controls with a graphical toolbar focused on common editing actions. Use recognizable icons, tooltips, and consider visible text labels for less familiar actions. Show rotation and mirroring controls when a component is selected. Keep zoom and grid-spacing controls in the View menu or other secondary controls; Ctrl+mouse-wheel remains the primary zoom interaction.
- **Rotation and mirroring:** these should be document-model operations, not render-only transforms. They change terminal positions and may invalidate connected routes, so apply the same transactional validation used for component movement. Decide how mirroring affects terminal identities and labels before implementation.
- **User-authored symbols:** start with an SVG import workflow and a Schematic step for identifying connection points, rather than building a full drawing editor immediately. Before implementation, decide how SVG artwork and terminal definitions are packaged or embedded so documents remain usable when the original external SVG is moved or changed. Symbol geometry, terminal identity, and file compatibility need a dedicated design plan.
- **Wire-drawing feedback:** highlight available terminals while drawing wires.

## Repository and release readiness

- Checked-in CMake presets and VS Code configuration no longer contain machine-specific paths: Qt kit paths go in a git-ignored `CMakeUserPresets.json`, and VS Code launch/deploy tasks read `QT_BIN_DIR` (see README and CLAUDE.md).
- Keep this roadmap and [DECISIONS.md](DECISIONS.md) as the repository source of truth. Update or remove items as code and decisions change.
