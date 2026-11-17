"""Keeps an animal friend's shell fur off its eyes and clothes: patches M_<Name>Fur (run inside Unreal; round 99).

Usage: UnrealEditor-Cmd <project> -run=pythonscript "-script=ue_fur_bare.py <spec>;<spec>;..."
  spec = Name|eye z|eye dx|eye rx|eye rz|band lo|band hi|cloth colours (comma list of white,beige,green,brown,dark,red,plum,ochre; or -)
  all sizes as fractions of the body's height (SK_<Name>'s bounds, feet 0 .. top 1), as read off a front view with a
  grid (output/fur_shells/check/fronts_grid.png): the eyes' height, how far each is from the middle, and their half
  width and half height with a little room; the clothes' heights and colours.

The user (round 99): "캐릭터 눈동자나 옷에도 털 쉐이더가 붙는부분 수정". The shells were chosen face by face from each
face's middle colour (blender_fur_shells_skel.py), and on the 9,000-face game meshes a face is big: brown irises and dark
pupils counted as fur, and faces half on a shirt carried fur across its edge. Making the shells again needs each rig
and its exact options; instead the fur's own material now leaves them out, pixel by pixel:
- the eyes: where the fur's root (the pre-skinned position, mesh space: X across, Y front, Z up) lies in an ellipse round
  either eye, seen from the front, on skin that faces front (the pre-skinned normal: the back of the head is left alone);
- the clothes: within the band's heights, where the body's texture under the root is one of the clothes' colours (the
  same colour rules the shells were made with, but for each pixel).
The patch wraps what feeds the opacity mask in a Custom node "FurBare"; run again, it replaces its own node.
"""
import sys

import unreal

U = unreal
eal = U.EditorAssetLibrary
mel = U.MaterialEditingLibrary


def log(msg):
    U.log(f"FURBARE {msg}")


CODE = r"""
const float4 Eye = float4(EYE);
const float2 Band = float2(BAND);
const float4 Cloth = float4(CLOTH);
const float4 Cloth2 = float4(CLOTH2);
// The eyes: an ellipse round each, seen from the front (the rig's middle line is x = 0), on skin that faces front (+Y)
if (N.y > 0.15)
{
    float2 a = (float2(abs(P.x), P.z) - float2(Eye.x, Eye.y)) / max(float2(Eye.z, Eye.w), 0.001);
    if (dot(a, a) < 1.0) return 0;
}
// The clothes: their colours (as the texture stores them, sRGB) within their heights
if (P.z > Band.x && P.z < Band.y)
{
    float3 c = pow(saturate(Col), 1.0 / 2.2);
    float mx = max(c.r, max(c.g, c.b));
    float mn = min(c.r, min(c.g, c.b));
    float sat = (mx - mn) / max(mx, 0.001);
    bool cloth = false;
    cloth = cloth || (Cloth.r > 0.5 && sat < 0.12 && mx > 0.7);                                   // white
    cloth = cloth || (Cloth.g > 0.5 && sat < 0.32 && mx > 0.6);                                   // beige
    cloth = cloth || (Cloth.b > 0.5 && c.g >= 0.9 * c.r && c.g > c.b);                            // green
    cloth = cloth || (Cloth.a > 0.5 && c.r > c.g && c.g > c.b && c.g > 0.45 * c.r && c.g < 0.75 * c.r && mx < 0.75 && sat > 0.35);  // brown
    cloth = cloth || (Cloth2.r > 0.5 && mx < 0.18);                                               // dark
    cloth = cloth || (Cloth2.g > 0.5 && c.r > 0.45 && c.g < 0.22 && c.b < 0.22);                  // red
    cloth = cloth || (Cloth2.b > 0.5 && c.r > c.g + 0.1 && c.b > c.g && sat > 0.3 && mx < 0.62);  // plum
    cloth = cloth || (Cloth2.a > 0.5 && c.r > c.g && c.g > c.b && c.g > 0.5 * c.r && sat > 0.5 && mx < 0.9);  // ochre
    if (cloth) return 0;
}
return Mask;
"""

