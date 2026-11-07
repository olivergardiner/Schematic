# Symbol style guide

Status: **draft for review; not yet an approved specification.**

This guide defines the visual family for symbols created for Schematic. It complements the [symbol import design](symbol-import.md), which defines supported SVG and metadata, and [DECISIONS.md](../../DECISIONS.md), which defines geometry and terminal compatibility. It does not change those contracts or authorize application changes.

The user has requested capacitors with **thin rectangular boxes for the plates**, with the **positive plate unfilled on a polarised capacitor**. That requirement is recorded below. Other dimensions, conventions, and defaults in this draft are proposals for review.

Additional user requirements: primitive passives use a **40-unit terminal-to-terminal span by default**. Component-specific terminal layouts take priority over general lead-shortening guidance. Leads should otherwise be short and reach the 10-unit grid; this avoids excessive op-amp supply leads without shrinking the passive family. These requirements apply even while the rest of the guide remains a draft.

## 1. Visual intent

- Use clear, restrained artwork suitable for guitar-pedal schematics, printed documentation, and presentation slides.
- Keep a consistent visual weight across passives, semiconductors, and ICs. Compare symbols at the same scene scale, rather than fitting each to an equally sized preview box.
- Use conventional schematic shapes with balanced proportions. Avoid decorative effects, shadows, gradients, colour coding, and package illustrations.
- Make polarity and input signs readable at normal editor zoom. Draw essential signs as geometry so they survive import without fonts.
- Keep artwork compact while leaving enough space around terminal tips for wires, branches, and nearby symbols.

## 2. Coordinate and terminal rules

| Property | Draft rule |
|---|---|
| Unit mapping | One root SVG user unit equals one scene unit. Physical SVG width/height do not set imported geometry size. |
| Authoring grid | 10 scene units for terminal placement. |
| Origin | `(0, 0)`, normally at the visual center of the main body. Use a documented exception when a terminal or other anchor is more useful. |
| Terminal positions | Both coordinates are multiples of 10 relative to the origin. |
| Artwork detail | Body edges, curves, plate thickness, and signs may use finer coordinates; they do not need to lie on the terminal grid. |
| Terminal location | The exposed tip of a visible lead. No invisible gap or lead continuing past the electrical endpoint. |
| Primitive passive span | Default to 40 units between terminal tips of simple two-terminal passives, including resistors and capacitors. This complete-symbol span takes priority over minimizing lead length. Document and review any exception. |
| Body-to-terminal lead | Connect the actual body attachment to the terminal specified by the component-family layout. Where no layout specifies a position, prefer the nearest practical outward intersection of the 10-unit grid, keeping the lead short and visible. |
| Lead direction | Horizontal or vertical at the terminal; avoid diagonal lead tips. |
| Default pose | Horizontal two-terminal parts; left-facing inputs and right-facing outputs for amplifiers. Other types specify their own default below. |
| Scaling | Choose dimensions in scene units. Never resize a symbol automatically when the document grid changes. |

These terminal rules are a convention for this new visual family. Existing valid off-grid library entries and documents remain supported. If a component origin is on the 10-unit grid, its terminals remain on that grid through quadrant rotations and flips.

### Body size and lead placement

These rules concern lead artwork inside the symbol, not document wire routing. Apply them in this order:

1. **Preserve released terminal interfaces.** Do not move existing terminals under the same symbol ID to enforce a new style rule.
2. **Apply the component-family layout.** For new simple two-terminal passives, place tips at `(-20, 0)` and `(20, 0)` in the default centered horizontal pose. Other types use their documented terminal positions, including the op-amp layout below.
3. **Keep terminals on the 10-unit grid.** Body details and lead attachment points may lie between grid lines; leads must meet their designated tips exactly.
4. **Minimize unnecessary lead length within those constraints.** Where no family layout fixes a terminal, prefer the nearest practical outward grid intersection. If the body attachment already lies on a grid intersection, use the next outward one to retain a visible lead. Do not add a diagonal lead to compensate for a poorly positioned attachment.

The current passive references satisfy this hierarchy. Resistor body edges at `x=±10` have 10-unit leads to tips at `x=±20`. Capacitor outer plate edges at `x=±5` have 15-unit leads to the same tips. Both intentionally span 40 units; the capacitor leads are not shortened to `x=±10` simply because those grid points are closer.

For the current op-amp, the supply attachments are at `(0, ±10)` and tips at `(0, ±20)`, giving 10-unit supply leads. Do not extend them to `(0, ±30)` merely to match another axis or a passive span. A future family needing a different layout should document that layout rather than relying on an unrestricted nearest-grid rule.

## 3. Stroke, fill, and detail

