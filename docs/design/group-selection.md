# Directional box selection plan

Status: **approved by the user on 2026-11-03; implemented.** Automated selection checks pass offscreen and with the native Windows backend. Both rectangle styles have been visually inspected in captured renders; a full interactive application walkthrough remains outstanding.

## Scope and starting behavior

At the start of this work, `SchematicView` enabled Qt's `RubberBandDrag`, without distinguishing enclosure and crossing gestures. The implementation replaces that with directional box selection in Select mode. Components and wire routes are selectable scene items; reference and value labels are separate items associated with their owning component. Group movement, rotation, flips, and mixed-selection restoration have since been implemented through the approved [group-transform design](group-transforms.md).

This plan covers selecting multiple objects. Group transforms and changes to undo/redo selection restoration were implemented as the separate follow-up linked above. Persistent groups remain later work.

## Approved interaction

- Begin a box with a left-button drag on empty canvas in Select mode, using the normal drag threshold so a click remains a click.
- Drag left to right to select only objects completely enclosed by the rectangle. Show a solid blue outline with a light translucent fill.
- Drag right to left to select objects touched by the rectangle. Show a dashed green outline with a light translucent fill.
- Determine direction from the current horizontal pointer position relative to the press position. Recompute the mode and selection if the pointer crosses the starting x coordinate. At equal x, use enclosure mode.
- Update selection live and retain the final selection on release. An unmodified box replaces selection; Ctrl held at the start adds matches to the selection captured at the start. Recompute from that captured selection on every move so objects cease to match when the box shrinks.
- Escape cancels an active box and restores the starting selection. Tool changes cancel an active box before entering the new tool.
- Preserve existing click selection, component dragging, middle-button panning, and Ctrl+wheel zoom. Box selection must not start in Place Component or Draw Wire mode.

## Selection geometry and object identity

A component, including its reference and value labels, is one logical selection target identified by its `ComponentId`. Enclosure requires the symbol and both displayed labels to fit inside the box; crossing matches when any of those parts intersects it. Empty space between separate parts should not count as touching the component. Use the existing scene shapes for the symbol and labels, transformed to scene coordinates, rather than introducing different symbol geometry.

Each complete wire route is one target identified by its `WireId`. Use its existing selectable stroked path for containment and intersection rather than its rectangular bounds, so a box in the empty interior of a bent route does not select it. Boundary contact counts as touching in crossing mode. Enclosure includes objects on the rectangle boundary.

Grid lines, terminal markers, junction dots, previews, and the selection rectangle are not independent selection targets. Selecting a wire does not automatically select its endpoints or other wires on its net; selecting a component does not automatically select attached wires.

These hit rules, component/label ownership, whole-route selection, and modifier behavior are the principal approved interaction choices.

## Architecture and implementation steps

1. Add scene-side helpers to capture and restore selected component and wire identities and evaluate the rectangle against logical targets. Apply component selection to its existing `ComponentItem`, including matches through its labels, and wire selection to its `WireItem`.
2. Add view-side gesture tracking and a viewport overlay for the directional rectangle. Convert viewport coordinates to scene coordinates for hit testing. Coordinate with scene edit mode and event handling so native rubber-band selection cannot compete with the custom gesture and ordinary item clicks and drags retain their behavior.
3. Add focused checks using the existing Qt Test conventions and perform the visual walkthrough below. After implementation is verified, update the roadmap and record settled behavior in `DECISIONS.md`.

Selection and the overlay remain transient scene/view state. They create no edit command and do not dirty the document. The document model, file format, symbol artwork and world-unit geometry, terminal positions, explicit connectivity graph, and orthogonal wire routes remain unchanged. `schematiccore` gains no Gui or Widgets dependency. Wire-selection restoration was outside this selection-only scope and is now implemented by the group-transform follow-up.

## Acceptance checks

- For the same rectangle, enclosure excludes a partially covered component or wire and crossing includes it. Fully enclosed objects match both directions.
- Labels participate in their owning component's selection; touching only a label selects that component in crossing mode, while leaving a label outside prevents enclosure selection.
- A box between separate parts of a component or inside a bent wire's empty bounding area does not produce a false crossing match. Rotated and mirrored symbols use their displayed geometry.
- A crossing selects only the touched routes, regardless of coincident crossings, shared nodes, or net membership. Junction dots remain derived display items.
- Reversing direction and shrinking the box update live selection correctly. Ctrl adds to the initial selection; plain drag replaces it; Escape restores it.
- Selection works at different zoom levels and grid spacings without snapping the box or changing geometry. Click selection, single-component movement, deletion of a mixed selection, pan, zoom, and other tools continue to work.
- Selection alone leaves document content, dirty state, and undo history unchanged.
- Manually inspect both outline styles, translucent fills, live highlighting, cancellation, and tool interactions. Report automated checks and manual checks separately.

## Verification record (2026-11-03)

- Built the application and test targets in Debug using Qt 6.11.1 and MinGW 13.1 in `build-selection-check`. The old MSVC build cache refers to a previous checkout location and an unavailable Visual Studio installation, so it was not reused.
- Added `schematicselection_tests`: component/label geometry and transforms, stroked wire geometry and distinct crossing identities, live direction changes and shrinking, Ctrl-add, Escape, tool changes, document rebinding, three zoom levels, unchanged document/history, attached-wire movement, mixed deletion, viewport mouse events, middle-button pan, and Ctrl+wheel zoom. All 10 Qt Test results (including initialization and cleanup) pass with the native Windows backend; the suite also passes offscreen.
- Wire hit shapes now explicitly use only the stroked route. Qt's default path-item shape also includes the implicitly closed interior of an open route, which failed the bent-wire empty-space regression check.
- Full CTest run: six of seven suites pass. `schematicopen_tests` aborts on a `MainWindow` destructor-time type assertion in `transformToolbarActionsAreUndoable`. The same failure was reproduced by building the unchanged HEAD revision separately with the same Debug toolchain. This existing issue remains unresolved.
- Visually inspected captured enclosure and crossing rectangles, including their fills and selected-component outlines. Native interaction checks are automated; the full manual application walkthrough remains outstanding.
