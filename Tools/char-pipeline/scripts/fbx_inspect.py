"""Minimal binary FBX inspector (stdlib only): polycount, skeleton, skin, animation, units, bounds.

Usage: python fbx_inspect.py model.fbx
"""
import struct
import sys
import zlib

ARRAY_FMT = {"f": "f", "d": "d", "l": "q", "i": "i", "b": "?"}


def read_props(buf, pos, count):
    props = []
    for _ in range(count):
        t = chr(buf[pos]); pos += 1
        if t in "YCIFDL":
            fmt = {"Y": "<h", "C": "<?", "I": "<i", "F": "<f", "D": "<d", "L": "<q"}[t]
            props.append(struct.unpack_from(fmt, buf, pos)[0]); pos += struct.calcsize(fmt)
        elif t in ARRAY_FMT:
            n, enc, clen = struct.unpack_from("<III", buf, pos); pos += 12
            raw = buf[pos:pos + clen]; pos += clen
            if enc == 1:
                raw = zlib.decompress(raw)
            props.append(struct.unpack(f"<{n}{ARRAY_FMT[t]}", raw))
        elif t in "SR":
            n = struct.unpack_from("<I", buf, pos)[0]; pos += 4
            data = buf[pos:pos + n]; pos += n
            props.append(data.decode("utf-8", "replace") if t == "S" else data)
        else:
            raise ValueError(f"unknown property type {t!r} at {pos}")
    return props


def read_node(buf, pos, wide):
    if wide:
        end, nprops, _plen = struct.unpack_from("<QQQ", buf, pos); pos += 24
    else:
        end, nprops, _plen = struct.unpack_from("<III", buf, pos); pos += 12
    nlen = buf[pos]; pos += 1
    if end == 0:
        return None, pos
    name = buf[pos:pos + nlen].decode(); pos += nlen
    props = read_props(buf, pos, nprops)
    # skip to children by re-walking props length
    node = {"name": name, "props": props, "children": []}
    pos = _props_end(buf, pos, nprops)
    while pos < end:
        child, pos = read_node(buf, pos, wide)
        if child is None:
            break
        node["children"].append(child)
    return node, end


def _props_end(buf, pos, count):
    for _ in range(count):
        t = chr(buf[pos]); pos += 1
        if t in "YCIFDL":
            pos += {"Y": 2, "C": 1, "I": 4, "F": 4, "D": 8, "L": 8}[t]
        elif t in ARRAY_FMT:
            pos += 12 + struct.unpack_from("<I", buf, pos + 8)[0]
        else:
            pos += 4 + struct.unpack_from("<I", buf, pos)[0]
    return pos


def walk(nodes):
    for n in nodes:
        yield n
        yield from walk(n["children"])


def main(path):
    buf = open(path, "rb").read()
    if not buf.startswith(b"Kaydara FBX Binary"):
        sys.exit("ASCII FBX는 지원하지 않습니다.")
    version = struct.unpack_from("<I", buf, 23)[0]
    wide = version >= 7500
    pos, top = 27, []
    while pos < len(buf) - 200:
        node, pos = read_node(buf, pos, wide)
        if node is None:
            break
        top.append(node)

    print(f"FBX version: {version}")
    for n in walk(top):
        if n["name"] == "P" and n["props"] and n["props"][0] in ("UpAxis", "FrontAxis", "UnitScaleFactor", "OriginalUnitScaleFactor"):
            print(f"  {n['props'][0]} = {n['props'][-1]}")

    meshes, tris_total, verts_total = 0, 0, 0
    lo, hi = [float("inf")] * 3, [float("-inf")] * 3
    for g in (n for n in walk(top) if n["name"] == "Geometry"):
        child = {c["name"]: c["props"][0] for c in g["children"] if c["props"]}
        if "Vertices" not in child:
            continue
        v, idx = child["Vertices"], child.get("PolygonVertexIndex", ())
        meshes += 1
        verts_total += len(v) // 3
        poly_sizes, cur = [], 0
        for i in idx:
            cur += 1
            if i < 0:
                poly_sizes.append(cur); cur = 0
        tris = sum(s - 2 for s in poly_sizes)
        tris_total += tris
        for k in range(0, len(v), 3):
            for a in range(3):
                lo[a] = min(lo[a], v[k + a]); hi[a] = max(hi[a], v[k + a])
        quads = sum(1 for s in poly_sizes if s == 4)
        print(f"Mesh {g['props'][1].split(chr(0))[0]!r}: verts={len(v)//3:,} polys={len(poly_sizes):,} "
              f"(quads={quads:,}) tris={tris:,}")

    print(f"TOTAL: meshes={meshes} verts={verts_total:,} tris={tris_total:,}")
    if meshes:
        print("Bounds (local units): " + ", ".join(f"{'XYZ'[a]} {lo[a]:.3f}..{hi[a]:.3f} (size {hi[a]-lo[a]:.3f})" for a in range(3)))

    models = [n for n in walk(top) if n["name"] == "Model" and len(n["props"]) >= 3]
    bones = [m["props"][1].split("\x00")[0] for m in models if m["props"][2] == "LimbNode"]
    deformers = [n["props"][2] for n in walk(top) if n["name"] == "Deformer" and len(n["props"]) >= 3]
    stacks = [n["props"][1].split("\x00")[0] for n in walk(top) if n["name"] == "AnimationStack"]
    curves = sum(1 for n in walk(top) if n["name"] == "AnimationCurve")
    materials = [n["props"][1].split("\x00")[0] for n in walk(top) if n["name"] == "Material"]
    print(f"Bones: {len(bones)}" + (f" e.g. {bones[:8]}" if bones else ""))
    print(f"Skin deformers: {deformers.count('Skin')}, clusters: {deformers.count('Cluster')}")
    print(f"Animation stacks: {stacks}, curves: {curves}")
    print(f"Materials: {materials}")


if __name__ == "__main__":
    main(sys.argv[1])
