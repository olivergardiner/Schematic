# Roadmap and review findings

This document is a working list, not a promise or a fixed schedule. The first usable milestone has its main model, rendering, editing, and JSON file workflows in place. A code review identified the defects below; they are documented for follow-up and **have not been fixed by this documentation update**.

## Milestone 2 plan

The following M2 scope and decisions have been agreed. Work is divided into chunks A–H. **Each chunk requires the user's approval before implementation begins; completion of one chunk does not authorize the next.** Keep each handoff limited to that chunk and its stated acceptance checks.

### Settled M2 decisions

- Horizontal mirroring is in M2. Arbitrary-angle rotation and vertical mirroring are not included.
- The project format advances to version 2 in chunk D. The loader accepts versions 1 and 2. Version 1 components are unmirrored; a `mirrored` property is invalid in version 1. Version 2 requires a boolean `mirrored` field on every component. `toJson()` always writes version 2, including `mirrored: false` where appropriate; opening and resaving a v1 file therefore upgrades it to v2.
- On load, a missing, empty, or whitespace-only reference is generated using the component kind's reference prefix and receives a warning. Generation is deterministic and avoids references already used by explicit labels or other generated labels.
- Non-empty duplicate references are rejected with an error identifying both components (kind and component ID) and the conflicting reference. Comparison is case-sensitive after trimming, matching the existing edit rule. This avoids silently renaming user-authored labels.
- Undo/redo is in M2. Its design note must settle clean/dirty state, selection restoration, command boundaries for drags and compound wire operations, and New/Open/save behavior before implementation.
- JFET is the first new symbol.

### Chunks and acceptance checks

**A — Repository and comment housekeeping**

- Replace code/test comments that cite obsolete numbered planning sections with durable explanations or links to `DECISIONS.md`.
- Keep the mirroring decision clear: out of scope for milestone 1 and in scope for milestone 2 (format v2).
- Add `CMakeUserPresets.json` to `.gitignore`; remove machine-specific paths from checked-in CMake/VS Code configuration and use a user preset for local Qt/compiler paths.
- Done when no obsolete step-plan references remain and a clean configure succeeds using a user preset.

**B — File and document reliability**

- Add the window close prompt using the existing unsaved-change flow.
- Check that `QSaveFile::write()` writes the complete byte array before committing.
- Prevent grid-control signals from mutating a newly loaded/new document during UI synchronization, and preserve the file's supported grid precision in the control/model.
- Serialize nodes in stable ID order.
- Implement and test the settled loader reference policy. Add reference-prefix metadata for the existing symbols as needed for deterministic generation; chunk D uses the same metadata for defaults on newly placed components.
- Done when focused tests cover reference repair/duplicate rejection, precise grid round trips and stable node order, and the close prompt is checked manually.

**C — Wire recovery, route reversal, and selection preservation**

- Keep a pending wire available for correction when a completion attempt is rejected; do not append duplicate consecutive vertices.
- Preserve selected component identities across scene rebuilds. Current wire items are not selectable, so this chunk must not silently expand scope to wire selection.
- Reject a collinear route that reverses direction (for example, `(0,0) → (50,0) → (30,0)`) through the shared route validator; ensure endpoint movement cannot commit such a route.
- Done when model tests cover route reversals, a manual check confirms a rejected wire can be corrected, and selection survives edits that rebuild the scene.

**D — Component transforms, references, and format v2**

- Add transactional `Document::rotateComponent()` and horizontal mirroring. Both update terminal-anchored route geometry atomically and reject a transform if any affected route becomes invalid.
- Add a `mirrored` component property and implement the v1/v2 loading and v2 writing contract above. Mirror local symbol geometry horizontally before applying quadrant rotation; keep label text readable and do not mirror it.
- Add per-kind reference prefixes (for example, R, C, D) for default references and a `showsReference` property for symbol kinds that should hide that label while retaining a unique internal reference.
- Done when headless tests cover rotation/mirror terminal positions, transactional rejection, defaults, and v1/v2 round trips including invalid version/property combinations.

**E — Additional built-in symbols**

- Add JFET first, followed by BJT, LED, switch, inductor, supply rail, and input/output/bidirectional signal ports.
- Signal direction and labels are presentation metadata; they do not infer or alter electrical connectivity.
- Done when each symbol definition and its JSON round trip are covered by focused tests.

**H — Undo/redo design and core integration (schedule before F and G)**

