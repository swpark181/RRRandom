"""Finish a Mixamo character imported by ue_import_mixamo.py (run inside Unreal).

Usage: UnrealEditor-Cmd <project> -run=pythonscript "-script=ue_setup_fox.py <dest>|<texture dir>"

- Renames the imported animations to A_ names (import the mesh FBX as SK_Fox.fbx so the mesh needs no rename).
- Makes walking animations in place: removes the hips' forward drift but keeps the bob. Root lock can't be
  used because it pins the hips at T-pose height, which is higher than in a walk, so the feet would float.
- Imports the Tripo PBR textures and builds M_Fox: the textures, tinted toward a "DiffuseColor" parameter
  by "TintStrength" so the game's team colors and random color events still show on a textured body.
"""
import sys

import unreal

dest, tex_dir = " ".join(sys.argv[1:]).split("|")
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()

# The mesh FBX is already named SK_Fox so the mesh and skeleton (SK_Fox_Skeleton) keep their import names:
# renaming the skeleton left SK_Fox pointing at the old skeleton path, because the redirector wasn't saved.
RENAMES = {
    "SK_Fox_Anim": "Anims/A_Fox_Idle",
    "Anims/Fox_RifleWalk": "Anims/A_Fox_Walk",
    "Anims/Fox_RifleFire": "Anims/A_Fox_Fire",
    "Anims/Fox_RifleFire2": "Anims/A_Fox_FireBurst",
    "Anims/Fox_RifleDeath": "Anims/A_Fox_Death",
    "Anims/Fox_TurnLeft": "Anims/A_Fox_TurnLeft",
}
IN_PLACE = ["Anims/A_Fox_Walk", "Anims/A_Fox_FireBurst"]
TEXTURES = {  # file in tex_dir -> (asset name, sRGB, compression)
    "fox_character_3d_model_basecolor.PNG": ("T_Fox_BaseColor", True, unreal.TextureCompressionSettings.TC_DEFAULT),
    "fox_character_3d_model_normal.PNG": ("T_Fox_Normal", False, unreal.TextureCompressionSettings.TC_NORMALMAP),
    "fox_character_3d_model_roughness.PNG": ("T_Fox_Roughness", False, unreal.TextureCompressionSettings.TC_MASKS),
    "fox_character_3d_model_metallic.PNG": ("T_Fox_Metallic", False, unreal.TextureCompressionSettings.TC_MASKS),
}


def log(msg):
    unreal.log(f"FOXSETUP {msg}")


def path(rel):
    return f"{dest}/{rel}"


for old, new in RENAMES.items():
    if eal.does_asset_exist(path(old)) and not eal.does_asset_exist(path(new)):
        eal.rename_asset(path(old), path(new))
        log(f"renamed {old} -> {new}")

