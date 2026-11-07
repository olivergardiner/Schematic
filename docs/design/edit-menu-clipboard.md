# Edit menu groups, shortcuts, and schematic clipboard

Status: **approved and implemented; verified 7 November 2026.**

## Starting point and scope

The Edit menu currently contains Undo, Redo, and Rename. Rotate/flip actions exist only on the toolbar; Delete is handled by the scene keyboard handler. There is no schematic Cut/Copy/Paste implementation. Share existing actions between menus and toolbars and add an undoable schematic clipboard through the same model/edit-controller boundary.

## Menu and shortcuts

Use separators between these groups rather than nested submenus:

| Group | Action | Shortcut |
|---|---|---|
| History | Undo / Redo | Platform standard Ctrl+Z / Ctrl+Y on Windows |
| Clipboard | Cut / Copy / Paste / Delete | Ctrl+X / Ctrl+C / Ctrl+V / Delete |
| Rotation | Rotate Left / Rotate Right | Shift+R / R |
| Flips | Flip Horizontal / Flip Vertical | H / V |
| Properties | Rename | F2 |

Use Qt standard key sequences for clipboard/history/delete. Retain Backspace as a canvas deletion alias. Transform shortcuts operate in Select mode with a valid selection and no active move gesture. Reuse the toolbar QActions so enabled state, shortcuts, descriptions, and operation semantics cannot diverge. Tooltips should expose the shortcuts.

Text editors retain normal text Cut/Copy/Paste/Delete/Undo and literal R/H/V input; canvas actions must not intercept them. Menu clipboard/delete actions route to the focused text editor when appropriate, including property fields and the grid control's line editor. Otherwise they target the schematic selection. Disable unavailable actions and refresh state on selection, tool/gesture, focus, and clipboard changes.

## Clipboard representation and connectivity

Use a versioned application MIME payload in the system clipboard, separate from the whole-document JSON format. Serialize a self-contained fragment with selected components, explicitly selected wire routes, required node identities, and the document-resolved definitions of used symbols. Preserve properties, visibility, custom block offsets, and component transforms. Copy does not modify the document or its undo history. Do not implicitly copy unselected wires.

When a selected wire endpoint refers to an unselected component, replace that endpoint in the fragment with a detached explicit node at its resolved position. Repeated references to the same external terminal use the same fragment node. Clone shared nodes by identity, never coordinate equality. Thus copied branches remain joined while crossings and unrelated coincident endpoints remain separate. A wire-only selection is valid. Selected components without their wires are also valid. Fragment endpoints must never refer back to the source document after paste.

Keep fragment extraction, validation, and insertion in Qt Core-only model helpers. Clipboard access, MIME handling, and actions belong in the application layer. Validate the complete payload before editing; reject malformed/future payloads with a useful message. Do not accept arbitrary text as schematic data.

## Cut and paste behavior

- Cut publishes a valid fragment first, then deletes the original selection using the existing deletion rules in one undo command. As today, removing components also removes incident wires that cannot resolve afterward. Copy includes only the explicitly selected wires; this consequence of Cut must be documented. Clipboard publication failure must leave the document unchanged.
- Paste allocates fresh component, wire, and node IDs and remaps every endpoint. Generate new unique references using the existing prefix policy; preserve all other property contents and visibility. Never retain a pasted duplicate reference.
- Keep each pasted component's document-resolved symbol definition. Reuse compatible identical definitions; reject a same-ID/different-definition conflict atomically with a clear message in this first version. Do not overwrite the library or existing document definitions during paste.
- Use a deterministic placement offset of one current grid step right and down from the copied fragment for the first paste, increasing by one step for repeated pastes of that payload. Apply one shared displacement without resnapping individual terminals, nodes, or vertices. This preserves off-grid geometry and orthogonal routes. Reset the cascade when the clipboard payload or destination document changes. Cursor-position paste is deferred.
- Validate all resulting references, definitions, identities, and routes before committing. A rejected paste leaves the document and history unchanged. A successful paste is one undo step and selects the new components/wires. Undo restores the previous selection; redo restores the pasted selection. Clipboard contents are not part of undo history.

The normal document format remains v6; clipboard fragments have their own version. No library updates, new symbol interfaces, automatic routing, or text parsing are included.

## Implementation and verification

1. Add focused model fragment extraction/insertion helpers and tests for fresh IDs/references, terminal remapping, branches, crossings, boundary detachment, wire-only selections, property/block preservation, symbol conflicts, and atomic rejection.
2. Add clipboard actions and the single-command Cut/Paste paths. Test copy without dirtying the document, Cut deletion consequences, repeated offsets, paste selection, undo/redo, and clean state.
3. Arrange menu groups and share rotate/flip/delete actions. Test shortcuts on the canvas, text editing without document mutations, focus-sensitive enabled state, malformed clipboard data, and gesture/tool restrictions.
4. Build Release, run affected core/undo/window/selection suites, inspect the native menu, and update README/DECISIONS/ROADMAP with verified behavior.

Approval should settle the boundary-wire detachment rule, Cut's existing incident-wire deletion behavior, unique pasted references, explicit wire selection, symbol-conflict rejection, and deterministic repeated-paste offset before implementation.

## Verification record

Implemented and verified on 7 November 2026. Release application build passed with MSVC and Qt 6.11.1. All seven CTest suites passed across the verification runs; after the final menu-focus change, the affected window suite passed again. Core tests cover fragment extraction without mutation, branch sharing, coincident but unrelated endpoints, detached boundary terminals, wire-only paste, fresh identities/references, properties/block offsets, malformed/future payload rejection, and atomic symbol-conflict rejection. Window tests cover standard clipboard shortcuts, repeated-paste offsets, selection and undo/redo, Cut incident-wire deletion, rotate/flip shortcuts, Delete, malformed clipboard rejection, tool restrictions, and text-menu isolation with an existing document undo command.

The native Windows menu/clipboard tests passed with normal desktop access, including opening Edit while property text is unfinished. Restricted execution initially caused Windows clipboard access-denied errors; the native checks were rerun with desktop access. The native menu capture was inspected for groups, shortcuts, and enabled states. These were automated interaction checks and capture inspection, not a full manual application walkthrough. Tests preserve and restore clipboard data where available.