for spec in " ".join(sys.argv[1:]).split(";"):
    spec = spec.strip()
    if not spec:
        continue
    name, ez, edx, erx, erz, blo, bhi, cloth = spec.split("|")
    ez, edx, erx, erz, blo, bhi = (float(v) for v in (ez, edx, erx, erz, blo, bhi))
    dest = f"/Game/Characters/{name}"
    mat = U.load_asset(f"{dest}/M_{name}Fur")
    body = U.load_asset(f"{dest}/SK_{name}")
    base_tex = U.load_asset(f"{dest}/Textures/T_{name}_BaseColor")
    if not mat or not body or not base_tex:
        raise RuntimeError(f"{name}: no fur material, body or base colour")
    bounds = body.get_bounds()
    z0 = bounds.origin.z - bounds.box_extent.z
    h = bounds.box_extent.z * 2.0
    log(f"{name}: body bounds origin {bounds.origin} extent {bounds.box_extent}; height {h:.1f} from z {z0:.1f}")

    def node(cls, x, y, **props):
        n = mel.create_material_expression(mat, cls, x, y)
        for k, v in props.items():
            n.set_editor_property(k, v)
        return n

    def vec(pname, x, y, z, w, px, py):
        return node(U.MaterialExpressionVectorParameter, px, py, parameter_name=pname, default_value=U.LinearColor(x, y, z, w))

    old = mel.get_material_property_input_node(mat, U.MaterialProperty.MP_OPACITY_MASK)
    if old is None:
        raise RuntimeError(f"{name}: nothing feeds the opacity mask")
    if isinstance(old, U.MaterialExpressionCustom) and old.get_editor_property("description") == "FurBare":
        # (patched before: back to what it wrapped, the node on its first input. The old node is left in the graph,
        # joined to nothing: deleting an expression trips an engine assertion in a commandlet, and a node that feeds
        # nothing is not in the shader. For the same reason the numbers are written into the node's code, not made
        # parameters: a left-over parameter of the same name would clash)
        old = mel.get_inputs_for_material_expression(mat, old)[0]
        log(f"{name}: replacing an earlier patch")

    colours = [c for c in cloth.split(",") if c and c != "-"]
    has = lambda c: 1.0 if c in colours else 0.0
    code = (CODE.replace("EYE", f"{edx * h:.3f}, {z0 + ez * h:.3f}, {erx * h:.3f}, {erz * h:.3f}")
            .replace("BAND", f"{z0 + blo * h:.3f}, {z0 + bhi * h:.3f}")
            .replace("CLOTH2", f"{has('dark')}, {has('red')}, {has('plum')}, {has('ochre')}")
            .replace("CLOTH", f"{has('white')}, {has('beige')}, {has('green')}, {has('brown')}"))
    bare = node(U.MaterialExpressionCustom, -300, 300, code=code, output_type=U.CustomMaterialOutputType.CMOT_FLOAT1, description="FurBare")
    names = ["Mask", "P", "N", "Col"]
    ins = []
    for nm in names:
        ci = U.CustomInput()
        ci.set_editor_property("input_name", nm)
        ins.append(ci)
    bare.set_editor_property("inputs", ins)
    mel.connect_material_expressions(old, "", bare, "Mask")
    # (the pre-skinned position and normal exist only in the vertex shader: carried to the pixels through interpolators;
    # fed straight in, the material failed to compile and the fur drew in the grey default material)
    for pin, cls, py in (("P", U.MaterialExpressionPreSkinnedPosition, 500), ("N", U.MaterialExpressionPreSkinnedNormal, 580)):
        carry = node(U.MaterialExpressionVertexInterpolator, -500, py)
        mel.connect_material_expressions(node(cls, -700, py), "", carry, "")
        mel.connect_material_expressions(carry, "", bare, pin)
    tex = node(U.MaterialExpressionTextureSample, -700, 660, texture=base_tex)
    mel.connect_material_expressions(tex, "RGB", bare, "Col")
    mel.connect_material_property(bare, "", U.MaterialProperty.MP_OPACITY_MASK)
    mel.recompile_material(mat)
    eal.save_asset(mat.get_path_name())
    # (the face is on the rig's middle line, x = 0; the bounds' middle is pulled aside by the tail)
    log(f"{name}: eyes z {z0 + ez * h:.1f} x +-{edx * h:.1f} r {erx * h:.1f} x {erz * h:.1f}; band {z0 + blo * h:.1f}..{z0 + bhi * h:.1f} {colours}")
log("DONE")
