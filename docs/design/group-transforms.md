# Group move, rotate, and flip plan

Status: **approved by the user on 2026-11-03; implemented.** Verification details are recorded below.

## Purpose and starting behavior

Extend [directional group selection](group-selection.md) with moving, rotating, and flipping the selected components and wire routes together. This is a temporary selection operation, not a persisted group feature.

Before this work, scene dragging moved one component and previewed only its artwork and labels. The model updated attached route endpoints transactionally for that one component. Rotate Left, Rotate Right, Flip Horizontal, and Flip Vertical acted on exactly one selected component. The undo controller stored document snapshots but restored only component selection.

The implemented scope includes mixed and wire-only selections, coherent wire previews, one atomic edit per gesture/action, and restoring component and wire selection through undo/redo. Persistent groups, arbitrary-angle rotation, scale, independent label placement, wire-corner editing, and automatic routing remain separate work.

## Interaction

- In Select mode, drag a selected component, its label, or a selected wire to move the current selection. Pressing an unselected target without modifiers selects that target before preparing its drag. An empty-canvas drag continues to select a box.
- Retain Ctrl-click selection toggling. A modifier selection click does not start a transform drag; release the modifier and drag the resulting selection to move it. This avoids toggling an item off while simultaneously moving it.
- Start movement only after the normal drag threshold. Clicking without dragging must not move or snap anything.
- For a drag begun on a component or its label, snap that component's origin to the document grid and apply the resulting single displacement to every moving object. Preserve all relative offsets, including off-grid offsets. A drag begun on a selected wire snaps the pointer displacement to grid increments, including in a mixed selection. Never snap each object independently.
- Enable the existing rotate/flip actions for any nonempty logical selection in Select mode, including wire-only selections. Keep Rename/F2 limited to exactly one component. Disable transform actions during an active drag.
- Rotate Left/Right applies a 90-degree counterclockwise/clockwise transform in canvas coordinates. Flip Horizontal exchanges left/right across a vertical axis; Flip Vertical exchanges top/bottom across a horizontal axis.
- Preserve the selection after successful transforms. Escape, tool changes, document replacement, and loss of focus cancel a move preview without modifying the document. Release commits the last previewed displacement.

## Pivot and geometry

For a selection containing exactly one component and no explicitly selected wires, preserve the current pivot at that component's origin. Otherwise use the center of the axis-aligned bounds of selected component artwork and selected wire centerlines in their original scene coordinates. Exclude labels, marker overlays, selection outlines, and any automatically affected wires from pivot calculation.

Compute the pivot once per action from the original selection. Rotation and flips do not snap the pivot or resulting positions: snapping individual results would distort the group and could move terminals inconsistently. The pivot may be off-grid. There is no custom-pivot UI in this scope.

Apply one common world-space transform to component origins and compose it with each component's existing quadrant rotation and local mirror state. Use exact quadrant coordinate operations rather than general-angle trigonometry. Local symbol artwork, terminal order/names/offsets, library IDs, and symbol sizes stay unchanged. Terminals resolve through the final component pose. Reference/value strings remain unchanged; labels stay upright and are repositioned using the existing component-label layout rules, rather than rotating their text or introducing label-offset persistence.

## Wire and junction rules

Electrical topology always comes from terminal references and shared node IDs. Coordinates never create, merge, split, or disconnect electrical connections during a transform. Node and wire IDs remain stable; net groups and junction dots are recomputed as today.

Determine the affected geometry from the original state, in this order:

1. Selected components receive the common transform.
2. The route set consists of explicitly selected wires plus any direct terminal-to-terminal wire whose two owning components are selected. This lets a pair of selected components carry its direct internal wiring even when the wire was not explicitly selected. Do not expand transitively through a whole net.
3. A shared node moves only if every incident wire belongs to that route set. Otherwise it is a boundary anchor and stays fixed. Nodes are derived transform participants, not new selectable objects. To carry a branched subcircuit rigidly, include its internal wire routes in the selection.
4. For every route in the route set, transform its original vertices with the common transform, then resolve both endpoints from the final terminal/node identities. If an endpoint is anchored outside the moving group, repair only that end of the transformed route.
5. Other routes retain their original interior vertices. If a terminal or node at either end moved, repair that end to its final identity-derived position. This includes connections leaving a moved component and wires attached to moved nodes.

A boundary connection stays attached to its original endpoint even when its selected wire's interior moves. Thus selecting only a wire can reshape or rotate its interior while leaving unselected terminal endpoints anchored. A straight wire between two fixed endpoints can produce a no-op or a rejected route; show a concise explanation for rejection. Nothing moves an unselected component implicitly.

Endpoint repair follows the existing deterministic orthogonal corner convention: when endpoint and adjacent vertex differ on both axes, insert `(endpoint.x, adjacent.y)` next to that endpoint. Remove adjacent duplicate vertices introduced by endpoint replacement, but do not add general simplification or collapse retraced corners. For routes with two changed ends, stage both final endpoints and both end repairs before validation. Do not validate after moving only the first end. Reject any final diagonal, zero-length, or backtracking route under the existing geometry validator.

Both-moved endpoints and a shared rigid transform must carry the route shape as a whole; they must not accidentally acquire extra corners from sequential single-component moves. If any final affected route is invalid, reject the entire selection transform without changing document contents, identities, selection, history, or dirty state. A boundary repair can change the group's visible bounds, so subsequent actions recompute their pivot; undo is the exact way to reverse a committed operation.

## Model, scene, and undo boundaries

