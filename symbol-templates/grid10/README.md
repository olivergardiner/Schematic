# Grid 10 symbol templates

Four original, matching SVG symbols with terminals on 10-unit offsets from a centered origin. Each SVG user unit equals one scene unit. All drawings use a 1-unit black stroke; bodies and polarity marks may use finer coordinates between terminals.

## Import

1. Keep each SVG beside its matching JSON file.
2. In Schematic, choose **File > Import Symbol...** and open the **JSON file**, which supplies the artwork path, metadata, origin, and exact terminal positions.
3. Review the preview and save to the library. Repeat for the other JSON files.

These four artworks now ship in the built-in library as `Resistor`, `Capacitor`, `ElectrolyticCapacitor`, and `OpAmp.Grid10`; manual import is not needed for standard use. This folder remains an editable authoring source. Its `Grid10.*` sidecar IDs are deliberately distinct from the packaged IDs, so manually imported templates can coexist with the built-ins. Importing only the SVG is also possible, but requires entering its metadata and terminals yourself.

The current importer may report `ignored unsupported SVG style or attribute details` for the stroke attributes that make these SVGs display correctly in external viewers. The application applies its own matching 1-unit black stroke; this warning does not indicate lost artwork or terminal data.

## Dimensions and terminals

Coordinates below are relative to origin `(0, 0)`; positive Y points down. At component positions on a 10-unit grid, every terminal remains on that grid through right-angle rotations and flips. Changing the document's grid spacing does not resize symbols.

| Pair | Artwork convention | Ordered terminals |
|---|---|---|
| `resistor.svg` / `resistor.json` | Rectangular resistor; 20 × 10 body, 40 tip-to-tip | `1`: (-20, 0); `2`: (20, 0) |
| `capacitor.svg` / `capacitor.json` | Two black-filled box plates, each 3 × 20 with a 4-unit gap; 40 tip-to-tip | `1`: (-20, 0); `2`: (20, 0) |
| `electrolytic-capacitor.svg` / `electrolytic-capacitor.json` | Same box geometry; hollow positive plate, black negative plate, positive mark | `+`: (-20, 0); `-`: (20, 0) |
| `op-amp.svg` / `op-amp.json` | 40 × 40 triangle; 60 horizontal span, 40 supply span | `in+`: (-30, -10); `in-`: (-30, 10); `out`: (30, 0); `v+`: (0, -20); `v-`: (0, 20) |

The op-amp uses logical terminal names rather than package pin numbers. Its non-inverting input is above its inverting input. Reference and value labels are supplied by the application; the SVGs contain no text elements, grids, or terminal-marker artwork.

The capacitor artwork was revised to box plates on 2026-11-06. IDs, terminal order, names, and positions are unchanged, so this is a compatible artwork update. Re-import the JSON and accept replacement to update an already imported library entry. The black negative plate requires the app build with monochrome fill support (document format v4); older builds may render that plate white. Updating the shared library does not silently replace artwork embedded in existing documents.

Follow-up refinements: both non-polarised capacitor plates are now black. Each op-amp supply lead is shortened by 10 units, placing `v+`/`v-` at Y = -20/+20. This changes its terminal signature, so the updated op-amp imports as **`Grid10.OpAmp.ShortSupply`**, displayed as **Op Amp - Short Supply (Grid 10)**, rather than replacing `Grid10.OpAmp`. Existing instances and wires keep the old definition; place the new variant when the shorter supplies are wanted. The input/output tips, body, and signs are unchanged.

## Edit and reuse

Edit SVG geometry in a vector editor or text editor, keeping terminal lead tips fixed. SVG `width` and `height` control display size only: import geometry comes from the coordinates, so scale the actual geometry and terminal metadata together if you intend to change its scene size. Keep the origin at zero for simple grid alignment.

Copy a template and assign a new ID before changing terminal names, order, count, or coordinates on a symbol already saved to the library. Artwork can be revised under the same ID when its terminal signature remains unchanged.

## Preview and verification

`preview.png` shows the actual imported artwork at the same scale in every panel. Its grid and blue terminal rings are preview annotations and are absent from the importable SVG files.

Verified on 2026-11-06 with the project's real SVG importer and symbol renderer: all four pairs import successfully, all 11 terminals lie on 10-unit offsets and touch their lead artwork, and document checks pass for connected wires, shared translation, four quadrant rotations, horizontal/vertical flips, and save/load round trips. No application source or built-in library entries were changed for these templates.

The revised capacitors also pass SVG export/re-import comparison and interior-pixel checks: both non-polarised plates render black, the electrolytic positive plate renders white and negative plate black, and the inter-plate gap remains unfilled. The refreshed common-scale preview was visually inspected.

The packaged-library refresh also checks exact converted-artwork equivalence against all four templates, their terminal signatures, coexisting imported template IDs, placement/wiring/undo, group transforms, and document round trips. All seven Release suites pass. The original built-in `OpAmp` is retained separately as a legacy entry; it is not the former long-supply `Grid10.OpAmp` authoring template.
