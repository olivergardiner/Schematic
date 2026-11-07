# Movable component property blocks

Status: **approved and implemented; verified 7 November 2026.**

## Purpose and current behavior

Keep each component's visible properties together in one movable canvas block so users can place its text clear of wires. Also default the Properties editor dock to the left, underneath the symbol browser.

The implemented [instance-properties design](instance-properties.md) stores ordered instance properties and visibility in format v5. `SchematicScene::addComponentItem()` currently draws separate upright text items: Reference above the artwork, Value and custom properties below, with a 4-unit gap between labels. Clicking or dragging a label currently acts on its component. There is no persisted text placement. The Properties dock defaults to the right; window layout is saved separately from documents.

This follow-up changes presentation and label interaction. Symbol artwork, terminal positions, explicit connectivity, wire routing, and property values remain independent of text placement. No automatic collision avoidance or leader lines are included.

## Approved behavior

- Draw one left-aligned, upright block per component: visible non-empty Reference first, then Value if visible and set, then visible non-empty custom properties in stored order. Preserve `Name: value` for custom rows. Visibility controls still apply independently to each row.
- Default the block just below the transformed artwork, aligned to its left edge with the existing 4-unit body clearance. Reference therefore joins the other properties below the symbol rather than remaining above it. Hidden or empty rows consume no space.
- Retain the current 8-unit font size. Use font-metric line spacing (at least the text bounds height) with **no additional inter-row gap**, replacing the current text-item height plus 4 units. Verify readability at ordinary editing zoom and with Unicode/descenders. Spacing is one shared renderer constant, not a per-instance setting in this stage.
- In Select mode, drag any text row to move the entire owning block. A click still selects its component and double-click still focuses Value in the editor. Symbol-body dragging continues to move the component and its attached wires. Block dragging affects only that block, even when other components are selected; it must never start a group component move.
- Use the normal drag threshold and preserve the pointer's grab offset. Placement is free in scene units, without grid snapping: the 10-unit electrical grid is too coarse for text clearance. Show a subtle block outline/move cursor on hover or while moving; do not add a permanent box or background.
- Provide **Reset property position** in the block's context menu and in the Properties editor for selected components. Reset restores automatic placement; resetting several components is one undo step. Disable it when no selected component has a custom position.
- Moving a block is allowed over wires and artwork. The user decides the best placement; no route or component moves to accommodate it.

## Model and transform contract

Add one optional `QPointF` property-block offset to `Component`, measured from the component origin along the **canvas axes** in world units. A missing offset means automatic layout; a present offset is the block's top-left anchor relative to that origin. It is instance layout data, not a property row or library definition. Validate finite coordinates in the model and loader.

The model does not measure fonts or derive artwork bounds. The scene's shared block layout helper computes the automatic anchor and text geometry from the document-resolved symbol and properties. A drag starts from that rendered anchor, previews a candidate, and commits the resulting relative offset through a model operation. Reset clears the offset.

The following behavior is intentional:

| Edit | Automatic placement | Custom placement |
|---|---|---|
| Component/group translation | Recomputed beside moved artwork | Same offset; block follows component |
| Component/group rotation or flip | Recomputed below final artwork | Preserve canvas-axis offset; text stays upright |
| Text/visibility/name change | Recompute rows and automatic anchor | Retain top-left anchor; grow/shrink down and right |
| Grid-spacing change | No layout scaling | No layout scaling or resnapping |
| Hide all rows, then show one | Automatic anchor when visible | Retain saved offset while block is absent |

Preserving the custom offset on rotation/flip avoids spinning manually positioned text around a component. A group transform still moves the component origins normally; block anchors follow those origins. This differs from geometrically rotating every label position as part of a rigid group and should be agreed before implementation.

No independent block selection is persisted. Box selection continues to treat artwork and the rendered property block as one component target, using their actual shapes rather than the empty rectangle between them. Group pivots continue to exclude labels. Labels never contribute terminal identities or topology.

## Scene interaction and undo

Introduce a dedicated block item (or one parent with row children) with one owner ID, one drag target, and one computed layout. Reuse that layout for drawing, hit testing, box selection, scene bounds, and component-move previews. It must remain upright without inheriting the component item's rotation/mirroring.

