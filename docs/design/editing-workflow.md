# Editing workflow design

Status: **approved design; stages 1 and 2 implemented.** Remaining implementation stages need their own scoped plan and approval (see [AGENTS.md](../../AGENTS.md)). Durable interaction choices are recorded in [DECISIONS.md](../../DECISIONS.md); [ROADMAP.md](../../ROADMAP.md) tracks the implementation work.

## 1. Purpose and scope

Make common placement and editing actions easier to find and use while preserving the editor's document and connectivity rules.

In scope:

- Extract symbol artwork generation from `SchematicScene` into a shared application-layer renderer.
- Replace the text-only symbol dock with a grouped, graphical palette driven by symbol-library category paths.
- Drag a palette symbol onto the canvas to place one snapped component.
- Refocus the toolbar on frequent editing actions, including rotation and horizontal mirroring.
- Return to Select mode with Escape and show connection-point feedback while drawing wires.

Out of scope: changes to the document format, connectivity rules, symbol definitions, multi-component transforms, a symbol search/filter UI, and user-authored symbols. The library remains the source of symbol IDs, artwork, terminals, reference metadata, and categories.

## 2. Current structure

- `LibrarySymbol` provides an ID, display name, category path, primitives, terminals, and label metadata. The built-in manifest fixes symbol order.
- `SchematicScene::addComponentItems()` looks up the library entry and converts its primitives to `QPainterPath` items. It applies mirroring and quadrant rotation to the artwork group; labels remain separate and readable.
- `MainWindow::createSymbolDock()` currently makes one text-only button per symbol. Clicking a button enters repeated PlaceComponent mode.
- `SchematicView` owns zoom, grid rendering, wheel zoom, and canvas event forwarding. `SchematicScene` owns canvas selection and edit-mode interactions.
- `DocumentEditController` provides the application-layer commit path and undo stack. Palette drops and toolbar edits must use it.

## 3. Guiding principles

1. **One artwork definition.** Canvas symbols and palette previews use the same primitive-to-path renderer, pen/fill rules, and symbol-local coordinates.
2. **Preview scaling is presentation only.** Palette thumbnails fit artwork into a fixed preview box. They do not alter component geometry, terminal offsets, document coordinates, or grid spacing.
3. **Symbol identity stays stable.** Palette entries carry the library's `SymbolId`; labels and category names are for display only. A palette operation never infers a symbol from its text or icon.
4. **The model remains authoritative.** Placement, rotation, and mirroring call `Document` mutators through `DocumentEditController`. Graphics items remain a view of the document.
5. **Feedback does not define connectivity.** Highlighting a terminal communicates a possible wire endpoint; actual connection still follows explicit terminal/node identities and the existing route validator.

## 4. Shared symbol renderer

Add an application-layer renderer that accepts a `LibrarySymbol` and produces the artwork paths and style information used by both the canvas and palette. It should not depend on `Document`, `QGraphicsScene`, or edit state.

- Preserve current scene appearance: black outlines, white fill for the existing closed polygon primitives, and the current line width policy.
- Keep output paths in the symbol's local world units. The canvas positions, mirrors, and rotates them as it does today.
- The palette paints the same paths into a fixed preview rectangle, scaled uniformly to fit the primitive bounds with a small margin. Center the result; do not include labels or terminal markers in the thumbnail.
- Handle empty/degenerate bounds defensively, even though library validation currently requires drawable primitives.

The concrete renderer API (for example, returning paths versus painting directly to a `QPainter`) can be settled during implementation, but it must let the canvas and thumbnail share path construction and style decisions rather than duplicate primitive switches.

## 5. Grouped graphical palette

### 5.1 Organization

Use a two-pane dock: a compact category tree on the left and a graphical symbol grid on the right. Build the tree from `libraryCategoryPath(symbol.category)`: each path segment is a category node. A symbol with category `Passives` belongs directly to that group; `Semiconductors/Diodes` belongs to the nested Diodes category.

Selecting a category filters the symbol grid. Selecting a parent includes its descendants, displayed in groups headed by their immediate child categories so the user can distinguish Diodes from Transistors while browsing Semiconductors. A leaf category displays its symbols without an extra heading. Symbols appear once in any selected view.

Use category strings as presentation labels, preserving library spelling and order. Populate category groups and symbols in first-seen/library order. Do not hard-code top-level category lists; new library entries should appear without palette-specific code changes.

### 5.2 Symbol tiles and interaction

- Each tile shows a shared-renderer thumbnail in one standard fixed-size box, with the symbol display name beside or below it. Preserve artwork aspect ratio and center it inside the box; do not vary tile dimensions by symbol shape.
- Keep the display name as the accessible name and provide a tooltip. Category rows are accessible grouping labels.
- A normal click on a symbol retains the current repeated PlaceComponent mode for users who prefer click-then-place.
- Dragging a symbol leaf starts a Qt drag carrying a versioned MIME value containing only its stable symbol ID. Use the thumbnail as the drag pixmap.
- Dropping onto the canvas adds exactly one instance at the snapped drop position. The palette does not change document state until a valid canvas drop is committed.
- A drag outside the canvas or an unknown ID is ignored with no edit and no undo command.

### 5.3 Category and thumbnail sizing

