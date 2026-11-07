# Instance properties and selection inspector

Status: **approved and implemented; verified 6 November 2026.**

## Purpose and starting behavior

Add a right-hand Properties dock for editing the selected symbol instances: reference, value, additional named properties, and each property's visibility on the drawing. Support single and multiple selections, including explicit mixed-value feedback.

Before this feature, `Component` stored reference and value as separate strings. `Document::setComponentLabels()` trims them and requires reference to be non-empty and document-wide unique. The scene draws reference above the symbol when the library's `showsReference` is true, and a non-empty value below it. Double-click opens a two-field dialog; F2 opens a reference rename dialog. Edits already pass through snapshot-based undo/redo, which restores component and wire selection.

This proposal makes property data and visibility instance-owned. It does not add electrical simulation semantics, change symbol terminals, or make a component's value affect its glyph.

## 1. Scope and main choices

- An instance has reserved **Reference** and **Value** properties plus an ordered collection of custom properties such as Tolerance, Voltage rating, Manufacturer, or Part number.
- Each property has its own text value and visible/hidden state. Visibility changes affect drawing labels, not whether the data is retained or editable.
- First-version values are single-line text, preserving strings such as `4k7`, `100 nF`, `±5%`, and `TL072`. No unit conversion, arithmetic, formulas, numeric validation, or device-specific property types are implied.
- The dock supports atomic batch edits for multiple components. Reference text is editable for a single component only; its visibility can still be edited in bulk. A batch-reference renumbering tool is separate work.
- Custom properties can be added, renamed, and removed on instances. The first version does not introduce library-defined property schemas or defaults beyond the existing reference/value behavior.
- Persist the richer instance data in document format **v5**, while continuing to read v1–v4.

These choices keep this stage useful without requiring a full component database or library editor. Stable property identities and the file format leave room for later typed/schema-driven properties.

## 2. Model representation

Introduce a Qt Core-only instance property value type containing:

| Field | Meaning |
|---|---|
| `id` | Stable string key within an instance; independent of display wording. |
| `name` | Display name for a custom property, such as `Tolerance`. Reserved properties have fixed UI labels. |
| `value` | Single-line string; leading/trailing whitespace is trimmed on commit as with current labels. |
| `visible` | Whether a non-empty value is drawn on the canvas. |

Store properties as an ordered collection in `Component`. Reserve `reference` and `value`, always present and always ordered first. Keep `reference()` / `value()` and the existing label-update API as adapters to this collection so there is only one source of truth, not strings maintained alongside a second property map.

Reference remains non-empty and unique across the entire document, including components whose reference is hidden. Value and custom values may be empty. A visible empty property remains configured as visible but produces no empty label item. Hiding a property never deletes its value.

Custom IDs are generated deterministically from the supplied name when the property is first added and remain unchanged on rename. The UI asks for a human-readable name, not an internal key. Validate non-empty names/keys, reserved-key collisions, duplicate keys, ambiguous duplicate display names, valid single-line strings, and required boolean visibility. A name/key collision must be reported rather than silently overwriting a different property. The precise normalization must be shared between single and batch Add operations and covered by tests.

The instance collection is authoritative after placement. For a new instance, Reference is generated as today and its visibility is initialized from `LibrarySymbol::showsReference`; Value starts empty with visibility true; custom properties start empty/hidden when added unless the user chooses otherwise in the Add dialog. Replacing library artwork must not reset existing instance properties or visibility. A hidden ground reference can be shown per instance without modifying the ground library entry.

## 3. Properties dock

Use a standard movable, closable `QDockWidget`, initially on the right. Give it a stable object name, add its toggle action to the existing View dock menu, and save/restore its position and visibility with the window layout. Define a sensible default for users whose saved layouts predate the dock.

Show a selection summary above a three-column editor:

| Property | Value | Visible |
|---|---|---|
| Reference | `R3` | Checked |
| Value | `10k` | Checked |
| Tolerance | `1%` | Unchecked |

- Reference and Value rows are permanent and cannot be removed or renamed. Their content and visibility can be edited subject to the reference restriction below.
- Use text editors for values and checkboxes for visibility. Provide Add Property and Remove Property controls; rename a custom property's display name through a clear single-selection action.
- Show the symbol name for one component, or the number of selected components for multiple components. Symbol ID/type is contextual read-only information, not an editable property that swaps component geometry.
- With no component selected, show an empty state. With wires only, say that the selection has no component properties. With components and wires selected, identify how many components will be edited; wires keep their selection and are not property targets.
- Disable property edits during a move preview so a candidate document cannot race an instance edit. The inspector should not flicker through empty selections while scene items are internally rebuilt.

### Existing edit gestures

Double-clicking a component or its visible label should select that component, reveal the dock, and focus its Value row. F2 / Rename should reveal the dock and focus Reference for exactly one selected component. Both paths use the same validation and commit logic as direct dock edits, replacing the old dialogs rather than maintaining two editing implementations.