- Use a **1-scene-unit black stroke** as the common line weight, including lead lines and outlines. Use the same weight for polarity and input signs unless a reviewed example demonstrates a need for an exception.
- Prefer square-ended straight leads and sharp joins on rectangles and triangles. Use smooth curves where they communicate a component's function.
- Outline interiors are white when a body needs to mask artwork behind it. In this guide, **unfilled/hollow** means a visibly open interior, not a transparent window that must expose crossing wires.
- A **solid plate or mark** means a black interior, distinct from a hollow white interior. Keep this distinction visible at normal drawing scale.
- Avoid tiny gaps, slivers, or isolated details that disappear at normal zoom. Proposed minimum internal opening: 1 scene unit after accounting for outline strokes.
- Draw plus/minus signs and arrows with paths or simple primitives. Essential symbol markings belong in the artwork; component references and values do not.

### Current rendering support

The symbol format now distinguishes **no fill, opaque white, and opaque black**. The scene, palette, and import preview share this behavior, and SVG import/export preserves monochrome fill. Existing converted entries keep their legacy white-fill defaults. Stroke colour, width, caps, and joins remain supplied by the renderer rather than preserved as arbitrary SVG styling.

Both hollow box plates and solid-black plates can now be represented. The user's template refinements specify two black plates for a non-polarised capacitor, and a hollow positive/black negative pair for a polarised capacitor. Inspect actual application rendering before approving further reference changes; do not substitute a curved plate without review.

The approved and implemented [black symbol fills design](black-symbol-fills.md) records support, compatibility, and verification. It does not settle the open capacitor styling choices in this guide.

## 4. Component-specific guidance

### Resistors

- Use the rectangular convention as the default for this family. A zigzag resistor, if wanted later, is a separately named visual variant.
- Proposed body: 20 units long by 10 high, centered at the origin, with a hollow/white interior.
- Horizontal leads connect the midpoint of each short edge to terminal tips at `(-20, 0)` and `(20, 0)`.
- No internal hatching, resistance text, or reference label.
- Ordered terminals: `1` at the left tip, then `2` at the right tip. Reference prefix: `R`.

### Non-polarised capacitors

- **Draw both plates as thin rectangular boxes, not single lines.** Keep their heights, thicknesses, and spacing equal.
- **Both plates are solid black**, following the user's template refinement.
- Proposed plate geometry: height 20, width 3; left rectangle from `x=-5` to `x=-2`, right rectangle from `x=2` to `x=5`, both from `y=-10` to `y=10`. This leaves a 4-unit geometric gap between plates and a visible interior after the 1-unit outline is applied.
- Leads meet the centers of the plates' outer edges, with terminal tips at `(-20, 0)` and `(20, 0)`. Leads end at the plate boundary rather than passing through its interior.
- Do not add polarity signs or a curved plate.
- Ordered terminals: `1` at the left tip, then `2` at the right tip. Reference prefix: `C`.

### Polarised / electrolytic capacitors

- **Use thin rectangular box plates and leave the positive plate hollow**, as requested by the user. Keep the plate geometry and total span consistent with the non-polarised capacitor.
- Positive plate hollow/white, negative plate solid black, matching the accepted template treatment. Rendering support is available.
- Default orientation: positive on the left, negative on the right. Polarity follows the terminal identities when the symbol rotates or flips.
- Proposed additional cue: a small `+` near the positive plate, above and to the left of the body. Keep it separate from the lead and from the application's reference label. Use a 4-unit-wide/high sign initially, then check it at normal zoom.
- Do not use a curved negative plate in this family. Do not reverse the plate treatments after a flip merely to keep the hollow plate on the left.
- Ordered terminals: `+` at `(-20, 0)`, then `-` at `(20, 0)`. Reference prefix: `C`.
- This convention identifies a polarised capacitor, not a package size, technology-specific footprint, or physical pin numbering scheme.

### Operational amplifiers

- Use a hollow/white triangle pointing right. Proposed vertices: `(-20, -20)`, `(-20, 20)`, and `(20, 0)`.
- Non-inverting input above inverting input in the default pose; draw `+` and `-` inside the triangle next to the respective inputs.
- Input terminal tips: `in+` at `(-30, -10)` and `in-` at `(-30, 10)`. Output tip: `out` at `(30, 0)`.
- For the current five-terminal variant, use `v+` at `(0, -20)` and `v-` at `(0, 20)`: the user requested shortening each previous supply lead by 10 units. Supply leads meet the triangle edges exactly at `(0, -10)` and `(0, 10)`. This terminal change requires the new `Grid10.OpAmp.ShortSupply` ID; the earlier `Grid10.OpAmp` interface remains frozen.
- Ordered terminals: `in+`, `in-`, `out`, `v+`, `v-`. Reference prefix: `U`. These are logical names, not package pin numbers.
- Keep input signs clear of the triangle edges and supply leads. Do not include a part number, reference, or value in the SVG.
- A three-terminal variant with omitted supply connections must have a distinct ID and be explicitly requested. Do not hide or remove terminals from an existing five-terminal entry under the same ID.

### Other component families

Add a short subsection and an approved example before creating a new family. At minimum, record:

