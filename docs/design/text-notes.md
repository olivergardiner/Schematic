# Wrapped text notes

Approved scope: dedicated document notes, edited in the existing Properties dock, with draggable wrapping-width handles and automatic height.

## Model and persistence

`TextNote` is independent of library symbols and electrical topology. Each note has its own document-scoped positive ID, top-left position in scene units, plain multiline text, and wrapping width. Default text is `Note`, default width is 160 scene units, and width must be finite and between 20 and 1,000,000 units. Positions and the right edge must be finite; embedded null characters are rejected. Empty text is permitted, so an empty selected note can still be edited.

Format v9 adds a required top-level `notes` array containing objects with `id`, `x`, `y`, `text`, and `width`. Duplicate/invalid IDs, wrong types and invalid geometry reject the whole load. Versions 1–8 remain readable with no notes; older builds reject v9. Notes have no terminals, reference designators, library dependencies, or effect on nets. Height is computed by the Qt text layout and is not persisted.

The version-1 clipboard envelope now embeds v9 document fragments, with v6–v8 fragments still readable. Note-only and mixed selections copy/paste with fresh note IDs and unchanged text, width, appearance and relative placement. Cursor placement includes note origins when calculating the copied group's anchor.

## Interaction

- **Edit > Add Text Note** and the **T** tool in the Tools toolbar enter note placement. Click the canvas to place a snapped note, return to Select, select the new note and focus its text in Properties. Note placement is single-shot, independently of the symbol repeat-placement preference.
- Select and drag the note body to move it. Its selected bounding box shows handles at the middle of each side. Dragging a handle changes width, with wrapping and height previewed automatically; dragging the left handle keeps the right edge fixed. Width adjustments snap to the document grid. Text remains upright during group transforms.
- The Properties dock supplies a plain multiline text editor and wrapping-width control for a single selected note. Enter inserts a paragraph break; Ctrl+Enter applies text, Escape restores the committed note. Leaving the editor applies a valid edit when its selection context is still current. As with component properties, changing the selection/document cancels stale pending edits. Mixed selections do not bulk-edit note text.
- Text-field clipboard and undo/redo operations remain local while the note editor has focus. Canvas selection, directional box selection, group movement, Delete/Cut, cursor Paste, undo and redo include note identities.
- Move and resize previews come from an original document snapshot; release commits one command, Escape cancels. Rejected/no-op edits create no history entries. Notes and their text are not scene-owned source data.

## Rendering and export

Notes use plain text, word wrapping with long-word fallback, a 10-scene-unit font, with transparent fill and no outline by default. Selected editing boxes and handles are transient. SVG export uses the same note rendering/layout and includes wrapped editable text, including a note-only schematic, without selection frames or handles. Rich text, font controls, vertical clipping, callout arrows and rotating text are outside this change.

## Checks

Model checks cover v9 round trips, v8 compatibility, malformed notes, atomic invalid edits, identity exhaustion, clipboard detachment/fresh IDs and mixed group transforms. Canvas checks cover side-handle resizing, reflow, fixed opposite edges, preview isolation, Escape, movement, group selection and undo. Window checks cover placement, Properties text/width editing, cancellation, clipboard, deletion/undo and SVG availability. Export checks cover paragraph breaks, reflow, editable vector text and absence of selection frames.

## Verification (10 November 2026)

The complete MinGW Release build and all eight CTest suites passed. After adding the final safeguards against unedited newline/width display normalization, the application and window test target were rebuilt and the full window suite passed again; the remaining seven suites were unchanged. Native Windows wrapped-note SVG rendering passed and its preview was visually inspected. Checks also cover local text-menu clipboard/undo and precision-preserving inspector focus changes. No full manual application walkthrough was performed.

## Note appearance (format v10)

Properties uses three exclusive icon buttons for Left/Centre/Right alignment, both for component property blocks and for note text. Component mixed selections show no checked button. Note alignment applies to every paragraph inside the fixed wrapping width; it does not move the note anchor.

Fill and Outline buttons open colour dialogs with alpha support. Each button has a menu with **None** to remove its background or border. Text remains black. Colours use `none`, `#RRGGBB` or `#AARRGGBB` strings in the Qt Core-only model; new notes default to Left/none/none. The outline is one scene unit and drawn inside the note bounds. These persistent appearances are included in SVG; selection frames and handles remain transient.

The v10 writer adds required `justification`, `fillColor` and `outlineColor` fields to each note. Invalid values reject the complete load. Versions 1–9 remain readable; v9 notes acquire default appearance. Older builds reject v10 rather than discard styling. Moving/resizing or changing text preserves appearance; clipboard fragments and snapshot undo/redo preserve it as well.

Verification: MinGW Release build and all eight CTest suites pass, including appearance round trips, legacy defaults, clipboard preservation, icon selection, colour acceptance/cancellation/removal, undo and SVG colour rendering.
Notes have four scene units of internal padding on every side. The saved wrapping width remains the overall box width; text wraps within the padded area. Canvas and SVG share this spacing.
