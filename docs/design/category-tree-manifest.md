# Category-tree symbol manifests

Status: **approved, implemented and verified on 2026-11-07.**

## Structure and ordering

The collection manifest contains an ordered `categories` array of nodes. A node has a single-segment `name`, optional ordered `symbols` filenames, and optional ordered `children` nodes. A node may contain symbols, children, both, or neither. Empty nodes explicitly declare an empty category.

```json
{
  "categories": [
    {
      "name": "Semiconductors",
      "children": [
        {
          "name": "Diodes",
          "symbols": ["diode.json", "schottky-diode.json", "zener-diode.json", "led.json"]
        },
        { "name": "Transistors" },
        {
          "name": "ICs",
          "children": [
            { "name": "Op Amps", "symbols": ["opamp-grid10.json", "opamp-no-supplies.json"] }
          ]
        }
      ]
    }
  ]
}
```

Array order determines sibling/category order and tile order within a category. Traversal is preorder: a node's own symbols precede its children. Parent views use that same ordering for their section headings. Hidden variants remain listed beside their visible counterparts and are skipped only when producing palette actions/tiles. The [packaged manifest](../../core/symbols/manifest.json) is the current complete example.

## Authority and validation

The manifest owns collection placement. Each native symbol retains a category path for standalone import/export; collection loading requires an exact match with its manifest-derived path, rather than silently overriding either value. Symbol IDs, terminal interfaces, source artwork and the v7 document format do not change.

Reject duplicate sibling names/full paths, duplicate filenames anywhere in a collection, duplicate symbol IDs, malformed node/array types, unsafe filenames, missing files and mismatching category paths. Names are non-empty trimmed single segments without slash or line-break characters. The same child name may occur beneath different parents. Filenames are safe `.json` basenames; source SVG/sidecar paths are managed separately. Reject mixed flat/tree manifests. A depth limit rejects excessively nested trees.

## Core and palette

`SymbolManifest` is the Qt Core parser/writer shared by packaged and writable collection handling. It exposes ordered category nodes and file/category entries. `SymbolLibrary` stores an ordered tree of category nodes and symbol IDs; lookup remains by stable ID.

The Widgets tree displays `SymbolLibrary::categoryTree()` directly, with no JSON parsing or hierarchy reconstruction in the UI. Tiles and parent sections use `paletteSymbols()` from the same tree. Programmatically added/imported entries are appended to their category; reimport in the same category retains the slot, while changing category removes the old membership and appends to the new node. Explicit empty nodes remain visible; undeclared branches containing only hidden symbols are omitted.

Multiple collections merge deterministically: the packaged tree establishes existing sibling order; writable collections append new siblings in their manifest order and append new symbols within matching categories. Compatible overrides retain the existing symbol slot unless their category changes. Loading an invalid writable collection leaves the live registry unchanged.

## Writable libraries and compatibility

The old `{ "symbols": [...], "categories": ["Path", ...] }` structure remains readable. Its symbol definitions supply category membership. Reading alone does not rewrite it. On a successful writable-library save, resolve the flat entries into a tree and write the nested form, preserving within-category order, declared empty categories and root-level `overrides`. Until file membership has been resolved, serializing a parsed legacy manifest retains its flat entries instead of dropping them.

Saving updates a symbol's location in the tree and preserves unaffected nodes/order. Native definitions, source files and manifest are committed through the existing staged batch/rollback path, with the manifest installed last. Cancellation or write failure restores the previous manifest and files. Standalone symbols and embedded document definitions still use their existing category strings and identity rules; documents do not embed collection manifests.

## Verification

Focused tests cover tree parsing/round trips, parent symbols plus children, explicit empty categories, hidden variants, category and symbol ordering, replacement across categories, strict category matching, duplicate/malformed/unsafe input, legacy reading and save-time upgrade, stable reimport slots, category moves, live/reloaded order, writable-load atomicity, staged rollback and overrides. UI tests deliberately add symbols in a different global order and verify the tree, parent headings and tile order follow the category model.

Release application and all test targets built with the local MSVC / Qt 6.11.1 kit. All seven Release CTest suites passed with the offscreen backend after the final implementation. Focused tests cover legacy serialization without entry loss, flat-to-tree writable upgrades, reimport/move order, malformed-input rejection, atomic writable loading, existing rollback/override behavior, and actual category/section/tile widget order. All 40 packaged placements were also checked against their native definition category paths. Documentation links and Markdown diff passed. No native manual walkthrough was performed for this change.