| Family | Choices to settle before authoring |
|---|---|
| Diodes, LEDs, Zeners, Schottky diodes | Body convention and fill; cathode treatment; arrow form and spacing; `A`/`K` ordering. |
| BJTs | Circle or no circle; base/collector/emitter geometry; arrow size; terminal order. |
| JFETs and MOSFETs | Channel, gate, body and arrow conventions; three/four-terminal variants; terminal order. |
| Potentiometers and trimmers | Resistor body convention; wiper direction and arrow; terminal order and default pose. |
| Ground and supply symbols | Shape, size, reference visibility, and terminal anchor. |
| Jacks and switches | Contact depiction, normally open/closed state, switched contacts, and logical terminal names. |
| Valves and specialised amplifiers | Envelope/body treatment, electrode geometry, explicit supply/heater terminals, and size exceptions. |

Do not silently choose a new convention because an external SVG library happens to use it. Bring the candidate geometry and terminal proposal to review first.

## 5. Labels and metadata

- The application supplies editable reference and value labels and keeps them upright. Leave them out of the SVG.
- Include only functional markings inside the artwork: input signs, polarity, arrows, or another marking explicitly approved for that component type.
- Use stable, descriptive symbol IDs and category paths through the existing library/import workflow. Name visual or terminal-interface variants distinctly.
- Put terminal names and coordinates in the JSON sidecar, in the intended index order. An SVG dot or line end alone is not terminal metadata.
- Check the naming and order before first library save. Once released, changing terminal count, order, names, or positions requires a new ID. Artwork and presentation metadata can be refined without changing that interface.
- Do not alter existing IDs or off-grid terminal interfaces merely to bring legacy entries into this visual family. Introduce a compatible presentation update or a new ID as appropriate.

## 6. Reference assets and approval

The user reviewed and refined the [Grid 10 templates](../../symbol-templates/grid10/README.md), then approved using their four artworks in the [built-in template refresh](builtin-template-refresh.md). They are the current references for those component types; this does not approve all remaining draft rules or future component families. Technical import verification and visual approval remain separate checks.

| Asset | Relationship to this draft |
|---|---|
| [Resistor](../../symbol-templates/grid10/resistor.svg) | Reviewed rectangular geometry; packaged as `Resistor`. |
| [Capacitor](../../symbol-templates/grid10/capacitor.svg) | Box plates refined to both black at the user's request; geometry and terminals retained. |
| [Electrolytic capacitor](../../symbol-templates/grid10/electrolytic-capacitor.svg) | Hollow positive box plate, solid-black negative box plate, positive mark; retained after the user's visual review. |
| [Op amp](../../symbol-templates/grid10/op-amp.svg) | Supply leads shortened by 10 units at the user's request; new `Grid10.OpAmp.ShortSupply` terminal signature. |

The four reference pairs and their packaged ID mapping are recorded in the template README and kept in version control. Any future comparison sheet should show examples at a common scene scale, with an unannotated view and a separate 10-unit-grid/terminal overlay. Clearly label proposed and approved examples. Reference assets illustrate the rules; if a reference and the text conflict, resolve that conflict before generating more symbols.

## 7. Acceptance checks for a new symbol

1. Compare it with an approved member of the same family at the same scale. Check proportions, stroke weight, fill, functional marks, and label clearance at normal editor zoom.
2. Import the SVG/JSON pair through the actual importer. Inspect the application's rendering, including any filled shapes; an external SVG preview alone is insufficient.
3. Check every terminal name, index, coordinate, exposed lead contact, and 10-unit offset against the component-family layout. For primitive passives, check the default 40-unit tip-to-tip span or the reviewed exception. Then check for unnecessary lead extension without overriding the family dimensions. Check that hidden helper geometry, grids, and terminal markers were not imported.
4. Check quadrant rotations and both flips. Terminal identities and polarity must follow the component; application reference/value labels remain upright.
5. Connect wires to every terminal and verify endpoint alignment. For shared movement, use the existing group-transform behavior; the symbol must not need a special routing rule.
6. Save and reopen a document containing it. Confirm appearance, terminal identities, and connection positions. Use focused geometry/import tests when adding released collection entries, following the existing project workflow.
7. Record visual approval separately from technical verification. An imported and validated symbol is not automatically an approved visual reference.

## 8. Open review choices

- Confirm the proposed 1-unit stroke and 20 × 10 resistor body. The 40-unit primitive-passive span takes priority over general lead-shortening guidance; the current passive references conform to that clarified hierarchy.
- Preserve the current capacitor references: both non-polarised plates black; polarised positive hollow and negative black. Further dimensional or polarity-mark changes should be reviewed against those examples.
- Use the implemented monochrome fill support when preparing examples that depend on black interiors; broader colour styling remains outside this guide.
- Confirm the op-amp input orientation, supply-pin presentation, and triangle proportions against a rendered example.
- Approve the reference assets after the capacitor revisions, then add a short pointer from `AGENTS.md` to this guide as the authoritative symbol-authoring specification.
