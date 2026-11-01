# J3 implementation plan: symbol import dialog

Status: **approved and implemented.** This plan records the J3 scope and acceptance checks for the import dialog and package commit described in the [approved symbol import design](symbol-import.md). It does not authorize J4 or J5.

## Goal

Let a user import a sidecar plus SVG, or a bare SVG, inspect the converted glyph, set its origin and terminals, complete metadata, and save a validated symbol package without modifying the selected source files.

## Scope

- Add `File > Import Symbol...` and a modal import dialog with source selection, metadata fields, live preview, terminal editing, and an error/warning list.
- Reuse J2's SVG profile conversion and F's `SymbolRenderer`; keep parsing and geometry conversion in `schematiccore` and UI/input handling in the application layer.
- Preserve input SVG bytes and sidecar metadata, writing normalized source copies and the converted inline symbol definition to the writable symbol library.
- Make a newly imported symbol visible and usable in the current session, and available after restart.
- Do not add SVG artwork drawing/editing, arbitrary SVG support, library browsing/management UI, or document undo entries for library changes.

## Dependency finding and implemented resolution

The original design placed the registry and writable-library work after the import dialog, but the app could not complete J3's save/refresh behavior with its then-current lookup path:

- The palette iterates `builtinSymbolLibrary()` directly in `MainWindow`.
- Placement, `Document::addComponent()`, `Component` terminal calculations, rendering, and JSON loading also look up symbols through the built-in library.
- J2's importer returns a `LibrarySymbol`, but there is no application-owned writable library or way to make an imported definition available to those lookups.

Saving files alone would make the dialog appear to succeed while the new symbol cannot be placed and disappears from the palette on restart. A temporary in-memory-only addition would also leave the saved package inactive after restart.

**Implemented resolution:** J3 included the registry and writable-library foundation needed for imported symbols to be usable immediately. The active application library now loads packaged symbols plus the writable collection, and the package store validates, stages, and installs imports. J4 remains focused on document embedding and conflict handling.

## Dialog behavior

1. `File > Import Symbol...` opens a modal `SymbolImportDialog`.
   - The chooser accepts a sidecar JSON or an SVG.
   - For a sidecar, resolve and validate its relative SVG reference before opening that file; populate the metadata, origin, and ordered terminals from the sidecar.
   - For a bare SVG, leave metadata and terminals empty for the user to enter.
   - Retain source bytes and paths for package copying; never write to either selected source file.
2. Add a headless `SymbolSvgImporter::importArtwork(svgBytes, origin)`-style API that returns converted primitives plus warnings/errors without requiring valid metadata or terminals. Keep `importSymbol()` as the final sidecar-to-`LibrarySymbol` validation path. This lets the preview update while the user is still completing fields and terminal placement.
3. Build a preview widget using `SymbolRenderer::primitives()` (or a shared fit-transform helper) so the SVG artwork, origin marker, grid, and terminal handles use the same scene-to-preview mapping. The artwork is scaled only for the preview.
4. Proposed preview interactions:
   - Click unused preview space to add the next terminal, snapping to 10 scene units.
   - Hold `Ctrl` while placing or dragging a terminal to position it freely.
   - Drag a terminal handle to move it; `Delete` removes the selected terminal.
   - Provide a terminal list with editable name and explicit move-up/move-down controls; list order is terminal index.
   - Drag the origin crosshair or enter its X/Y values numerically. Show terminal coordinates in source SVG space in the list, and preview them at `terminal - origin`.
5. Show ID, name, category, reference prefix, and `showsReference` fields. Validate filename-safe ID characters, required metadata, terminal names/positions, and the final converted symbol before enabling Import.
6. Show importer warnings and errors in the dialog. Add non-blocking terminal warnings for off-grid positions and terminals more than 2 scene units from artwork. Hidden `display="none"` content remains silent per the SVG profile.
7. On Import, ask before replacing a writable symbol with the same ID. Permit replacement only when terminal count, order, names, and positions are unchanged. Do not offer replacement of a packaged/read-only entry. Stage and validate the converted JSON, updated manifest, source SVG, and normalized sidecar before commit; report write failures without reporting success. Refresh the registry-backed palette after a successful commit.

## Proposed implementation files and seams

- `symbolimportdialog.h/.cpp`: dialog form, live validation, interactions, and import result.
- `symbolpreviewwidget.h/.cpp`: fitted artwork, grid/origin/terminal overlays, and terminal hit-testing/dragging.
- Extend `core/symbolsvg.*`: artwork-only conversion and safe sidecar-reference resolution, preserving the existing headless importer/exporter API.
- Add a small package-write service next to the registry/store rather than embedding multi-file persistence in widget code. It receives validated output bytes and reports structured errors.
- Add the import action to `MainWindow`; it owns the dialog, calls the package service, and refreshes the category tree/grid only after a committed save.

## Tests and acceptance

- Headless Core tests cover artwork-only conversion at multiple origins, warnings/errors, and sidecar-relative path validation before any file is opened.
- Test the package writer against a temporary directory: new package, same-ID replacement with unchanged terminals, rejection when the signature changes, and injected/write-failure handling that leaves the previous package intact.
- Qt widget tests cover field validation, terminal ordering/name editing, snapping versus Ctrl-free placement, Delete, and origin changes where the existing Qt Test setup supports stable event simulation.
- Manual walkthrough: import a bare SVG, fill metadata, set the origin both numerically and in the preview, place snapped and free terminals, rename/reorder them, confirm off-grid and distant-terminal warnings, save, see the symbol in the palette, place it on the canvas, restart, and confirm it remains available. Repeat with a sidecar and verify its original files were not modified.
- Verify all four existing CTest targets and the app build/deploy.

## Costly choices resolved by approval

- **Sequence:** the approved J3 implementation absorbs the registry/storage foundation, so J3 precedes the remaining document-embedding work in J4.
- **Preview interaction:** Ctrl-click/drag is proposed for free terminal placement; ordinary placement snaps to the 10-unit grid. Origin can be dragged or entered numerically.
- **Package replacement:** preserve the terminal signature under an existing ID, and never replace packaged/read-only entries from this dialog.

## Review boundary

J3 is implemented within this approved scope. J4 document embedding is covered by the separate [J4 implementation plan](symbol-import-j4-plan.md), which is approved and implemented.
