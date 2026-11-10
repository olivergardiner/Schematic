# Symbol style guide

Status: **broader guidance remains a draft. The reviewed Grid 10 reference artwork and Block Schematics conventions below are approved.**

This guide defines the visual family for symbols created for Schematic. It complements the [symbol import design](symbol-import.md), which defines supported SVG and metadata, and [DECISIONS.md](../../DECISIONS.md), which defines geometry and terminal compatibility. It does not change those contracts or authorize application changes.

The user has requested capacitors with **thin rectangular boxes for the plates**, with the **positive plate unfilled on a polarised capacitor**. The reviewed Grid 10 artwork and the Block Schematics family are approved references. Other dimensions, conventions, and defaults remain proposals for review.

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
- **Arrowheads are filled solid black by default**, using closed geometry. Use an open head only for an explicitly reviewed exception. This applies to potentiometer wipers and variable-filter arrows.

### Current rendering support

The symbol format now distinguishes **no fill, opaque white, and opaque black**. The scene, palette, and import preview share this behavior, and SVG import/export preserves monochrome fill. Existing converted entries keep their legacy white-fill defaults. Stroke colour, width, caps, and joins remain supplied by the renderer rather than preserved as arbitrary SVG styling.

Both hollow box plates and solid-black plates can now be represented. The user's template refinements specify two black plates for a non-polarised capacitor, and a hollow positive/black negative pair for a polarised capacitor. Inspect actual application rendering before approving further reference changes; do not substitute a curved plate without review.

The approved and implemented [black symbol fills design](black-symbol-fills.md) records support, compatibility, and verification. The current capacitor artwork is approved through the template refresh; later styling changes require review.

## 4. Component-specific guidance

### Resistors

- Use the rectangular convention as the default for this family. A zigzag resistor, if wanted later, is a separately named visual variant.
- Approved reference body: 20 units long by 10 high, centered at the origin, with a hollow/white interior.
- Horizontal leads connect the midpoint of each short edge to terminal tips at `(-20, 0)` and `(20, 0)`.
- No internal hatching, resistance text, or reference label.
- Ordered terminals: `1` at the left tip, then `2` at the right tip. Reference prefix: `R`.

### Non-polarised capacitors

- **Draw both plates as thin rectangular boxes, not single lines.** Keep their heights, thicknesses, and spacing equal.
- **Both plates are solid black**, following the user's template refinement.
- Approved reference plate geometry: height 20, width 3; left rectangle from `x=-5` to `x=-2`, right rectangle from `x=2` to `x=5`, both from `y=-10` to `y=10`. This leaves a 4-unit geometric gap between plates and a visible interior after the 1-unit outline is applied.
- Leads meet the centers of the plates' outer edges, with terminal tips at `(-20, 0)` and `(20, 0)`. Leads end at the plate boundary rather than passing through its interior.
- Do not add polarity signs or a curved plate.
- Ordered terminals: `1` at the left tip, then `2` at the right tip. Reference prefix: `C`.

### Polarised / electrolytic capacitors

- **Use thin rectangular box plates and leave the positive plate hollow**, as requested by the user. Keep the plate geometry and total span consistent with the non-polarised capacitor.
- Positive plate hollow/white, negative plate solid black, matching the accepted template treatment. Rendering support is available.
- Default orientation: positive on the left, negative on the right. Polarity follows the terminal identities when the symbol rotates or flips.
- Approved additional cue: a small `+` near the positive plate, above and to the left of the body. Keep it separate from the lead and from the application's reference label. The reference uses a 4-unit-wide/high sign.
- Do not use a curved negative plate in this family. Do not reverse the plate treatments after a flip merely to keep the hollow plate on the left.
- Ordered terminals: `+` at `(-20, 0)`, then `-` at `(20, 0)`. Reference prefix: `C`.
- This convention identifies a polarised capacitor, not a package size, technology-specific footprint, or physical pin numbering scheme.

### Operational amplifiers