While a text cell is active, Delete, Backspace, Ctrl+A, and normal text undo must operate on its text rather than deleting/selecting scene objects. Document Undo/Redo remains available through the application actions; define shortcut handling explicitly so a keystroke cannot execute both text and document undo.

## 4. Multiple selections and mixed state

Group properties by their stable IDs, not by row number or translated label. Show Reference and Value first, followed by the union of custom property IDs in a deterministic order. Annotate a custom row present on only some targets with a count, for example `2 of 3 components`.

| Situation | Value display | Visible display |
|---|---|---|
| All targets have the property and the same value | The shared value | Checked/unchecked if uniform; mixed otherwise |
| All targets have it, with differing values | Placeholder **`[Multiple values]`** | Independently aggregated |
| Property is absent on some targets | Placeholder **`[Not set on all]`**, with presence count | Mixed, unless all targets have the same explicit state |
| All targets have an empty value | Empty editable value, not a mixed placeholder | Reflects the saved visibility flags |

Mixed placeholders are editor state, never literal property values. Focusing a mixed row, opening the dock, or refreshing selection must not write anything. If a user intentionally types the same characters as a placeholder, that is real text and must be distinguishable from the placeholder state.

- Entering a value into a multi-selection row explicitly applies that value to every selected component. If a custom property is missing on some targets, create it there as part of this edit; show that applicability in the row's tooltip/help text before commit. Newly created properties use the row's common name (or the stable key if existing names differ) and start hidden; existing visibility flags are unchanged.
- A mixed visibility checkbox uses an explicit first action to set all targets visible; the next sets all hidden. Merely displaying its partial state does not edit anything.
- When a visibility-only action touches a custom property missing on some targets, create it there with an empty value and the chosen visibility. Do not copy another component's value implicitly. The row remains `[Multiple values]` if its resulting values differ.
- Editing a value does not change existing visibility flags; editing visibility does not change existing values. A row refresh recomputes these independently.
- Remove Property deletes that custom property from all selected components that have it, as one operation. It never removes reserved Reference or Value.
- Reference text shows the common value or `[Multiple values]` but is read-only when more than one component is selected, with an explanation about unique references. Reference visibility remains editable. This avoids assigning one duplicate reference to an entire selection.
- Custom name editing is single-selection only. If the same custom ID has different names across targets, show the key with a tooltip listing the names; do not silently rename existing properties during a value or visibility edit.

Example: selecting resistors with values `10k` and `22k` shows `[Multiple values]`. Typing `47k` and committing changes both values in one undo step. If one value label was hidden and the other visible, those individual visibility flags stay unchanged until the user edits the Visible checkbox.

## 5. Commit, validation, and undo

Expose a batch property-edit API on `Document`, accepting component IDs and explicit operations: set value, set visibility, add/remove custom property, or rename its display name. Do not pass widgets, scene items, or UI mixed-state markers into the model. Resolve all targets before mutation, deduplicate IDs, and validate the complete candidate document before committing once.

Validate final references against all components, preserve every property not included in the patch, and reject the whole operation if any target or property is invalid. Do not loop over mutating single-component setters and leave earlier components changed if a later one fails. Keep the model Qt Core-only and use the existing `DocumentEditController` for one snapshot command per committed row/checkbox/add/remove operation across the selection. No-ops and rejected changes create no history entry.

Text edits commit on Enter/Tab or on leaving a cell while the same document/selection context is still active. Escape restores the stored value or mixed placeholder. A selection change, New/Open, document Undo/Redo, or move start cancels an uncommitted edit rather than applying it to a newly selected set. Capture target IDs plus an application document/selection revision when editing starts; a deferred focus-out callback may commit only if that context still matches. This also protects against reused component IDs after New/Open or undo.

Show validation errors beside the editor, retain the entered text for an explicit rejected commit, and leave the document unchanged. Refreshing the inspector from selection, undo/redo, or a scene rebuild must block widget-to-model edit signals. Preserve the edited row and keyboard focus across a successful self-triggered rebuild where practical.

Undo/redo restores values, names, visibility, ordering, and component/wire selection with the existing clean-index behavior. Selection or dock refresh alone never dirties the document. Adding a property with both an initial value and visibility is one command, not several independent updates.

## 6. Canvas presentation

Extend component label items to identify both their owning component and property key. Clicking or dragging any visible property label still selects/moves its component; labels are not separate electrical or transform targets.

- Draw the visible Reference above the artwork, as today. Draw visible Value below it, followed by other visible properties in their stored order.
- Reference and Value display their values alone; custom properties display `Name: value` so multiple labels remain understandable.
- Use the current label font and gap as the baseline. Stack labels with their actual text heights and a consistent gap; hidden/empty rows do not reserve vertical space.
- Use the component's transformed artwork bounds for the top/bottom anchors so rotated/mirrored symbols do not collide with an unrotated placement assumption. Text remains upright and is laid out again after a transform; labels do not become mirrored text.
- Show all requested visible labels, even if they increase the drawing bounds. Do not silently truncate saved text or drop labels. Manual label placement and collision avoidance are later work.