- Write and approve a design note before coding. It must define clean-index dirty state, New/Open/save interaction, selection restoration, and transaction boundaries: a component drag is one command and a completed branched wire operation is one command.
- Keep `schematiccore` Qt Core-only. Put any `QUndoStack` or scene/controller integration in the application layer.
- Establish a single edit-commit path and route existing edits plus the M2 model operations through it. This gives the palette drop and toolbar actions in F/G an undoable path from their first implementation.
- Done when undo/redo tests cover model state and clean/dirty transitions, and manual checks confirm selection restoration and one-step undo for a drag and a branched wire.

**F — Shared renderer, grouped palette, and drag-to-place**

- Extract the current primitive path generation into a shared application-layer renderer used by both canvas and palette, so symbol artwork matches.
- Build grouped Passives, ICs, Transistors, Connectors, and Signals palette sections with graphical symbols, accessible names, and tooltips.
- Support dragging a palette symbol onto the canvas and placing it at the snapped drop position through the edit-commit path.
- Done when palette artwork matches the canvas and a drop places one snapped, undoable component.

**G — Frequent-action toolbar and interaction polish**

- Add a graphical toolbar with in-code icons for frequent actions, including rotation and horizontal mirror. Enable transform actions only when one component is selected.
- Move zoom and grid spacing controls to the View menu/secondary controls; Ctrl+mouse-wheel remains the primary zoom interaction.
- Escape returns to Select mode; highlight valid terminals while drawing wires.
- Done when a manual walkthrough covers placement, rotation, mirroring, undo, and wire drawing.

**H - Tidy up M2 comments

- Make sure that there are no stale references to chunks once M2 is complete

### Self-contained handoff brief: chunks A–C

Use this brief when requesting implementation of any one of A, B, or C. It assumes no context beyond this repository, `AGENTS.md`, `DECISIONS.md`, and this section of `ROADMAP.md`.

**Shared instructions:** First inspect the relevant code and current working tree. Implement only the named chunk, in small reviewable changes. Preserve the Qt Core-only model boundary and the existing project conventions. Add focused tests for model behavior, run the smallest relevant checks, and report exactly what ran. Do not start another chunk without explicit user approval. Do not broaden a chunk into M2 features assigned to D–H.

**Chunk A brief:** Clean up obsolete code/test comment references to numbered planning sections; ensure mirroring is described as out of scope for milestone 1 and in scope for milestone 2 (format v2); make checked-in CMake and VS Code configuration portable; ignore `CMakeUserPresets.json` and put machine-specific paths there. Do not change application behavior. Acceptance: no obsolete step-plan references remain; a clean configure succeeds from a user preset.

**Chunk B brief:** Implement the window-close unsaved-changes prompt; check full `QSaveFile::write()` completion; synchronize grid controls without value-change callbacks mutating the document and preserve supported precision; serialize nodes by ascending ID; and implement the settled loader reference policy. Missing/blank references are generated deterministically with warnings, avoiding explicit/generated collisions. Reject duplicate non-empty references after trimming, case-sensitively, with an error that names both components by kind and ID plus the duplicate text. Add focused tests for loader policies, grid precision round trips, and stable node order. Add reference-prefix metadata for existing symbols if needed, but defer changing default labels for newly placed parts to D. Check the close prompt manually. Do not change the file format version in B.

**Chunk C brief:** Keep pending wires editable after rejected completion and suppress duplicate consecutive points. Preserve selection of currently selectable components by stable component ID when rebuilding the scene; wire selection is outside this chunk because wire items are not selectable today. Extend shared route validation to reject collinear backtracking and ensure component endpoint movement uses the same validator. Add model tests for reversals and manual interaction checks for wire recovery and selection retention. Do not add rotation, mirroring, undo/redo, or palette drag/drop here.

## Correctness and reliability follow-up

The following items were confirmed by source review. Prioritize them before adding larger product features:

1. **Protect unsaved work on window close.** Implemented (chunk B): `MainWindow::closeEvent()` calls `maybeSave()`. Not yet manually verified - check New/Open/Exit/title-bar close with Save, Discard and Cancel before treating this as fixed.
2. **Preserve a pending wire after rejected completion.** The scene currently cancels the draft after any completion attempt, including a rejected route. Avoid appending duplicate consecutive corners and keep the draft available for correction when completion fails.
3. **Validate route reversals.** The route validator rejects diagonals and zero-length segments but currently accepts collinear backtracking such as `(0,0) → (50,0) → (30,0)`. Decide whether to reject or normalize redundant/retraced geometry, then cover it with model tests.
4. **Apply reference rules while loading.** Fixed (chunk B), covered by tests: missing/blank references are generated from per-kind prefixes with a warning; duplicate non-empty references are rejected with an error naming both components.
5. **Preserve grid precision on open.** Fixed (chunk B): the control is synchronized with signals blocked, so loading cannot rewrite the document value, and the file value round-trips exactly (tested). The spin box shows 4 decimals and is limited to 0.1-1000, so a valid file value outside that range displays clamped but is kept unchanged in the model.
6. **Check the full save write.** Fixed (chunk B): a short or failed `QSaveFile::write()` cancels the save instead of committing. Not covered by an automated test (no practical way to force a short write headlessly).
7. **Make serialized node order stable.** Fixed (chunk B): `toJson()` writes nodes in ascending ID order (tested).

