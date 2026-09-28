# Project context and working guidance

## Product goal

This project is a Qt 6 desktop application for drawing clear, presentation-quality guitar pedal schematics. Its priority is visual readability and layout control. It is a schematic drawing tool, not a PCB layout or circuit simulation tool.

The intended editor should provide a sensible configurable grid, standard symbols with editable reference/value labels, and wires that snap to terminals and remain connected when components move. Wires need corners, branches, and visible junctions. Crossings should remain distinct from joined connections.

## Current technical direction

- The user selected C++ and is leaning toward Qt 6.
- The repository currently uses CMake and Qt 6 Widgets, with `QGraphicsScene`/`QGraphicsView`-style scene and view classes (`schematicscene.*`, `schematicview.*`). Preserve the existing project conventions unless inspection gives a concrete reason to change them.
- Keep the schematic document model independent of Qt rendering items and input handling. The scene/view should display and edit the model, not be the source of truth.
- Represent components, terminals, nets, and wire routes as structured objects. Model electrical connectivity separately from wire geometry so rerouting does not change which terminals are connected.
- Treat undo/redo as a later decision unless the user explicitly brings it into the first milestone.

## First usable milestone

Keep the initial milestone focused on:

- A canvas with pan, zoom, and grid snapping.
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

## Repository notes

- The project is currently named `Schematic`; the product name is not settled.
- Build system: CMake. Qt requirement currently specified as Qt 6.5 with Core, Gui, and Widgets.
- Existing source includes `main.cpp`, `mainwindow.*`, `schematicscene.*`, and `schematicview.*`.
- README build instructions use the CMake `default` preset; a `mingw` preset is also mentioned.
