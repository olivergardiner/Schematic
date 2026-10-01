# Schematic

Schematic is a Qt 6 desktop application for drawing clear, presentation-quality guitar pedal schematics. It focuses on the layout and communication of a circuit; it is not a PCB layout or circuit simulation tool.

## Current scope

The application has a graphics canvas with grid snapping, pan and zoom, component placement and editing, orthogonal terminal-connected wires, branching, and versioned JSON documents. The built-in symbol set is resistor, capacitor, diode, ground, op-amp, potentiometer, and input/output jack.

See [ROADMAP.md](ROADMAP.md) for known issues and future product work, and [DECISIONS.md](DECISIONS.md) for architecture choices that should be preserved unless deliberately revisited.

## Build

Requirements: CMake, a C++17 compiler, and Qt 6.5 or newer with Core, Gui, and Widgets. Qt Test is needed when `BUILD_TESTING` is enabled (the default).

The checked-in `default` (Visual Studio) and `mingw` presets contain no machine-specific paths, so CMake must be told where Qt is installed. Create a git-ignored `CMakeUserPresets.json` that inherits from a checked-in preset and sets `CMAKE_PREFIX_PATH`:

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
