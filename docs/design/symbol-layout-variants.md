# Symbol layout variants: OTA bias position

Status: **approved, implemented and verified on 2026-11-07.**

## Scope

Add Bias position: Above / Below to Properties for a single selected OTA. Use ordinary authored symbol definitions, with and without supply terminals. Do not introduce package grouping, scripts, arbitrary parameters, supply-terminal switching, or general symbol replacement in this step.

## Symbols and metadata

- Keep the two current OTA IDs and terminal interfaces as the Below layouts. Add an Above definition for each supply configuration, with its own stable ID.
- Above changes only the bias lead: Iabc is at (30, -20), joined vertically to the upper circle intersection (30, -sqrt(20)). Below remains at (30, 20), joined to the lower intersection. Inputs, output, supplies, body and output circles are unchanged.
- Add optional layout-control metadata to native symbol definitions. Each control has a stable key, display label, current option and ordered choices. A choice names its target symbol ID and an explicit source-to-target terminal-name mapping. No OTA-specific branching belongs in the document or Properties implementation.
- Validate unique control/option keys, a matching current option, and complete one-to-one terminal mappings when source and target definitions are available. This first step permits only layout changes with the same terminal roles/count; adding or removing supplies is deferred.
- The original plan kept all four OTA layouts in the palette. The user-approved follow-up hides the two Above alternatives using optional `paletteVisible: false`. The palette now shows **OTA (With Supplies)** and **OTA (No Supplies)**, both initially Below; all four definitions remain ordinary library entries.

## Document edit and wires

- The selected symbol ID is the layout state; do not duplicate it in a free-text property or a separate instance flag.
- Add a model operation that selects a declared layout choice. Resolve the target definition, stage any library additions and component/wire edits on a candidate Document, and commit only after all checks pass.
- Preserve component ID, position, rotation, mirroring, reference/value/custom properties, their visibility and property-block offset. Existing automatic label placement may follow changed artwork bounds; explicit offsets remain fixed.
- Resolve terminal mapping by explicit names to source/target indices, and update incident endpoint references as needed. Component identities, wire IDs, nodes and connectivity remain intact. The OTA mapping is identity by role, including Iabc.
- Reuse deterministic orthogonal endpoint repair and validation from component movement. Resolve positions through the component's existing mirror/rotation transform. The rest of the symbol does not flip when bias position changes.
- Reject unknown/unavailable choices, invalid mappings or invalid repaired routes without changing any part of the document. A no-op produces no undo entry.

## Properties and undo

- Show a separate labelled dropdown for declared layout controls above the text-property table, with no canvas visibility checkbox. It is not a custom text property.
- Initially enable it only for a single component and outside an active move. Multiple-selection layout edits are deferred.
- Follow the inspector's existing pending-text/selection-epoch handling, then commit the change as one snapshot-based undo action. Refresh the scene and retain selection; selection red highlighting remains intact.
- Show an actionable error and retain the previous selection if the edit fails. Above/Below refer to the untransformed symbol pose, as documented in the tooltip.

## Persistence and compatibility

- Introduce format v7 for embedded layout metadata and its referenced alternative definitions; continue reading v1-v6. Writers embed the transitive closure of layout alternatives for every used symbol, so the control works after reopening without the installed library.
- Validate embedded variant relationships using the document's own definitions. Use existing symbol-conflict review rather than silently combining different installed and embedded versions. Missing required embedded alternatives in v7 are a load error.
- Extend symbol JSON and SVG-sidecar metadata round trips to preserve the optional controls; SVG artwork remains unchanged. Incomplete standalone imports preserve unresolved choices in the library, but placement requires their complete compatible family; document creation/updates reject unresolved controls rather than writing an incomplete file.
- Existing documents retain their symbol appearance, terminal coordinates and connections. Older embedded definitions without layout metadata remain ordinary symbols; do not silently replace their snapshots or infer capabilities from ID spelling. The existing explicit library-update flow can adopt the revised definition when desired.

## Reviewable implementation steps and verification

