# Rails, grounds and signal ports

Status: **approved, implemented and verified on 2026-11-07.**

## Scope and connectivity

Add rails, ground variants and input/output/bidirectional ports through the ordinary symbol library. Every symbol has one explicit wire terminal. Names and direction are presentation metadata: two VCC, AGND or identically named ports do not merge separate nets. No hidden power connections, global nets, electrical-rule checks, or package model are added. Existing wire snapping, movement, rotation, mirroring, selection, undo and document topology remain unchanged.

## Categories and initial set

Use these categories, in order, beneath Signals:

- `Signals/Power Rails`: one visible **Power Rail**, default Value `Vcc`. Other names use the same symbol.
- `Signals/Grounds`: Earth, Chassis Ground, Signal Ground. Signal Ground starts unnamed and can be named through Value.
- `Signals/Ports`: Input, Output, Bidirectional.

Retain existing symbol ID `Ground`, prefix `GND`, hidden-reference default and terminal `1` at (0, -20). Use the compact three-bar artwork specified below. Rename its presentation to Earth and move it into Grounds. This does not change its terminal signature or existing embedded snapshots.

New symbols have terminal name `connection` at the positions below. All have hidden references by default. Rails use prefix `PWR`, grounds `GND`, and ports `PORT`; the instance still has a unique internal reference even when it is hidden.

## Geometry and immutable terminal layouts

Use 1-unit black strokes, short visible leads, white interiors for outlined ground/port bodies, and a solid black dot at the rail intersection. Dimensions are scene units, independent of grid spacing. No labels are baked into artwork.

| Entry / stable ID | Geometry and default pose | Terminal | Default Value |
|---|---|---|---|
| Power Rail / `Rail.VCC` | Horizontal bar with vertical lead and filled intersection dot | (0, 20) | `Vcc` |
| Earth / `Ground` | Three horizontal bars, widths 20/12/4 at y = 0/4/8; unchanged 20-unit shaft | (0, -20), named `1` | empty |
| Chassis Ground / `Ground.Chassis` | Horizontal bar with three angled strokes | (0, -20) | empty |
| Signal Ground / `Ground.Signal` | Downward white triangle | (0, -20) | empty |
| Input / `Port.Input` | Right-pointing outlined flag, signal leaves its right tip towards the schematic | (20, 0) | `IN` |
| Output / `Port.Output` | Right-pointing outlined flag, signal enters from the schematic at its left edge | (-20, 0) | `OUT` |
| Bidirectional / `Port.Bidirectional` | Double-pointed outlined flag, connection on the right | (20, 0) | `IO` |

- Power Rail: vertical lead from (0, 20) to (0, 0), horizontal bar from (-10, 0) to (10, 0), and a filled black circle of radius 2 at their intersection (0, 0). The dot is symbol artwork, not an additional terminal or document junction. Edit Value for other supply names, and use existing flips/rotations for orientation.
- Signal/named ground: shaft from (0, -20) to (0, 0); white triangle vertices (-10, 0), (10, 0), (0, 10).
- Chassis ground: shaft from (0, -20) to (0, 0), bar (-10, 0) to (10, 0), with angled strokes from (-10, 0), (0, 0), (10, 0) respectively to (-15, 7), (-5, 7), (5, 7).
- Input/output flag: white polygon (-10, -6), (4, -6), (10, 0), (4, 6), (-10, 6). Input lead connects (10, 0) to (20, 0); output lead connects (-20, 0) to (-10, 0). Both indicate left-to-right signal flow, with different exposed terminal sides.
- Bidirectional flag: white hexagon (-10, 0), (-4, -6), (4, -6), (10, 0), (4, 6), (-4, 6); lead connects (10, 0) to (20, 0).

## Editable names and library defaults

Add optional `defaultValue` to native symbol JSON and SVG sidecar metadata. It defaults to empty, accepts trimmed single-line text, and is omitted when empty. Reject non-string or multiline values. Preserve it on import/export, embedding and library updates through the same paths as other symbol metadata.

Only creation of a new component copies this default into its existing Value property. Value remains instance-owned, editable, independently visible and non-unique. Library updates, loading, changing a layout, and clipboard paste must not overwrite existing instance text. Paste preserves the copied Value; older embedded definitions without a default remain unchanged. The field is presentation metadata within the current v7 format; there is no new instance field or document-version bump.

