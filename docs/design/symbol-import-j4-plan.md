# J4 implementation plan: document symbol snapshots

Status: **approved and implemented; automated checks pass. Manual visual walkthrough outstanding.** This plan covers format v3 symbol embedding and conflict resolution from the [approved symbol authoring design](symbol-import.md). It does not authorize J5 symbol-content work.

## Goal

Make each saved schematic self-contained with respect to the symbol definitions it uses. Opening a document must preserve its saved artwork and metadata while resolving topology from that document's terminal definitions. When an embedded definition differs from the active library, the user reviews the difference before the document is opened.

## Starting state and dependency

- `Document::toJson()` currently writes format v2; `Document::fromJson()` accepts only v1 and v2.
- Components, terminal geometry, rendering, placement, and load validation still perform symbol lookups through `applicationSymbolLibrary()`.
- `DocumentLoadResult` carries errors and warnings but no structured symbol-conflict data; `MainWindow::onOpen()` replaces the active document immediately after successful parsing.
- The writable package store can add or replace writable entries but cannot explicitly update a packaged entry. It also currently requires original SVG and sidecar bytes, while v3 embeds the converted `LibrarySymbol` definition only.

J4 therefore needs to introduce document-scoped symbol resolution before changing the file version. A saved document must never depend on a mutable global definition for an ID it already uses.

## Scope

- Add a per-document symbol set and make document operations, wire endpoint calculation, loading, and scene rendering resolve symbols through that set.
- Write format v3 with one validated converted symbol definition for each distinct symbol ID used by the document. Continue to load v1 and v2 by resolving their component IDs from the current active library; the next save upgrades them to v3.
- Parse and validate the v3 embedded definitions, reject duplicate embedded IDs, reject invalid definitions and missing definitions for used IDs, and derive wire endpoint positions from the resolved terminal definitions.
- Return structured conflict records from headless loading so the application can present the approved choices without adding UI dependencies to `schematiccore`.
- Resolve all choices before replacing the current document. A cancelled prompt, invalid file, or failed library update must leave the active document and its file path unchanged.
- Add an explicit, persisted writable-library override for a user-approved update of a packaged symbol with the same ID and terminal signature. Unmarked ID collisions remain errors; no source silently shadows another.
- Do not change terminal compatibility rules, wire topology, import authoring, SVG profile, or symbol artwork.

## Model and serialization design

1. Store the document's resolved definitions in a `SymbolLibrary` (or an equivalent ID-to-definition value collection) owned by `Document`. Expose a const lookup such as `Document::symbol(SymbolId)` and document-scoped terminal-count/terminal-position helpers.
2. Remove model and scene dependence on global symbol lookups. `Component` geometry helpers should take an explicit `LibrarySymbol` definition, or the equivalent calculations should live on `Document`; all route validation and scene rendering must use the document's resolved definition.
3. When adding a component, copy its active library definition into the document's symbol set. When loading v3, embedded definitions take precedence for that document. For v1/v2, resolve definitions from the active library and retain them in memory so a subsequent save produces a stable v3 snapshot.
4. `toJson()` emits `formatVersion: 3` and a deterministic `symbols` array ordered by symbol ID. Each entry uses `SymbolLibrary::symbolToJson()`. The array contains exactly the definitions used by document components, not the entire library.
5. `fromJson()` accepts versions 1, 2, and 3. V1/v2 keep their existing `mirrored` rules and current-library lookup behavior. V3 requires the symbols array, validates every entry through the existing library-symbol validator, rejects duplicate IDs, and rejects any component whose ID has no embedded definition. Extra unused definitions should be rejected or warned and discarded consistently; this plan proposes rejecting them as malformed document content so writers and readers enforce the same minimal set.
6. All v3 wire terminal references and stored route endpoints are validated/recomputed against the resolved document symbol definitions, never against whichever version happens to be active globally.

## Conflict behavior and application flow

`Document::fromJson()` remains headless. It builds a candidate document using embedded definitions and reports structured conflicts comparing each used embedded definition with the active library entry:

| Conflict | Available choice | Result |
|---|---|---|
| Same ID and same terminal signature, different artwork or presentation metadata | Use embedded in this document; update the library to the embedded version; keep the library version | The first option changes only the document. The second updates the writable library through an explicit override when necessary. The third uses the active library definition in this document. |
| Same ID but different terminal signature | Use embedded in this document; cancel opening | Never offer a library update or silently substitute terminals, since saved wire endpoints reference terminal indexes. |
| ID missing from active library | Use embedded in this document; add the embedded definition to the writable library | The add option persists the converted definition and refreshes the palette. Since the original SVG source is not embedded, generate canonical SVG and sidecar bytes with `SymbolSvgExporter` for the writable package. |
| No active-library conflict | No prompt | Use the embedded definition in this document. |

Show all conflicts in a single modal review dialog, with one decision per symbol and only valid choices available for that row. Provide a clear Cancel action. The dialog operates on the candidate `DocumentLoadResult`; it does not modify the live document or library while the user is still choosing.

