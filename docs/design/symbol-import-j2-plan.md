# J2 implementation plan: headless SVG import and export

Status: **implemented.** This plan covers J2 of the [approved symbol import design](symbol-import.md). It does not authorize J3–J5.

## Goal

Convert a symbol SVG plus its JSON sidecar into a validated `LibrarySymbol`, and serialize any valid `LibrarySymbol` back to the restricted SVG profile plus sidecar. Keep the conversion headless and in `schematiccore` (Qt Core only), so J3 can reuse it without making the scene or dialog responsible for parsing SVG.

## Scope

- Parse the sidecar metadata and a local SVG reference, then convert the supported SVG profile in `symbol-import.md` into existing primitives, including J1 `Path` primitives.
- Apply nested group and element transforms, inherit `fill`/`fill-rule`/`display`, normalize artwork and terminal coordinates by the selected origin, and report ignored content as warnings.
- Parse SVG path commands M/L/H/V/C/S/Q/T/A/Z in both cases; convert smooth, quadratic, and arc commands to cubic segments, keeping arc and ellipse approximation within 0.1 scene units.
- Export a valid `LibrarySymbol` as profile SVG and sidecar data with origin `(0, 0)` and explicit styling. Preserve terminal order and offsets.
- Keep all parsing, conversion, validation, and serialization in Qt Core. Do not add UI, palette/storage management, a registry, document embedding, or complete-schematic export.

## Proposed implementation

1. Add `core/symbolsvg.h/.cpp` with importer/exporter value types. The importer accepts sidecar JSON and SVG bytes plus the source directory used to resolve the sidecar's relative SVG reference; it returns an optional validated symbol and separate warning/error lists. The exporter accepts a `LibrarySymbol` and returns SVG and sidecar bytes plus the suggested `<id>.svg` and `<id>.json` filenames. It does not write files, leaving package writes and replacement policy to J3.
2. Keep sidecar parsing separate from `SymbolLibrary::parseSymbol()`: sidecars have `svg` and `origin` instead of inline `primitives`. Reuse `validateLibrarySymbol()` on the converted result. Reject absolute, URL, and directory-escaping SVG references; validate the sidecar ID against the approved filename-safe ASCII rule.
3. Implement one recursive XML traversal that carries parent affine transform, inherited fill state, fill rule, and display state. Convert supported elements into primitives in document order. Use small Qt-Core-only helpers for affine composition, SVG number lists, path tokenization/state, and cubic arc approximation. Report unsupported elements with their names and summarize ignored unsupported style attributes.
4. Normalize coordinates after transforms by subtracting the sidecar origin from both artwork and terminals. Do not scale from `viewBox`, `width`, or `height`. Continue to use `validateLibrarySymbol()` for geometry and terminal validation; return no symbol when parsing or validation has errors.
5. Serialize primitives using supported SVG elements where straightforward (`line`, `polyline`, `polygon`, `circle`) and `<path>` for paths and any geometry that must be expressed as a path. Emit explicit fill and fill-rule values, XML-escape sidecar strings, and use stable numeric formatting that round-trips finite `qreal` values.

## Tests and acceptance

- Add the new source files to `schematiccore` and focused tests to `schematiccore_tests`; keep the target Qt Core-only.
- Add fixture-based import tests for nested groups; each transform type and transform order; inherited and overridden fill/display; all supported shape types; all SVG path commands; subpaths and fill rules; origin normalization; ignored content warnings; malformed XML, numbers, path data, sidecars, and out-of-profile references.
- Check arc and ellipse approximation against reference points with a maximum deviation of 0.1 scene units, including transformed cases and rounded rectangles.
- Export every current library symbol, import the result, and compare metadata, terminals, and primitive geometry. Compare cubic paths within the agreed 0.1-unit tolerance; compare exact line/polygon/circle values where the conversion preserves them.
- Confirm existing library loading, rendering, and core tests remain green. Resolve the currently unexplained silent exit in `schematicrenderer_tests` before treating the J1 test baseline as green.

## Decisions to keep stable

- `schematiccore` remains Qt Core only; no `QPainterPath`, GUI XML helper, or SVG rendering dependency enters the data model.
- Import uses scene units directly and applies transforms before origin subtraction; document dimensions never scale symbols.
- Import warnings do not prevent a valid symbol from being returned; malformed supported content and invalid converted symbols are errors.
- The exporter returns a pair of in-memory files; J3 owns writing, staging, and replacement of library packages.

## Review boundary

This approval covered only J2 implementation. If the path-command parser or arc conversion needs a materially different scope, pause with a revised proposal. J3 import-dialog and package-writing work remains a separate plan and approval.

## Completion

The importer and exporter are implemented in `core/symbolsvg.*`. The configured Release build and all four CTest targets pass. Import tests cover the profile commands and transform types, fill inheritance, origin normalization, warnings/errors, arc and ellipse tolerance, and export/re-import of all current symbols. The J1 renderer test's pixel assertion was corrected to check painted artwork within the actual preview bounds.
