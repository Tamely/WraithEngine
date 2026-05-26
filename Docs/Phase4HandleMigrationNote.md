# Phase 4 Handle Migration Note

Date: 2026-05-25

## What moved to internal handles in this slice

- `SceneObjectHandle` is now the internal stable identity for scene objects.
- `EditorSession` maintains handle-to-string and string-to-handle translation tables.
- Selection authority now stores handles internally and projects string ids back into `EditorSessionState::SelectedObjectIds`.
- Collaboration lock authority now stores handle-keyed state internally and projects string ids back into `EditorSceneState::CollaborationByObjectId`.
- `EditorSceneMeshInstance` now carries `ObjectHandle` so mesh ownership is no longer tied only to string id lookups.
- Physics runtime bodies and physics transform updates now carry `SceneObjectHandle`, so the editor runtime binding no longer depends on string ids to route simulation updates back into scene authority.
- Scene save/load now persists object handles on nodes and object records so handles survive round trips.

## Compatibility layers kept intentionally

- Browser protocol commands and events still use string `objectId` values.
- `EditorSceneItem::Id`, `EditorObjectDetails::ObjectId`, collaboration event payloads, and render-facing mesh debug ids still remain string-based for migration compatibility.
- Scene serialization still treats string ids as the public/object graph identifier and now adds handle metadata alongside them rather than replacing them.

## What still depends on string ids

- Command payloads, event payloads, and headless protocol parse/serialize paths.
- `EditorSceneState::ObjectDetailsById`.
- Generated asset child naming and asset-root linkage (`GeneratedFromAssetRootId`).
- Scripting lifecycle and internal calls still key script instances by string object id.
- Some physics logging/debug strings still report object ids even though runtime ownership is handle-backed.

## What still depends on `Instance`

- Authoritative hierarchy mutation still uses the live `Instance` tree for duplicate, delete, reparent, and tree rebuild flows.
- World transform recomputation still walks `Instance` parent chains.
- Validation for reparent cycle checks still traverses the `Instance` hierarchy.
- Mesh asset expansion/removal still mutates through `Instance` parenting before syncing the projected `Items` tree.

## Recommended next slices

- Move scripting instance binding from string object ids to `SceneObjectHandle`.
- Replace handle-era hierarchy operations with handle/array-backed parent-child storage so duplicate, delete, reparent, and transform propagation stop depending on `Instance` as an authoritative mutation structure.
- Collapse `ObjectDetailsById` and other string-keyed authority maps behind handle-keyed storage with string ids retained only as protocol and persistence projections.
