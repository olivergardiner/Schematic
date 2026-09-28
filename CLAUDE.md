# Working notes for AI agents (Claude, etc.)

This file complements `AGENTS.md`. `AGENTS.md` states the product goal and
the collaboration workflow; this file records environment specifics, the
detailed design decisions reached during planning discussion (which go
beyond what's written in `AGENTS.md`), current implementation status, and
conventions observed in the repository. Read both before making changes.

## Environment (this machine)

- Qt 6.11.1 installed at `D:/Qt/6.11.1`, with `msvc2022_64` and `mingw_64`
  kits. `CMakePresets.json` points `CMAKE_PREFIX_PATH` at the msvc2022_64
  kit for the `default` preset and mingw_64 for the `mingw` preset.
- Visual Studio 18 (2026) Community is installed; the `default` CMake preset
  uses generator `"Visual Studio 18 2026"`.
- No `qmake`, `ninja`, or `vcpkg` on PATH — builds go through the CMake
  presets only. CMake 4.3.1 is on PATH.
- Build/run from bash (git-bash-style POSIX shell on Windows):
  ```
  cmake --preset default
  cmake --build build --config Debug
  PATH="/d/Qt/6.11.1/msvc2022_64/bin:$PATH" ctest --test-dir build -C Debug --output-on-failure
  ```
  The Qt `bin` directory must be on `PATH` to run any built executable
  directly (DLL loading), including test executables.
- `.vscode/tasks.json` and `launch.json` mirror the sibling `ValveTesterSoftware`
  project's conventions (Build Debug/Release tasks, `windeployqt6` deploy
  tasks, `cppvsdbg` launch configs).

## Repository conventions observed

- No `.ui` files for new code — `MainWindow` etc. are hand-written C++, not
  Qt Designer forms. Keep new widget code consistent with this.
- CMake target sources list headers explicitly alongside `.cpp` files (see
  `qt_add_executable(Schematic ...)` and `add_library(schematiccore ...)`).
- No test infrastructure existed anywhere in the sibling repos on this
  machine (`ValveTesterSoftware`, `tonestack`) before this project. This
  project introduces `QtTest` + CTest as the test convention going forward
  (see "Testing conventions" below) since no prior pattern existed to match.

## Design decisions finalized during planning (beyond AGENTS.md)

These were worked out interactively before any milestone-1 code was
written. Treat them as settled unless the user explicitly revisits them.

### Connectivity is an explicit identity graph, not coordinate coincidence

This is the single most important, hardest-to-change decision in the model.

- A `Net` is **computed on demand**, never persisted, via a union-find over
  explicit connection-point *identities* — not over coincident `QPointF`
  coordinates. On a snapped grid, unrelated points frequently land on the
  same coordinate; treating that as a connection would violate "crossings
  should not connect unless explicitly joined."
- Identities are: `TerminalRef {ComponentId, TerminalId}` (always exists,
  one per component terminal) and `NodeId` (a document-scoped id that comes
  into existence *only* via an explicit branch operation — never inferred
  from geometry).
- Every persisted `WireRoute` has a `start` and `end`, each a `WireEndpoint`
  = tagged reference to either a `TerminalRef` or a `NodeId`. No free-
  floating/unattached persisted endpoint in milestone 1 — both ends of a
  committed wire must resolve to something real.
- Two wires are electrically joined **iff** they share an endpoint
  *identity* (not iff their vertex lists happen to touch at the same
  coordinate).
- Branching (T-ing off an existing wire) is an explicit operation:
  `Document::branchWireAt(WireId, QPointF)` allocates a fresh `NodeId`,
  splits the existing route into two routes that both end at that node, and
  gives the new stub route an endpoint at that same node. This is the only
  way a `NodeId` is created.
- A wire whose *geometry* crosses another wire's segment, with no explicit
  join/branch performed there, shares no identity with it and is
  disconnected by construction — no interior-point/coordinate intersection
  test exists anywhere in the net computation, so this falls out for free
  rather than needing a special case.
- Junction dots are a rendering query, not a stored object:
  `Document::junctionPoints()` tallies how many route-ends reference each
  identity; **3 or more** references gets a dot (2 is a plain pass-
  through/corner; 1 is a plain lead end).
