# Undo and redo design

Status: **approved; implementation completed.** Durable choices are recorded in [DECISIONS.md](../../DECISIONS.md); [ROADMAP.md](../../ROADMAP.md) tracks remaining work.

## 1. Purpose and scope

Add undo and redo for edits to the open schematic document. The feature includes edit grouping, dirty-state behavior, and restoring the user's selection where practical. It does not include undo for view-only changes, tool selection, pan/zoom, or symbol-library management.

The implementation should keep `schematiccore` independent of Qt Gui/Widgets. Undo-stack and UI integration belong in the application layer.

## 2. Existing structure before implementation

- `MainWindow` owned one `Document` instance and tracked modified state with `m_documentModified`.
- `SchematicScene` edited the bound document through its mutators, rebuilt its graphics items after a successful edit, and emitted `documentEdited()`.
- `MainWindow` also performed edits directly for grid spacing and component labels, then refreshed the scene where needed.
- New and Open replaced the contents of the existing `Document` object and rebound/rebuilt the scene. Save cleared the modified flag only after a successful write.
- `Document` is a Qt Core value type containing components, wires, nodes, and ID allocation state. Its mutators reject invalid changes without committing them.

This design moves those mutation paths through one application-layer commit path so scene interactions and MainWindow actions cannot bypass undo history.

## 3. Proposed user-visible behavior

- Provide Undo and Redo actions with standard shortcuts and enabled state derived from the undo stack.
- Each successful user edit creates one undo command. A rejected or no-op operation creates none.
- Undo and redo restore document contents, connectivity, grid spacing, and identity allocation state to the state before/after that command.
- Selection is restored to selected component IDs when those components still exist. Wire items are selectable, but the existing scene rebuild does not preserve their selection; restoring wire selection is outside the first implementation.
- Undo/redo does not change the current file path. Save and Save As continue to write the current document; only a successful save marks the current history position clean.
- New and successful Open ask the existing `maybeSave()` question when the current history position is dirty, then clear the old document's undo history. A failed/cancelled open leaves the current document and history unchanged.
- Closing a dirty document continues to use the existing unsaved-change prompt.

## 4. Command boundaries

One command corresponds to one user-intent edit, not one low-level model mutation.

| User action | Proposed command |
|---|---|
| Place one component | One command |
| Drag one component | One command on mouse release; preview movement is not committed |
| Delete selection | One command for all deleted selected components/wires |
| Rotate or mirror one component | One command per action |
| Edit reference/value labels | One command after a successful dialog commit |
| Change grid spacing | One command per committed control change |
| Draw a branched wire | One command for the new route and all route splits/nodes it creates |
| Delete a wire or branch a wire through a future direct action | One command for the full operation |

Changing tools, selection, zoom, pan, or an uncommitted wire preview does not create a command. Rejected edits and edits whose resulting document is identical to the original do not create a command.

## 5. Proposed architecture

### 5.1 Application-layer edit commit

Introduce a single application-layer edit coordinator, owned by `MainWindow`, that executes a requested operation and records its successful result. Scene input should request/submit edits through this path rather than silently mutating the document outside it. MainWindow actions, label editing, and grid changes use the same path.

The scene remains a renderer/input adapter; `Document` remains the source of truth. After a command applies, undo/redo restores document state and requests a scene rebuild while preserving the applicable component selection.

### 5.2 Command state representation (proposal)

For the first implementation, use before/after `Document` value snapshots in application-layer undo commands. This reuses the model's existing validated operations, naturally captures related topology changes and ID counters, and avoids adding `QUndoStack` to `schematiccore`.

The operation runs against the live document. On success, capture the after-state and push a command containing both snapshots and the before/after selection. Since pushing a `QUndoCommand` invokes its `redo()`, the command's redo restores the captured after-state; this is safe even though the operation already applied it. On undo, restore the before-state. In both cases, rebuild the scene and restore the saved selection IDs that still exist.

This is simple and robust for the current small-document use case, but copies the full document per command. Review memory/performance expectations before approval. If snapshots are rejected, choose explicit reversible commands per operation and specify how compound mutations and ID allocation are captured; do not mix both strategies ad hoc.

### 5.3 Document object lifetime

Keep the address of `MainWindow::m_document` stable because `SchematicScene` holds a pointer to it. Restoring a snapshot assigns its contents into that object; do not replace the object itself while the scene is bound. New/Open may assign a successfully-created or loaded document into the same object, then clear the undo stack and rebuild the scene.

## 6. Dirty state and file operations

Use the undo stack's clean index as the dirty-state source rather than maintaining a second independent modified flag.

- A new empty document starts with an empty, clean history.
- A successful Save or Save As sets the undo stack's clean index after the file write succeeds. A failed or cancelled save leaves the clean index unchanged.
- Undoing back to the saved history position makes the document clean; redoing away from it makes it dirty.
- If the user saves, then undoes and creates a different edit, the clean marker remains associated with the saved history entry; the new branch is dirty.
- Successful New/Open clears history and marks the new document clean. A failed load or a cancelled file dialog does not disturb the current history.
- Saving a document loaded from an older supported format still marks the current in-memory state clean after the upgraded format is successfully written.

Confirm that this clean-index behavior matches the desired user experience before implementation. If document state can change outside undoable operations, those changes must either be routed through the commit path or explicitly reset/mark the history clean.

## 7. Selection restoration

Capture selected component IDs immediately before and after each command. After undo or redo, rebuild the scene and reselect IDs that exist in the restored document. For a command that creates a component, redo should select it and undo should restore the prior selection. For deletion, undo should restore the deleted selection; redo should restore the selection that remained after deletion.

Selection is UI state, not document state. It must not affect serialization or dirty state. If a saved component ID no longer exists after a restore, skip it. Wire selection is not restored in the first implementation. Multi-selection behavior remains limited by the current editor capabilities.

## 8. Risks and constraints

- Whole-document snapshots consume memory proportional to document size times retained history. Establish whether a practical undo limit is needed; any limit must not remove the current clean marker without updating dirty-state behavior.
- A scene rebuild invalidates graphics-item pointers. Store selection by stable component ID only.
- All successful mutations must use the common commit path. Direct mutation can otherwise bypass history or make undo restore stale state.
- Compound operations such as `addWireBranching()` must remain atomic at the model level and produce one command at the UI level.
- Undo restoring ID counters permits IDs from undone additions to be reused by later edits. Confirm that IDs are only document-local identities and this reuse is acceptable.

## 9. Acceptance checks

After design approval, implementation is complete when:

- Focused tests cover undo/redo of component placement, movement, deletion, labels, grid spacing, transforms, and a branched wire operation, including topology and ID allocation state.
- Tests cover no command for rejected/no-op edits and dirty/clean transitions across save, undo, redo, and branching history.
- Tests cover New/Open history reset behavior at the application boundary where feasible; manual checks cover prompts and failed/cancelled file operations.
- Manual checks confirm selection restoration for component placement, movement, deletion, and undo/redo; a drag and branched wire each undo in one step.
- Existing model tests pass, and `schematiccore` remains Qt Core-only.

## 10. Settled choices

- Use before/after full-document snapshots for the first version. Revisit only if measured memory use becomes a problem.
- Include the operations in the command table, including grid-spacing changes and multi-component deletion.
- Treat each committed grid-spacing change as its own command; do not coalesce changes in the first version.
- Do not impose a history-size limit in the first version. Revisit if normal use shows material memory pressure.
- Restoring an undone snapshot restores its ID allocation counters. Reuse of IDs from undone edits is acceptable because IDs are document-local and only current state is serialized.