- Use a hollow/white triangle pointing right. Approved reference vertices: `(-20, -20)`, `(-20, 20)`, and `(20, 0)`.
- Non-inverting input above inverting input in the default pose; draw `+` and `-` inside the triangle next to the respective inputs.
- Input terminal tips: `in+` at `(-30, -10)` and `in-` at `(-30, 10)`. Output tip: `out` at `(30, 0)`.
- For the current five-terminal variant, use `v+` at `(0, -20)` and `v-` at `(0, 20)`: the user requested shortening each previous supply lead by 10 units. Supply leads meet the triangle edges exactly at `(0, -10)` and `(0, 10)`. The authoring template uses `Grid10.OpAmp.ShortSupply`; its packaged equivalent is `OpAmp.Grid10`. The original packaged `OpAmp` and its Compatibility category were removed with user approval on 2026-11-07 because no saved schematics depended on them; no alias or migration is provided. The distinct earlier authoring-template ID `Grid10.OpAmp` is not repurposed.
- Ordered terminals: `in+`, `in-`, `out`, `v+`, `v-`. Reference prefix: `U`. These are logical names, not package pin numbers.
- Keep input signs clear of the triangle edges and supply leads. Do not include a part number, reference, or value in the SVG.
- [Op-Amp (No Supplies)](../../core/symbols/opamp-no-supplies.json), ID `OpAmp.Grid10.NoSupplies`, keeps the triangle, input signs and three signal terminals unchanged and omits both supply leads and terminals. It is an ordinary separate library entry.

### Amplifier supply variants (approved)

- As a general convention, provide powered amplifier symbols **with and without supply terminals**, using distinct stable IDs. Keep signal geometry and terminal names/positions consistent between variants. Choosing a supply-free variant omits only the supply interface; do not hide terminals on an existing symbol or infer hidden document nets.
- These amplifier entries use prefix `U`, visible references, and logical terminal names rather than package pin numbers.
- The LM13700-style Darlington buffer below is an explicit exception: its collectors are internally connected in the device, its output is open emitter, and it has no exposed supply terminal or additional supply variant.

### OTAs and Darlington buffer (approved)