- Add a Qt Core-only batch transform API to `Document`, taking component IDs, explicit wire IDs, and a restricted transform description (translation, quadrant rotation about a pivot, or canvas-axis reflection about a pivot). It must validate IDs and finite inputs, deduplicate targets, and reject an empty/invalid request without mutation. The scene never supplies graphics items as model inputs.
- Build all candidate component poses, node positions, and routes from the same original state. Validate the complete candidate, then commit once. Do not implement a group operation by looping over the existing public single-component mutators: those validate intermediate states and can partially apply a group edit. Keep existing single-component API behavior compatible.
- During dragging, derive each preview from the original snapshot plus the current displacement, using the same batch logic on a temporary document. Display component, label, node, junction, and affected-wire geometry from that candidate. The live document and undo history remain unchanged. Keep the last valid preview if a candidate is invalid, show a concise rejection cue, and reject release at an invalid pointer position rather than committing an earlier position silently.
- Store preview participants and selections by model identities. Cancel before a rebuild/rebind can invalidate graphics items. A zero-displacement or otherwise unchanged result creates no undo command.
- Route a successful move/rotate/flip through `DocumentEditController` as one document snapshot command. Extend its selection value to include both component and wire IDs, and update existing selection callback users/tests. Restore surviving IDs after command application, undo, redo, and scene rebuilds. This deliberately supersedes the initial wire-selection restoration deferral in [the undo design](undo-redo.md).
- Keep project format v3 unchanged. Selection, pivot, drag state, and temporary groups are not serialized. Saved components, nodes, and routes use their existing fields, and load validation remains in force.

## Implementation stages (completed)

1. **Atomic model transform:** implement candidate generation and batch validation, with focused model tests for internal, external, and branched wiring, transform composition, and rejection without mutation.
2. **Selection restoration and group movement:** extend undo selection state, preserve mixed selections, add component/label/wire drag entry points, snapping, candidate previews, cancellation, and one-command commits.
3. **Group rotation and flips:** connect existing toolbar actions to the batch API, implement pivot calculation and enablement, update action wording/tooltips, and verify mixed and wire-only selections.

These stages implement the approved scope. Settled behavior is recorded in `DECISIONS.md`; unresolved follow-up work remains in the roadmap.

## Acceptance checks

- Moving selected components uses one displacement, preserves spacing and off-grid offsets, and keeps a directly connected internal route rigid. Clicking without dragging does not snap anything.
- A fully selected branched subcircuit carries nodes and internal routes rigidly. A partially selected branch leaves its shared boundary node fixed and repairs affected ends without changing net membership. Crossings remain electrically separate.
- Explicitly selected wires, unselected direct internal wires, external connections, and wire-only selections follow the rules above. Reordering selected IDs produces identical output. Both-end updates are validated only in their final state.
- Rotate and flip tests cover already rotated/mirrored components, single-component origin pivots, group-center pivots, off-grid results, and upright labels. For isolated rigid selections, four quarter-turns and two identical flips restore the original geometry within existing coordinate tolerance.
- Invalid IDs, nonfinite input, degenerate/retracing routes, and any rejected member leave the entire document byte-for-byte unchanged and create no history entry. No-ops also create none.
- Previews match committed geometry, including attached wires and junctions; invalid candidates and cancellation never change the live document. Test Escape, focus/tool changes, undo/rebuild, and new/open during a preview.
- Every accepted gesture/action is one undo step. Undo/redo restores geometry, component and wire selection, connectivity, and clean/dirty state. Existing placement, deletion, label editing, and New/Open selection callbacks continue to work after the selection-value change.
- Save/load round trips preserve transformed geometry and terminal/node references in format v3. `schematiccore` continues to link Qt Core only.
- Native UI walkthrough covers component/label/wire drags, box selection, modifier clicks, pan/zoom, action enablement, pivot behavior, external connections, invalid previews, cancellation, and undo/redo. Report automated results and manual checks separately.

## Approved principal choices

The approved choices are the group-center pivot, snapping through the dragged component, automatic inclusion of direct internal wires, boundary-node anchoring, wire-only behavior, and rejection of invalid final routes. Mixed-selection restoration extends the undo interface while retaining the snapshot history strategy.

## Verification record (2026-11-03)

- Application and tests built with Qt 6.11.1 using MinGW 13.1 Debug and the local Visual Studio 2026 Release preset.
- Focused model tests cover atomic internal routing (including an intermediate single-component move that would fail), input order and duplicates, rigid rotations, shared-node movement and boundary anchoring, wire-only endpoint repair, rejection without partial mutation, mirrored quadrant composition, invalid requests, and format-v3 round trips.
- Scene/view tests cover actual viewport drags with repeated preview replacement, unchanged live documents during previews, mixed-selection undo/redo and clean state, geometry-based group pivots excluding labels, upright text, wire-only drags/flips, Ctrl-click selection, off-grid snapping, click and return-to-origin no-ops, invalid releases, Escape, focus/tool changes, rebuilds, and document replacement. Toolbar integration tests cover single and group rotation/flips, mixed selection restoration, and wire-only action enablement.
- Offscreen and native Windows selection/transform checks pass. Release test execution uses the Qt kit's platform-plugin directory explicitly: the deployed application folder contains the Windows plugin, while the offscreen test backend remains in the Qt kit. The existing MinGW Debug shutdown assertion noted in the roadmap remains separate from this feature.
- All seven CTest suites pass in MSVC Release. After the final return-to-origin no-op adjustment, the three scene/window suites were rebuilt and passed again; the unchanged model, undo, renderer, and package-store suites had already passed. The native Windows selection suite reports 13 passing Qt Test results, including initialization and cleanup.
- Visually inspected a native captured move preview showing the selected components, connecting route, and upright labels. The native interaction checks above are automated; a full manual application walkthrough remains outstanding.