Use a consistent thumbnail box and tile size so the grid scans cleanly. Scale each symbol independently into the preview box while preserving its aspect ratio. Long names remain visible through tooltips and accessible names; do not enlarge tiles unpredictably for one symbol. The category tree and symbol grid should be separated by an adjustable splitter; the symbol grid scrolls when the dock is short or narrow.

## 6. Canvas drop and placement

Enable drops on `SchematicView` (or a small canvas drop adapter owned by it). On drag-enter, accept only the palette's symbol-ID MIME type and only when the ID resolves in the built-in library. On drop:

1. Convert the event position to scene/world coordinates.
2. Snap the component origin using the open document's exact grid spacing, using the same rounding rule as click placement.
3. Call `Document::addComponent()` through `DocumentEditController` and retain the generated component ID.
4. Commit one undo command named for the placed symbol, rebuild from the document, and select the new component.

The drop uses symbol origin as the snap point, matching the existing PlaceComponent click behavior. Terminals may remain off-grid according to the world-coordinate contract. A rejected placement makes no document change and shows no phantom preview after the drag ends.

Click placement and drag placement should share a single application helper so default references, selection behavior, commit description, and undo semantics cannot drift.

## 7. Frequent-action toolbar

Refocus the primary toolbar on document editing:

- Select and Draw Wire modes.
- Undo and Redo, enabled from the undo stack.
- Rotate 90 degrees and horizontal mirror, enabled only when exactly one component is selected.
- Keep file actions as appropriate, but remove Zoom In/Out/Fit and grid spacing from the primary toolbar. Zoom remains in the View menu and Ctrl+mouse-wheel remains the main zoom interaction; put grid spacing in a secondary View control.

Use recognizable in-code icons, tooltips, and accessible names. Keep visible text for actions whose icons may be unclear. Rotation and mirroring each call the existing transactional `Document::rotateComponent()` and `Document::setComponentMirrored()` through `DocumentEditController`; each action is one undo command. If the model rejects a transform because an attached route would become invalid, leave the document and selection unchanged and show a concise status message.

Toolbar mode actions and palette click-to-place remain mutually exclusive. A palette drag is a one-off operation and does not change the active tool.

## 8. Escape and wire-drawing feedback

### 8.1 Escape behavior

Agreed behavior: Escape cancels the current wire draft (if any) and returns to Select mode in one press. It does not undo a committed edit. Keep the toolbar's checked mode synchronized when Escape changes modes. This replaces the current behavior where Escape only discards the wire preview and leaves Draw Wire selected.

### 8.2 Connection-point feedback

While Draw Wire mode is active, render a small, zoom-stable marker at each component terminal so available connection points are easy to see. Use an overlay or scene items that are not selectable and are not part of the document. While a wire draft is active, distinguish the current candidate under the pointer from other terminals, explicit nodes, and branch sites on wire segments.

The markers and hover cue are visual hints only. They do not reserve terminals, promise that every route to them will validate, or change endpoint identity. The hover cue identifies the candidate node or point on a wire segment under the pointer; wire segments do not receive persistent markers because every point along them can be a branch site. The existing status messages continue to explain a rejected completion.

## 9. Undo, selection, and state boundaries

- One palette drop creates one component-placement command and selects the placed component. Undo removes it and restores the prior component selection; redo recreates it and selects it.
- A click in repeated PlaceComponent mode continues to create one command per placed component.
- Each rotation or mirror action is one command. Rejected transforms create none.
- Palette expansion, category selection, hover, drag preview, tool changes, and wire-terminal highlights are view state and never dirty the document.
- Keep all edits on the established `DocumentEditController` path. `schematiccore` remains Qt Core-only; renderer, widgets, drag/drop handling, and toolbar integration stay in the application layer.

## 10. Proposed implementation stages

Each substantial stage should have its own short implementation plan and approval, following [AGENTS.md](../../AGENTS.md).

1. **Shared renderer and palette — implemented.** Extract the renderer, build the category tree and graphical symbol leaves, and preserve click-to-place behavior.
2. **Drag-to-place — implemented.** Add the stable-ID drag payload and canvas drop path through the edit controller. Verify snapping, selection, and undo/redo.
3. **Toolbar and interaction feedback.** Add transform actions, move zoom/grid controls to secondary UI, synchronize Escape-to-Select, and highlight wire terminals.

## 11. Acceptance checks

- Canvas symbol artwork is visually unchanged after renderer extraction, and palette thumbnails use the same primitives and fill/outline behavior.
- Every library symbol appears exactly once under its category path, in library order; adding a valid library entry requires no palette-specific code change.
- A drag drop creates one snapped, selected, undoable component. Undo and redo each restore the expected document and selection; drops outside the canvas do nothing.
- Existing click-to-place continues to work and creates one undo command per component.
- Rotation/mirror toolbar actions are enabled only for one selected component, preserve route validation, and undo/redo correctly.
- Escape cancels a pending wire and returns to Select; wire-terminal markers appear only in Draw Wire mode, track the document after edits/undo, and do not affect hit testing or connectivity.
- View-only actions do not change dirty state. `schematiccore` continues to link Qt Core only and all existing headless model tests pass.
- Manual walkthrough covers palette navigation, click placement, drag placement, undo/redo, transforms, wire feedback, rejected placement, and Escape behavior.
