"""Bring an animal friend's shell fur in (run inside Unreal, legacy FBX importer; round 95).

Usage: UnrealEditor-Cmd <project> -run=pythonscript -dpcvars=Interchange.FeatureFlags.Import.FBX=0
           "-script=ue_import_fur_shells.py <Name>|<fur fbx>|<strands png>[|<fur length cm, 3.6>[|<face>]]"
  <fur fbx> / <strands png>: blender_fur_shells_skel.py's output (SK_<Name>Fur.fbx and SK_<Name>Fur_strands.png).

Makes, in /Game/Characters/<Name>:
- SK_<Name>Fur on SK_<Name>_Skeleton (vertex colours kept: R = height in the fur, G = edge fade, B = 1 on the base layer;
  UV1 = the even fur unwrap). URRRandomFurComponent draws it over the body, following its pose.
- Textures/T_<Name>FurStrands (linear, masks): R distance to the strand's centre, G its length, B a random phase.
- M_<Name>Fur: masked, flat like the body (roughness 1, no specular), base colour = T_<Name>_BaseColor darkened at the
  roots ("RootShade") and on the base layer under the fur. Opacity = round strands that narrow to the tip, fewer layers
  far away ("FullLayersTo" / "FewLayersFrom", cm). World position offset = the tip-weighted (h^2) bend: the game's
  push ("FurPush", custom primitive data 0-2, in fur lengths, world space: drag, inertia, spring) plus a scrolling
  wind noise ("WindDir", "WindStrength"), capped at 1.3 fur lengths, times "FurLength" (cm).
- Tidy up close (2026-11-20, the "FurTidy" node): seen from the front the long strands stand apart and the eye looks
  down the gaps to the roots, more so the nearer the camera. So where the fur faces the camera ("TidyFacingFrom" ..
  "TidyFacingFull": the cosine between its normal and the way to the camera) and the camera is near ("TidyNear" cm and
  nearer: fully, "TidyFar" and further: not at all; the distance a 90 degree view would show it this big from, so a
  long lens counts as near too), the long strands shrink to "TidyLength" of their height and to "TidyCoarse" of their
  width, and a short fine undercoat grows in between ("TidyFine" x as many strands across, "TidyFineRadius" wide,
  "TidyLength" high), shaded more evenly ("TidyEven"). The fur at the silhouette, seen from the side, stays as it was.
  Facing alone tidies only by "TidyFacing" (0.55: a cheek seen from the front keeps its grain; at 1 it went smooth).
  With <face> ("x,y,z,rx,ry,rz": an ellipsoid in the mesh's own space, "TidyFaceAt" / "TidyFaceSize") the inner face
  (muzzle, mouth, round the eyes) is tidy up close whichever way it is seen from, its edge fading from "TidyFaceEdge".
  "TidyShow" 1 paints the tidy share pink, to place the ellipsoid.
  "TidyNear" = "TidyFar" = -1 turns it off.

<fur fbx> and <strands png> may be empty ("Fox|||"): only the material is rebuilt, on the mesh and texture already
there, and it keeps the old material's "RootShade", "FurLength", "FullLayersTo", "FewLayersFrom", "WindStrength".
With no <face>, the inner face on the old material (if it had one) is kept, whether or not the mesh is brought in again.
"""
import sys

import unreal

U = unreal
args = " ".join(sys.argv[1:]).split("|")
name, fbx, strands = args[:3]
fur_cm = float(args[3]) if len(args) > 3 and args[3] else 3.6  # the fur length in the game: 2% of the model x its MeshScale (fox 1.8)
# <face>: the inner face, six numbers "x,y,z,rx,ry,rz" in the mesh's own units (the middle and the radii of an ellipsoid)
face = [float(v) for v in args[4].split(",")] if len(args) > 4 and args[4] else [0.0, 0.0, 0.0, 1.0, 1.0, 1.0]
dest = f"/Game/Characters/{name}"
eal = U.EditorAssetLibrary
mel = U.MaterialEditingLibrary
tools = U.AssetToolsHelpers.get_asset_tools()


def log(msg):
    U.log_warning(f"FURSHELLS {msg}")


