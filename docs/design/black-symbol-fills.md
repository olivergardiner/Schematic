# Black symbol fills

Status: **approved by the user on 2026-11-06; implemented and verified.**

## Purpose and scope

Enable solid-black polygon interiors, including a possible negative capacitor plate in the [draft symbol style guide](symbol-style-guide.md), while preserving hollow/white symbol bodies and unfilled outlines.

Use the same fill representation for polygons, circles, and paths, including rectangles and ellipses converted by the SVG importer. This avoids giving black polygons a separate rendering or import path. Keep strokes black with the existing common width. Arbitrary colours, opacity, gradients, textures, general SVG styling, and a new fill-editing UI are outside this scope.

This is a presentation change. It does not alter component positions, symbol dimensions, terminal signatures, connectivity, routing, snapping, or transform behavior. `schematiccore` remains dependent on Qt Core only.

## Starting behavior and defects (resolved)

- `SymbolPrimitive` stores optional `filled` and a path fill rule. An omitted `filled` means filled for a Polygon and unfilled for other kinds.
- `RenderedSymbolPrimitive` reduces this to a boolean. Both the canvas and shared preview renderer paint filled interiors white; no black interior can survive that conversion.
- SVG import records whether fill is enabled but discards the distinction between black and white. It already handles inherited fill, group overrides, and fill rules.
- SVG export writes `fill="black"` for every filled primitive, including white-filled library shapes. Thus exported artwork can differ from its application rendering even before this feature.
- Converted library JSON is also embedded in version-3 documents. Existing symbol conflict/replacement checks compare converted definitions, so fill must participate consistently in serialization and appearance comparison.

Changing every `filled=true` shape to black would break existing resistor and op-amp bodies. Preserve their current white interiors explicitly through the compatibility rules below.

## Approved model and JSON

Retain `filled` as the fill-enable flag and add a Qt Core-only colour enum, for example `PrimitiveFillColor { White, Black }`, defaulting to White. Keep the existing independent `PathFillRule` (`nonzero`/`evenodd`). Use one shared helper for resolving the per-kind fill default so renderer, exporter, and validation cannot disagree.

Add an optional primitive JSON property:

```json
{
  "kind": "Polygon",
  "points": [[2, -10], [5, -10], [5, 10], [2, 10]],
  "filled": true,
  "fillColor": "black"
}
```

| Input/state | Effective result |
|---|---|
| Polygon with neither property | White-filled polygon, preserving legacy behavior |
| Other primitive with neither property | Unfilled, preserving legacy behavior |
| `filled: true`, no `fillColor` | White interior, preserving legacy behavior |
| `filled: false` | No fill; no background masking |
| Effective fill enabled and `fillColor: "white"` | White interior, masking artwork underneath |
| Effective fill enabled and `fillColor: "black"` | Solid black interior |

Reject unknown colours, wrong JSON types, invalid enum values, and black colour on an effectively unfilled primitive. Black fill on Line/Polyline is rejected. Legacy explicit white fill flags on those open primitive kinds remain accepted and serialized as before; compatibility tests cover this case. SVG Line/Polyline imports remain unfilled. Path filling retains its existing SVG implicit-closure behavior.

The serializer omits default White colour, preserving the existing representation for legacy symbols, and writes `fillColor: "black"` when needed. A supplied explicit White is normalized to the default so it does not create a false library conflict. Keep legacy `filled` serialization behavior and path fill rules stable. Terminal-signature comparison remains independent of fill; appearance comparison must include effective colour.

## SVG import and export

- On fresh SVG import, follow SVG fill semantics: explicit/inherited `none` means no fill, white means white fill, and black means black fill. The initial SVG fill remains black when unspecified. An element's fill overrides its inherited group fill as today.
- Support named `black`/`white`, short/long opaque hex equivalents, and the opaque RGB equivalents `rgb(0,0,0)` / `rgb(255,255,255)`, with normal whitespace/case handling. Define and test the accepted spellings rather than adding arbitrary colour support.
- Reject unsupported fill colours, paint servers, and unresolved values such as `currentColor` with a useful message. Do not silently turn them into white or black. Keep the existing policy for unrelated unsupported SVG styling; this change does not broaden it into general CSS support.
- Export effective no-fill/white/black as `fill="none"`, `fill="white"`, or `fill="black"`. Continue exporting the standard black stroke and the existing fill rule.
- Test both library → SVG → library and SVG → library → SVG. Fill colour and fill rule must survive together, including compound paths with holes.

**Legacy SVG ambiguity:** older exports wrote black even when the stored library shape rendered white. The new importer cannot distinguish such a file from an intentional black shape. Do not guess from geometry or filenames. Re-export the original stored library definition with the corrected exporter, or explicitly change its source SVG to white when appropriate. Previously converted library/document entries keep their existing white appearance because missing `fillColor` means White.

## Rendering and selection

Extend `RenderedSymbolPrimitive` to carry the resolved fill colour alongside its path and fill-enable flag. Map the Core enum to a brush in the application layer. Use one shared fill-to-brush helper in the canvas and preview painting so all of these agree:

- Canvas, including group-move candidate previews.
- Palette thumbnails.
- Import-dialog preview.
- Other previews that already consume the shared renderer.

No brush means transparent interior; White means an opaque white interior; Black means an opaque black interior. Preserve primitive order, so white bodies continue to mask lower artwork and black shapes do not unexpectedly cover later markings. Preserve path fill rules so holes remain holes.

Review item hit shapes alongside brush changes. Filled polygon interiors should be selectable; unfilled paths should retain the current selection behavior. Grid overlays, terminal markers, wire hit testing, and endpoint identity are unchanged. Inspect a white body and a black plate over a wire to verify layering and masking on the real canvas.