Block dragging is a separate transient scene gesture from component/wire movement and box selection. Invalidate pending inspector text before changing selection or beginning the gesture. Keep the document unchanged during preview; mouse release creates one edit-controller command for the offset. A click/no displacement creates no command and does not turn automatic placement into custom placement. Escape, focus loss, tool changes, New/Open, undo/redo, scene rebuilds, or component deletion cancel the preview. Prevent overlapping move gestures and inspector edits while it is active.

Preserve an existing multi-selection when dragging the block of one of its members. Clicking an unselected block follows ordinary component selection rules; Ctrl-click retains its current toggle behavior, while Ctrl-drag is not a bulk block-position command. Double-click and wire-tool hit behavior must remain consistent with the existing component/label paths.

Undo/redo restores automatic/custom placement, selection, and clean state through existing document snapshots. Rejected/non-finite changes and no-ops add no history. A property-only move must leave all component poses, terminal coordinates, nodes, wire routes, and property contents byte-for-byte unchanged.

## Persistence and compatibility

Write document **format v6**; continue reading v1–v6. Add optional per-component `propertyBlockOffset: { "x": 24, "y": -16 }` in v6. Absence means automatic placement; require both finite numeric coordinates when present. V1–v5 load with automatic placement and cannot supply this new field. All existing property and embedded-symbol validation remains in place. Older builds reject v6 instead of silently losing manual text placement.

Legacy documents adopt the new single-block layout on opening. They retain their property values, visibility, artwork, and connectivity, but their labels visibly move from the former split layout. Preserving that split appearance would conflict with the requested single-block model and is outside this proposal. Saving upgrades the format as usual.

## Properties dock default

Create the Properties dock in the left dock area and vertically split it below `symbolDock` before restoring the saved window layout. Give the symbol browser more initial height, while keeping a useful minimum inspector height. Keep existing dock object names and layout-state version so saved user arrangements, including a deliberate right-hand dock, continue to restore. The new default applies when there is no usable saved layout; it does not forcibly rearrange an existing workspace. Users can drag their existing Properties dock beneath Symbols.

Check that the three-column editor fits at the shared left-column width and that the symbol grid still wraps correctly. Both docks remain movable, floatable, closable, and available from View. No document-format field is needed for application dock placement.

## Implementation steps and acceptance

1. Add optional placement state, finite-coordinate validation, atomic set/reset APIs, and v6 serialization/migration. Test round trips, invalid input rejection, automatic/custom reset, and legacy defaults.
2. Replace split labels with a shared compact block renderer. Test row order, visibility, empty rows, Unicode, tight spacing, upright transforms, custom anchors, actual-shape selection, and label-free group pivots.
3. Add block-only dragging, previews, cancellation, reset, and snapshot undo/redo. Test zoom-independent grab offsets, drag threshold/no-op handling, multi-selection preservation, inspector context cancellation, and unchanged electrical geometry and routes.
4. Set the left/below default dock layout and verify fresh settings, saved right-hand layouts, hidden/reopened docks, and narrow-window sizing. Update README, DECISIONS, and the earlier properties design after verification.

Run the affected model/persistence, undo, scene/selection, and window tests and a Release build. Inspect native Windows captures of a compact resistor block, several custom rows, a rotated op-amp, and a manually placed block alongside wires. Exercise drag/cancel/reset, save/reopen, and undo/redo; distinguish automated results from manual visual checks.

Approval covers the unified default below the artwork, free block dragging, canvas-axis offsets preserved through rotation/flip, compact spacing, legacy relayout, v6 persistence, and the left/below dock default with saved-layout preservation. Implementation is complete.

## Verification record

Release build passed with MSVC and Qt 6.11.1. All seven CTest suites passed across the verification runs; after the final block outline/interaction changes, the affected placement, window, and selection suites passed again. Core coverage includes finite-offset validation, atomic missing-target rejection, v6 round trips, v5 migration, reset/no-op behavior, and offset preservation through transforms. Scene tests exercise free placement at 2× zoom, live preview without document mutation, multi-selection preservation, unchanged component poses and wire JSON, undo/redo, reset, Escape cancellation, and displaced-text selection. Window tests exercise bulk reset, the fresh left/below default, saved right-hand layouts, dock visibility, and inspector context handling.

Native Windows runs of block dragging/layout and dock/reset tests passed. Native captures were visually inspected for compact Reference/Value rows clear of a connecting wire, custom rows, a rotated op-amp, and the left dock layout. This was automated gesture testing plus capture inspection, not a full manual application walkthrough.
