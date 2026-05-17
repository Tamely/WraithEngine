#!/usr/bin/env python3
"""Generate minimal GLB files for primitive shapes."""

import json
import math
import struct
import os

OUT_DIR = os.path.join(os.path.dirname(__file__), "..", "Content", "Primitives")


def pack_glb(positions, normals, indices):
    """Pack positions, normals, indices into a GLB buffer and return (json_dict, bin_bytes)."""
    buf = bytearray()

    def align4(b):
        rem = len(b) % 4
        if rem:
            b += b"\x00" * (4 - rem)
        return b

    pos_offset = len(buf)
    for p in positions:
        buf += struct.pack("<fff", *p)
    pos_len = len(buf) - pos_offset

    nor_offset = len(buf)
    for n in normals:
        buf += struct.pack("<fff", *n)
    nor_len = len(buf) - nor_offset

    # align to 4 bytes before indices
    buf = bytearray(align4(bytes(buf)))

    idx_offset = len(buf)
    use_u32 = len(positions) > 65535
    fmt = "<I" if use_u32 else "<H"
    for i in indices:
        buf += struct.pack(fmt, i)
    idx_len = len(buf) - idx_offset
    idx_component = 5125 if use_u32 else 5123  # UNSIGNED_INT / UNSIGNED_SHORT

    buf = bytearray(align4(bytes(buf)))
    bin_bytes = bytes(buf)

    pmin = [min(p[i] for p in positions) for i in range(3)]
    pmax = [max(p[i] for p in positions) for i in range(3)]

    gltf = {
        "asset": {"version": "2.0"},
        "scene": 0,
        "scenes": [{"nodes": [0]}],
        "nodes": [{"mesh": 0}],
        "meshes": [{
            "primitives": [{
                "attributes": {"POSITION": 0, "NORMAL": 1},
                "indices": 2,
                "mode": 4,
            }]
        }],
        "accessors": [
            {
                "bufferView": 0, "byteOffset": 0,
                "componentType": 5126, "count": len(positions),
                "type": "VEC3", "min": pmin, "max": pmax,
            },
            {
                "bufferView": 1, "byteOffset": 0,
                "componentType": 5126, "count": len(normals),
                "type": "VEC3",
            },
            {
                "bufferView": 2, "byteOffset": 0,
                "componentType": idx_component, "count": len(indices),
                "type": "SCALAR",
            },
        ],
        "bufferViews": [
            {"buffer": 0, "byteOffset": pos_offset, "byteLength": pos_len, "target": 34962},
            {"buffer": 0, "byteOffset": nor_offset, "byteLength": nor_len, "target": 34962},
            {"buffer": 0, "byteOffset": idx_offset, "byteLength": idx_len, "target": 34963},
        ],
        "buffers": [{"byteLength": len(bin_bytes)}],
    }
    return gltf, bin_bytes


def write_glb(name, positions, normals, indices):
    gltf, bin_bytes = pack_glb(positions, normals, indices)
    json_bytes = json.dumps(gltf, separators=(",", ":")).encode("utf-8")
    # pad to 4 bytes with spaces
    rem = len(json_bytes) % 4
    if rem:
        json_bytes += b" " * (4 - rem)

    json_chunk = struct.pack("<II", len(json_bytes), 0x4E4F534A) + json_bytes
    bin_chunk  = struct.pack("<II", len(bin_bytes),  0x004E4942) + bin_bytes
    total = 12 + len(json_chunk) + len(bin_chunk)
    header = struct.pack("<III", 0x46546C67, 2, total)

    path = os.path.join(OUT_DIR, name + ".glb")
    with open(path, "wb") as f:
        f.write(header + json_chunk + bin_chunk)
    print(f"Written {path} ({total} bytes)")


def make_cube():
    # 6 faces, 4 verts each, 2 tris each
    face_defs = [
        # normal,        4 corners (position offsets)
        ((0, 0, 1),  [(-1,-1,1),(1,-1,1),(1,1,1),(-1,1,1)]),
        ((0, 0,-1),  [(1,-1,-1),(-1,-1,-1),(-1,1,-1),(1,1,-1)]),
        ((0, 1, 0),  [(-1,1,-1),(1,1,-1),(1,1,1),(-1,1,1)]),
        ((0,-1, 0),  [(-1,-1,1),(1,-1,1),(1,-1,-1),(-1,-1,-1)]),
        ((1, 0, 0),  [(1,-1,-1),(1,-1,1),(1,1,1),(1,1,-1)]),
        ((-1,0, 0),  [(-1,-1,1),(-1,-1,-1),(-1,1,-1),(-1,1,1)]),
    ]
    positions, normals, indices = [], [], []
    for n, corners in face_defs:
        base = len(positions)
        for c in corners:
            positions.append(tuple(x * 0.5 for x in c))
            normals.append(n)
        indices += [base, base+1, base+2, base, base+2, base+3]
    return positions, normals, indices


