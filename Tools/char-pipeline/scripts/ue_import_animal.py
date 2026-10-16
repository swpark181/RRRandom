"""Bring one more Mixamo brawler into the game like the fox (run inside Unreal, legacy FBX importer).

Usage: UnrealEditor-Cmd <project> -run=pythonscript -dpcvars=Interchange.FeatureFlags.Import.FBX=0
           "-script=ue_import_animal.py <Name>|<mixamo dir>|<base color texture>[|<mesh fbx>]"
e.g.  "-script=ue_import_animal.py Rabbit|C:/work/char-pipeline/input/rabbit/mixamo|C:/.../rr_rabbit_basecolor.JPEG"

<mixamo dir> holds the Mixamo exports named <name>_<what>.fbx (see FILES): <name>_SK.fbx is the "Two Hand Rifle Idle"
With Skin, the rest are animations only. <mesh fbx> is that mesh with a tail rig (blender_animal_tail.py): like the fox's
(ue_import_fox_tail.py), it goes in over SK_<Name> onto the skeleton made from <name>_SK.fbx, which just gains the tail
bones (the other bones' reference pose is compared and logged).

Into /Game/Characters/<Name> (cleared first):
- SK_<Name> + SK_<Name>_Skeleton, and Anims/A_<Name>_Idle, Walk, Fire, Death, Victory, Dance1..4, Defeat1..3, AimIdle,
  AimRunF/B/L/R, the names ARRRandomCharacter looks for (RRRandomSpecies.cpp). The walk and the dances are made to stay in
  place the way ue_setup_fox.py / ue_import_fox_anim.py did the fox's: the hips' start-to-end drift is taken out.
- Textures/T_<Name>_BaseColor and M_<Name>, the fox's material as it is now: the base color tinted toward "DiffuseColor"
  by "TintStrength", flat (roughness 1, no specular or metal, no normal map: ue_make_toon.py), then the pastel switch
  (ue_fox_pastel.py) and the hit flash (ue_fox_hitflash.py), run on it here.
Logs the hips' walk drift (the walk's speed at scale 1, for RRRandomSpecies.cpp) and every animation's length.
"""
import os
import shutil
import sys
import tempfile

import unreal

args = " ".join(sys.argv[1:]).split("|")
name, mixamo_dir, base_tex = args[0].strip(), args[1].strip(), args[2].strip()
low = name.lower()
tail_fbx = args[3].strip() if len(args) > 3 and args[3].strip() else None
dest = f"/Game/Characters/{name}"
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
SCRIPTS = os.path.dirname(os.path.abspath(__file__))
# file suffix -> (asset name, take the hips' drift out)
FILES = {
    "walk": ("Walk", True), "fire": ("Fire", False), "death": ("Death", False), "victory": ("Victory", True),
    "aim_idle": ("AimIdle", False), "run_f": ("AimRunF", False), "run_b": ("AimRunB", False), "run_l": ("AimRunL", False),
    "run_r": ("AimRunR", False), "dance1_twist": ("Dance1", True), "dance2_shimmy": ("Dance2", True),
    "dance3_runningman": ("Dance3", True), "dance4_chicken": ("Dance4", True), "defeat1_defeated": ("Defeat1", False),
    "defeat2_crying": ("Defeat2", False), "defeat3_sad": ("Defeat3", False),
}


def log(msg):
    unreal.log(f"ANIMAL {msg}")


def import_fbx(path, folder, asset_name=None, skeleton=None):
    ui = unreal.FbxImportUI()
    ui.automated_import_should_detect_type = False
    ui.import_as_skeletal = True
    ui.import_animations = True
    ui.import_materials = False
    ui.import_textures = False
    ui.create_physics_asset = False
    if skeleton:
        ui.import_mesh = False
        ui.skeleton = skeleton
        ui.mesh_type_to_import = unreal.FBXImportType.FBXIT_ANIMATION
    else:
        ui.import_mesh = True
        ui.mesh_type_to_import = unreal.FBXImportType.FBXIT_SKELETAL_MESH
    ui.anim_sequence_import_data.set_editor_property(
        "animation_length", unreal.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME)
    task = unreal.AssetImportTask()
    task.filename = path
    task.destination_path = folder
    if asset_name:
        task.destination_name = asset_name
    task.automated = True
    task.replace_existing = True
    task.save = True
    task.options = ui
    asset_tools.import_asset_tasks([task])
    return [unreal.load_asset(p) for p in task.imported_object_paths]


