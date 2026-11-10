# Schematic

Schematic is a Qt 6 desktop application for drawing clear, presentation-quality guitar pedal schematics. It focuses on the layout and communication of a circuit; it is not a PCB layout or circuit simulation tool.

## Current scope

The application has a graphics canvas with grid snapping, pan and zoom, component placement and editing, orthogonal terminal-connected wires, branching, and versioned JSON documents. The standard built-in symbols are resistor, capacitor, electrolytic capacitor, diode, Schottky diode, Zener diode, LED, PNP/NPN transistors, N/P channel JFETs and MOSFETs, ground, grid-aligned op-amp with and without supply terminals, OTA variants, LM13700 Darlington buffer, potentiometer, preset resistor, and input/output jack. Block Schematics adds buffer and a two-input summing node, with high-pass, low-pass, band-pass and band-stop filters and their variable versions under Block Schematics > Filters, each in a 40 × 40 scene-unit box with left inputs and right outputs. Resistor/capacitor/op-amp artwork follows the [Grid 10 templates](symbol-templates/grid10/README.md). Signals contains Power Rails (one rotatable/flippable Power Rail, defaulting to `Vcc`), Grounds (Earth, Chassis and Signal Ground; name the latter through Value when needed) and Ports (Input, Output and Bidirectional). Their editable Value labels start with useful defaults and their references start hidden. Matching names do not connect separate nets. The packaged library contains forty entries in total.

See [ROADMAP.md](ROADMAP.md) for known issues and future product work, and [DECISIONS.md](DECISIONS.md) for architecture choices that should be preserved unless deliberately revisited.

## Text notes

Choose **Edit > Add Text Note** or the **T** tool, then click the canvas. The note is selected and its multiline text opens in Properties. Enter inserts a paragraph break; **Ctrl+Enter** applies text and Escape discards an uncommitted edit. Drag the note to move it, or drag either side handle of its selected box to change wrapping width; height adjusts automatically. Properties also provides a width control. Notes support box/group selection, clipboard actions, undo/redo and SVG export without the editing box.

## Recent files

Use **File > Open Recent** to reopen one of the ten most recently opened or saved schematics. The list is remembered between sessions, with the newest first. Entries show their folders to distinguish identical filenames and use the normal unsaved-change and symbol-review flow. Failed or cancelled operations do not change the list. Unavailable files remain listed so they can be retried later. **Clear Recent Files** clears only the history, not the files or the current document.

## Editing connections

In Select mode, double-click a component terminal or existing junction to switch to Draw Wire and start a connection there. Click a terminal, junction or wire to connect to it. Double-click a free point to finish at a snapped free endpoint while staying in Draw Wire; press Escape to cancel and return to Select. Double-clicking the component body still opens its properties.

Placing a new component by canvas click or palette drop connects terminals that land exactly on an existing wire or bare junction after snapping. For example, place a Power Rail so its terminal touches the rail wire: it connects and gains a junction dot. Placement and connection undo together. Separate wire crossings and points already occupied by another component terminal stay unconnected, with a status message. Moving an existing component onto a wire does not create a new connection. Deleting a connected component preserves its wires in place, including the rail wire: terminal endpoints become free nodes, retaining connections between wires that shared the same terminal. Only explicitly selected wires are deleted.

In Select mode, click near a wire to select it; selected wires are red. Drag a segment of a single selected wire to reshape it: horizontal segments move up/down and vertical segments move left/right, snapping to the document grid. Adjacent segments stretch or shrink; terminal and junction positions stay fixed, with connecting bends added when dragging an end segment. Redundant collinear vertices form one straight segment. Crossings do not create connections.

Dragging previews the route without changing the document. Release commits one undoable edit; Escape cancels. Invalid routes are rejected. Mixed or multiple selections retain group movement.

## Editing properties