Existing directional box selection should include every visible property label as part of its owning component. Hidden properties have no label shape. Group-transform pivots continue to exclude label bounds, and property visibility never moves components, terminals, nodes, or wire routes. Candidate move previews use the same label renderer as committed documents.

## 7. Persistence and migration

Write **format v5**, accepting v1–v5. A component stores one ordered `properties` array instead of duplicate top-level reference/value strings. For example:

```json
"properties": [
  { "id": "reference", "value": "R1", "visible": true },
  { "id": "value", "value": "10k", "visible": true },
  { "id": "tolerance", "name": "Tolerance", "value": "1%", "visible": false }
]
```

V5 validates the reserved rows, unique IDs, custom names, strings, visibility booleans, ordering, and document-wide references. Reject duplicate legacy top-level `reference`/`value` fields in v5 rather than choosing between two sources of truth. Older builds reject v5 rather than silently discarding custom properties or visibility.

For v1–v4 migration, retain the current reference generation/trim/uniqueness and value-trim policy. Construct the two reserved properties from the loaded strings. Initialize Reference visibility from the document-resolved symbol's `showsReference` (including an embedded v3/v4 definition), and Value visibility to true. Empty Value remains undrawn. Existing files therefore retain their effective label visibility. Persist those instance choices on the next save; later library updates do not overwrite them.

Library symbol format, terminal signatures, SVG artwork, symbol-package storage, and embedded definition conflict policy remain unchanged. New instance properties travel with document snapshots and saved components; the original SVG does not carry their per-instance values.

## 8. Implementation stages

1. **Property model, batch editing, and v5:** add the value type/collection and compatibility accessors, initialize defaults, implement atomic operations, migrate v1–v4, and test data/visibility/ordering and reference validation before introducing UI.
2. **Property labels:** render the visible collection with property ownership and deterministic layout; check selection, movement previews, rotation/flips, and label bounds.
3. **Single-selection inspector:** add the right dock, text/visibility editors, custom-property operations, error feedback, layout persistence, and shared double-click/F2 paths.
4. **Mixed selection and undo integration:** add aggregation, partial-presence rows, tri-state visibility, atomic bulk operations, context-safe commit/cancellation, and focus/shortcut handling. Verify existing editing actions still work.

These approved steps are implemented. Settled behavior is recorded in DECISIONS.md.

## 9. Acceptance checks

- Reference, Value, and several custom properties persist independently per instance. Hiding a property retains its text. A ground instance can override reference visibility without changing its library definition.
- Reserved fields cannot be removed/renamed; invalid references, duplicate IDs/names, missing targets, and malformed values reject the whole batch with no partial changes or undo command.
- Mixed text/visibility and partial-presence rows follow the table above. Untouched placeholders are never stored; bulk value edits preserve visibility, and visibility edits preserve values.
- One action across many selected components is one undo step. Undo/redo restores properties, order, selection, and clean state; a same-value/no-op edit creates none.
- Pending edits cannot reach a changed selection or a new/reopened/restored document. Inspector updates and scene rebuilds create no edits. Text-edit shortcuts do not operate on scene items unexpectedly.
- Visible labels render consistently, stay upright after transforms, stack without overlap within the component's label stack, and participate correctly in box selection and movement. Hidden labels disappear without altering topology or group pivots.
- V1–v4 loads retain prior values and effective visibility; v5 round trips custom properties and rejects malformed data/future versions. Library replacement and embedded-symbol choice do not reset instance property data.
- The right dock restores its layout, works with a closed/reopened panel, and handles no selection, wires only, and mixed component/wire selection. Double-click and F2 use the same inspector editing path.
- Run focused Core/persistence, undo, scene/selection, and window tests. Native walkthrough covers mixed values, partial presence, error handling, visibility, keyboard focus, dock layout, transforms, save/reopen, and undo/redo; report automated and manual results separately.

## 10. Choices for approval

Review the string-only first version, custom instance properties without library schemas, read-only reference text on multi-selection, partial-presence creation rules, per-row commit/cancellation policy, deterministic label stacks, instance-owned visibility, and format-v5 representation. These are the choices most costly to change after users create documents with the feature.

## Verification record

The Release application build and all seven CTest suites passed. After the final canvas-selection cancellation guard, the three affected placement, document-open/inspector, and selection suites passed again. The three inspector scenarios also passed using the native Windows platform: mixed edits and custom properties, context cancellation and keyboard handling, and dock restoration and ground visibility. The captured native inspector layout was visually checked. Coverage includes v1–v4 migration, v5 validation and round trips, atomic rejection, undo/redo, label transforms, and unchanged wire data.

## Movable-block follow-up

The approved and implemented [property-block placement design](property-block-placement.md) supersedes the original split label layout and right-hand dock default described above. Visible Reference, Value, and custom rows now form one compact block with optional instance placement, saved in format v6. The Properties dock defaults beneath Symbols on the left; existing saved application layouts are retained.