def hips_track(anim):
    n = unreal.AnimationLibrary.get_num_keys(anim)
    keys = [unreal.AnimationLibrary.get_bone_pose_for_frame(anim, "Hips", i, False) for i in range(n)]
    return n, keys


def take_out_drift(anim):
    n, keys = hips_track(anim)
    if n < 2:
        return 0.0, 0.0
    pos = [k.translation for k in keys]
    dx, dy = pos[-1].x - pos[0].x, pos[-1].y - pos[0].y
    fixed = [unreal.Vector(p.x - dx * i / (n - 1), p.y - dy * i / (n - 1), p.z) for i, p in enumerate(pos)]
    anim.controller.set_bone_track_keys("Hips", fixed, [k.rotation for k in keys], [k.scale3d for k in keys], False)
    return dx, dy


if eal.does_directory_exist(dest):
    eal.delete_directory(dest)

# The mesh: the FBX file name gives the mesh and skeleton their names (renaming a skeleton afterwards loses it, see
# ue_setup_fox.py), so it goes in as a copy called SK_<Name>.fbx
temp = tempfile.mkdtemp()
mesh_copy = os.path.join(temp, f"SK_{name}.fbx").replace("\\", "/")
shutil.copyfile(f"{mixamo_dir}/{low}_SK.fbx", mesh_copy)
made = import_fbx(mesh_copy, dest)
mesh = next(a for a in made if isinstance(a, unreal.SkeletalMesh))
skeleton = mesh.get_editor_property("skeleton")
# The mesh FBX's own take (SK_<Name>_Anim) isn't used: the idle is imported below under its name
if eal.does_asset_exist(f"{dest}/SK_{name}_Anim"):
    eal.delete_asset(f"{dest}/SK_{name}_Anim")
log(f"mesh {mesh.get_path_name()} skeleton {skeleton.get_path_name()} bounds {mesh.get_bounds().box_extent}")


def ref_pose(skel):
    pose = unreal.AnimPoseExtensions.get_reference_pose(skel)
    return {str(n): unreal.AnimPoseExtensions.get_ref_bone_pose(pose, n, unreal.AnimPoseSpaces.LOCAL)
            for n in unreal.AnimPoseExtensions.get_bone_names(pose)}


if tail_fbx:
    before = ref_pose(skeleton)
    ui = unreal.FbxImportUI()
    ui.automated_import_should_detect_type = False
    ui.import_as_skeletal = True
    ui.import_mesh = True
    ui.import_animations = False
    ui.import_materials = False
    ui.import_textures = False
    ui.create_physics_asset = False
    ui.mesh_type_to_import = unreal.FBXImportType.FBXIT_SKELETAL_MESH
    ui.skeleton = skeleton
    task = unreal.AssetImportTask()
    task.filename = tail_fbx
    task.destination_path = dest
    task.destination_name = f"SK_{name}"
    task.automated = True
    task.replace_existing = True
    task.save = True
    task.options = ui
    asset_tools.import_asset_tasks([task])
    mesh = unreal.load_asset(f"{dest}/SK_{name}")
    if mesh.get_editor_property("skeleton") != skeleton:
        raise RuntimeError("the tail mesh ended up on another skeleton")
    after = ref_pose(skeleton)
    worst_move = max((before[n].translation - after[n].translation).length() for n in before)
    worst_turn = max(abs(unreal.MathLibrary.quat_angular_distance(before[n].rotation, after[n].rotation)) * 57.2958 for n in before)
    log(f"tail mesh in: bones {len(before)} -> {len(after)} ({', '.join(sorted(set(after) - set(before)))}); "
        f"the others moved at most {worst_move:.3f} cm, turned {worst_turn:.3f} deg")