1. Add metadata types, parsing/validation and Above symbol data, with terminal and artwork tests.
2. Add atomic model switching and focused tests for every connected terminal, transformed instances, branches, failed route rollback and no-op behavior.
3. Add v7 persistence and closure checks, compatibility fixtures, missing/invalid metadata rejection, and symbol import/export round trips.
4. Add the single-selection Properties dropdown and checks for selection changes, pending edits, undo/redo and restored red highlighting.
5. Update the style guide, decisions, roadmap and format documentation. Build the app, run affected existing suites, and inspect both bias positions rendered with and without supplies.

The main lasting choices are metadata embedded with symbol definitions, explicit terminal-role mapping, and selecting an authored symbol ID rather than introducing a second instance geometry system.

## Implemented metadata contract

Optional native/sidecar `paletteVisible` defaults to true. False hides a symbol from palette actions and tiles and from symbol-derived categories, while keeping lookup, rendering, layout switching and embedded snapshots unchanged. The serializer omits the true default; non-boolean values are rejected. The two Above OTA alternatives use false. Declared empty categories remain supported through the [nested manifest tree](category-tree-manifest.md).

The optional native/sidecar `layoutControls` array contains objects with `key`, `label`, `current`, and `choices`. A choice contains `key`, `label`, `symbol` (target ID), and `terminals`, an array of `{ "from": "Iabc", "to": "Iabc" }` mappings covering **every** source terminal. Keys use lower-case identifiers (`^[a-z][a-z0-9_]*$`); labels are non-empty single-line strings. The current choice must target the containing symbol with identity mappings. Target controls must declare the same control key and the chosen current option; terminal mappings must cover both interfaces one-to-one.

The first control is `bias_position`, labelled **Bias position**, with ordered choices `below` / **Below** and `above` / **Above**. Supply configurations link only to their matching counterpart:

| Configuration | Below ID | Above ID |
|---|---|---|
| With supplies | `OTA.WithSupplies.OutputSource.VerticalBias` | `OTA.WithSupplies.OutputSource.BiasAbove` |
| No supplies | `OTA.NoSupplies.OutputSource.VerticalBias` | `OTA.NoSupplies.OutputSource.BiasAbove` |

Native parsing validates each entry independently. Collection loading also checks mappings whose targets are present. Placement and document symbol updates require the complete reachable family. V7 loading resolves and validates the closure exclusively from embedded definitions and rejects unrelated unused definitions. Headless loading retains embedded definitions and reports installed-library conflicts through the existing review mechanism.

Clipboard version 1 now writes v7 document fragments and still accepts v6 fragments. Copy includes the source document's alternative snapshots, and paste checks conflicts for all of them before committing. The layout selection is represented by the component's symbol ID, so no extra instance field or geometry path is introduced.

## Verification record (2026-11-07)

- Release application and test targets built with the local MSVC / Qt 6.11.1 kit. All seven Release CTest suites passed with the offscreen platform; after the final inspector/import validation changes, the affected document-open suite passed again.
- Core tests cover metadata validation, native and SVG-sidecar round trips, all bias/supply choices, every rotation/mirror pose, connected terminals, explicit index remapping, branch preservation, invalid-route rollback, no-ops, standalone v7 loading without an installed library, v6 snapshot preservation, malformed/missing alternatives and self-contained clipboard paste.
- Inspector checks cover dropdown presence/visibility, Above/Below switching, one-step layout undo/redo, preserved selection/red highlighting, pending text, stale context cancellation, multi-selection hiding, and failed-edit feedback with no history/document change.
- Visually inspected captured offscreen OTA palettes and inspector rendering. The offscreen environment renders text with placeholder glyphs, so readable labels/options are checked through widget text assertions. A native Windows inspector run could not activate its window (`qWaitForWindowActive` failed before interaction); no native interactive walkthrough is claimed.
- Documentation links and Markdown diff checked. Existing document snapshots and the original Below terminal interfaces are preserved.

### Palette visibility follow-up (2026-11-07)

The user approved hiding the Above alternatives and simplifying the visible OTA names. Release application build and all seven offscreen CTest suites pass. Coverage checks true-by-default/strict-boolean metadata, native and SVG-sidecar preservation, hidden-definition lookup and saved layout switching, absent palette actions for Above, and the four-tile Op Amps palette. Visually inspected the reduced palette capture.
