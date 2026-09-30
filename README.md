# Schematic

Schematic is a Qt 6 desktop application for drawing clear, presentation-quality guitar pedal schematics. It focuses on the layout and communication of a circuit; it is not a PCB layout or circuit simulation tool.

## Current scope

The application has a graphics canvas with grid snapping, pan and zoom, component placement and editing, orthogonal terminal-connected wires, branching, and versioned JSON documents. The built-in symbol set is resistor, capacitor, diode, ground, op-amp, potentiometer, and input/output jack.

See [ROADMAP.md](ROADMAP.md) for known issues and future product work, and [DECISIONS.md](DECISIONS.md) for architecture choices that should be preserved unless deliberately revisited.

## Build

Requirements: CMake, a C++17 compiler, and Qt 6.5 or newer with Core, Gui, and Widgets. Qt Test is needed when `BUILD_TESTING` is enabled (the default).

Configure and build with the default preset:

```sh
cmake --preset default
cmake --build --preset default
```

A `mingw` preset is also provided. If Qt Test is not installed, configure with `-DBUILD_TESTING=OFF` to build the application without the test target.

## Project documentation

- [AGENTS.md](AGENTS.md): product goals and collaboration guidance.
- [DECISIONS.md](DECISIONS.md): settled technical and interaction decisions.
- [ROADMAP.md](ROADMAP.md): current review findings, open decisions, and later product ideas.
- [CLAUDE.md](CLAUDE.md): local development environment and repository conventions.
