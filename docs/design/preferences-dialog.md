# Preferences dialog

Status: **approved and implemented; verified 7 November 2026.**

## Scope and current behavior

Add **Edit > Preferences…** as the entry point for application preferences. The first version provides the dialog and tab structure only, with one empty **General** tab. No actual preference controls or settings changes are included.

`MainWindow` currently constructs the Edit menu and its shared actions. Existing dialogs use Qt Widgets layouts and `QDialogButtonBox`; application window layout already uses `QSettings`. Preferences belong in the application UI, independent of the schematic document and its Qt Core-only model.

## Menu and dialog behavior

- Place **Preferences…** at the end of Edit, after Rename and a separator, with **Ctrl+,** as its shortcut. Keep it available with an empty document and in any editing mode. Existing menu groups and shortcuts remain intact.
- Implement a small `PreferencesDialog` in `preferencesdialog.h/.cpp`, parented to the main window, titled **Preferences**. Open it modally through the normal `QDialog::exec()` path so repeated invocation cannot create competing dialogs.
- Use a `QVBoxLayout` containing a `QTabWidget` and a standard button box. Add one page titled **General**, selected on every opening. Leave its content genuinely empty, with a layout ready for future controls; do not add placeholder settings or explanatory text.
- Provide a single **Close** button. Escape and the window close control also dismiss the dialog. OK, Cancel, Apply, and Restore Defaults are unnecessary while there are no editable preferences.
- Start at a modest size, approximately 480 × 320 logical pixels, using normal Qt font scaling and a resizable layout. Use standard keyboard tab navigation and accessible tab/button labels. Do not persist dialog geometry or the selected tab in this stage.

Opening a modal dialog uses the existing focus-loss cancellation paths for transient canvas movement and box selection. Preserve the chosen editing tool. Existing pending text edits retain their normal validation and focus-loss behavior; the Preferences action must not introduce a second property-edit path. The dialog itself neither edits the schematic nor creates undo commands, changes selection, or modifies the document's clean state.

## Settings boundary and future growth

Preferences are application-wide settings, separate from per-document properties such as the current grid spacing. Future approved preference controls can be grouped into additional tabs, with General remaining first. Use the existing application `QSettings` namespace when persistent controls are introduced; do not add an empty settings schema, singleton, or preferences service now.

Define validation, defaults, persistence keys, and when changes take effect with the first real preference. That future design should also decide whether OK/Cancel/Apply replace Close. This scaffold does not imply immediate application or automatic saving of future controls. There is no document-format change.

## Implementation and verification

1. Add the standalone Qt Widgets dialog with its empty General page and Close button. Include its files in the application and existing window-test target as needed.
2. Add a stable `preferencesAction` at the end of Edit, connect it to the dialog, and assign Ctrl+,. Keep preference logic out of `schematiccore`.
3. Build Release and check that the menu action and shortcut open the modal dialog, General is selected and empty, resizing works, and Close/Escape/window close allow reopening. Check that invoking it from a text field does not invoke canvas shortcuts and that opening/closing from a settled document leaves content, selection, undo history, and clean state unchanged. Use a focused existing window check if useful; no new test framework is needed.
4. Inspect the native dialog and menu layout. Update README, ROADMAP, and this design's status after verification; record durable settings decisions only when real preferences are added.

Approval covers the empty General tab, modal standalone dialog, Close-only button row, Ctrl+, shortcut, and application-wide settings boundary. Implementation is complete.

## Verification record

The Release application build passed with MSVC and Qt 6.11.1. The affected `schematicopen_tests` suite passed. Its focused preferences lifecycle check also passed on native Windows: menu action and Ctrl+, opening, General tab selection and empty content, resizing, Close/Escape/window-close dismissal, and reopening. Document JSON, selected component IDs, undo index, and clean state remained unchanged. The captured native dialog was visually inspected. No real preferences, settings schema, or persistence were introduced.