def make_sphere(segments=32, rings=16):
    positions, normals = [], []
    for r in range(rings + 1):
        phi = math.pi * r / rings
        for s in range(segments + 1):
            theta = 2 * math.pi * s / segments
            x = math.sin(phi) * math.cos(theta)
            y = math.cos(phi)
            z = math.sin(phi) * math.sin(theta)
            positions.append((x * 0.5, y * 0.5, z * 0.5))
            normals.append((x, y, z))

    indices = []
    stride = segments + 1
    for r in range(rings):
        for s in range(segments):
            a = r * stride + s
            b = a + 1
            c = (r + 1) * stride + s
            d = c + 1
            indices += [a, c, b, b, c, d]
    return positions, normals, indices


def make_cylinder(segments=32):
    positions, normals, indices = [], [], []

    # Side
    for s in range(segments + 1):
        theta = 2 * math.pi * s / segments
        x = math.cos(theta)
        z = math.sin(theta)
        for y in (-0.5, 0.5):
            positions.append((x * 0.5, y, z * 0.5))
            normals.append((x, 0, z))

    side_count = (segments + 1) * 2
    for s in range(segments):
        a = s * 2
        b = a + 1
        c = a + 2
        d = a + 3
        indices += [a, c, b, b, c, d]

    # Top cap
    top_center = len(positions)
    positions.append((0, 0.5, 0)); normals.append((0, 1, 0))
    top_ring_start = len(positions)
    for s in range(segments):
        theta = 2 * math.pi * s / segments
        positions.append((math.cos(theta) * 0.5, 0.5, math.sin(theta) * 0.5))
        normals.append((0, 1, 0))
    for s in range(segments):
        indices += [top_center, top_ring_start + s, top_ring_start + (s+1) % segments]

    # Bottom cap
    bot_center = len(positions)
    positions.append((0, -0.5, 0)); normals.append((0, -1, 0))
    bot_ring_start = len(positions)
    for s in range(segments):
        theta = 2 * math.pi * s / segments
        positions.append((math.cos(theta) * 0.5, -0.5, math.sin(theta) * 0.5))
        normals.append((0, -1, 0))
    for s in range(segments):
        indices += [bot_center, bot_ring_start + (s+1) % segments, bot_ring_start + s]

    return positions, normals, indices


def make_cone(segments=32):
    positions, normals, indices = [], [], []

    apex = (0, 0.5, 0)
    slope = 0.5  # sin of half-angle for normal blending

    # Side: apex + ring
    apex_idx = len(positions)
    # We'll duplicate apex per segment for correct normals
    ring_start = len(positions)
    for s in range(segments + 1):
        theta = 2 * math.pi * s / segments
        x = math.cos(theta)
        z = math.sin(theta)
        # Side normal points outward and up at 45°
        nx = x * slope
        nz = z * slope
        ny = slope
        ln = math.sqrt(nx*nx + ny*ny + nz*nz)
        positions.append((x * 0.5, -0.5, z * 0.5))  # base ring
        normals.append((nx/ln, ny/ln, nz/ln))
    apex_ring_start = len(positions)
    for s in range(segments + 1):
        theta = 2 * math.pi * s / segments
        x = math.cos(theta); z = math.sin(theta)
        nx = x * slope; nz = z * slope; ny = slope
        ln = math.sqrt(nx*nx + ny*ny + nz*nz)
        positions.append(apex)
        normals.append((nx/ln, ny/ln, nz/ln))

    for s in range(segments):
        a = ring_start + s
        b = ring_start + s + 1
        c = apex_ring_start + s
        indices += [a, b, c]

    # Base cap
    base_center = len(positions)
    positions.append((0, -0.5, 0)); normals.append((0, -1, 0))
    base_ring_start = len(positions)
    for s in range(segments):
        theta = 2 * math.pi * s / segments
        positions.append((math.cos(theta) * 0.5, -0.5, math.sin(theta) * 0.5))
        normals.append((0, -1, 0))
    for s in range(segments):
        indices += [base_center, base_ring_start + (s+1) % segments, base_ring_start + s]

    return positions, normals, indices


def make_plane():
    positions = [(-0.5, 0, -0.5), (0.5, 0, -0.5), (0.5, 0, 0.5), (-0.5, 0, 0.5)]
    normals   = [(0, 1, 0)] * 4
    indices   = [0, 2, 1, 0, 3, 2]
    return positions, normals, indices


os.makedirs(OUT_DIR, exist_ok=True)
write_glb("Cube",     *make_cube())
write_glb("Sphere",   *make_sphere())
write_glb("Cylinder", *make_cylinder())
write_glb("Cone",     *make_cone())
write_glb("Plane",    *make_plane())
print("Done.")
