# Chunk J design note: user-authored symbols via SVG import

Status: **draft, awaiting approval.** No implementation starts until this note is approved (see AGENTS.md). Once approved, record settled points in DECISIONS.md and keep ROADMAP.md pointing here.

## 1. Purpose and scope

The built-in library has seven symbols. Rather than hand-writing more library entries, the library grows through import: the user draws a symbol as plain SVG in any tool, marks its connection points, and imports it. After import the symbol is an ordinary library entry.

In scope for chunk J:

- A restricted "symbol SVG profile" and an importer for it.
- A sidecar JSON file that carries metadata and terminals and references the SVG.
- A new `Path` primitive so curved artwork can be stored.
- A symbol exporter (library symbol back to profile SVG plus sidecar).
- An import dialog with a preview and click-to-place terminals.
- A user library directory and a registry that serves built-in plus user symbols.
- Embedding the symbol definitions a document uses (format v3) and a conflict prompt on open.

Out of scope: a drawing editor, arbitrary SVG support, text inside symbols, per-symbol styling, document export to SVG/PNG/PDF (a separate later item), signal ports (deferred E3), and individual symbol content (E1/E2), which is created with this tooling afterwards.

## 2. Guiding principles

1. **Terminals are the contract; the glyph is presentation.** Saved wires refer to terminals by index and route endpoints derive from terminal offsets. Nothing in `Document`, `Component` or the file format reads artwork. So the importer converts the glyph faithfully and validates the terminals strictly.
2. **Stored symbols are self-contained.** After import the original SVG is never needed. The importer's output is a normal `LibrarySymbol` (primitives plus terminals).
3. **The symbol must not care how the SVG was drawn.** Groups, layers, transforms and unit conventions are resolved at import and leave no trace in the stored symbol.
4. **`schematiccore` stays Qt Core only.** The importer and exporter use `QXmlStreamReader`/`QXmlStreamWriter` and pure math, so they are headless and testable like the rest of the model.
5. **Existing compatibility rules still apply.** A released symbol's terminal order and positions never change; changed terminals need a new ID. Built-in IDs cannot be shadowed.

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
  "terminals": [
    { "name": "1", "x": -20, "y": 0 },
    { "name": "2", "x": 10, "y": -20 },
    { "name": "3", "x": 10, "y": 20 }
  ]
}
```

- `svg` is a path relative to the sidecar file. Exactly one of `svg` or `primitives` must be present.
- Terminals live only in the sidecar, in the SVG's user-unit coordinates. The SVG itself carries no terminal markers.
- Terminal `name` is optional on input and defaults to the 1-based index (`"1"`, `"2"`, ...), matching the built-ins. Array order is the terminal index and is explicit.
- All existing `validateLibrarySymbol()` rules apply to the converted result: non-empty unique names, unique finite offsets, non-empty prefix, valid category path.
- After a successful import the app stores a **converted entry** (inline `primitives`, no `svg` reference) in the user library. That converted entry is what the app loads at startup and what documents embed.

## 4. Symbol SVG profile v1

### 4.1 Coordinates

- Coordinates are the SVG's **user units**, which are scene units. The origin is the symbol's origin at rotation 0, and the y axis points down, as in the scene. The importer does no re-centring, scaling or flipping.
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

Path data accepts M, L, H, V, C, S, Q, T, A and Z, absolute and relative. Everything is normalised to move/line/cubic/close segments on import: arcs, quadratics and ellipses become cubic segments, not straight-line approximations.

### 4.3 Transforms are fully applied

`transform` is an attribute (not an element) that any element may carry. The importer composes `matrix`, `translate`, `scale`, `rotate` (with optional centre), `skewX` and `skewY` down every level of `<g>` nesting and applies the result to each shape's points. Cubic segments map exactly under any affine transform, so the result is exact. Where a transform changes a shape's kind (a non-uniformly scaled circle, a rounded rect), it is stored as a `Path`. No transform survives into the stored symbol.

### 4.4 Out-of-profile content is ignored with a warning

Elements and features outside the profile are **ignored, not errors**, and each produces a warning naming it: `text`, `image`, `use`, `defs`, `style` elements and `class` attributes, gradients, filters, clip paths, masks, markers, nested `svg`, and any other unknown element. Unknown attributes, stroke width, colour, dash, opacity and similar style are ignored, with one summary warning rather than one per attribute. The renderer owns line weight and colour so every symbol looks consistent.

Consequences the warnings must make clear:

- An ignored `use` or `defs` means the referenced shapes are missing from the symbol.
- An ignored clip path or mask means the clipped shape imports unclipped.
- Fill driven by a `class` or `<style>` rule is not seen (see 4.5).

Elements with `display="none"` are skipped silently.

### 4.5 Fill

Only fill is honoured as style. It is read from the `fill` attribute or the `fill` property of an inline `style` attribute, and inherited from `<g>` ancestors. `fill="none"` means open. Anything else on a closed shape, including no fill at all (the SVG default is black), means a filled body, drawn white by the renderer like the built-in resistor and op-amp. Line and polyline are never filled. This matches the renderer's existing behaviour.

### 4.6 Errors

These stop the import: not well-formed XML; unreadable or invalid sidecar; no drawable primitive left after ignoring out-of-profile content; malformed numbers or path data in an in-profile element (the error names the element); non-finite coordinates; any converted-symbol validation failure, including bad terminals.

## 5. Symbol data model changes (J1)

- New primitive kind `Path`: an ordered list of segments, each `MoveTo`, `LineTo`, `CubicTo` or `Close`. Proposed JSON: `{ "kind": "Path", "filled": false, "segments": [["M", x, y], ["L", x, y], ["C", x1, y1, x2, y2, x, y], ["Z"]] }`. The exact JSON shape is settled in J1.
- New optional `filled` flag per primitive. Defaults preserve today's behaviour: `Polygon` is filled by default, `Line`, `Polyline`, `Circle` and `Path` are not. No existing symbol file changes.
- Bounds for label placement: the core cannot call `QPainterPath`, so bounds for cubic segments use the exact curve extrema, not the control-point hull.
- `validateLibrarySymbol()` additions: at least one segment, a leading `MoveTo`, finite coordinates, correct segment arity.
- The scene maps each segment to `QPainterPath` directly and fills when `filled`. Existing symbols render identically.

## 6. Terminals and placement

- The sidecar's terminal list is the single source of truth. The import dialog (section 9) is the main way to author it, by clicking on the preview.
- Terminal placement snaps to the default 10-unit grid, with a modifier to place freely. Terminals do not have to be on the grid (world-coordinate contract in DECISIONS.md); being off the grid or away from the artwork only produces warnings.
- Warnings (never blocking): a terminal is off the 10-unit grid; a terminal is not within a small tolerance (proposed 2 units) of any primitive's geometry.
- Terminal order is placement order, editable before the first save. Once a symbol has been saved to the user library it is treated as released: terminals are frozen (section 7).

## 7. User library, registry and identity rules

- User symbols are stored as converted entries in a per-user directory under `QStandardPaths::AppDataLocation`, in the same layout `SymbolLibrary::loadFromDirectory()` reads (a `manifest.json` plus entry files). The authoring sidecar and SVG are kept beside them so a symbol can be re-imported.
- **Registry refactor.** `builtinSymbolLibrary()` is called directly from `component.cpp`, `document.cpp`, `documentjson.cpp`, `schematicscene.cpp`, `mainwindow.cpp` and the tests. A single symbol lookup replaces those calls, serving built-in plus user symbols. This is the largest mechanical change in J and is done on its own with all tests green before anything else relies on it.
- **Built-in IDs cannot be shadowed** by user or embedded symbols.
- **Failure policy.** A bad built-in entry stays fatal. A bad user entry is skipped with a reported error and never prevents the app starting.
- **Re-import rule.** Re-importing an existing user ID is allowed only if the terminal list (order, names, positions) is unchanged, so artwork can be refined but connection points cannot move. Changed terminals require a new ID. This is enforced at import time.

## 8. Documents, embedding and conflicts (J5)

### 8.1 Embedding (format v3)

- Format v3 adds a `symbols` array holding the full converted entry of every **user** symbol the document uses. Built-in symbols are never embedded. The loader accepts v1, v2 and v3; writers emit v3 (an empty `symbols` array when none are used). Saving a loaded v1/v2 document upgrades it, as before.
- An embedded entry whose ID matches a built-in is a load error. Embedded entries are validated like library entries; an invalid embedded entry rejects the document with an error naming the ID.

### 8.2 Per-document symbol set

Each `Document` resolves symbols from its own set: its embedded entries layered over the registry. `Component::terminalPosition()` and every other symbol lookup must therefore go through the document's set rather than a global. This is the largest structural change in J and J5 begins with a short implementation plan for it.

### 8.3 Conflicts on open

`Document::fromJson()` stays headless and reports conflicts in `DocumentLoadResult`. `MainWindow` then asks the user.

| Situation | Behaviour |
|---|---|
| Same ID, **same terminals**, different artwork | Prompt: use the document's version in this document only; update my library to the document's version; keep my library's version. All are safe because artwork never affects saved wires. |
| Same ID, **different terminals** | Prompt: use the document's version in this document only, or cancel. Updating the library is not offered, because it would break other documents and violates the freeze rule. |
| ID not in my library | Load from the embedded copy and offer to add it to my library. |
| Headless load with no prompt (tests, tooling) | The document's embedded definition wins for that document. |

## 9. Import dialog (J3)

`File > Import Symbol...`: choose a sidecar or a bare SVG, then a preview built with F's shared renderer.

- The preview shows the converted artwork with terminals overlaid, and lists errors and warnings (including everything the importer ignored).
- Click to add the next terminal; drag to move; Delete to remove; names and order are editable until the first save.
- Metadata fields (ID, name, category, reference prefix, show reference) are filled from the sidecar and editable. A bare SVG gets blank fields to complete.
- On confirm: write the sidecar and SVG to the authoring area, write the converted entry to the user library, and refresh the palette. Library changes are not undoable (H covers document edits only).

## 10. Symbol exporter (J2)

The exporter writes any `LibrarySymbol` as a profile SVG plus its sidecar, with terminals in SVG coordinates. The SVG is a plain stroked and filled drawing so it looks sensible when opened in an editor; the importer ignores that styling. Uses: round-trip tests (export a built-in, re-import, compare), and a starting point for authoring a new symbol.

## 11. Sub-steps and acceptance checks

Each step needs the user's approval before it starts.

- **J1: Path primitive.** `Path` kind, `filled` flag, validation and bounds, scene rendering, library JSON parsing. Done when the existing seven symbols render and load unchanged, and focused tests cover segment validation, bounds, and `filled` defaults.
- **J2: Importer and exporter (headless).** SVG parsing per the profile, full transform application, sidecar parsing, warning/error reporting, and the exporter. Done when hand-written fixture SVGs (nested groups, each transform type, arcs, ellipses, rounded rects, ignored elements, malformed input) produce hand-computed primitives, and each built-in survives export then re-import with identical primitives and terminals. If this step proves too large, the exporter becomes J2b.
- **J3: Import dialog.** Preview, click-to-place terminals, metadata editing, warnings, writing the user entry. Done when a manual walkthrough covers importing a new symbol, placing and reordering terminals, and seeing warnings for an off-grid terminal and an ignored element.
- **J4: Registry and user library.** Single symbol lookup, user library directory, per-entry failure policy, palette refresh, re-import freeze rule. Done when all existing tests pass against the new lookup, a bad user entry is skipped without stopping startup, and re-importing with changed terminals is rejected.
- **J5: Embedding and conflicts.** Per-document symbol set, format v3, conflict reporting and the prompts in 8.3. Done when model tests cover v1/v2/v3 loading, embedded-symbol validation, each conflict row, and v3 round trips, and the prompts are checked manually.

## 12. Dependencies

J3 needs F's shared renderer (the artwork-source seam) and H's edit-commit path for placing imported symbols. J4 must not start until J2 and J3 are accepted.

## 13. Decisions recorded from review

| Question | Decision |
|---|---|
| Physical `width`/`height` units | Ignored |
| `transform` | Fully applied; the symbol does not depend on how the SVG was drawn |
| Default terminal names | 1-based index |
| Terminal placement | Click-to-place in the import dialog, early (J3) |
| Metadata | Sidecar JSON referencing the SVG; terminals live in the sidecar |
| Embedding | Option C: embed used user symbols, prompt on conflict |
| Out-of-profile SVG content | Ignored with a warning, not an error |
| Symbol exporter | Included in J2 |
| Document export (SVG/PNG/PDF) | Deferred, separate from J |