Select components to edit Reference, Value, and custom text properties in the Properties dock, which defaults beneath Symbols on the left. Each row has a visibility checkbox; hiding a property retains its value. Add, rename, or remove custom properties with the controls below the table. Double-click a component to focus Value; F2 focuses Reference. The View menu can show the dock again.

With several components selected, differing values show `[Multiple values]`; properties missing from some instances show `[Not set on all]`. Editing applies to all selected components in one undo step. Reference text and custom-name editing require a single component. Enter, Tab, or leaving a field commits it; Escape cancels uncommitted text. Changing selection or document cancels pending text. Visible custom labels appear below Value as `Name: value`.

Visible properties form one compact block below each symbol, starting with Reference. Drag any row to move the whole block freely without moving the component or wires. Use **Reset property position** in the block context menu or Properties dock to restore automatic placement. Custom block positions follow component movement and stay upright through rotation/flips. Existing saved dock layouts are preserved; drag the Properties dock beneath Symbols to change an older layout.

## Edit menu and clipboard

The Edit menu groups history, clipboard/delete, rotation, flips, and Rename. Use Ctrl+X/C/V for Cut/Copy/Paste, Delete to remove a selection, R / Shift+R to rotate right/left, and H / V to flip horizontally/vertically. Transform shortcuts apply on the canvas; property fields retain normal typing and text editing. Menu clipboard actions and Undo/Redo edit text while a text field has focus.

Copy includes selected components and explicitly selected wires. Copied branches retain their connectivity; endpoints on unselected components become detached nodes. Paste creates fresh identities and unique references, preserves properties and text placement, and centres the pasted group at the snapped canvas cursor position. Menu Paste uses the last canvas pointer position, or the visible canvas centre before any pointer activity. Repeated pastes use the cursor position without adding an offset. Cut and Paste each form one undo step. Cutting components preserves unselected incident wires as detached wire ends, just as Delete does. A symbol definition conflicting with one already used in the destination rejects the paste without changes.

## Preferences

Open **Edit > Preferences…** or press **Ctrl+,** for the tabbed Preferences dialog. **General > CAD-style mouse wheel** is enabled by default: wheel zooms and Ctrl+wheel scrolls vertically. Disable it for wheel scrolling and Ctrl+wheel zoom. **Appearance** sets the property block grid spacing (default **2 units**), independently of the document grid. **OK** saves and applies changes immediately and remembers them across sessions; **Cancel**, Escape, or window close discards them. Changing spacing does not reposition existing blocks.

## SVG export

Use **File > SVG Export**, **Ctrl+E**, or the page-and-arrow icon on the File toolbar. Export includes the whole committed schematic and its visible labels, fitted with a small margin on a transparent background. It preserves vector artwork and white symbol fills; text remains editable. Grid, selection highlights and unfinished edits are excluded. The save dialog supplies the `.svg` suffix. Export does not change the schematic filename or saved/undo state.

## Build

Requirements: CMake, a C++17 compiler, and Qt 6.5 or newer with Core, Gui, Widgets, and SVG. Qt Test is needed when `BUILD_TESTING` is enabled (the default).

The checked-in `default` (Visual Studio 2026) and `mingw` presets contain no machine-specific paths, so CMake must be told where Qt is installed. If using Visual Studio 2022, override `generator` with `Visual Studio 17 2022` in your local configure preset. The MSVC output directory retains its historical `build-vs2022` name. After changing Visual Studio versions, run `cmake --fresh --preset local` (using your local preset name) before rebuilding to replace the old generator cache. Create a git-ignored `CMakeUserPresets.json` that inherits from a checked-in preset and sets `CMAKE_PREFIX_PATH`:

```json
{
  "version": 3,
  "configurePresets": [
    {
      "name": "local",
      "inherits": "default",
      "cacheVariables": { "CMAKE_PREFIX_PATH": "<path to your Qt kit, e.g. C:/Qt/6.x.y/msvc2022_64>" }
    }
  ],
  "buildPresets": [
    { "name": "local", "configurePreset": "local" }
  ]
}
```

