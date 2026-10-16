"""Bring in a creature rigged in Blender (blender_creature_base.py + blender_animal_tail.py), no animations (run in Unreal,
legacy FBX importer). The game poses its bones itself (a poseable mesh).

Usage: UnrealEditor-Cmd <project> -run=pythonscript -dpcvars=Interchange.FeatureFlags.Import.FBX=0
           "-script=ue_import_creature.py <Folder name>|<Asset name>|<rig fbx>|<base color texture>"
e.g.  "...|DragonClassic|DragonClassic|C:/work/char-pipeline/input/dragon_classic/dragon_classic_rig.fbx|C:/.../basecolor.JPEG"

Into /Game/Characters/<Folder> (cleared first): SK_<Asset> + SK_<Asset>_Skeleton, Textures/T_<Asset>_BaseColor and
M_<Asset>: the base color flat like the brawlers' (roughness 1, no specular or metal), with "Glow" (0) adding the base
color as emissive, for a creature lit up by its own fire.
"""
import os
import shutil
import sys
import tempfile

import unreal

args = " ".join(sys.argv[1:]).split("|")
folder, name, rig_fbx, base_tex = (a.strip() for a in args[:4])
dest = f"/Game/Characters/{folder}"
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()


def log(msg):
    unreal.log(f"CREATURE {msg}")


if eal.does_directory_exist(dest):
    eal.delete_directory(dest)

# Named by the file: copied as SK_<Asset>.fbx so the mesh and skeleton get their names from it
temp = tempfile.mkdtemp()
copy = os.path.join(temp, f"SK_{name}.fbx").replace("\\", "/")
shutil.copyfile(rig_fbx, copy)
ui = unreal.FbxImportUI()
ui.automated_import_should_detect_type = False
ui.import_as_skeletal = True
ui.import_mesh = True
ui.import_animations = False
ui.import_materials = False
ui.import_textures = False
ui.create_physics_asset = False
ui.mesh_type_to_import = unreal.FBXImportType.FBXIT_SKELETAL_MESH
task = unreal.AssetImportTask()
task.filename = copy
task.destination_path = dest
task.automated = True
task.replace_existing = True
task.save = True
task.options = ui
asset_tools.import_asset_tasks([task])
mesh = unreal.load_asset(f"{dest}/SK_{name}")
skeleton = mesh.get_editor_property("skeleton")
pose = unreal.AnimPoseExtensions.get_reference_pose(skeleton)
bones = [str(n) for n in unreal.AnimPoseExtensions.get_bone_names(pose)]
log(f"mesh {mesh.get_path_name()} bounds {mesh.get_bounds().box_extent}, {len(bones)} bones: {', '.join(bones)}")
for bone in bones:
    t = unreal.AnimPoseExtensions.get_ref_bone_pose(pose, bone, unreal.AnimPoseSpaces.WORLD).translation
    log(f"bone {bone} at ({t.x:.1f}, {t.y:.1f}, {t.z:.1f})")

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


base = node(unreal.MaterialExpressionTextureSample, -700, -200, texture=tex,
            sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
mel.connect_material_property(base, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
glow = node(unreal.MaterialExpressionScalarParameter, -700, 100, parameter_name="Glow", default_value=0.0)
lit = node(unreal.MaterialExpressionMultiply, -350, 50)
mel.connect_material_expressions(base, "RGB", lit, "A")
mel.connect_material_expressions(glow, "", lit, "B")
mel.connect_material_property(lit, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
for prop, value in ((unreal.MaterialProperty.MP_ROUGHNESS, 1.0), (unreal.MaterialProperty.MP_SPECULAR, 0.0),
                    (unreal.MaterialProperty.MP_METALLIC, 0.0)):
    mel.connect_material_property(node(unreal.MaterialExpressionConstant, -350, 250, r=value), "", prop)
mel.recompile_material(mat)
eal.save_asset(mat.get_path_name())

# Fresh entries (changing the ones read back didn't stick on this mesh), keeping the slot names
materials = [unreal.SkeletalMaterial(material_interface=mat, material_slot_name=m.get_editor_property("material_slot_name"))
             for m in mesh.get_editor_property("materials")]
mesh.set_editor_property("materials", materials)
eal.save_asset(mesh.get_path_name())
log("materials: " + ", ".join(f"{m.get_editor_property('material_slot_name')}={m.get_editor_property('material_interface').get_name()}"
                              for m in unreal.load_asset(f"{dest}/SK_{name}").get_editor_property("materials")))
eal.save_directory(dest)
log("assets: " + ", ".join(sorted(a.split(".")[0].replace(dest + "/", "") for a in eal.list_assets(dest, recursive=True))))
log("DONE")