for rel in IN_PLACE:
    anim = unreal.load_asset(path(rel))
    bone = "hips"
    # The raw-track getters come back empty in 5.8, so sample the hips at every key instead
    n = unreal.AnimationLibrary.get_num_keys(anim)
    keys = [unreal.AnimationLibrary.get_bone_pose_for_frame(anim, bone, i, False) for i in range(n)]
    pos = [k.translation for k in keys]
    dx, dy = pos[-1].x - pos[0].x, pos[-1].y - pos[0].y
    fixed = [unreal.Vector(p.x - dx * i / (n - 1), p.y - dy * i / (n - 1), p.z) for i, p in enumerate(pos)]
    anim.controller.set_bone_track_keys(bone, fixed, [k.rotation for k in keys], [k.scale3d for k in keys], False)
    after = [unreal.AnimationLibrary.get_bone_pose_for_frame(anim, bone, i, False).translation for i in (0, n // 2, n - 1)]
    log(f"in place {rel}: drift ({dx:.1f},{dy:.1f}) over {n} keys -> now " + " .. ".join(f"({p.x:.1f},{p.y:.1f},{p.z:.1f})" for p in after))

tasks = []
for file, (name, _, _) in TEXTURES.items():
    task = unreal.AssetImportTask()
    task.filename = f"{tex_dir}/{file}"
    task.destination_path = path("Textures")
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = False
    tasks.append(task)
asset_tools.import_asset_tasks(tasks)
tex = {}
for file, (name, srgb, compression) in TEXTURES.items():
    t = unreal.load_asset(path(f"Textures/{name}"))
    t.set_editor_property("srgb", srgb)
    t.set_editor_property("compression_settings", compression)
    tex[name] = t
log(f"textures {list(tex)}")

mat_path = path("M_Fox")
if eal.does_asset_exist(mat_path):
    eal.delete_asset(mat_path)
mat = asset_tools.create_asset("M_Fox", dest, unreal.Material, unreal.MaterialFactoryNew())
mat.set_editor_property("used_with_skeletal_mesh", True)


def node(cls, x, y):
    return mel.create_material_expression(mat, cls, x, y)


def sampler(name, x, y, sampler_type):
    s = node(unreal.MaterialExpressionTextureSample, x, y)
    s.set_editor_property("texture", tex[name])
    s.set_editor_property("sampler_type", sampler_type)
    return s


base = sampler("T_Fox_BaseColor", -900, -200, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
normal = sampler("T_Fox_Normal", -900, 300, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
rough = sampler("T_Fox_Roughness", -900, 100, unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
metal = sampler("T_Fox_Metallic", -900, 0, unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)

color = node(unreal.MaterialExpressionVectorParameter, -900, -450)
color.set_editor_property("parameter_name", "DiffuseColor")
color.set_editor_property("default_value", unreal.LinearColor(1, 1, 1, 1))
strength = node(unreal.MaterialExpressionScalarParameter, -900, -550)
strength.set_editor_property("parameter_name", "TintStrength")
strength.set_editor_property("default_value", 0.35)
white = node(unreal.MaterialExpressionConstant3Vector, -650, -550)
white.set_editor_property("constant", unreal.LinearColor(1, 1, 1, 1))
tint = node(unreal.MaterialExpressionLinearInterpolate, -450, -450)
mel.connect_material_expressions(white, "", tint, "A")
mel.connect_material_expressions(color, "", tint, "B")
mel.connect_material_expressions(strength, "", tint, "Alpha")
mult = node(unreal.MaterialExpressionMultiply, -250, -250)
mel.connect_material_expressions(base, "RGB", mult, "A")
mel.connect_material_expressions(tint, "", mult, "B")

mel.connect_material_property(mult, "", unreal.MaterialProperty.MP_BASE_COLOR)
mel.connect_material_property(normal, "RGB", unreal.MaterialProperty.MP_NORMAL)
mel.connect_material_property(rough, "R", unreal.MaterialProperty.MP_ROUGHNESS)
mel.connect_material_property(metal, "R", unreal.MaterialProperty.MP_METALLIC)
mel.recompile_material(mat)

mesh = unreal.load_asset(path("SK_Fox"))
materials = mesh.get_editor_property("materials")
for m in materials:
    m.set_editor_property("material_interface", mat)
mesh.set_editor_property("materials", materials)
log(f"mesh slots {len(materials)} -> M_Fox")

eal.save_directory(dest)

skeleton = mesh.get_editor_property("skeleton")
if skeleton is None:
    raise RuntimeError("SK_Fox has no skeleton")
for a in eal.list_assets(dest + "/Anims", recursive=False):
    anim_skel = unreal.load_asset(a).get_editor_property("skeleton")
    if anim_skel != skeleton:
        raise RuntimeError(f"{a} uses skeleton {anim_skel.get_path_name() if anim_skel else None}")
log(f"skeleton check ok: mesh and every animation use {skeleton.get_path_name()}")
log("assets: " + ", ".join(sorted(a.split(".")[0].replace(dest + "/", "") for a in eal.list_assets(dest, recursive=True))))
log("DONE")
