"""Prepare an unrigged binary FBX for Mixamo upload.

- Drops embedded texture data (Video/Content); texture file references are kept.
- Drops BindPose objects. Tripo exports one even without bones, and Mixamo then
  fails with "unable to map your existing skeleton".
Usage: python fbx_strip_textures.py in.fbx out.fbx
"""
import struct
import sys

from fbx_inspect import _props_end

FOOTER_MAGIC = bytes.fromhex("f85a8c6adef5d97ecce90ce3758f2901")  # FBX file end marker


def copy_node(buf, pos, wide, out):
    hdr = "<QQQ" if wide else "<III"
    hsize = struct.calcsize(hdr)
    end, nprops, plen = struct.unpack_from(hdr, buf, pos)
    nlen = buf[pos + hsize]
    if end == 0:
        out += buf[pos:pos + hsize + 1]
        return pos + hsize + 1, True
    name = buf[pos + hsize + 1:pos + hsize + 1 + nlen]
    ppos = pos + hsize + 1 + nlen
    pend = _props_end(buf, ppos, nprops)
    props = buf[ppos:pend]
    if name == b"Pose" or (name == b"ObjectType" and props == b"S\x04\0\0\0Pose"):
        return end, False
    if name == b"Content":
        props = b"R" + struct.pack("<I", 0)
        nprops = 1

    start = len(out)
    out += b"\0" * hsize + bytes([nlen]) + name + props
    cpos = pend
    while cpos < end:
        cpos, was_null = copy_node(buf, cpos, wide, out)
        if was_null:
            break
    struct.pack_into(hdr, out, start, len(out), nprops, len(props))
    return end, False


def main(src, dst):
    buf = open(src, "rb").read()
    version = struct.unpack_from("<I", buf, 23)[0]
    wide = version >= 7500
    out = bytearray(buf[:27])
    pos = 27
    while True:
        pos, was_null = copy_node(buf, pos, wide, out)
        if was_null:
            break
    # footer: 16-byte id, 4 zero bytes, pad to 16, version, 120 zeros, magic
    out += buf[pos:pos + 16] + b"\0" * 4
    out += b"\0" * ((16 - len(out) % 16) % 16)
    out += struct.pack("<I", version) + b"\0" * 120 + FOOTER_MAGIC
    open(dst, "wb").write(out)
    print(f"{len(buf):,} -> {len(out):,} bytes: {dst}")


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
