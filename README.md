# Schematic

Schematic is a Qt 6 desktop application for drawing clear, presentation-quality guitar pedal schematics. It focuses on the layout and communication of a circuit; it is not a PCB layout or circuit simulation tool.

## Current scope

The application has a graphics canvas with grid snapping, pan and zoom, component placement and editing, orthogonal terminal-connected wires, branching, and versioned JSON documents. The standard built-in symbols are resistor, capacitor, electrolytic capacitor, diode, ground, grid-aligned op-amp, potentiometer, and input/output jack. Resistor/capacitor/op-amp artwork follows the [Grid 10 templates](symbol-templates/grid10/README.md). The original op-amp remains under Compatibility > Legacy Symbols; the packaged library contains nine entries in total.

See [ROADMAP.md](ROADMAP.md) for known issues and future product work, and [DECISIONS.md](DECISIONS.md) for architecture choices that should be preserved unless deliberately revisited.

## Editing properties

Select components to edit Reference, Value, and custom text properties in the Properties dock, which defaults beneath Symbols on the left. Each row has a visibility checkbox; hiding a property retains its value. Add, rename, or remove custom properties with the controls below the table. Double-click a component to focus Value; F2 focuses Reference. The View menu can show the dock again.

With several components selected, differing values show `[Multiple values]`; properties missing from some instances show `[Not set on all]`. Editing applies to all selected components in one undo step. Reference text and custom-name editing require a single component. Enter, Tab, or leaving a field commits it; Escape cancels uncommitted text. Changing selection or document cancels pending text. Visible custom labels appear below Value as `Name: value`.

Visible properties form one compact block below each symbol, starting with Reference. Drag any row to move the whole block freely without moving the component or wires. Use **Reset property position** in the block context menu or Properties dock to restore automatic placement. Custom block positions follow component movement and stay upright through rotation/flips. Existing saved dock layouts are preserved; drag the Properties dock beneath Symbols to change an older layout.

## Edit menu and clipboard

The Edit menu groups history, clipboard/delete, rotation, flips, and Rename. Use Ctrl+X/C/V for Cut/Copy/Paste, Delete to remove a selection, R / Shift+R to rotate right/left, and H / V to flip horizontally/vertically. Transform shortcuts apply on the canvas; property fields retain normal typing and text editing. Menu clipboard actions and Undo/Redo edit text while a text field has focus.

Copy includes selected components and explicitly selected wires. Copied branches retain their connectivity; endpoints on unselected components become detached nodes. Paste creates fresh identities and unique references, preserves properties and text placement, and offsets each successive paste one grid step right/down. Cut and Paste each form one undo step. Cutting components also removes their incident wires, as Delete does, even when those wires were not selected for copying. A symbol definition conflicting with one already used in the destination rejects the paste without changes.

## Preferences

Open **Edit > Preferences…** or press **Ctrl+,** for the tabbed Preferences dialog. The initial **General** tab is empty. Close the dialog with Close, Escape, or the window close control; no preferences are stored yet.

## Build

Requirements: CMake, a C++17 compiler, and Qt 6.5 or newer with Core, Gui, and Widgets. Qt Test is needed when `BUILD_TESTING` is enabled (the default).

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

## Project format

Saved .schematic files use JSON format version 6. The symbols array contains exactly one validated converted definition per used symbol ID, ordered by ID. Components, explicit nodes and orthogonal wire routes retain their identity-based connectivity. Documents resolve geometry and artwork from their own symbol snapshots. Versions 1–5 remain readable and upgrade on save. Version 4 introduced black and white symbol fills. Version 5 stores each component's ordered properties, including individual visibility, instead of separate reference/value strings. Reference and Value are mandatory reserved properties. Version 6 adds optional component-relative property-block offsets. Older builds reject v6 rather than discarding layout data. Older documents adopt the compact unified block layout when opened.

Symbol artwork supports transparent interiors, opaque white interiors, and opaque black interiors with the common black outline. In converted-symbol JSON, `filled` enables filling and optional `fillColor: "black"` selects black; omitted colour preserves legacy white fills. SVG import/export preserves `none`, white, and black fills, including group inheritance and compound-path fill rules. Other fill colours are rejected. Standalone black-fill symbol packages require this supporting build; older builds have no package-version guard and may ignore the new colour property.

Older SVG exports wrote black even for library shapes that displayed white. If such a source should remain white, re-export its stored library definition using this build or explicitly change the SVG fill to white before import. See the [black-fill design and compatibility notes](docs/design/black-symbol-fills.md).

On opening v3 or later, differing or missing library symbols are reviewed together. Embedded definitions can remain document-local; compatible definitions can update the library or be replaced by the active library version. Different terminal signatures cannot update the library. Library changes are staged as a batch; packaged replacements require an explicit persisted override. Cancelled conflict review leaves the open document unchanged.