skeleton = U.load_asset(f"{dest}/SK_{name}_Skeleton")
if not skeleton:
    raise RuntimeError(f"no {dest}/SK_{name}_Skeleton")

if fbx:
    ui = U.FbxImportUI()
    ui.automated_import_should_detect_type = False
    ui.import_as_skeletal = True
    ui.import_mesh = True
    ui.import_animations = False
    ui.import_materials = False
    ui.import_textures = False
    ui.create_physics_asset = False
    ui.mesh_type_to_import = U.FBXImportType.FBXIT_SKELETAL_MESH
    ui.skeleton = skeleton
    ui.skeletal_mesh_import_data.set_editor_property("vertex_color_import_option", U.VertexColorImportOption.REPLACE)
    ui.skeletal_mesh_import_data.set_editor_property("normal_import_method", U.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    task = U.AssetImportTask()
    task.filename = fbx
    task.destination_path = dest
    task.destination_name = f"SK_{name}Fur"
    task.automated = True
    task.replace_existing = True
    task.save = True
    task.options = ui
    tools.import_asset_tasks([task])
mesh = U.load_asset(f"{dest}/SK_{name}Fur")
if not mesh or mesh.get_editor_property("skeleton") != skeleton:
    raise RuntimeError("the fur mesh didn't land on the skeleton")
log(f"mesh {mesh.get_path_name()} bounds {mesh.get_bounds().box_extent}")

if strands:
    ttask = U.AssetImportTask()
    ttask.filename = strands
    ttask.destination_path = f"{dest}/Textures"
    ttask.destination_name = f"T_{name}FurStrands"
    ttask.automated = True
    ttask.replace_existing = True
    ttask.save = True
    tools.import_asset_tasks([ttask])
strand_tex = U.load_asset(f"{dest}/Textures/T_{name}FurStrands")
strand_tex.set_editor_property("srgb", False)
strand_tex.set_editor_property("compression_settings", U.TextureCompressionSettings.TC_MASKS)
# no mips: averaged distances would melt the strands into solid bands a few metres away
strand_tex.set_editor_property("mip_gen_settings", U.TextureMipGenSettings.TMGS_NO_MIPMAPS)
eal.save_asset(strand_tex.get_path_name())
base_tex = U.load_asset(f"{dest}/Textures/T_{name}_BaseColor")

mpath = f"{dest}/M_{name}Fur"
kept = {}
face_on = len(args) > 4 and bool(args[4])
if eal.does_asset_exist(mpath):
    if not face_on:
        # (no <face> given: the inner face placed on the old material stays, also when the mesh is brought in again —
        # a new fur for the same body would otherwise lose it without a word)
        old = U.load_asset(mpath)
        if "TidyFace" in [str(n) for n in mel.get_scalar_parameter_names(old)] \
                and mel.get_material_default_scalar_parameter_value(old, "TidyFace") > 0.0:
            at = mel.get_material_default_vector_parameter_value(old, "TidyFaceAt")
            size = mel.get_material_default_vector_parameter_value(old, "TidyFaceSize")
            face = [at.r, at.g, at.b, size.r, size.g, size.b]
            face_on = True
            log(f"kept the old material's inner face: {[round(v, 2) for v in face]}")
        old = None
    if not fbx:
        # (a material-only rebuild keeps what was tuned on the old one: the rabbit's lighter roots, the fur lengths)
        old = U.load_asset(mpath)
        have = [str(n) for n in mel.get_scalar_parameter_names(old)]
        for pn in ("RootShade", "FurLength", "FullLayersTo", "FewLayersFrom", "WindStrength"):
            if pn in have:
                kept[pn] = mel.get_material_default_scalar_parameter_value(old, pn)
        if len(args) > 3 and args[3]:
            kept.pop("FurLength", None)
        log(f"kept from the old material: {kept}")
        old = None
    eal.delete_asset(mpath)
mat = tools.create_asset(f"M_{name}Fur", dest, U.Material, U.MaterialFactoryNew())
mat.set_editor_property("blend_mode", U.BlendMode.BLEND_MASKED)
mat.set_editor_property("opacity_mask_clip_value", 0.5)
mat.set_editor_property("used_with_skeletal_mesh", True)


def node(cls, x, y, **props):
    n = mel.create_material_expression(mat, cls, x, y)
    for k, v in props.items():
        n.set_editor_property(k, v)
    return n


def custom(code, out_type, inputs, x, y, desc):
    n = node(U.MaterialExpressionCustom, x, y, code=code, output_type=out_type, description=desc)
    ins = []
    for nm in inputs:
        ci = U.CustomInput()
        ci.set_editor_property("input_name", nm)
        ins.append(ci)
    n.set_editor_property("inputs", ins)
    return n


def scalar(nm, v, x, y):
    return node(U.MaterialExpressionScalarParameter, x, y, parameter_name=nm, default_value=kept.get(nm, v))


vc = node(U.MaterialExpressionVertexColor, -1400, 0)
base = node(U.MaterialExpressionTextureSample, -1400, -300, texture=base_tex)
uv1 = node(U.MaterialExpressionTextureCoordinate, -1700, 300, coordinate_index=1)
strand = node(U.MaterialExpressionTextureSample, -1400, 300, texture=strand_tex,
              sampler_type=U.MaterialSamplerType.SAMPLERTYPE_MASKS)
mel.connect_material_expressions(uv1, "", strand, "UVs")

# Tidy up close: 0 = the fur as it is .. 1 = groomed: near the camera AND (facing it OR on the inner face).
# The inner face = an ellipsoid in the mesh's own space before skinning ("TidyFaceAt" its middle, "TidyFaceSize" its
# radii, "TidyFace" how much it counts, 0 = no such place), worked out per vertex and handed to the pixel shader.
face_at = node(U.MaterialExpressionVectorParameter, -2700, 2200, parameter_name="TidyFaceAt",
               default_value=U.LinearColor(face[0], face[1], face[2], 0))
face_size = node(U.MaterialExpressionVectorParameter, -2700, 2400, parameter_name="TidyFaceSize",
                 default_value=U.LinearColor(face[3], face[4], face[5], 0))
region = custom(
    "float3 q = (L - At) / max(Size, float3(0.001, 0.001, 0.001));\n"
    "return 1.0 - smoothstep(Edge, 1.0, length(q));",
    U.CustomMaterialOutputType.CMOT_FLOAT1, ["L", "At", "Size", "Edge"], -2400, 2200, "FurFace")
mel.connect_material_expressions(node(U.MaterialExpressionPreSkinnedPosition, -2700, 2130), "", region, "L")
mel.connect_material_expressions(face_at, "", region, "At")
mel.connect_material_expressions(face_size, "", region, "Size")
mel.connect_material_expressions(scalar("TidyFaceEdge", 0.6, -2700, 2600), "", region, "Edge")
region_ps = node(U.MaterialExpressionVertexInterpolator, -2150, 2200)
mel.connect_material_expressions(region, "", region_ps, "")
tidy = custom(
    "float3 to = Cam - P;\n"
    "float dist = max(length(to), 0.001);\n"
    "float facing = dot(normalize(N), to / dist);\n"
    "dist /= max(abs(ResolvedView.ViewToClip[0][0]), 0.05);\n"
    "float nearK = Far > Near ? 1.0 - smoothstep(Near, Far, dist) : 0.0;\n"
    "return nearK * max(Facing * smoothstep(FaceFrom, FaceFull, facing), saturate(Region * Face));",
    U.CustomMaterialOutputType.CMOT_FLOAT1,
    ["N", "P", "Cam", "Near", "Far", "FaceFrom", "FaceFull", "Region", "Face", "Facing"], -1900, 1700, "FurTidy")
mel.connect_material_expressions(region_ps, "", tidy, "Region")
mel.connect_material_expressions(scalar("TidyFace", 1.0 if face_on else 0.0, -2300, 2140), "", tidy, "Face")
mel.connect_material_expressions(node(U.MaterialExpressionVertexNormalWS, -2300, 1650), "", tidy, "N")
mel.connect_material_expressions(
    node(U.MaterialExpressionWorldPosition, -2300, 1720,
         world_position_shader_offset=U.WorldPositionIncludedOffsets.WPT_EXCLUDE_ALL_SHADER_OFFSETS), "", tidy, "P")
mel.connect_material_expressions(node(U.MaterialExpressionCameraPositionWS, -2300, 1790), "", tidy, "Cam")
mel.connect_material_expressions(scalar("TidyNear", 90.0, -2300, 1860), "", tidy, "Near")
mel.connect_material_expressions(scalar("TidyFar", 320.0, -2300, 1930), "", tidy, "Far")
mel.connect_material_expressions(scalar("TidyFacingFrom", 0.3, -2300, 2000), "", tidy, "FaceFrom")
mel.connect_material_expressions(scalar("TidyFacingFull", 0.5, -2300, 2070), "", tidy, "FaceFull")
mel.connect_material_expressions(scalar("TidyFacing", 0.55, -2300, 2105), "", tidy, "Facing")

color = custom(
    "float h = VC.r; float w = VC.g;\n"
    "float cut = lerp(1.0, TidyLen, Tidy);\n"
    "float shade = VC.b > 0.5 ? lerp(1.0, RootShade, w) : lerp(RootShade, 1.1, saturate(h / cut));\n"
    "shade = lerp(shade, 1.0, Tidy * Even);\n"
    "return lerp(Col * shade, float3(1.0, 0.0, 0.6), Tidy * Show);",
    U.CustomMaterialOutputType.CMOT_FLOAT3, ["Col", "VC", "RootShade", "Tidy", "Even", "TidyLen", "Show"], -900, -300, "FurColor")
tidy_len = scalar("TidyLength", 0.4, -1200, 800)
mel.connect_material_expressions(tidy, "", color, "Tidy")
mel.connect_material_expressions(scalar("TidyEven", 0.25, -1200, -80), "", color, "Even")
mel.connect_material_expressions(tidy_len, "", color, "TidyLen")
mel.connect_material_expressions(scalar("TidyShow", 0.0, -1200, -10), "", color, "Show")
mel.connect_material_expressions(base, "RGB", color, "Col")
mel.connect_material_expressions(vc, "", color, "VC")
mel.connect_material_expressions(scalar("RootShade", 0.88, -1200, -150), "", color, "RootShade")
mel.connect_material_property(color, "", U.MaterialProperty.MP_BASE_COLOR)

mask = custom(
    "float h = VC.r; float w = VC.g;\n"
    "if (w < 0.02) return 0;\n"
    "if (VC.b > 0.5) return 1;\n"
    "float lod = saturate((Dist - Near) / max(Far - Near, 1.0));\n"
    "if (h > 1.0 - 0.85 * lod + 0.001) return 0;\n"
    # the long strands, shorter and thinner where tidy ...
    "float len = lerp(0.55, 1.0, S.g) * lerp(1.0, TidyLen, Tidy);\n"
    "float coarse = 0.48 * lerp(1.0, Coarse, Tidy) * (1.0 - h / len);\n"
    # ... and a short fine undercoat growing in between them (the same strand texture, more cells across)
    "float lenF = lerp(0.55, 1.0, SF.g) * TidyLen;\n"
    "float fine = FineRadius * Tidy * (1.0 - h / lenF);\n"
    "return (S.r < coarse || SF.r < fine) ? 1 : 0;",
    U.CustomMaterialOutputType.CMOT_FLOAT1,
    ["S", "VC", "Dist", "Near", "Far", "Tidy", "TidyLen", "Coarse", "SF", "FineRadius"], -900, 300, "FurMask")
fine_uv = node(U.MaterialExpressionMultiply, -1700, 900)
mel.connect_material_expressions(uv1, "", fine_uv, "A")
mel.connect_material_expressions(scalar("TidyFine", 2.0, -1950, 960), "", fine_uv, "B")
strand_fine = node(U.MaterialExpressionTextureSample, -1400, 900, texture=strand_tex,
                   sampler_type=U.MaterialSamplerType.SAMPLERTYPE_MASKS)
mel.connect_material_expressions(fine_uv, "", strand_fine, "UVs")
mel.connect_material_expressions(tidy, "", mask, "Tidy")
mel.connect_material_expressions(tidy_len, "", mask, "TidyLen")
mel.connect_material_expressions(scalar("TidyCoarse", 0.5, -1200, 660), "", mask, "Coarse")
mel.connect_material_expressions(strand_fine, "", mask, "SF")
mel.connect_material_expressions(scalar("TidyFineRadius", 0.55, -1200, 730), "", mask, "FineRadius")
mel.connect_material_expressions(strand, "", mask, "S")
mel.connect_material_expressions(vc, "", mask, "VC")
mel.connect_material_expressions(node(U.MaterialExpressionPixelDepth, -1200, 450), "", mask, "Dist")
mel.connect_material_expressions(scalar("FullLayersTo", 1500.0, -1200, 520), "", mask, "Near")
mel.connect_material_expressions(scalar("FewLayersFrom", 4000.0, -1200, 590), "", mask, "Far")
mel.connect_material_property(mask, "", U.MaterialProperty.MP_OPACITY_MASK)

push = node(U.MaterialExpressionVectorParameter, -1400, 800, parameter_name="FurPush",
            default_value=U.LinearColor(0, 0, 0, 0))
push.set_editor_property("use_custom_primitive_data", True)
push.set_editor_property("primitive_data_index", 0)
wind = custom(
    "float h = VC.r; float w = VC.g;\n"
    "if (VC.b > 0.5) return float3(0, 0, 0);\n"
    "float g = 0.55 + 0.45 * sin(T * 1.3) * sin(T * 0.7 + 1.0);\n"
    "float3 p = P / 100.0;\n"
    "float ph1 = dot(p, float3(7, 3, 5)) - T * 6.0;\n"
    "float ph2 = dot(p, float3(-4, 6, 2)) - T * 4.3;\n"
    "float n = sin(ph1) + 0.6 * sin(ph2 * 1.7 + 0.5);\n"
    "float m = cos(ph1 * 1.3 + 2.0) + 0.5 * sin(ph2);\n"
    "float3 wd = normalize(WindDir + float3(0, 0, 0.0001));\n"
    "float3 side = normalize(cross(wd, float3(0, 0, 1)) + float3(0.0001, 0, 0));\n"
    "float3 d = F + Strength * g * (wd * (0.42 + 0.22 * n) + side * 0.18 * m + float3(0, 0, 0.12 * n));\n"
    "float mag = length(d);\n"
    "d *= mag > 1.3 ? 1.3 / mag : 1.0;\n"
    "return d * Len * h * h * w;",
    U.CustomMaterialOutputType.CMOT_FLOAT3, ["VC", "F", "T", "P", "WindDir", "Strength", "Len"], -900, 800, "FurWind")
mel.connect_material_expressions(vc, "", wind, "VC")
mel.connect_material_expressions(push, "", wind, "F")
mel.connect_material_expressions(node(U.MaterialExpressionTime, -1400, 1000), "", wind, "T")
mel.connect_material_expressions(node(U.MaterialExpressionPreSkinnedPosition, -1400, 1080), "", wind, "P")
mel.connect_material_expressions(node(U.MaterialExpressionVectorParameter, -1400, 1160, parameter_name="WindDir",
                                      default_value=U.LinearColor(1, 0, 0, 0)), "", wind, "WindDir")
mel.connect_material_expressions(scalar("WindStrength", 1.0, -1400, 1320), "", wind, "Strength")
mel.connect_material_expressions(scalar("FurLength", fur_cm, -1400, 1400), "", wind, "Len")
mel.connect_material_property(wind, "", U.MaterialProperty.MP_WORLD_POSITION_OFFSET)

mel.connect_material_property(node(U.MaterialExpressionConstant, -600, 100, r=1.0), "", U.MaterialProperty.MP_ROUGHNESS)
mel.connect_material_property(node(U.MaterialExpressionConstant, -600, 160, r=0.0), "", U.MaterialProperty.MP_SPECULAR)
mel.connect_material_property(node(U.MaterialExpressionConstant, -600, 220, r=0.0), "", U.MaterialProperty.MP_METALLIC)
mel.recompile_material(mat)
eal.save_asset(mpath)

mats = [U.SkeletalMaterial(material_interface=mat, material_slot_name=s.get_editor_property("material_slot_name"))
        for s in mesh.get_editor_property("materials")]
mesh.set_editor_property("materials", mats)
eal.save_asset(mesh.get_path_name())
log(f"material {mpath} on {len(mats)} slot(s); strands {strand_tex.get_path_name()}")
log("DONE")
