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

## Later presentation and editing features

These are product ideas from review, not confirmed requirements for the first milestone:

- SVG, PNG, or PDF export.
- Free-text annotations and callouts; improved label placement for rotated or crowded symbols.
- Rotation controls for placed components.
- Undo/redo.
- More guitar-pedal symbols (such as JFETs, BJTs, LEDs, switches, inductors, and supply symbols) and op-amp input polarity marks.
- Terminal highlighting during wire drawing, symbol icons in the palette, Escape-to-Select behavior, and moving multiple selected components together.
- Automated UI/file-I/O tests where they can be added without undermining the existing headless model-test setup.

## Repository and release readiness

- The current CMake presets and VS Code launch configuration contain machine-specific paths. Before sharing the project broadly, move personal kit paths into a git-ignored `CMakeUserPresets.json` and make checked-in configuration portable.
- Keep this roadmap and [DECISIONS.md](DECISIONS.md) as the repository source of truth. Update or remove items as code and decisions change.