Smaller file/UI follow-ups from the review:

- Add the `.schematic` suffix when saving if the chosen path has no extension.
- Consider showing a shorter document name rather than the full path in the title bar.
- Make persistent load warnings discoverable after the temporary status-bar message expires (for example, retain them in a log or document notice).

## Decisions to settle before changing the model or file contract

- **Symbol geometry compatibility:** version 1 derives route endpoints from current built-in terminal positions. Moving a terminal offset can make an existing saved route invalid on load. Freeze geometry before public files exist, or design explicit symbol-definition versioning/migration before changing it.
- **Default references:** current generated labels use the global component ID and full symbol names (for example, `Resistor2`). Decide whether to move to conventional per-kind references such as `R1`/`C1`, and whether grounds or jacks should have references, before changing the defaults or uniqueness rules.
- **Junction dots at component terminals:** decide whether a terminal shared by multiple wire routes should display a dot. Current derived dots are based on shared route endpoint identities with three or more wire ends.
- **Off-grid terminals:** op-amp terminal offsets are not all grid multiples. This follows the world-coordinate design decision; changing offsets for grid alignment would alter symbol geometry and saved-wire compatibility, so do not do it as a cosmetic fix.
- **Document compatibility for future symbols and transforms:** custom symbol definitions, changed terminal positions, and mirroring affect how documents reopen. Choose how definitions are versioned or stored with documents before shipping these capabilities.

## Later presentation and editing features

These are product ideas from review, not confirmed requirements for the first milestone:

- SVG, PNG, or PDF export.
- Free-text annotations and callouts; improved label placement for rotated or crowded symbols.
- Undo/redo.
- More guitar-pedal symbols (such as JFETs, BJTs, LEDs, switches, inductors, and supply symbols) and op-amp input polarity marks.
- Escape-to-Select behavior and moving multiple selected components together.
- Automated UI/file-I/O tests where they can be added without undermining the existing headless model-test setup.

## Editing and symbol-library direction

The following are user-requested directions for the next product iteration. Details can be planned before implementation:

- **Grouped symbol palette:** organize symbols into simple categories such as Passives, ICs, Transistors, Connectors, and Signals. Signals can include ground and supply symbols plus labelled input, output, and bidirectional signal connectors. Signal labels and direction are presentation metadata; they must not infer electrical connectivity.
- **Drag-to-place palette:** users should be able to drag a symbol from the palette onto the canvas to place it, rather than selecting a text item and clicking to place. Show the actual graphical symbol in the palette instead of a text-only name; retain accessible names and tooltips.
- **Frequent-action toolbar:** replace text-heavy controls with a graphical toolbar focused on common editing actions. Use recognizable icons, tooltips, and consider visible text labels for less familiar actions. Show rotation and mirroring controls when a component is selected. Keep zoom and grid-spacing controls in the View menu or other secondary controls; Ctrl+mouse-wheel remains the primary zoom interaction.
- **Rotation and mirroring:** these should be document-model operations, not render-only transforms. They change terminal positions and may invalidate connected routes, so apply the same transactional validation used for component movement. Decide how mirroring affects terminal identities and labels before implementation.
- **User-authored symbols:** start with an SVG import workflow and a Schematic step for identifying connection points, rather than building a full drawing editor immediately. Before implementation, decide how SVG artwork and terminal definitions are packaged or embedded so documents remain usable when the original external SVG is moved or changed. Symbol geometry, terminal identity, and file compatibility need a dedicated design plan.
- **Wire-drawing feedback:** highlight available terminals while drawing wires.

## Repository and release readiness

- Checked-in CMake presets and VS Code configuration no longer contain machine-specific paths: Qt kit paths go in a git-ignored `CMakeUserPresets.json`, and VS Code launch/deploy tasks read `QT_BIN_DIR` (see README and CLAUDE.md).
- Keep this roadmap and [DECISIONS.md](DECISIONS.md) as the repository source of truth. Update or remove items as code and decisions change.
