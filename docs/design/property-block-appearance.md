# Property-block grid and justification

Status: **approved and implemented; verified 7 November 2026.**

## Scope

Add a property-block snapping grid in Preferences > Appearance, default 2 scene units, and per-instance Left / Middle / Right justification with per-symbol creation defaults. Keep the existing component/wire grid, topology, symbol geometry and upright text behavior unchanged.

## Appearance preference

- Keep General first and add Appearance. Provide **Property block grid spacing**, default **2 units**, with a positive numeric value (0.1–1000 units, two decimal places).
- Replace the scaffold's Close button with OK / Cancel. Changes are staged; OK stores `appearance/propertyBlockGridSpacing` in the existing QSettings namespace and applies it to the scene. Cancel/Escape change nothing. Missing, non-finite or out-of-range saved values fall back to 2.
- This is application-wide editing behavior, not a document property or undo command. Do not move existing labels when the spacing changes.
- During property-block dragging, snap the block's anchor to the global scene grid, independently on X/Y, using deterministic nearest-grid rounding. Preserve the pointer's original grab offset. Store the resulting component-relative offset through the existing undo path. Component and wire dragging continue using the document grid.

## Justification and anchor

- Add **Property block justification: Left / Middle / Right** to Properties, separate from the text-property table. Apply to one or multiple selected components; show no checked icon when they differ. A bulk change is atomic and one undo step. Disable during active movement and follow the inspector's existing pending-edit/context checks.
- The property block has one shared horizontal anchor. Left rows begin at it; Middle rows are centred on it; Right rows end at it. All visible non-empty rows use the same justification and retain the current compact vertical spacing.
- For automatic placement, use the transformed artwork bounds' left edge, horizontal centre or right edge respectively, with the existing gap below the artwork. This makes a named rail/ground's Middle block centre on its symbol instead of merely centring rows inside a left-positioned rectangle.
- For manually placed blocks, the saved offset denotes that same anchor relative to the component origin along canvas axes. Changing justification keeps the manual anchor fixed; text grows on the appropriate side of it. Rotation/mirroring keep text upright and preserve manual anchor offsets, as today.
- Reset property position clears only the manual offset; it retains justification. Hidden/empty blocks retain both settings, so naming an initially unnamed ground uses its configured justification.

## Model, library defaults and persistence

- Add a first-class three-value justification enum in schematiccore and an instance field on Component; do not encode behavior in a custom text property.
- Add optional native/sidecar `defaultPropertyJustification`, default Left, with values `left`, `middle`, `right`. Omit the Left default on write; reject invalid types/values. Preserve it through import/export and embedded symbol snapshots.
- Set Middle on Power Rail, Earth, Chassis Ground and Signal Ground. All other symbols retain Left unless their definition explicitly chooses otherwise.
- New placement copies the library default into the instance. Library updates and layout switching do not overwrite existing choices; paste copies the instance choice. No terminal signatures or symbol IDs change.
- Write document v8 with explicit per-component `propertyBlockJustification`. Continue reading v1–v7, initializing those instances to Left so their existing automatic/manual positions retain their appearance. Existing `propertyBlockOffset` coordinates are therefore unchanged on migration. V8 offsets are anchor offsets; old Left offsets already have that meaning. Older builds reject v8 instead of discarding alignment. Clipboard version 1 accepts existing v6/v7 and new v8 payloads.

## Implementation and verification

1. Add enum, validated library default, instance state, atomic model edit and v8 persistence/migration, including clipboard and import/export preservation.
2. Lay out rows around the anchor and implement the justification control with mixed-selection, context safety and undo/redo.
3. Add the Appearance preference, staged settings persistence and independent anchor-grid snapping.
4. Test all three row alignments, automatic anchors, unequal row widths, manual anchors, hidden/empty rows, transforms, label edits, snap behavior with off-grid component origins, positive/negative coordinates, preference OK/Cancel/fallback, undo, save/load, paste and legacy migration. Check that document/terminal connectivity is unaffected.
5. Build the application, run the affected existing suites, inspect rendered blocks and the Appearance tab, and update the style guide, preferences/property-placement designs, README, decisions and roadmap.

### Verification results

- Release application and test targets built successfully with the Visual Studio configuration.
- All seven CTest suites passed with the Qt offscreen platform. Checks cover native/SVG metadata round-trips and validation, instance defaults, v8 persistence and v6/v7 migration, paste, all three row anchors, unequal row widths, rotated upright text, mixed-selection editing, undo/redo, reset preservation, independent 2/3.5-unit snapping, preferences cancellation/acceptance and invalid-setting fallback.
- Inspected the captured Appearance dialog layout. The offscreen environment renders text as placeholder squares; widget assertions verify the labels and values. Native desktop interaction was not checked in this pass.
- Markdown changes and whitespace checked with `git diff --check`.

The Properties justification control now uses exclusive Left/Centre/Right icon buttons with tooltips and accessible names. The same icons align note paragraphs within their fixed-width box.