Then configure and build:

```sh
cmake --preset local
cmake --build --preset local
```

Alternatively, pass `-DCMAKE_PREFIX_PATH=<Qt kit>` to `cmake --preset default` directly. A `mingw` preset is also provided (inherit from it the same way). If Qt Test is not installed, configure with `-DBUILD_TESTING=OFF` to build the application without the test target.

## Project documentation

- [AGENTS.md](AGENTS.md): product goals and collaboration guidance.
- [DECISIONS.md](DECISIONS.md): settled technical and interaction decisions.
- [ROADMAP.md](ROADMAP.md): current review findings, open decisions, and later product ideas.
- [CLAUDE.md](CLAUDE.md): local development environment and repository conventions.

Select a single OTA to change **Bias position: Below / Above** in Properties. The change preserves terminal roles, repairs attached orthogonal wires, and is undoable; invalid routes leave the document unchanged. Directions refer to the normal symbol pose before rotation/flipping. The palette shows one OTA for each supply configuration, initially with bias below; use the dropdown to choose Above. The Above definitions stay in the library and saved documents but are hidden from the palette. Existing embedded symbols without this metadata retain their saved artwork and do not gain the control silently; adopt the current library definition through symbol-conflict review if wanted.

## Symbol palette order

The [library manifest](core/symbols/manifest.json) declares the category tree. Each node has a `name`, an optional ordered `symbols` array of JSON filenames, and optional ordered `children`. A node's own symbols appear before its children. Hidden variants stay in their category but do not appear as tiles. Standalone symbol category paths must match collection placement. Writable libraries still read the older flat form and upgrade it on their next successful save. See the [manifest design](docs/design/category-tree-manifest.md) for validation and import ordering.

## Project format

Saved .schematic files use JSON format version 10. Version 9 adds plain multiline notes with stable IDs, positions and wrapping widths; note heights are derived and older builds reject newer formats. The symbols array contains each used symbol and all alternatives referenced by its layout controls, ordered by ID. Components, explicit nodes and orthogonal wire routes retain their identity-based connectivity. Documents resolve geometry and artwork from their own symbol snapshots. Versions 1-9 remain readable and upgrade on save, with no notes in versions 1-8. Version 4 introduced black and white symbol fills. Version 5 stores each component's ordered properties, including individual visibility, instead of separate reference/value strings. Reference and Value are mandatory reserved properties. Version 6 adds optional component-relative property-block offsets. Version 7 embeds layout controls and their alternatives so they remain editable without the installed library. Version 8 stores each component’s Left/Middle/Right property block justification; older builds reject v8. Older drawings retain Left justification. Older documents adopt the compact unified block layout when opened.

Symbol artwork supports transparent interiors, opaque white interiors, and opaque black interiors with the common black outline. In converted-symbol JSON, `filled` enables filling and optional `fillColor: "black"` selects black; omitted colour preserves legacy white fills. SVG import/export preserves `none`, white, and black fills, including group inheritance and compound-path fill rules. Other fill colours are rejected. Standalone black-fill symbol packages require this supporting build; older builds have no package-version guard and may ignore the new colour property.

Older SVG exports wrote black even for library shapes that displayed white. If such a source should remain white, re-export its stored library definition using this build or explicitly change the SVG fill to white before import. See the [black-fill design and compatibility notes](docs/design/black-symbol-fills.md).

On opening v3 or later, differing or missing library symbols are reviewed together. Embedded definitions can remain document-local; compatible definitions can update the library or be replaced by the active library version. Different terminal signatures cannot update the library. Library changes are staged as a batch; packaged replacements require an explicit persisted override. Cancelled conflict review leaves the open document unchanged.

Note Properties provides Left/Centre/Right icon buttons and Fill/Outline colour buttons, including **None** options. Format v10 persists note alignment and box colours; v9 notes retain transparent, borderless, left-aligned defaults. Component property-block justification also uses icon buttons.
