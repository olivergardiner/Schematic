# Complete-schematic SVG export

Approved scope: File > SVG Export, Ctrl+E, and one shared File-toolbar action with a page/outward-arrow icon.

## Output

- Export the complete committed document, including component artwork, visible reference/value/custom property rows, wrapped text notes, wires and derived junction dots. Preserve transforms, property-block placement and justification, line weights, black fills and opaque white symbol fills.
- Use a separate, unselected `SchematicScene` bound to a document snapshot. This reuses the canvas geometry and label layout without reading or changing the live scene. The export scene stays in Select mode and has no background brush; canvas grid, terminal indicators, hover cues, selection colours and unfinished edits are excluded.
- Fit the SVG viewBox to the rendered item bounds plus a 10-scene-unit margin on every side. Export is independent of canvas zoom, pan and document grid. An opaque white rectangle covers the full viewBox beneath the schematic, including the margins. Drawing geometry remains vector; labels remain editable SVG text and use the canvas font settings. Other viewers need suitable fonts to reproduce text metrics.
- Generate through Qt SVG in the application layer. `schematiccore` continues to depend on Qt Core only. Document and clipboard formats do not change.

## File and application behaviour

- A save dialog filters SVG files and supplies the `.svg` suffix. The initial filename follows the current schematic name, or `schematic.svg` for an unnamed document.
- Render into memory, then use `QSaveFile` without direct-write fallback to replace the destination atomically. Rendering and write failures produce feedback and do not replace an existing destination with partial output.
- Disable export for a document with no components, wires or notes. Note-only schematics are exportable, with wrapped text but no editing frames or handles. Export otherwise works independently of editing mode and selection; it does not change the schematic filename, document contents, undo history or clean state. Cancelling the dialog produces no export.
- PNG/PDF, page sizes, export settings and converting text to outlines are outside this change.

## Validation

Focused exporter checks cover XML validity, vector paths rather than embedded images, editable/escaped text and hidden properties, monochrome fills, opaque white margins, junction dots, tight bounds, rotated/mirrored artwork, exclusion of live selection/drafts, valid SVG reload and file-failure preservation. The window check covers a shared menu/toolbar action, icon, Ctrl+E, cancellation, the default suffix, and unchanged document/history/filename/selection.

## Verification record (8 November 2026)

The MinGW Release application build passed. The focused SVG exporter CTest suite passed offscreen with fonts available, and all six Qt Test results passed using the native Windows backend. The rendered native SVG preview was visually inspected. The full window CTest suite passed in both MinGW Release and Debug, including the SVG action/save-dialog/state checks. Integration also fixed the pre-existing undo-stack title callback during window destruction that caused the Debug type assertion and could corrupt memory in Release. No full manual application walkthrough was performed.

Note paragraph alignment, background fill and outline colours (including alpha) use the shared note renderer and are exported. Transient selection bounds remain excluded.
