# Built-in symbol template refresh

Status: **approved by the user on 2026-11-06; implemented and verified.** The user also confirmed that no existing documents would be compromised; the approved mapping still retains the original op-amp interface.

## Requested result

Ship the four current [Grid 10 templates](../../symbol-templates/grid10/README.md) as the standard built-in resistor, capacitor, electrolytic capacitor, and op-amp. Use the reviewed black capacitor plates and short op-amp supply leads. Users should not need to import these files to obtain the new artwork.

## Approved mapping and compatibility

| Built-in entry | Proposed change |
|---|---|
| `Resistor` | Replace artwork from `resistor.svg`; keep the existing ID, name, category, prefix, and terminal signature. |
| `Capacitor` | Replace artwork from `capacitor.svg` with two black box plates; keep the existing ID, name, category, prefix, and terminal signature. |
| New `ElectrolyticCapacitor` | Add the electrolytic template with hollow positive and black negative plates, plus its positive mark. Prefix `C`, category `Passives`, ordered terminals `+` at (-20, 0), `-` at (20, 0). |
| New `OpAmp.Grid10` | Add the short-supply template as the standard **Op-Amp** in `Semiconductors/ICs`, prefix `U`. Ordered terminals: `in+` (-30, -10), `in-` (-30, 10), `out` (30, 0), `v+` (0, -20), `v-` (0, 20). |
| Existing `OpAmp` | Keep its original artwork and terminal signature for old documents. Rename its presentation to **Op-Amp (Legacy)** and place it in `Compatibility/Legacy Symbols`, making the grid-aligned entry the normal choice. |

The built-in library therefore has nine entries: the original seven identities, one new electrolytic, and one new op-amp. The legacy op-amp remains available through the same registry and palette category machinery; no hidden-symbol metadata or separate legacy loader is introduced.

The old `OpAmp` terminals are (-28, -8), (-28, 8), (28, 0), (0, -16), and (0, 16). They must not be changed under that ID. Merely substituting the new template under `OpAmp` would move connection endpoints in older documents and violate the released terminal contract.

New built-in IDs are distinct from the existing `Grid10.*` template IDs. Already imported template packages can remain installed without creating a packaged/writable duplicate-ID conflict at startup. Their visual duplicates are not automatically removed or renamed. Existing same-ID writable overrides for `Resistor`/`Capacitor` continue to follow the established override policy.

Versions 1/2 resolve the retained legacy IDs and keep their original terminal geometry. Versions 3/4 use their embedded symbol definitions and existing presentation-conflict review; changing the shared library does not silently redraw existing documents. The legacy op-amp name/category change may produce a presentation-only conflict on opening an embedded old definition; use the current review flow. No document-format or migration change is needed.

## Implementation

1. Convert the four SVG/sidecar pairs with the actual `SymbolSvgImporter`, then write ordinary converted primitive JSON for the packaged entries. Apply the mapping above rather than editing terminals by hand. Preserve the approved template geometry, fills, and primitive order.
2. Update `core/symbols/manifest.json`, the CMake resource list, and the new symbol-ID constants. Place the new electrolytic beside the capacitor and the new op-amp in the normal op-amp position. Keep the legacy entry in the manifest and resolve its palette location from category metadata.
3. Extend the existing library tests for metadata/order, all frozen terminal signatures, new black/white fills, and exact imported-template artwork equivalence. Preserve the old op-amp terminal tests and pre-library document fixtures. Update assumptions that the packaged collection contains seven entries, including palette tests affected by the additional passive.
4. Verify placement, connections, group transforms, undo/redo, and save/load for the new entries. Check that old op-amp files retain their original terminal coordinates and imported `Grid10.*` packages coexist with the new entries.
5. Update the README, decisions, roadmap, and template/style-guide references after verification. Rebuild the Release app and run the affected existing suites; visually inspect the new palette and rendered symbols, reporting automated checks separately from any manual walkthrough.

## Scope boundaries

All entries remain ordinary library data served by the same validation, lookup, and management path. The model stays Qt Core-only. Symbol geometry remains in scene units, grid spacing does not resize it, and explicit terminal/node identity continues to define connectivity. No new routing, rendering, selection, file-format, or palette infrastructure is included. Diode, ground, potentiometer, and jack artwork are unchanged.

## Acceptance checks

- A fresh app offers the reviewed resistor, black-plate capacitor, polarised capacitor, and short-supply op-amp without a manual import.
- The resistor and capacitor signatures are unchanged; both new symbols have their documented grid-aligned signatures. Each passive spans 40 units tip to tip.
- The new op-amp's input/output span is 60 units and supply span is 40; the old `OpAmp` still resolves to its frozen terminal coordinates.
- Packaged artwork matches the SVG templates after conversion, including hollow versus black interiors and polarity/input signs.
- Pre-library files keep connected op-amp endpoints; embedded documents retain their snapshots and use existing conflict review.
- Previously imported template IDs do not cause new duplicate-ID startup errors. No user library files are modified by the refresh.
- New entries place, connect, rotate/flip, and round-trip through undo and save/load using the existing editing paths. Relevant Core, renderer, storage, placement, selection, and document-open checks pass.

## Approved compatibility choice

Use the new `OpAmp.Grid10` identity as the normal op-amp and retain `OpAmp` under the visible Compatibility category. This protects the released terminal interface while making the new templates the standard choices.

## Verification record (2026-11-06)

- Converted the four source SVG/sidecar pairs with the actual importer, writing ordinary packaged primitive JSON and applying only the approved ID/name/category mapping. Updated the manifest, resource list, ID constants, and metadata/terminal/artwork tests.
- The Core suite compares every converted packaged definition against its source template, verifies both capacitor fill patterns, and adds all four imported `Grid10.*` IDs to a copy of the packaged registry without conflicts. Wiring, shared movement, rotation/flip, and embedded-document round trips pass for each refreshed entry.
- New-entry placement tests cover snapped placement, connected wires, undo and redo. Existing original-op-amp terminal and pre-library document checks remain in place and pass. Updated the passive palette layout check from three to four tiles.
- Built the Release app and all test executables with the local Visual Studio 2026 / Qt 6.11.1 kit. All seven Release CTest suites passed. Visually inspected captured passive and IC palettes; no full interactive manual walkthrough is claimed.
- The palette layout check also passed separately on the native Windows backend, in addition to the offscreen window-suite run.
- User library files and existing document contents were not modified. The authoring templates keep their existing `Grid10.*` identities; built-in definitions use the mapping above.