Names initially use the existing upright property block below the artwork; users can drag that block to suit the drawing. This step does not add a new label placement system or print text inside port flags. Reference visibility is initialized from `showsReference: false`, with Value visible by default. Hiding a reference does not make it a net name.

## Implementation and checks

1. Add/validate/serialize `defaultValue` and copy it on placement. Preserve native/SVG metadata and test old defaults, explicit edits, load, paste and undo behavior.
2. Add the six new JSON definitions, retain and recategorize Earth, and update manifest/resources/category order and library metadata tests. The library grows from 34 to 40 definitions; hidden OTA layouts remain hidden.
3. Test immutable terminals, wire attachment, connected movement, transforms and save/load for every entry. Verify equal Value labels do not join separate nets, and that editing a name cannot change connectivity.
4. Update the style guide, decisions and README. Build the application, run the relevant existing suites and inspect rendered category palettes and names on the canvas. Record automated checks separately from any native manual interaction.

Long-term choices requiring separate approval include logical net naming/global rails, extra ground conventions, names inside flags, specialised label anchors, and direction/layout dropdowns for these new symbols.

## Verification record (2026-11-07)

- Built the Release application and all existing test targets with MSVC / Qt 6.11.1. All seven CTest suites passed using the offscreen backend.
- Library/model coverage checks all thirteen connection entries (the retained Earth plus twelve additions), immutable terminal coordinates, default values and reference visibility, wire attachment, connected movement, every quadrant/mirror pose, group transforms and save/load. Identical names remain disconnected until explicitly wired, and name edits preserve topology.
- Default-value coverage checks optional/empty defaults, trimming, malformed and multiline rejection, SVG sidecar preservation, creation-only initialization, explicit empty values, library updates, saved-instance values, clipboard paste and v6 loading. Placement tests include undo/redo for every new entry.
- Window checks verify category order and tile counts (5 rails, 5 grounds, 3 ports), default name editing, hidden references and undo without net changes. Visually inspected all three palette captures and the canvas arrangement. Offscreen text uses placeholder glyphs in this environment; label content is checked through metadata and widget assertions. No native interactive walkthrough is claimed.
- Checked documentation links and Markdown diff. The existing Earth artwork and terminal identity remain unchanged; project writers continue to use v7.

### Rail simplification (2026-11-07)

The user requested one generic bar-and-dot Power Rail instead of separate arrow rail choices. `Rail.VCC` keeps its terminal signature and now defaults to `Vcc`; four earlier rail entries are hidden, not reidentified or removed. The Release app built, and core, renderer and document-open suites passed. The one-tile rail palette was visually inspected. Tests cover the new lead/bar/dot geometry, hidden compatibility entries, editable defaults, connected transforms and save/load.

### Simplification and removal (2026-11-07)

Signal Ground is now one visible unnamed symbol, named through Value when needed. The user confirmed that no older drawings need support and authorized deleting unused symbols. The four redundant rail definitions and separate AGND/DGND definitions were removed from data, manifest, resources and ID constants. The current library has 40 definitions, including two hidden OTA layouts still used by the bias dropdown. Grounds now contains Earth, Chassis Ground and Signal Ground. Earlier verification records above describe the original larger set and the intermediate hidden-compatibility approach, superseded by this removal.

The removal follow-up built successfully and all seven offscreen Release suites passed. Core checks assert the six retired IDs are absent. Window checks verify a three-entry Grounds palette and that Signal Ground starts unnamed, accepts a Value name, and returns to unnamed on undo without changing nets. The reduced ground palette was visually inspected.

## Earth artwork refinement (9 November 2026)

Earth now uses three bars from (-10, 0) to (10, 0), (-6, 4) to (6, 4), and (-2, 8) to (2, 8), matching the 20-unit primary width of the newer grounds. The shaft stays from (0, -20) to (0, 0). This supersedes the original instruction to retain its larger artwork; ID, terminal count/name/position and metadata remain unchanged. Embedded symbol snapshots in existing documents retain their stored artwork until the current library definition is adopted.
