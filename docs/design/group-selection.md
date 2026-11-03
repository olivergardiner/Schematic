# Directional box selection plan

Status: **proposed; awaiting approval before implementation.**

## Scope and current behavior

Add directional box selection in Select mode. `SchematicView` currently enables Qt's `RubberBandDrag`, without distinguishing enclosure and crossing gestures. Components and wire routes are selectable scene items; reference and value labels are separate items associated with their owning component. Component dragging currently moves one component.

This plan covers selecting multiple objects. Moving multiple selected components together, persistent groups, group transforms, and changes to undo/redo selection restoration are separate work.

## Proposed interaction

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

These hit rules, component/label ownership, whole-route selection, and modifier behavior are the principal interaction choices to review before implementation.

## Architecture and implementation steps

1. Add scene-side helpers to capture and restore selected component and wire identities and evaluate the rectangle against logical targets. Apply component selection to its existing `ComponentItem`, including matches through its labels, and wire selection to its `WireItem`.
2. Add view-side gesture tracking and a viewport overlay for the directional rectangle. Convert viewport coordinates to scene coordinates for hit testing. Coordinate with scene edit mode and event handling so native rubber-band selection cannot compete with the custom gesture and ordinary item clicks and drags retain their behavior.
3. Add focused checks using the existing Qt Test conventions and perform the visual walkthrough below. After implementation is verified, update the roadmap and record settled behavior in `DECISIONS.md`.

Selection and the overlay remain transient scene/view state. They create no edit command and do not dirty the document. The document model, file format, symbol artwork and world-unit geometry, terminal positions, explicit connectivity graph, and orthogonal wire routes remain unchanged. `schematiccore` gains no Gui or Widgets dependency. Existing limitations in restoring wire selection after a scene rebuild remain outside this scope.

## Acceptance checks

- For the same rectangle, enclosure excludes a partially covered component or wire and crossing includes it. Fully enclosed objects match both directions.
- Labels participate in their owning component's selection; touching only a label selects that component in crossing mode, while leaving a label outside prevents enclosure selection.
- A box between separate parts of a component or inside a bent wire's empty bounding area does not produce a false crossing match. Rotated and mirrored symbols use their displayed geometry.
- A crossing selects only the touched routes, regardless of coincident crossings, shared nodes, or net membership. Junction dots remain derived display items.
- Reversing direction and shrinking the box update live selection correctly. Ctrl adds to the initial selection; plain drag replaces it; Escape restores it.
- Selection works at different zoom levels and grid spacings without snapping the box or changing geometry. Click selection, single-component movement, deletion of a mixed selection, pan, zoom, and other tools continue to work.
- Selection alone leaves document content, dirty state, and undo history unchanged.
- Manually inspect both outline styles, translucent fills, live highlighting, cancellation, and tool interactions. Report automated checks and manual checks separately.