## Persistence and compatibility

**Approved: write project format version 4**, accepting versions 1–4 when reading. Version 4 retains the version-3 document structure and adds the new primitive fill-colour capability to embedded symbol definitions. Newly saved documents use version 4 consistently, including documents with no black fills. Older app builds reject the unsupported document version rather than ignoring the colour and displaying black shapes as white.

- Existing v1–v3 documents remain loadable and keep their legacy white-fill defaults. Saving upgrades them to v4 through the normal save path.
- No new component, terminal, node, or wire fields are needed. Update version checks and the documentation; test that unsupported later versions still fail cleanly.
- A same-ID symbol update changing only fill is an artwork update with an unchanged terminal signature. Use existing library replacement and document-conflict review paths; do not silently replace document artwork.
- Exercise writable packages, retained SVG sources, document embedding, and same-ID appearance comparison. Do not reinterpret historical stored white fills as recovered black fills.
- Standalone converted-symbol JSON currently has no independent schema/version guard. Old app builds may ignore `fillColor`; do not promise these new black-fill packages are compatible with those builds. Document the minimum supporting release. A separately versioned library-package system is a later choice, not implicitly included here.

The approved policy uses one v4 writer rather than choosing an output version based on current symbol content.

## Implementation steps

1. **Core representation and compatibility:** add the enum/default resolution, parser/serializer rules, and validation; preserve legacy white appearance and canonical representations. Add focused tests before changing rendering.
2. **SVG conversion:** preserve monochrome colour through inheritance and overrides, validate supported values, and fix export of white-filled primitives. Extend existing SVG round-trip fixtures.
3. **Shared rendering:** carry resolved fill into the canvas and every preview; check layering, fill-rule holes, selection, rotation, and flips. Add image-based assertions on interior pixels rather than only checking model flags.
4. **Document persistence:** add v4 support, keep v1–v3 loading, and verify embedded symbols, writable packages, conflict prompts, and save/load/undo snapshots containing both black and white fills.
5. **Verification and documentation:** create a small visual fixture containing a white body, black plate, outline-only shape, and compound path. Update the import design, README, decisions, roadmap, and style guide's capability note after verification.

All steps are implemented within the approved scope. Replacing the existing capacitor templates with the draft plate design is separate symbol-content work; enabling black fill does not approve the negative-plate convention or other draft style choices.

## Acceptance checks

- All current packaged symbols and legacy fixtures retain their previous white/no-fill appearance and terminal interfaces.
- Black, white, and no-fill JSON parse and serialize correctly; invalid types/colours and contradictory inputs fail without modifying the active library or document.
- Imported SVG inheritance, explicit overrides, default black, `none`, supported colour spellings, and `nonzero`/`evenodd` paths give the intended result. Unsupported paint values produce useful errors.
- SVG export renders a white-filled op-amp white and a black plate black in an external viewer, and re-import preserves those effective fills.
- Pixel checks distinguish opaque black, opaque white, and transparency in the shared renderer. Native canvas and palette/import previews agree; holes, layering, rotated/flipped artwork, and selection work correctly.
- Version-1/2/3 documents load; a v3 symbol with `filled: true` remains white; v4 embedded black fill survives save/reopen and undo/redo. Unknown future document versions are rejected.
- Library artwork replacement with an unchanged terminal signature preserves the existing ID and uses existing conflict review. Explicit White versus omitted default White does not produce a spurious appearance conflict.
- Run the existing relevant Core, renderer, package-store, placement, selection, and document-open checks. Report automated results separately from visual/manual checks, and do not mark an unperformed walkthrough complete.

## Approved principal choices

The approved choices are the additive `fillColor` enum/property with legacy White defaults, monochrome-only SVG import behavior, the corrected exporter, and the version-4 writer policy. General colour styling, package-version infrastructure, and capacitor template revisions remain separate work.

## Verification record (2026-11-06)

- Built the app and test targets using the local Visual Studio 2026 Release preset and Qt 6.11.1. Refreshed an invalid VS2022 build cache using the existing VS2026 local preset before building; no portable preset changes were needed.
- All seven Release CTest suites passed. After adding the visual fixture and a colour-only presentation-conflict assertion, rebuilt the affected tests and reran the Core and document-open suites successfully.
- Core checks cover legacy defaults, explicit-white canonicalization, invalid JSON/enum/contradictory inputs, accepted legacy open white-fill flags, SVG inheritance/overrides/default black, supported spellings, unsupported paints, and SVG round trips. Existing packaged-symbol SVG round trips now check colour too.
- Persistence checks cover a black-filled v4 embedded symbol, legacy v3 white fill, v1/v2 loading, rejection of future versions, colour-only presentation conflicts with unchanged terminals, writable package replacement/reload, and undo/redo snapshots with clean-state restoration.
- Renderer tests sample opaque black, opaque white, transparent interiors, and an evenodd hole. Canvas tests exercise a rotated/mirrored polygon over a blue line, checking white masking, black fill, no-fill transparency, and interior selection. A widget test separately checks the import preview, avoiding an assumption that all previews already use the same painter.
- [Visual fixture SVG](../../tests/fixtures/monochrome-fill.svg) and [sidecar](../../tests/fixtures/monochrome-fill.json) exercise a white body, black plate, outline-only body, and compound-path hole over lower artwork. The native Windows import-preview check passed. Visually inspected its captured rendering and the canvas capture; full interactive manual application walkthrough is not claimed.
- No released symbol geometry, terminal signatures, or capacitor templates were changed. Existing template archive/previews remain as previously delivered.
