# Chunk J design note: SVG-based symbol authoring

Status: **revised draft, awaiting approval.** No implementation starts until this note is approved (see AGENTS.md). Once approved, record settled points in DECISIONS.md and keep ROADMAP.md pointing here.

## 1. Purpose and scope

The current symbol library has seven entries. The library grows through a standard authoring path: draw the glyph in an external SVG editor, import it, add the required metadata and connection points, then save it as a validated library entry. All symbols use the same definition format, validation, lookup, and document rules; where an entry is distributed or stored does not give it a different kind.

In scope for chunk J:

- A restricted "symbol SVG profile" and an importer for it.
- A sidecar JSON file that carries metadata and terminals and references the SVG.
- A new `Path` primitive so curved artwork can be stored.
- A symbol exporter (library symbol back to profile SVG plus sidecar).
- An import dialog with a preview and click-to-place terminals.
- A library registry that combines packaged collections and the writable library using the same entry schema and management path.
- Embedding the definitions of all symbols a document uses (format v3) and a conflict prompt on open.

Out of scope: a general-purpose drawing editor, arbitrary SVG support, text inside symbols, per-symbol styling, complete-schematic export to SVG/PNG/PDF (a separate later feature), signal ports (deferred E3), and individual symbol content (E1/E2), which is created with this tooling afterwards.

## 2. Guiding principles

1. **Terminals are the contract; the glyph is presentation.** Saved wires refer to terminals by index and route endpoints derive from terminal offsets. Nothing in `Document`, `Component` or the file format reads artwork. So the importer converts the glyph faithfully and validates the terminals strictly.
2. **Stored symbols are self-contained.** After import the original SVG is never needed. The importer's output is a normal `LibrarySymbol` (primitives plus terminals).
3. **The symbol must not care how the SVG was drawn.** Groups, layers, transforms and unit conventions are resolved at import and leave no trace in the stored symbol.
4. **`schematiccore` stays Qt Core only.** The importer and exporter use `QXmlStreamReader`/`QXmlStreamWriter` and pure math, so they are headless and testable like the rest of the model.
5. **Terminal compatibility is the identity contract.** A released symbol's terminal count, order, names, and positions never change; any change to them needs a new ID. Glyph artwork and presentation metadata may change under the same ID because they do not alter electrical connectivity. Symbol IDs are unique across the active library; duplicate IDs are reported as conflicts rather than shadowing another entry.

## 3. Sidecar format

The sidecar is a library entry whose artwork is an `svg` reference instead of inline `primitives`:

```json
{
  "id": "NJFET",
  "name": "N-Channel JFET",
  "category": "Semiconductors/Transistors/JFET",
  "referencePrefix": "Q",
  "showsReference": true,
  "svg": "njfet.svg",
  "origin": { "x": 0, "y": 0 },
  "terminals": [
    { "name": "1", "x": -20, "y": 0 },
    { "name": "2", "x": 10, "y": -20 },
    { "name": "3", "x": 10, "y": 20 }
  ]
}
```

- `svg` is a local path relative to the sidecar file. Network URLs and absolute paths are not accepted. Exactly one of `svg` or `primitives` must be present.
- `id` is a stable string and also determines filenames in library storage. It must contain only ASCII letters, digits, `.`, `_`, or `-`, and start with a letter or digit.
- `origin` is an `{ "x": number, "y": number }` coordinate in root SVG user space; an omitted origin defaults to `{ "x": 0, "y": 0 }`. The import dialog displays this default and lets the user change it. Artwork and terminals are both translated relative to it.
- Terminals live only in the sidecar, in root SVG user-unit coordinates. The SVG itself carries no terminal markers.
- Terminal `name` is optional on input and defaults to the 1-based index (`"1"`, `"2"`, ...). Array order is the terminal index and is explicit.
- All existing `validateLibrarySymbol()` rules apply to the converted result: non-empty unique names, unique finite offsets, non-empty prefix, valid category path.
- After a successful import the app stores a **converted entry** (inline `primitives`, no `svg` reference) in the symbol library. That converted entry is what the app loads and what documents embed.

## 4. Symbol SVG profile v1

### 4.1 Coordinates