- Moving a component updates `Component::position` and the derived
  coordinates of routes anchored to its terminals, without changing any
  identity reference. `Document::moveComponent` is transactional and can
  reject a candidate position. For each affected endpoint, it removes
  redundant adjacent vertices when the endpoint coincides with them; if
  that would collapse a direct 2-vertex route to zero length, the entire
  move is rejected with no document changes. Otherwise, a diagonal segment
  introduced at the moved endpoint gets one horizontal-first corner at
  `(newEndpoint.x, adjacentVertex.y)`. The untouched portion of the route
  remains unchanged, and the candidate route is checked by the shared route
  geometry validator before the component move commits. Net membership is
  undisturbed because endpoint identities never change.
- **Tradeoff accepted**: this is more bookkeeping than pure coordinate
  flood-fill (needs a `NodeId` type, a real split-and-relink operation for
  branching, and the wire tool must distinguish "click on a terminal/node"
  vs. "click on a bare interior wire point" vs. "click on empty space").
  Justified because coordinate coincidence is not a reliable signal of
  user intent once grid-snapping is in play.

### Deleting a component cascades to its attached wires (intentional)

`Document::removeComponent(ComponentId)` must remove (or re-terminate, for
a shared branch node) every `WireRoute` anchored to one of that component's
terminals. This is *required*, not just convenient: since every persisted
wire's both ends must resolve to a real terminal or node, leaving a wire
anchored to a deleted component's terminal would create exactly the invalid
dangling-reference state the load-time validator (below) is designed to
reject. Specifically:
- If the route's other end is a plain terminal or a node with no other
  references, remove the whole route.
- If the route's other end is a node still shared by other routes (the
  deleted component was one leg of a branch), remove only that leg; the
  node and remaining routes stay intact, and `junctionPoints()` naturally
  reflects the reduced reference count (e.g. a 3-way tie becoming a 2-way
  pass-through) since junctions are never persisted.

### Route geometry rules (validated at both creation time and load time)

Applied via the shared `validateRouteGeometry()` helper so interactive
mutators (`addWire`, `branchWireAt`, and `moveComponent`) and `fromJson()`
enforce identical rules:
- At least 2 vertices.
- Every coordinate finite (`std::isfinite`) — rejects NaN/Inf from a
  corrupted/hand-edited file.
- No zero-length consecutive-vertex segments.
- **Every segment must be axis-aligned (purely horizontal or purely
  vertical) — no diagonals.** Confirmed requirement for milestone 1. The
  wire-drawing tool must insert an automatic corner when the user's next
  click isn't purely horizontal/vertical from the last point, choosing
  **horizontal-first, then vertical** by default (an explicit corner
  choice by the user, e.g. a modifier key or click-drag corner placement,
  overrides this default — exact UI mechanism to be decided during tool
  implementation).
- Grid alignment (vertices on grid multiples) is **not** a validity
  requirement — it's an editing-time convenience enforced by snapping, not
  an electrical-correctness rule. An off-grid but otherwise well-formed
  route from a hand-edited file is valid, just untidy.

### Endpoint coordinates are always derived from identity, never trusted from file

After parsing and validating a route's `start`/`end` references and
interior vertices, the **first and last vertices are always recomputed**
from the resolved identity, discarding whatever coordinate was literally
stored in the file for those two points:
- Terminal-anchored end → `Component::terminalPosition(TerminalId)` using
  the component's *loaded* position/rotation.
- Node-anchored end → the position stored in the file's `nodes[]` map.

Interior corner vertices are taken as-is (nothing else defines them).

- If the file's stored coordinate disagrees with the derived value by more
  than a small epsilon, this is **not** a fatal validation error — it's
  recorded in a `LoadResult.warnings` list (separate from `errors`) so the
  UI can report e.g. "Wire 7's start point was recalculated from its
  terminal" without rejecting an otherwise-valid file.
- **Route geometry must be re-validated after endpoint derivation**,
  using the same helper as above. Restoring a derived endpoint can make the
  first/last segment diagonal or zero-length even if the file's literal
  coordinates originally passed validation (e.g. a stale endpoint that
  happened to be axis-aligned with its neighbour, but the component's real
  terminal position is not). If re-validation fails after derivation, the
  route is rejected (an `errors` entry, not just a warning).
