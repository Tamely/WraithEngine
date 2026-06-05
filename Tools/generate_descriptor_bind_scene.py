#!/usr/bin/env python3

import json
import pathlib
import struct
import sys


def write_shared_material_asset(content_root: pathlib.Path) -> None:
    positions = [
        (-0.5, -0.5, 0.5),
        (0.5, -0.5, 0.5),
        (0.5, 0.5, 0.5),
        (-0.5, 0.5, 0.5),
        (-0.5, -0.5, -0.5),
        (-0.5, 0.5, -0.5),
        (0.5, 0.5, -0.5),
        (0.5, -0.5, -0.5),
        (-0.5, 0.5, -0.5),
        (-0.5, 0.5, 0.5),
        (0.5, 0.5, 0.5),
        (0.5, 0.5, -0.5),
        (-0.5, -0.5, -0.5),
        (0.5, -0.5, -0.5),
        (0.5, -0.5, 0.5),
        (-0.5, -0.5, 0.5),
        (0.5, -0.5, -0.5),
        (0.5, 0.5, -0.5),
        (0.5, 0.5, 0.5),
        (0.5, -0.5, 0.5),
        (-0.5, -0.5, -0.5),
        (-0.5, -0.5, 0.5),
        (-0.5, 0.5, 0.5),
        (-0.5, 0.5, -0.5),
    ]
    normals = [
        (0.0, 0.0, 1.0),
        (0.0, 0.0, 1.0),
        (0.0, 0.0, 1.0),
        (0.0, 0.0, 1.0),
        (0.0, 0.0, -1.0),
        (0.0, 0.0, -1.0),
        (0.0, 0.0, -1.0),
        (0.0, 0.0, -1.0),
        (0.0, 1.0, 0.0),
        (0.0, 1.0, 0.0),
        (0.0, 1.0, 0.0),
        (0.0, 1.0, 0.0),
        (0.0, -1.0, 0.0),
        (0.0, -1.0, 0.0),
        (0.0, -1.0, 0.0),
        (0.0, -1.0, 0.0),
        (1.0, 0.0, 0.0),
        (1.0, 0.0, 0.0),
        (1.0, 0.0, 0.0),
        (1.0, 0.0, 0.0),
        (-1.0, 0.0, 0.0),
        (-1.0, 0.0, 0.0),
        (-1.0, 0.0, 0.0),
        (-1.0, 0.0, 0.0),
    ]
    uvs = [(0.0, 0.0), (1.0, 0.0), (1.0, 1.0), (0.0, 1.0)] * 6
    indices = [
        0, 1, 2, 0, 2, 3,
        4, 5, 6, 4, 6, 7,
        8, 9, 10, 8, 10, 11,
        12, 13, 14, 12, 14, 15,
        16, 17, 18, 16, 18, 19,
        20, 21, 22, 20, 22, 23,
    ]

    buffer = bytearray()

    def append_floats(values):
        for value in values:
            buffer.extend(struct.pack("<f", value))

    pos_offset = len(buffer)
    for vertex in positions:
        append_floats(vertex)
    while len(buffer) % 4:
        buffer.append(0)

    normal_offset = len(buffer)
    for normal in normals:
        append_floats(normal)
    while len(buffer) % 4:
        buffer.append(0)

    uv_offset = len(buffer)
    for uv in uvs:
        append_floats(uv)
    while len(buffer) % 4:
        buffer.append(0)

    index_offset = len(buffer)
    for index in indices:
        buffer.extend(struct.pack("<H", index))

    mins = [min(vertex[i] for vertex in positions) for i in range(3)]
    maxs = [max(vertex[i] for vertex in positions) for i in range(3)]

    meshes = []
    for material_index in range(3):
        meshes.append(
            {
                "primitives": [
                    {
                        "attributes": {
                            "POSITION": 0,
                            "NORMAL": 1,
                            "TEXCOORD_0": 2,
                        },
                        "indices": 3,
                        "material": material_index,
                    }
                ]
            }
        )

    nodes = []
    scene_nodes = []
    for index in range(100):
        nodes.append(
            {
                "mesh": index % 3,
                "translation": [float(index % 10) * 2.0, 0.0, float(index // 10) * 2.0],
            }
        )
        scene_nodes.append(index)

    gltf = {
        "asset": {"version": "2.0"},
        "scene": 0,
        "scenes": [{"nodes": scene_nodes}],
        "nodes": nodes,
        "meshes": meshes,
        "materials": [
            {
                "pbrMetallicRoughness": {
                    "baseColorFactor": [1.0, 0.2, 0.2, 1.0],
                    "metallicFactor": 0.0,
                    "roughnessFactor": 0.8,
                }
            },
            {
                "pbrMetallicRoughness": {
                    "baseColorFactor": [0.2, 1.0, 0.2, 1.0],
                    "metallicFactor": 0.0,
                    "roughnessFactor": 0.8,
                }
            },
            {
                "pbrMetallicRoughness": {
                    "baseColorFactor": [0.2, 0.2, 1.0, 1.0],
                    "metallicFactor": 0.0,
                    "roughnessFactor": 0.8,
                }
            },
        ],
        "buffers": [
            {"uri": "shared_materials_cube.bin", "byteLength": len(buffer)}
        ],
        "bufferViews": [
            {
                "buffer": 0,
                "byteOffset": pos_offset,
                "byteLength": len(positions) * 12,
                "target": 34962,
            },
            {
                "buffer": 0,
                "byteOffset": normal_offset,
                "byteLength": len(normals) * 12,
                "target": 34962,
            },
            {
                "buffer": 0,
                "byteOffset": uv_offset,
                "byteLength": len(uvs) * 8,
                "target": 34962,
            },
            {
                "buffer": 0,
                "byteOffset": index_offset,
                "byteLength": len(indices) * 2,
                "target": 34963,
            },
        ],
        "accessors": [
            {
                "bufferView": 0,
                "componentType": 5126,
                "count": len(positions),
                "type": "VEC3",
                "min": mins,
                "max": maxs,
            },
            {
                "bufferView": 1,
                "componentType": 5126,
                "count": len(normals),
                "type": "VEC3",
            },
            {
                "bufferView": 2,
                "componentType": 5126,
                "count": len(uvs),
                "type": "VEC2",
            },
            {
                "bufferView": 3,
                "componentType": 5123,
                "count": len(indices),
                "type": "SCALAR",
                "min": [0],
                "max": [max(indices)],
            },
        ],
    }

    (content_root / "shared_materials_cube.bin").write_bytes(buffer)
    (content_root / "shared_materials_cube.gltf").write_text(
        json.dumps(gltf, indent=2) + "\n", encoding="utf-8"
    )


def write_scene(content_root: pathlib.Path) -> None:
    scene = {
        "version": 1,
        "meshAsset": "",
        "nodes": [
            {
                "id": "world",
                "parentId": None,
                "displayName": "World",
                "kind": "Folder",
                "visible": True,
            },
            {
                "id": "lighting",
                "parentId": "world",
                "displayName": "Lighting",
                "kind": "Folder",
                "visible": True,
            },
            {
                "id": "directional-light",
                "parentId": "lighting",
                "displayName": "DirectionalLight",
                "kind": "Light",
                "visible": True,
            },
            {
                "id": "GridAsset",
                "parentId": "world",
                "displayName": "GridAsset",
                "kind": "Mesh",
                "visible": True,
            },
        ],
        "objects": [
            {
                "id": "world",
                "displayName": "World",
                "kind": "Folder",
                "visible": True,
                "isGeneratedAssetChild": False,
                "supportsTransform": False,
                "transformReadOnly": True,
            },
            {
                "id": "lighting",
                "displayName": "Lighting",
                "kind": "Folder",
                "visible": True,
                "isGeneratedAssetChild": False,
                "supportsTransform": False,
                "transformReadOnly": True,
            },
            {
                "id": "directional-light",
                "displayName": "DirectionalLight",
                "kind": "Light",
                "visible": True,
                "isGeneratedAssetChild": False,
                "supportsTransform": True,
                "transformReadOnly": False,
                "location": [0.70909, 25.0, -8.0],
                "rotationDegrees": [-45.0, 30.0, 0.0],
                "scale": [1.0, 1.0, 1.0],
                "lightColor": [1.0, 0.98, 0.92],
                "lightIntensity": 4.0,
                "lightDirection": [0.35, 0.7, 0.2],
            },
            {
                "id": "GridAsset",
                "displayName": "GridAsset",
                "kind": "Mesh",
                "visible": True,
                "isGeneratedAssetChild": False,
                "supportsTransform": True,
                "transformReadOnly": False,
                "location": [-9.0, 0.0, -22.0],
                "rotationDegrees": [0.0, 0.0, 0.0],
                "scale": [1.0, 1.0, 1.0],
                "assetRelativePath": "shared_materials_cube.gltf",
            },
        ],
        "meshNameToObjectId": {},
    }
    (content_root / "scene.json").write_text(
        json.dumps(scene, indent=2) + "\n", encoding="utf-8"
    )


def ensure_engine_symlink(content_root: pathlib.Path, repo_root: pathlib.Path) -> None:
    source = repo_root / "Content" / "Engine"
    target = content_root / "Engine"
    if target.exists() or target.is_symlink():
        return
    target.symlink_to(source)


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: generate_descriptor_bind_scene.py <output-root>", file=sys.stderr)
        return 1

    output_root = pathlib.Path(sys.argv[1]).resolve()
    repo_root = pathlib.Path(__file__).resolve().parents[1]
    content_root = output_root / "Content"
    content_root.mkdir(parents=True, exist_ok=True)

    ensure_engine_symlink(content_root, repo_root)
    write_shared_material_asset(content_root)
    write_scene(content_root)

    print(output_root)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