- Put [OTA (With Supplies)](../../core/symbols/ota-with-supplies.json), ID `OTA.WithSupplies.OutputSource.VerticalBias`, and [OTA (No Supplies)](../../core/symbols/ota-no-supplies.json), ID `OTA.NoSupplies.OutputSource.VerticalBias`, alongside the other op-amps in `Semiconductors/ICs/Op Amps`.
- Use the op-amp's white triangle and differential input signs. Show the current source after the triangle as two overlapping, unfilled circles centred at (26, 0) and (34, 0), each radius 6. Do not add an arrow to the current source. The first circle meets the triangle tip at (20, 0); the output lead runs from (40, 0) to (50, 0). Ordered signal terminals: `in+` (-30, -10), `in-` (-30, 10), `out` (50, 0), then amplifier-bias `Iabc` (30, 20). Omit the linearising-diode bias connection.
- The bias lead runs vertically from (30, 20) to the lower intersection of the two circles at (30, √20). It has no arrow. This keeps it distinct from the negative supply lead. The `.OutputSource.VerticalBias` IDs replace the previous OTA palette entries because the output and bias terminals moved; old IDs are not aliases for this layout. The powered variant appends `v+` (0, -20), `v-` (0, 20), using the existing op-amp supply artwork.
- Above-bias alternatives use IDs `OTA.WithSupplies.OutputSource.BiasAbove` and `OTA.NoSupplies.OutputSource.BiasAbove`. Only `Iabc` and its lead change: the terminal is at (30, -20) and the vertical lead meets the upper circle intersection at (30, -√20). All other artwork, terminal names and positions remain identical. Both poses are authored library entries, but Above sets `paletteVisible: false` and is chosen through Properties. Only the default Below layout appears for each supply configuration in the palette, named **OTA (With Supplies)** or **OTA (No Supplies)**. Their `bias_position` metadata links Below/Above choices with explicit role mappings. A single selected OTA offers **Bias position** in Properties, interpreted before rotation and mirroring. Connected edits repair routes atomically and are undoable; supply configuration does not change. See the [layout-variant design](symbol-layout-variants.md).
- Put [Darlington Buffer (LM13700)](../../core/symbols/darlington-buffer-lm13700.json), ID `DarlingtonBuffer.LM13700`, in `Semiconductors/ICs/Buffers`. Draw two NPN stages, filled emitter arrows and an internal common-collector connection, without an enclosing circle. Ordered terminals: `in` (-20, 0), then open-emitter `out` (10, 20). Do not expose a collector or supply terminal, and do not create a supply-free duplicate.
- The [TI LM13700 datasheet](https://www.ti.com/lit/ds/symlink/lm13700.pdf) is the device reference for the separate OTA and Darlington buffer. These are presentation symbols, not a package pinout or simulation model.

### Diodes (approved)

- The current [Diode](../../core/symbols/diode.json) uses a 10 × 10 white triangular body, preserving its previous 1:1 aspect ratio while matching the resistor's 10-unit thickness. Vertices: (-5, -5), (-5, 5), (5, 0).
- The cathode bar runs from (5, -5) to (5, 5). Use the common 1-unit black stroke and leads from x = -20 to -5 and x = 5 to 20 on y = 0.
- Ordered terminals remain `anode` at (-20, 0), then `cathode` at (20, 0); prefix `D`, category `Semiconductors/Diodes`, references visible by default. The user accepted the resized standard diode. All three new variants share its body and terminal layout.
- [Schottky Diode](../../core/symbols/schottky-diode.json), ID `SchottkyDiode`: hooked cathode bar through (2, -3), (2, -5), (5, -5), (5, 5), (8, 5), (8, 3).
- [Zener Diode](../../core/symbols/zener-diode.json), ID `ZenerDiode`: bent cathode bar through (2, -7), (5, -5), (5, 5), (8, 7).
- [LED](../../core/symbols/led.json), ID `LED`: retain the straight cathode bar and add two parallel light arrows pointing outward towards the upper right, with filled black triangular heads. The shafts run from (-3, -7) to (3, -13) and from (3, -5) to (9, -11). Optical arrows are artwork, not additional terminals. Future diode variants require review.

### Semiconductor categories (approved)

Use `Semiconductors/Diodes`, `Semiconductors/Transistors`, and IC subcategories `Semiconductors/ICs/Op Amps`, `Semiconductors/ICs/Buffers`. The six transistor entries below belong directly to Transistors, without subcategories. Library manifest metadata can also declare empty categories without placeholder symbols. The existing op-amp moves into Op Amps without changing its ID or terminals.

### Transistors (approved)

- Put all six initial variants directly in `Semiconductors/Transistors`. Use no enclosing circle, the common 1-unit stroke, filled black arrowheads, prefix `Q`, and references visible by default.
- All variants have a **30-unit default width**, from the base/gate terminal at x = -20 to collector/emitter or drain/source terminals at x = 10. Align the left-hand vertical bar at **x = -5** for every family: BJT base bar, JFET channel bar, MOSFET gate bar. Keep the base/gate lead horizontal; the other two leads finish vertically with no horizontal terminal stubs. Geometry and terminal positions do not scale with the document grid.
- Ordered roles remain index 0 base/gate, index 1 collector/drain, index 2 emitter/source. For N versions, positions are (-20, 0), (10, -20), (10, 20). Draw P versions **vertically flipped by default**, with positions (-20, 0), (10, 20), (10, -20). Flip the entire artwork, retaining the correct polarity arrows and terminal identities. These are logical names, not package pin numbers.
- [PNP](../../core/symbols/pnp.json) and [NPN](../../core/symbols/npn.json), IDs `PNP.Compact30` and `NPN.Compact30`: ordered names `base`, `collector`, `emitter`; vertical base bar with sloping collector/emitter branches. The emitter arrow points towards the base for PNP and away from it for NPN.
- [N Channel JFET](../../core/symbols/n-channel-jfet.json) and [P Channel JFET](../../core/symbols/p-channel-jfet.json), IDs `NChannelJFET.Compact30` and `PChannelJFET.Compact30`: ordered names `gate`, `drain`, `source`; continuous vertical channel, with the gate arrow pointing into the channel for N and away for P. JFET drain/source branches attach at y = ±8 on the bar spanning y = ±10, matching the MOSFET attachment levels. Exposed terminal positions remain unchanged.
- [N Channel MOSFET](../../core/symbols/n-channel-mosfet.json) and [P Channel MOSFET](../../core/symbols/p-channel-mosfet.json), IDs `NChannelMOSFET.Compact30` and `PChannelMOSFET.Compact30`: ordered names `gate`, `drain`, `source`; enhancement-mode broken channel with three separate strokes and a visibly isolated gate. Draw the body connection tied to source; its arrow points into the channel for N and away for P. There is no separate body terminal and no body-diode artwork.
- Polarity conventions can be compared with the BJT, JFET and MOSFET figures in [Toshiba's transistor reference](https://toshiba.semicon-storage.com/content/dam/toshiba-ss-v3/master/en/semiconductor/knowledge/e-learning/discrete/discrete-basic-chap3_en.pdf). The dimensions, shared layout, and omission of circles follow this project's approved design.
- The revised terminal layout uses new `.Compact30` IDs and replaces the original six palette entries; the previous IDs are not aliases for the new geometry.
- Depletion-mode MOSFETs, four-terminal devices and other variants require separate approval and distinct IDs. Released terminal names, order and positions remain immutable under each ID.

### Potentiometers and preset resistors (approved)

- Match the resistor's centred 20 × 10 hollow/white body, from (-10, -5) to (10, 5), with end terminals at (-20, 0) and (20, 0).
- Ordered terminals: `1` at (-20, 0), `2` at (20, 0), then `wiper` at (0, -20). Both use reference prefix `VR`, visible by default, in `Passives`. The existing `Potentiometer` terminal interface is unchanged; the new preset ID is `PresetResistor`.
- The potentiometer wiper approaches from above. Its filled black triangular arrowhead has points (-4, -13), (0, -5), (4, -13), meeting the body at its top edge.
- Indicate clockwise wiper travel towards terminal `2` with a short straight arrow inside the resistor body: a shaft from (-6, 0) to (5, 0) and a filled black head with vertices (2, -2), (6, 0), (2, 2). The mark is artwork, not a junction or extra terminal. It rotates and mirrors with the component and continues to identify terminal `2`.
- Preset Resistor uses the same layout and clockwise mark, replacing the wiper arrowhead with a short crossbar from (-4, -9) to (4, -9) to indicate tool adjustment. Its shaft ends at (0, -9), leaving the bar distinct from the body outline; it has no wiper arrowhead.
- Approved examples: [Potentiometer](../../core/symbols/potentiometer.json) and [Preset Resistor](../../core/symbols/preset-resistor.json).

### Block Schematics (approved)

These conventions and the current packaged examples were approved on 2026-11-07. They represent drawing blocks using ordinary explicit terminal connectivity, with no simulation behavior.

- Use a centred **40 × 40 scene-unit square**, from (-20, -20) to (20, 20), with an opaque white interior and the common 1-unit black outline.
- Inputs face left and outputs face right. Use 10-unit horizontal leads from the box edges to terminal tips at x = -30 and x = 30, giving a 60-unit total horizontal span. This block-family layout is separate from the 40-unit primitive-passive tip-to-tip rule.
- Keep functional artwork inside the box and draw it as geometry. References and values are supplied by the application. Reference prefix: `B`; references are visible by default.
- Buffer: right-pointing hollow triangle inside the box. Ordered terminals: `in` at (-30, 0), then `out` at (30, 0). Category: `Block Schematics`. ID: `Block.Buffer`.
- Summing node: sigma inside the box. Ordered terminals: `in1` at (-30, -10), `in2` at (-30, 10), then `out` at (30, 0). Category: `Block Schematics`. ID: `Block.SummingNode`.
- Grid changes do not resize blocks. Released terminal count, order, names and positions remain immutable under the same ID.

#### Filters (approved)

- Place all fixed and variable filters in `Block Schematics/Filters`.
- Use response-curve glyphs: rising for high pass, falling for low pass, a central pass band for band pass, and a central notch for band stop. Positive scene Y points down, so higher response appears towards the top of the box.
- Variable versions add a diagonal arrow from (-14, 14) to (14, -14), with a closed, solid-black triangular arrowhead through (7, -14), (14, -14), and (14, -7). Keep the underlying response curve the same as the corresponding fixed filter.
- All eight filters have ordered terminals `in` at (-30, 0), then `out` at (30, 0). Variable filters have **no additional control terminal**. Box geometry, leads, prefix and reference visibility match the fixed versions.
- Fixed IDs: `Block.HighPass`, `Block.LowPass`, `Block.BandPass`, `Block.BandStop`. Variable IDs: `Block.VariableHighPass`, `Block.VariableLowPass`, `Block.VariableBandPass`, `Block.VariableBandStop`.

### Rails, grounds and signal ports (approved)

- Use `Signals/Power Rails`, `Signals/Grounds`, and `Signals/Ports`. All have one explicit wire terminal and hidden references by default. Names and direction are presentation only: matching labels never join disconnected nets. See the [connection-symbol design](signal-connections.md) for every stable ID, terminal position and approved vertex list.
- Power Rail: one visible symbol, prefix `PWR`, initial Value `Vcc`. Its vertical lead runs from terminal (0, 20) to (0, 0), meeting a short horizontal bar from (-10, 0) to (10, 0). Put a filled black circle of radius 2 at the intersection, with no arrowhead. The dot is artwork, not a separate junction/terminal. Other names use Value editing and other orientations use the existing flip/rotate controls. Retain ID `Rail.VCC`.
- Earth uses a compact three-bar body matching the newer ground family: bars from (-10, 0) to (10, 0), (-6, 4) to (6, 4), and (-2, 8) to (2, 8), with a shaft from (0, -20) to (0, 0). Keep released ID `Ground` and terminal `1` at (0, -20), presented as **Earth** in Grounds. Chassis ground uses a horizontal bar and three angled strokes; Signal Ground uses a downward white triangle. New grounds have terminal `connection` at (0, -20) and prefix `GND`. Signal Ground starts with empty Value; enter `AGND`, `DGND` or any other name when needed. Separate AGND/DGND entries are not provided; use the same Signal Ground and edit Value.
- Ports use white outlined flags, with no text baked into the body. Input and Output both indicate left-to-right signal flow, but Input exposes its right tip at (20, 0) and Output its left edge at (-20, 0). Bidirectional uses a double-pointed white hexagon with a right terminal at (20, 0). Prefix `PORT`; initial Values `IN`, `OUT`, `IO`.
- The common 1-unit black stroke applies; port outlines are flag bodies rather than solid arrowheads. All new terminal names are `connection`. Geometry is in scene units and does not scale with grid spacing.
- Optional native/sidecar `defaultValue` supplies initial label text on new placement only. The existing Value property owns subsequent edits, visibility and persistence, including explicit empty values. Existing upright movable property blocks initially sit below artwork; users can drag them. Library updates, load, paste and layout switching do not reset values.

### Other component families

Add a short subsection and an approved example before creating a new family. At minimum, record:

| Family | Choices to settle before authoring |
|---|---|
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

The user reviewed and refined the [Grid 10 templates](../../symbol-templates/grid10/README.md), then approved using their four artworks in the [built-in template refresh](builtin-template-refresh.md). They are the current references for those component types. The Block Schematics examples listed below are also approved; remaining draft rules and future component families still require review. Technical import verification and visual approval remain separate checks.

| Asset | Relationship to this draft |
|---|---|
| [Resistor](../../symbol-templates/grid10/resistor.svg) | Reviewed rectangular geometry; packaged as `Resistor`. |
| [Capacitor](../../symbol-templates/grid10/capacitor.svg) | Box plates refined to both black at the user's request; geometry and terminals retained. |
| [Electrolytic capacitor](../../symbol-templates/grid10/electrolytic-capacitor.svg) | Hollow positive box plate, solid-black negative box plate, positive mark; retained after the user's visual review. |
| [Op amp](../../symbol-templates/grid10/op-amp.svg) | Supply leads shortened by 10 units at the user's request; authoring ID `Grid10.OpAmp.ShortSupply`, packaged as `OpAmp.Grid10`. |

Approved Block Schematics examples are the native library definitions; no separate SVG authoring references have been created:

| Example | Approved convention |
|---|---|
| [Buffer](../../core/symbols/block-buffer.json) | White box with a right-pointing triangle. |
| [Summing node](../../core/symbols/block-summing-node.json) | White box with sigma and two left inputs. |
| [High pass](../../core/symbols/block-high-pass.json), [low pass](../../core/symbols/block-low-pass.json), [band pass](../../core/symbols/block-band-pass.json), [band stop](../../core/symbols/block-band-stop.json) | Fixed response curves. |
| [Variable high pass](../../core/symbols/block-variable-high-pass.json), [variable low pass](../../core/symbols/block-variable-low-pass.json), [variable band pass](../../core/symbols/block-variable-band-pass.json), [variable band stop](../../core/symbols/block-variable-band-stop.json) | Matching response curves with diagonal arrows. |

The four Grid 10 reference pairs and their packaged ID mapping are recorded in the template README and kept in version control. Any future comparison sheet should show examples at a common scene scale, with an unannotated view and a separate 10-unit-grid/terminal overlay. Clearly label proposed and approved examples. Reference assets illustrate the rules; if a reference and the text conflict, resolve that conflict before generating more symbols.

## 7. Acceptance checks for a new symbol

1. Compare it with an approved member of the same family at the same scale. Check proportions, stroke weight, fill, functional marks, and label clearance at normal editor zoom.
2. Import the SVG/JSON pair through the actual importer. Inspect the application's rendering, including any filled shapes; an external SVG preview alone is insufficient.
3. Check every terminal name, index, coordinate, exposed lead contact, and 10-unit offset against the component-family layout. For primitive passives, check the default 40-unit tip-to-tip span or the reviewed exception. Then check for unnecessary lead extension without overriding the family dimensions. Check that hidden helper geometry, grids, and terminal markers were not imported.
4. Check quadrant rotations and both flips. Terminal identities and polarity must follow the component; application reference/value labels remain upright.
5. Connect wires to every terminal and verify endpoint alignment. For shared movement, use the existing group-transform behavior; the symbol must not need a special routing rule.
6. Save and reopen a document containing it. Confirm appearance, terminal identities, and connection positions. Use focused geometry/import tests when adding released collection entries, following the existing project workflow.
7. Record visual approval separately from technical verification. An imported and validated symbol is not automatically an approved visual reference.

## 8. Open review choices

- Broader use of the 1-unit stroke across future families remains draft; the reviewed resistor reference already establishes its 20 × 10 body. The 40-unit primitive-passive span takes priority over general lead-shortening guidance.
- Preserve the current capacitor references: both non-polarised plates black; polarised positive hollow and negative black. Further dimensional or polarity-mark changes should be reviewed against those examples.
- Use the implemented monochrome fill support when preparing examples that depend on black interiors; broader colour styling remains outside this guide.
- Preserve the approved op-amp input orientation, short supply leads and triangle proportions; review future variants separately.
- Review the remaining general rules and future-family guidance before making this guide the authoritative symbol-authoring specification or adding an authority pointer from `AGENTS.md`. The accepted reference artwork and Block Schematics conventions do not require renewed approval.

## Property block alignment

Property rows share a horizontal anchor and use Left, Middle or Right justification. Default to Left; rails and grounds default to Middle. Set optional native JSON/SVG sidecar `defaultPropertyJustification` to `left`, `middle` or `right` (omitted means Left). This default applies on placement only; instance choices survive edits, library updates, layout switching and paste. Automatic blocks sit below transformed artwork, anchored to its matching left/centre/right position. Manual anchors snap to the application grid in Preferences > Appearance, default 2 scene units. Text stays upright under component rotation and flipping; reset position preserves justification.