After confirmation, apply any requested library changes as a single staged batch. Extend `SymbolPackageStore` to write canonical exported SVG/sidecar data for embedded definitions and record explicit packaged-symbol overrides in the writable manifest. Loading honors only those explicit overrides; ordinary duplicate IDs remain reported conflicts. On any batch failure, roll back all staged library files and abort opening. Once all requested changes succeed, apply the chosen active-library or embedded definitions to the candidate document, then bind it to the scene and commit the file path/history reset.

## Implementation files and seams

- `core/document.h/.cpp`: document-owned symbol definitions, symbol lookup, terminal geometry, add-component snapshotting.
- `core/component.h/.cpp`: make symbol-dependent geometry use an explicit definition instead of the global registry.
- `core/documentjson.cpp` and `core/documentloadresult.h`: v3 parsing/writing, deterministic embedding, structured conflicts, and validation diagnostics.
- `schematicscene.cpp`: render glyphs and terminal markers from `m_document`'s resolved symbols.
- `symbolpackagestore.h/.cpp`: canonical package generation for embedded definitions, explicit override metadata, batch stage/rollback, and active-registry refresh after commit.
- `mainwindow.cpp/.h` plus a focused conflict dialog: review conflicts and choices, preserve current state on cancel/failure, and commit the loaded candidate only after successful resolution.
- Document and scene tests: exercise model behavior without Qt Widgets; UI prompt behavior can use the existing Qt Test setup where stable.

## Acceptance checks

- v1 and v2 fixtures still load with unchanged rotation, mirror, reference, and wire behavior; saving either emits v3.
- v3 round trips preserve all used definitions and render/resolve terminals from the embedded copies even when the active library contains different artwork or terminal geometry under the same ID.
- Reject invalid embedded symbols, duplicate embedded IDs, missing used definitions, and invalid terminal references with errors identifying the symbol/component path.
- Verify deterministic symbol ordering and exact used-symbol coverage in serialized documents.
- Cover every conflict row and choice, including explicit packaged override persistence across application restart.
- Verify cancel and injected batch-write failure leave the open document, current path, undo history, and library unchanged.
- Verify library update/add decisions update the palette and do not modify an existing document's embedded snapshot unless that document is explicitly saved after selecting the library version.
- Run the full headless CTest suite and app build after implementation; manually check conflict prompts, cancel behavior, opening, saving, and reopening a v3 file.

## Costly choices called out for approval

- **Document ownership:** symbol definitions become document model state. This intentionally removes global symbol lookup from core geometry and rendering paths.
- **Snapshot timing:** new placements capture the current library definition; legacy v1/v2 documents capture their resolved definitions when loaded and become self-contained on save.
- **Minimal v3 set:** the embedded array must contain exactly the definitions used by components; unused entries are proposed as a load error.
- **Packaged updates:** a user-approved library update may explicitly override a packaged symbol only when the terminal signature is unchanged. The override is persisted and distinguishable from an accidental duplicate ID.
- **Batch library mutation:** all add/update choices from one open operation are staged together and rolled back together if any write fails; opening is committed only after that batch succeeds.


## Implementation and verification (2026-11-01)

- Document-owned symbol definitions now drive component terminal geometry, wire endpoint resolution, net enumeration, terminal markers, and scene artwork. The first definition resolved for an ID remains the document's definition for subsequent placements of that ID.
- Writers emit v3 with exactly the used symbol definitions in ID order. Readers accept v1/v2/v3 and reject duplicate, invalid, missing, or unused v3 definitions.
- Headless loads preserve embedded definitions and return structured missing, presentation, and terminal conflicts. A single modal review offers only valid choices. Cancelled review leaves even dirty undo history intact.
- Embedded additions and compatible updates generate canonical SVG/sidecar sources. Explicit packaged overrides persist in the writable manifest's optional "overrides" ID array; unmarked collisions still fail. Multi-package writes stage together and restore prior files on failure. Non-file target collisions are rejected.
- The unsaved-document prompt runs only after successful package installation, before registry publication; cancellation rolls back that batch. Thus a failed library write does not change the current document's path or clean state.
- Added model tests for snapshots, conflict types, geometry isolation, malformed embedding, deterministic ordering and compatibility; package tests for restart, cancellation, and partial-batch rollback; and a window-level Qt Test suite for conflict options, dirty-state preservation, failed library writes, palette refresh, and save/reopen.

Verified on the local MSVC/Qt 6.11.1 kit:

- App and all test targets built with: cmake --build build-j1-check --config Debug --parallel 2.
- With the Qt bin directory on PATH: ctest --test-dir build-j1-check -C Debug --output-on-failure — all six suites passed.
- git diff --check passed.
- A manual visual walkthrough was attempted, but computer-use app approval timed out. The window-level tests exercise the actual dialogs offscreen; visual layout and native file-picker walkthrough remain unverified.
