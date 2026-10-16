"""Convert a static (unrigged) binary FBX mesh to OBJ + MTL, for Mixamo upload.

OBJ has no skeleton/pose concept, so Mixamo cannot mistake it for an already-rigged model.
Writes raw geometry coordinates (node transforms are ignored; Tripo stores Y-up geometry,
which is what OBJ/Mixamo expect).
Usage: python fbx_to_obj.py in.fbx out.obj [basecolor texture file name]
"""
import struct
import sys
from pathlib import Path

from fbx_inspect import read_node


def load(path):
    buf = open(path, "rb").read()
    wide = struct.unpack_from("<I", buf, 23)[0] >= 7500
    pos, top = 27, []
    while pos < len(buf) - 200:
        node, pos = read_node(buf, pos, wide)
        if node is None:
            break
        top.append(node)
    return top


def child(node, name):
    return next((c for c in node["children"] if c["name"] == name), None)


def value(node, name, default=None):
    c = child(node, name)
    return c["props"][0] if c and c["props"] else default


def layer(geom, layer_name, data_name, index_name, poly_index, corners):
    """Return per-corner index into the layer's data array, plus the flat data array."""
    el = child(geom, layer_name)
    if el is None:
        return None, None
    data = value(el, data_name)
    mapping = value(el, "MappingInformationType")
    ref = value(el, "ReferenceInformationType")
    index = value(el, index_name) if ref == "IndexToDirect" else None
    if mapping == "ByPolygonVertex":
        per_corner = list(range(corners))
    elif mapping in ("ByVertex", "ByVertice", "ByControlPoint"):
        per_corner = list(poly_index)
    else:
        raise ValueError(f"{layer_name}: unsupported mapping {mapping}")
    if index is not None:
        per_corner = [index[i] for i in per_corner]
    return per_corner, data


def main(src, dst, texture=None):
    top = load(src)
    objects = next(n for n in top if n["name"] == "Objects")
    geom = next(c for c in objects["children"] if c["name"] == "Geometry" and child(c, "Vertices"))
    verts = value(geom, "Vertices")
    raw = value(geom, "PolygonVertexIndex")
    poly_index = [i if i >= 0 else ~i for i in raw]
    uv_idx, uvs = layer(geom, "LayerElementUV", "UV", "UVIndex", poly_index, len(raw))
    n_idx, normals = layer(geom, "LayerElementNormal", "Normals", "NormalsIndex", poly_index, len(raw))

    dst = Path(dst)
    mtl = dst.with_suffix(".mtl")
    lines = [f"mtllib {mtl.name}", "o character"]
    lines += [f"v {verts[i]:.6f} {verts[i+1]:.6f} {verts[i+2]:.6f}" for i in range(0, len(verts), 3)]
    if uvs:
        lines += [f"vt {uvs[i]:.6f} {uvs[i+1]:.6f}" for i in range(0, len(uvs), 2)]
    if normals:
        lines += [f"vn {normals[i]:.6f} {normals[i+1]:.6f} {normals[i+2]:.6f}" for i in range(0, len(normals), 3)]
    lines.append("usemtl character")

    face = []
    for corner, i in enumerate(raw):
        v = (~i if i < 0 else i) + 1
        t = uv_idx[corner] + 1 if uvs else ""
        n = n_idx[corner] + 1 if normals else ""
        face.append(f"{v}/{t}/{n}" if normals else (f"{v}/{t}" if uvs else f"{v}"))
        if i < 0:
            lines.append("f " + " ".join(face))
            face = []
    dst.write_text("\n".join(lines) + "\n")
    mtl.write_text("newmtl character\nKd 1 1 1\n" + (f"map_Kd {texture}\n" if texture else ""))
    print(f"{dst}: {len(verts)//3:,} verts, {sum(1 for l in lines if l.startswith('f ')):,} faces, "
          f"uv={'yes' if uvs else 'no'}, normals={'yes' if normals else 'no'}")


if __name__ == "__main__":
    main(*sys.argv[1:])