- Coordinates are the SVG's **user units**, with one SVG user unit mapping to one scene unit and the y axis pointing down. Element transforms are applied first, then the selected sidecar origin is subtracted from all artwork and terminals so the symbol's local origin is `(0, 0)`. There is no scaling or flipping. The reference authoring workflow is to export plain SVG from Inkscape; fixtures should use files produced by that workflow.
- `viewBox` is only a canvas. `width`, `height` and any physical units are ignored: they set the size the SVG draws at on a page and never change geometry. With no `viewBox`, user units are used as they are.

### 4.2 Supported elements

| Element | Stored as |
|---|---|
| `line` | `Line` |
| `polyline` | `Polyline` |
| `polygon`, `rect` | `Polygon` (a `rect` with rounded corners becomes a `Path`) |
| `circle` | `Circle` (becomes a `Path` if the transform makes it non-circular) |
| `ellipse`, `path` | `Path` |
| `g` | grouping only; carries `transform`, `fill` and `display` |

Path data accepts M, L, H, V, C, S, Q, T, A and Z, absolute and relative. Everything is normalised to move/line/cubic/close segments on import. Quadratics convert exactly to cubics. SVG arcs and ellipses are approximated by cubic segments within a maximum geometric error of 0.1 scene units.

### 4.3 Transforms are fully applied

`transform` is an attribute (not an element) that any element may carry. The importer composes `matrix`, `translate`, `scale`, `rotate` (with optional centre), `skewX` and `skewY` down every level of `<g>` nesting using SVG's defined transform-list and nesting order, then applies the result to each shape's points. Affine transforms are applied exactly to supported points and cubic segments; arc and ellipse approximations remain within the stated tolerance. Where a transform changes a shape's kind (a non-uniformly scaled circle, a rounded rect), it is stored as a `Path`. No transform survives into the stored symbol.

### 4.4 Out-of-profile content is ignored with a warning

Elements and features outside the profile are **ignored, not errors**, and each produces a warning naming it: `text`, `image`, `use`, `defs`, `style` elements and `class` attributes, gradients, filters, clip paths, masks, markers, nested `svg`, and any other unknown element. Unknown attributes, stroke width, colour, dash, opacity and similar style are ignored, with one summary warning rather than one per attribute. The renderer owns line weight and colour so every symbol looks consistent.

Consequences the warnings must make clear:

- An ignored `use` or `defs` means the referenced shapes are missing from the symbol.
- An ignored clip path or mask means the clipped shape imports unclipped.
- Fill driven by a `class` or `<style>` rule is not seen (see 4.5).

Elements with `display="none"` are skipped silently.

### 4.5 Fill

Fill and fill rule are the only honoured styles. They are read from presentation attributes or the matching properties in an inline `style` attribute, and inherited from `<g>` ancestors. `fill="none"` means open. Anything else on a closed shape, including no fill at all (the SVG default is black), means a filled body, drawn white by the renderer. Line and polyline are never filled. `fill-rule` supports SVG's `nonzero` and `evenodd` values, defaults to `nonzero`, and applies to compound paths. Class-based and stylesheet rules remain unsupported and produce the warnings above.

### 4.6 Errors

These stop the import: not well-formed XML; unreadable or invalid sidecar; no drawable primitive left after ignoring out-of-profile content; malformed numbers or path data in an in-profile element (the error names the element); non-finite coordinates; any converted-symbol validation failure, including bad terminals.

## 5. Symbol data model changes (J1)

- New primitive kind `Path`: an ordered list of segments, each `MoveTo`, `LineTo`, `CubicTo` or `Close`. Proposed JSON: `{ "kind": "Path", "filled": false, "fillRule": "nonzero", "segments": [["M", x, y], ["L", x, y], ["C", x1, y1, x2, y2, x, y], ["Z"]] }`. The exact JSON shape is settled in J1.
- New optional `filled` flag per primitive. Defaults preserve today's behaviour: `Polygon` is filled by default, `Line`, `Polyline`, `Circle` and `Path` are not. No existing symbol file changes.
- `Path` stores a fill rule (`nonzero` or `evenodd`); legacy and non-Path primitives default to `nonzero`.
- Bounds for label placement: the core cannot call `QPainterPath`, so bounds for cubic segments use the exact curve extrema, not the control-point hull.
- `validateLibrarySymbol()` additions: at least one segment, a leading `MoveTo`, finite coordinates, correct segment arity.
- The scene maps each segment to `QPainterPath` directly and fills when `filled`. Existing symbols render identically.