- Consequence: the endpoint coordinates written by `toJson()` are
  advisory/informational only — the identity references are the real
  source of truth for where a route begins and ends.

### Load-time validation (`Document::fromJson`)

Returns a result type (not a bare bool/Document) so the caller can report
specifics: something like
`struct LoadResult { std::optional<Document> document; QStringList errors; QStringList warnings; };`
- Duplicate id rejection: component ids, wire ids, and node ids must each
  be unique within their own namespace.
- Every `TerminalRef` must resolve to a component id present in the file,
  and the `TerminalId` must be in range for that component's `SymbolKind`
  (checked against `SymbolDefinition::terminals.size()`).
- Every `NodeId` reference must resolve to an entry in the file's `nodes[]`.
- Route geometry checks (above) apply to every route, both before and
  after endpoint derivation.
- **Policy: reject the whole file on any error** (no partial/best-effort
  load) for milestone 1. A "load what's valid, report the rest" mode is a
  possible future enhancement, not in scope now.

### Other confirmed milestone-1 scope decisions

- **Rotation**: quadrants only (0/90/180/270), no arbitrary angle or
  mirroring. Implemented as `enum class Rotation` in `core/component.h`
  with `rotateOffset()` doing an axis swap/negation, not a general affine
  transform.
- **Pan**: `SchematicView` currently has zoom (Ctrl+wheel) and rubber-band
  selection (`QGraphicsView::RubberBandDrag`) but **no pan yet** — this was
  a gap identified during planning, not yet implemented. Plan: middle-
  mouse-button drag pans the canvas regardless of edit mode, via
  `mousePressEvent`/`mouseMoveEvent`/`mouseReleaseEvent` overrides in
  `SchematicView` adjusting scrollbars directly, leaving left-button
  behavior untouched.
- **Grid spacing**: must be genuinely configurable, not just a `Document`
  field with no UI (a stored-but-unreachable setting was judged insufficient
  given this was requested explicitly). Plan: a small toolbar control (spin
  box or combo of common spacings) writing `Document::gridSpacing`,
  triggering `SchematicView` background redraw and updated snap increment.
  Not yet implemented as of the current step.
- **Wire reshaping** (dragging an already-placed corner after the wire is
  committed) is deferred past milestone 1 — delete-and-redraw is the
  milestone-1 workflow. The model's arbitrary-vertex-list design supports
  reshaping later without a model change; it's a scene/tool-layer addition.
- Undo/redo, arbitrary rotation/mirroring, net naming, and a user-authored
  symbol editor remain explicitly out of scope (per `AGENTS.md`).

## File format (version 1)

Versioned JSON via `QJsonDocument`, Core-only:
```json
{
  "formatVersion": 1,
  "grid": { "spacing": 10 },
  "components": [
    { "id": 1, "kind": "Resistor", "x": 100, "y": 50, "rotation": 90,
      "reference": "R1", "value": "10k" }
  ],
  "nodes": [ { "id": 1, "x": 100, "y": 80 } ],
  "wires": [
    { "id": 1, "points": [[100,50],[100,80]],
      "start": {"component": 1, "terminal": 0},
      "end":   {"node": 1} }
  ]
}
```
Nets and junctions are **never serialized** — both are recomputed from the
endpoint-identity graph on load. The Core-only `Document::toJson()` and
`Document::fromJson()` APIs implement this schema. Loads reject unsupported
versions and invalid topology/geometry; stale endpoint coordinates are
recomputed from identities with warnings, and unreferenced nodes are dropped
with warnings. `symbolKindName()`/`symbolKindFromName()` are the stable
serialization contract for `SymbolKind`.

## Testing conventions established

- `Qt6::Test` + CTest, headless (Core + Test only, no Gui/Widgets) so the
  model can be tested without a display/QPA platform plugin.
- One `tst_<thing>.h`/`.cpp` pair per class/module under test, each a plain
  `QObject`-derived test class with `private slots:` test methods — no
  `QTEST_MAIN` in individual files.