# The idle from the With Skin idle, then everything else
anims = {}
for suffix, (what, in_place) in [("SK", ("Idle", False))] + list(FILES.items()):
    src = f"{mixamo_dir}/{low}_{suffix}.fbx"
    if not os.path.exists(src):
        log(f"missing {src}")
        continue
    asset = f"A_{name}_{what}"
    got = [a for a in import_fbx(src, f"{dest}/Anims", asset, skeleton) if isinstance(a, unreal.AnimSequence)]
    if not got:
        log(f"no animation in {src}")
        continue
    anim = got[0]
    if anim.get_name() != asset:
        eal.rename_asset(anim.get_path_name().split(".")[0], f"{dest}/Anims/{asset}")
        anim = unreal.load_asset(f"{dest}/Anims/{asset}")
    line = f"{asset}: {unreal.AnimationLibrary.get_sequence_length(anim):.2f} s"
    if in_place:
        dx, dy = take_out_drift(anim)
        length = unreal.AnimationLibrary.get_sequence_length(anim)
        line += f", drift ({dx:.1f},{dy:.1f}) taken out = {(dx * dx + dy * dy) ** 0.5 / max(length, 1e-3):.1f}/s"
    eal.save_asset(anim.get_path_name().split(".")[0])
    anims[what] = anim
    log(line)

# Base color texture and the material
task = unreal.AssetImportTask()
task.filename = base_tex
task.destination_path = f"{dest}/Textures"
task.destination_name = f"T_{name}_BaseColor"
task.automated = True
task.replace_existing = True
task.save = True
asset_tools.import_asset_tasks([task])
tex = unreal.load_asset(f"{dest}/Textures/T_{name}_BaseColor")
tex.set_editor_property("srgb", True)

mat = asset_tools.create_asset(f"M_{name}", dest, unreal.Material, unreal.MaterialFactoryNew())
mat.set_editor_property("used_with_skeletal_mesh", True)


def node(cls, x, y, **props):
    n = mel.create_material_expression(mat, cls, x, y)
    for key, value in props.items():
        n.set_editor_property(key, value)
    return n


base = node(unreal.MaterialExpressionTextureSample, -900, -200, texture=tex,
            sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
color = node(unreal.MaterialExpressionVectorParameter, -900, -450, parameter_name="DiffuseColor",
             default_value=unreal.LinearColor(1, 1, 1, 1))
strength = node(unreal.MaterialExpressionScalarParameter, -900, -550, parameter_name="TintStrength", default_value=0.35)
white = node(unreal.MaterialExpressionConstant3Vector, -650, -550, constant=unreal.LinearColor(1, 1, 1, 1))
tint = node(unreal.MaterialExpressionLinearInterpolate, -450, -450)
mel.connect_material_expressions(white, "", tint, "A")
mel.connect_material_expressions(color, "", tint, "B")
mel.connect_material_expressions(strength, "", tint, "Alpha")
mult = node(unreal.MaterialExpressionMultiply, -250, -250)
mel.connect_material_expressions(base, "RGB", mult, "A")
mel.connect_material_expressions(tint, "", mult, "B")
mel.connect_material_property(mult, "", unreal.MaterialProperty.MP_BASE_COLOR)
for prop, value in ((unreal.MaterialProperty.MP_ROUGHNESS, 1.0), (unreal.MaterialProperty.MP_SPECULAR, 0.0),
                    (unreal.MaterialProperty.MP_METALLIC, 0.0)):
    mel.connect_material_property(node(unreal.MaterialExpressionConstant, -250, 200, r=value), "", prop)
mel.connect_material_property(node(unreal.MaterialExpressionConstant3Vector, -250, 400,
                                   constant=unreal.LinearColor(0, 0, 1, 1)), "", unreal.MaterialProperty.MP_NORMAL)
mel.recompile_material(mat)
eal.save_asset(mat.get_path_name())

# The fox material's later additions, by the same scripts
mat_path = f"{dest}/M_{name}"
for script in ("ue_fox_pastel.py", "ue_fox_hitflash.py"):
    saved = sys.argv
    sys.argv = [script, mat_path]
    exec(compile(open(os.path.join(SCRIPTS, script), encoding="utf-8").read(), script, "exec"), {"__name__": "__main__"})
    sys.argv = saved

materials = mesh.get_editor_property("materials")
for m in materials:
    m.set_editor_property("material_interface", unreal.load_asset(mat_path))
mesh.set_editor_property("materials", materials)
eal.save_asset(mesh.get_path_name())
eal.save_directory(dest)
for a in eal.list_assets(f"{dest}/Anims", recursive=False):
    if unreal.load_asset(a).get_editor_property("skeleton") != skeleton:
        raise RuntimeError(f"{a} is on another skeleton")
log("assets: " + ", ".join(sorted(a.split(".")[0].replace(dest + "/", "") for a in eal.list_assets(dest, recursive=True))))
log("DONE")