## 6. Terminals and placement

- The sidecar's origin and terminal list are the single source of truth. The import dialog (section 9) is the main way to author them, by clicking on the preview or entering coordinates.
- Terminal placement snaps to the default 10-unit grid, with a modifier to place freely. Terminals do not have to be on the grid (world-coordinate contract in DECISIONS.md); being off the grid or away from the artwork only produces warnings.
- Warnings (never blocking): a terminal is off the 10-unit grid; a terminal is not within a small tolerance (proposed 2 units) of any primitive's geometry.
- Terminal order is placement order, editable before the first save. Once a symbol has been saved to the library it is treated as released: terminals are frozen (section 7).

## 7. Library registry and identity rules

- Every symbol is the same kind of library entry, with the same schema, validation, palette behavior, import/export workflow, and identity rules. Packaged and writable collections are storage/distribution sources only; they do not create separate symbol types.
- The writable collection lives under `QStandardPaths::AppDataLocation/symbol-library/`, with a `manifest.json` and one converted JSON entry per symbol. Its `sources/` subdirectory retains the original SVG and metadata sidecar under the validated symbol ID. This lets an entry be re-imported without depending on the original file location.
- **Registry refactor.** The current `builtinSymbolLibrary()` function is called directly from `component.cpp`, `document.cpp`, `documentjson.cpp`, `schematicscene.cpp`, `mainwindow.cpp` and the tests. Replace it with one symbol-library registry that loads the same entry format from the configured library sources. If two collections contain the same ID, report a conflict; no source silently shadows another. An explicit replacement can update the writable collection only when the terminal signature is unchanged. This is the largest mechanical change in J and is done on its own with all tests green before anything else relies on it.
- **Failure policy.** Invalid entries are skipped with a reported error; other valid entries remain available. A malformed manifest or unreadable collection is reported without silently replacing the active registry with a partial collection.
- **Re-import rule.** Re-importing any existing ID is allowed only if the terminal signature (count, order, names, positions) is unchanged, so artwork and presentation metadata can be refined but the symbol's electrical interface cannot change. Any terminal-signature change requires a new ID. This is enforced at import time.

## 8. Documents, embedding and conflicts (J5)

### 8.1 Embedding (format v3)

- Format v3 adds a `symbols` array holding the full converted entry of every symbol the document uses. This preserves the artwork and metadata used when the document was saved, even if the shared library later changes. The loader accepts v1, v2 and v3; writers emit v3 (an empty `symbols` array when none are used). Saving a loaded v1/v2 document upgrades it, as before; those earlier formats have no embedded symbol definitions and use the current library entries.
- Embedded entries are validated like library entries; an invalid embedded entry rejects the document with an error naming the ID. A document may not contain duplicate symbol IDs.

### 8.2 Per-document symbol set

Each `Document` resolves symbols from its own set: its embedded entries layered over the registry. `Component::terminalPosition()` and every other symbol lookup must therefore go through the document's set rather than a global. This is the largest structural change in J and J5 begins with a short implementation plan for it.

### 8.3 Conflicts on open

`Document::fromJson()` stays headless and reports conflicts in `DocumentLoadResult`. `MainWindow` then asks the user.

| Situation | Behaviour |
|---|---|
| Same ID, **same terminal signature**, different artwork or presentation metadata | Prompt: use the document's version in this document only; update the library entry to the document's version; keep the library's version. All are safe for connectivity because the terminal signature is unchanged. |
| Same ID, **different terminals** | Prompt: use the document's version in this document only, or cancel. Updating the library is not offered, because it would break other documents and violates the freeze rule. |
| ID not in the active library | Load from the embedded copy and offer to add it to the library. |
| Headless load with no prompt (tests, tooling) | The document's embedded definition wins for that document. |

## 9. Import dialog (J3)

`File > Import Symbol...`: choose a sidecar or a bare SVG, then a preview built with F's shared renderer.

