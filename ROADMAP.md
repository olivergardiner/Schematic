# Roadmap and review findings

This document is a working list, not a promise or a fixed schedule. The first usable milestone has its main model, rendering, editing, and JSON file workflows in place. A code review identified the defects below; they are documented for follow-up and **have not been fixed by this documentation update**.

## Correctness and reliability follow-up

The following items were confirmed by source review. Prioritize them before adding larger product features:

1. **Protect unsaved work on window close.** `MainWindow::maybeSave()` is used for New and Open, but the main window has no close-event handler to call it when the application window closes.
2. **Preserve a pending wire after rejected completion.** The scene currently cancels the draft after any completion attempt, including a rejected route. Avoid appending duplicate consecutive corners and keep the draft available for correction when completion fails.
3. **Validate route reversals.** The route validator rejects diagonals and zero-length segments but currently accepts collinear backtracking such as `(0,0) → (50,0) → (30,0)`. Decide whether to reject or normalize redundant/retraced geometry, then cover it with model tests.
4. **Apply reference rules while loading.** Interactive edits reject empty and duplicate trimmed references, but JSON loading should enforce the same document invariant.
5. **Preserve grid precision on open.** The UI spin box shows two decimal places while the file format accepts finer values. Loading a more precise grid spacing can round it and mutate the document. Preserve the loaded value or explicitly constrain the file format and control consistently.
6. **Check the full save write.** `writeDocumentTo()` checks `QSaveFile::commit()` but currently ignores the byte count returned by `write()`.
7. **Make serialized node order stable.** `toJson()` iterates nodes through a `QHash`; sort them by ID so identical documents produce stable JSON across runs.

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

- The current CMake presets and VS Code launch configuration contain machine-specific paths. Before sharing the project broadly, move personal kit paths into a git-ignored `CMakeUserPresets.json` and make checked-in configuration portable.
- Keep this roadmap and [DECISIONS.md](DECISIONS.md) as the repository source of truth. Update or remove items as code and decisions change.