- A single shared `tests/main.cpp` runs every test class via
  `QTest::qExec(&test, argc, argv)`, OR-ing return codes together, registered
  as one `add_test(NAME schematiccore_tests COMMAND schematiccore_tests)`.
  Add one `QTest::qExec<T>` line per new test class here rather than
  registering a new CTest executable per file.
- Every test method has a comment explaining *why* the case matters (not
  just what it checks) — see `tests/tst_component.h`/`tst_symboldefinition.h`
  for the established style.
- **Expected values must not be derived by calling the same function (or
  another function sharing its logic) that is under test.** A first draft of
  `terminalPositionCombinesRotationAndTranslation()` computed its expected
  terminal positions by calling `rotateOffset()` directly - the same
  function `Component::terminalPosition()` calls internally - so the test
  would have passed even if both shared an identical bug. Fixed by
  hand-computing literal expected coordinates instead. Apply this when
  testing `Document` in step 2 as well: e.g. a `computeNets()` test should
  not re-derive the expected net membership using the same union-find code
  path it's meant to be checking.
- Run via: `ctest --test-dir build -C Debug --output-on-failure`, or the
  test binary directly with `-v2` for full per-assertion output (must have
  Qt `bin` on `PATH`).

## Current implementation status (update this section as work progresses)

**Step 1 — complete.** `schematiccore` static library (Core-only) exists:
- `core/identifiers.h` — `ComponentId`, `TerminalId`, `NodeId`, `WireId`
  typedefs + invalid sentinels (the latter two anticipate step 2).
- `core/symbolkind.h/.cpp` — closed `SymbolKind` enum (7 built-in kinds) +
  name round-trip functions for serialization.
- `core/symboldefinition.h/.cpp` — static built-in geometry/terminal data
  for all 7 symbols, authored directly in world/scene units (see the
  coordinate contract note below — **not** a separately-scaled "grid unit").
  **Gotcha hit and fixed**: `symbolTable()[kind]` on a `const QHash` returns
  by value, not reference — returning that from a `const SymbolDefinition &`
  -returning function is a dangling-reference bug (caught by an MSVC C4172
  warning). Use `table.constFind(kind)` / `it.value()` instead, which
  references the container's stored value directly.
- `core/component.h/.cpp` — `Component` (id, kind, position, quadrant
  `Rotation`, reference, value) with `terminalPosition()` /
  `rotateOffset()`. Qt-Core-only, no scene/widget dependency.
- **Coordinate contract clarified after review**: `Component::position()`,
  `terminalPosition()`, and all `SymbolDefinition` geometry are in the same
  world/scene units — there is no separate "symbol unit"/"grid cell" scale.
  `Document::gridSpacing` (step 2) will be purely a snap increment and
  background grid-line spacing; it never scales symbol artwork or terminal
  offsets, so changing grid spacing does not resize placed components. An
  earlier draft of the header comments implied scaling that the code never
  implemented — comments in `component.h`, `symboldefinition.h`, and
  `symboldefinition.cpp` were corrected to state this explicitly before
  the document model (which introduces `gridSpacing`) is built on top.
- `tests/` — `schematiccore_tests` with `TstSymbolDefinition` and
  `TstComponent`, 13 test methods total, all passing, including
  `terminalPositionCombinesRotationAndTranslation()` (added after review),
  which cross-checks `Component::terminalPosition()` against hand-computed
  expected coordinates at all 4 rotations for the op-amp's 5 asymmetric
  terminals — independent of `rotateOffset()`'s own implementation, unlike
  the earlier tests which only checked identity/translation-only cases or
  `rotateOffset()` in isolation.