- The preview shows the converted artwork, the current origin and terminals, and lists errors and warnings (including everything the importer ignored). The origin can be set in the preview or numerically; importing subtracts it from both artwork and terminal coordinates.
- Click to add the next terminal; drag to move; Delete to remove; names and order are editable until the first save.
- Metadata fields (ID, name, category, reference prefix, show reference) are filled from the sidecar and editable. A bare SVG gets blank fields to complete.
- On confirm: copy the source SVG and sidecar to `sources/<id>.svg` and `sources/<id>.json` in the writable library. Write the final dialog metadata, origin and terminal list into the copied sidecar, set its `svg` path to `<id>.svg`, write the converted JSON entry as `<id>.json` beside the manifest, and refresh the palette. Never modify the selected source files. Stage and validate all outputs before replacing an existing package; ask before replacing an entry with the same ID. Same-ID replacement is allowed only with an unchanged terminal signature. Library changes are not undoable (H covers document edits only).

## 10. Symbol exporter (J2)

The exporter writes any `LibrarySymbol` as a profile SVG plus its sidecar, with origin `(0, 0)` and terminals in SVG user coordinates. The SVG is a plain stroked and filled drawing so it looks sensible when opened in an editor; the importer ignores that styling. This exports an individual symbol, not a complete schematic. Uses: round-trip tests (export a library entry, re-import, compare), and a starting point for authoring a new symbol.

## 11. Sub-steps and acceptance checks

Each step needs the user's approval before it starts.

- **J1: Path primitive.** `Path` kind, `filled` flag and fill rule, validation and bounds, scene rendering, library JSON parsing. Done when all existing symbols render and load unchanged, and focused tests cover segment validation, bounds, fill-rule behavior, and `filled` defaults.
- **J2: Importer and exporter (headless).** SVG parsing per the profile, origin normalization, full transform application, sidecar parsing, warning/error reporting, and the individual-symbol exporter. Done when fixture SVGs from the stated Inkscape workflow (nested groups, each transform type, arcs within tolerance, ellipses, rounded rects, fill rules, ignored elements, malformed input) produce expected primitives, and every current library entry survives export then re-import with equivalent primitives and terminals within the curve tolerance. If this step proves too large, the exporter becomes J2b.
- **J3: Import dialog.** Preview, origin selection, click-to-place terminals, metadata editing, warnings, and safe writing of the symbol package. Done when a manual walkthrough covers importing a new symbol, setting the origin, placing and reordering terminals, and seeing warnings for an off-grid terminal and an ignored element.
- **J4: Registry and library storage.** Single symbol lookup, common entry format across library sources, per-entry failure policy, palette refresh, and re-import freeze rule. Done when all existing tests pass against the new lookup, an invalid entry is skipped without blocking valid entries, duplicate IDs are reported, and re-importing with changed terminals is rejected.
- **J5: Embedding and conflicts.** Per-document symbol set, format v3, conflict reporting and the prompts in 8.3. Done when model tests cover v1/v2/v3 loading, embedded-symbol validation, each conflict row, and v3 round trips, and the prompts are checked manually.

## 12. Dependencies

J3 needs F's shared renderer (the artwork-source seam) and H's edit-commit path for placing imported symbols. J4 must not start until J2 and J3 are accepted.

## 13. Decisions recorded from review

| Question | Decision |
|---|---|
| Physical `width`/`height` units | Ignored |
| `transform` | Fully applied; the symbol does not depend on how the SVG was drawn |
| Symbol origin | Preview/numeric selection; normalize artwork and terminals to `(0, 0)` |
| Arc and ellipse conversion | Cubic approximation within 0.1 scene units |
| Symbol identity | Terminal count, order, names and positions are immutable under an ID; artwork and presentation metadata may change |
| Compound path fill | Preserve SVG `nonzero` and `evenodd` fill rules |
| Default terminal names | 1-based index |
| Terminal placement | Click-to-place in the import dialog, early (J3) |
| Metadata | Sidecar JSON referencing the SVG; terminals live in the sidecar |
| Embedding | Embed every used symbol definition, prompt on conflict |
| Out-of-profile SVG content | Ignored with a warning, not an error |
| Symbol exporter | Included in J2 |
| Complete-schematic SVG export | Required later, separate from J; PNG/PDF remain possible future formats |
