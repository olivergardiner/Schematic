# Project context and working guidance

## Product goal

This project is a Qt 6 desktop application for drawing clear, presentation-quality guitar pedal schematics. Its priority is visual readability and layout control. It is a schematic drawing tool, not a PCB layout or circuit simulation tool.

The intended editor should provide a sensible configurable grid, standard symbols with editable reference/value labels, and wires that snap to terminals and remain connected when components move. Wires need corners, branches, and visible junctions. Crossings should remain distinct from joined connections.

## Current technical direction

- The user selected C++ and Qt 6 Widgets / Graphics View is the project direction.
- The repository currently uses CMake and Qt 6 Widgets, with `QGraphicsScene`/`QGraphicsView`-style scene and view classes (`schematicscene.*`, `schematicview.*`). Preserve the existing project conventions unless inspection gives a concrete reason to change them.
- Keep the schematic document model independent of Qt rendering items and input handling. The scene/view should display and edit the model, not be the source of truth.
- Represent components, terminals, and wire routes as structured objects. Electrical connectivity must be separate from route coordinates: use explicit terminal references and shared node identities for branches, and compute net groups from that identity graph rather than inferring connection from coincident coordinates. Crossings do not connect unless the user explicitly joins them.
- Use world/scene units for symbol geometry, component positions, and terminal positions. Grid spacing controls snapping and grid-line spacing; it does not scale symbols. Right-angle rotations preserve axis-aligned symbol geometry, but do not imply that every terminal lies on a grid intersection.
- Milestone-1 wire routes are orthogonal. The wire tool should insert deterministic corners for diagonal pointer movement. When loading, derive route endpoint coordinates from their terminal or node identities; warn on stale serialized endpoint coordinates and validate the resulting route geometry.
- Net groups and junction dots are derived from explicit topology; persistent net IDs and a separately persisted junction list are not required for milestone 1.
- Treat undo/redo as a later decision unless the user explicitly brings it into the first milestone.

## First usable milestone

Keep the initial milestone focused on:

- A canvas with pan, zoom, and grid snapping.
- Configurable grid spacing, stored with the document and changeable through a simple UI control.
- Selection, moving, and deletion.
- Built-in symbols: resistor, capacitor, diode, ground, op-amp, potentiometer, and input/output jack.
- Editable component reference and value labels.
- Terminal-connected wires with corners, branches, and visible junction dots.
- Saving and reopening in a documented, versioned project format.

No PCB features, simulation, automatic routing, or user-authored symbol editor are in scope for this milestone.

## Collaboration and implementation workflow

- For substantial feature work, first inspect the existing code and conventions and propose a short, reviewable implementation plan. Explain the model, symbol geometry and terminal placement, net/routing representation, and scene/view interaction. Call out choices that would be costly to change later.
- Do not begin implementation until the user has approved that plan. Once approved, implement only the agreed milestone and work in small, reviewable steps.
- Keep changes aligned with the approved scope. Prefer focused tests for connectivity, moving connected components, and save/load when implementing the model and persistence; do not expand into unrelated test infrastructure.
- Explain what changed and how it was checked when reporting completed work. Do not imply tests were run if they were not.

## Build and test choices

- The project uses C++17 and CMake. The `schematiccore` target should depend on Qt Core, not Qt Gui or Widgets.
- `BUILD_TESTING` defaults to `ON`, so the standard presets include the headless Qt Test suite and require the Qt Test component. An app-only configuration can set `-DBUILD_TESTING=OFF`; keep this option documented for users who do not have Qt Test installed.
- Run the smallest checks that give confidence in the change and report exactly what was run. Do not repeat a successful build or test run unless the code changed afterward, the result was inconclusive, or a later change could affect it. For documentation-only changes, check the Markdown diff and links; do not build or run tests.

## Repository notes

- The project is currently named `Schematic`; the product name is not settled.
- Build system: CMake. Qt requirement currently specified as Qt 6.5 with Core, Gui, and Widgets for the app; Qt Test is required only when `BUILD_TESTING` is enabled.
- Existing source includes `main.cpp`, `mainwindow.*`, `schematicscene.*`, and `schematicview.*`.
- Checked-in CMake presets (`default`, `mingw`) are portable; Qt kit paths belong in a git-ignored `CMakeUserPresets.json`, as described in the README.

## Documentation

- Keep this file focused on product goals and collaboration guidance.
- `DECISIONS.md` records settled design choices; `ROADMAP.md` tracks unresolved defects, open choices, and later product ideas.
- The repository documentation is the source of truth. Keep claims about current behavior aligned with the code; identify known issues as unresolved until they are fixed and verified.