- Top-level `CMakeLists.txt` sets `CMAKE_CXX_STANDARD 17` explicitly (not
  set anywhere previously; needed for `std::optional`/`std::variant` used by
  `WireEndpoint` in step 2) and defines the `schematiccore` library target.
  **Testing is gated behind `option(BUILD_TESTING ... ON)`**: `Qt6::Test` is
  only `find_package`'d, and `enable_testing()`/`add_subdirectory(tests)`
  only run, inside `if(BUILD_TESTING)` — added after review so building just
  the desktop app (`-DBUILD_TESTING=OFF`) does not require the Test
  component at all. Verified by a clean configure+build with
  `-DBUILD_TESTING=OFF` producing `Schematic.exe` with no test target built
  and no `Qt6::Test` lookup in the configure log.
  **`BUILD_TESTING` defaults to ON, confirmed as an intentional choice (not
  an oversight) after a second review round**: this project is under active
  development and `AGENTS.md`'s workflow treats focused tests as a
  first-class part of each milestone, so the `default`/`mingw` CMake
  presets (used for day-to-day development) build and run
  `schematiccore_tests` by default and therefore do require `Qt6::Test` to
  be installed. `-DBUILD_TESTING=OFF` remains the documented opt-out for an
  app-only build with no `Qt6::Test` dependency. If a future preset is added
  specifically for app-only distribution builds, set
  `"BUILD_TESTING": "OFF"` in that preset's `cacheVariables` rather than
  changing the option's default.
- **Rotation/grid-alignment comment corrected (second review round)**: an
  earlier comment on `enum class Rotation` in `component.h` claimed
  right-angle rotation keeps terminals grid-aligned. That's false in
  general — e.g. the op-amp's `(-28,-8)` terminal offset is off-grid at a
  10-unit spacing regardless of rotation, since grid alignment depends on
  whether the *un-rotated* offset happens to be a multiple of the grid
  spacing, which quadrant rotation cannot change. What right-angle rotation
  actually guarantees is that axis-aligned symbol geometry (horizontal/
  vertical lines in the artwork) stays axis-aligned after rotation, never
  becoming diagonal. The comment now states this correctly and explicitly
  flags that terminal snapping relies on `terminalPosition()`'s actual
  computed value, not an assumption that terminals sit on grid points.
- The existing `Schematic` app links `schematiccore` but does not yet use
  it — no behavior change to the running app in this step.

**Step 2 — complete.** `WireEndpoint`/`NodeId`/`WireRoute` and the
Core-only `Document` model are implemented in `core/`. The document owns
component, wire, and node identities; routes use explicit terminal or node
endpoints; branching splits a route at an explicit point; nets and junction
points are derived from endpoint identities; and component moves update
attached route geometry transactionally while preserving orthogonality.
Focused document and route-geometry tests cover connectivity, crossings,
branching, movement, deletion, invalid input, and grid spacing. The changed
model and test sources were verified with a successful Debug build of all
three targets (`schematiccore`, `Schematic`, and `schematiccore_tests`) and a
passing CTest run (`ctest --test-dir build -C Debug --output-on-failure`:
1/1 CTest entries passed; all three QObject test classes passed when the test
binary was run directly with `-v2`). An earlier agent session saw an MSBuild
`Path`/`PATH` environment error, but it did not reproduce in the verified
build session and is not a current project issue.

**Step 3 — complete.** Core-only version-1 JSON serialization and
validated loading are provided by `Document::toJson()`/`fromJson()` in
`core/documentjson.cpp`. Loading validates ids, kinds, rotations, references,
coordinates, and orthogonal routes; derives endpoint coordinates from
identities; reports stale-coordinate and orphan-node warnings; and returns no
document when errors occur. Focused round-trip and load-validation tests are
in `tests/tst_documentjson.*`.

**Review round found and fixed**: the initial
`rejectsInvalidIdsReferencesAndGeometry()` diagonal-geometry test case
corrupted only a 2-vertex wire's two endpoint coordinates, which get
unconditionally overwritten by derived identity positions on load - so the
corruption could never survive to fail revalidation. Fixed by introducing an
off-axis *interior* vertex instead, which derivation leaves untouched.
Verified with a Debug build of all three targets and a passing CTest run
(`ctest --test-dir build -C Debug --output-on-failure`: 1/1 CTest entries
passed; all four QObject test classes, including `TstDocumentJson`, passed
when the test binary was run directly with `-v2`).

**Not yet started**:
read-only scene rendering (step 4), interactive editing — select/move/
delete/place/draw-wire, plus `SchematicView` pan and grid-spacing control
(step 5), and `MainWindow` file I/O + symbol palette + label editing
(step 6). See the step list above for what each covers.
