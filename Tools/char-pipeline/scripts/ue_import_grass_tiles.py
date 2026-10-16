"""Import the grass floor tiles made by blender_grass_tiles.py (run inside Unreal).

Usage: UnrealEditor-Cmd <project> -run=pythonscript -dpcvars=Interchange.FeatureFlags.Import.FBX=0
           "-script=ue_import_grass_tiles.py <dest>|<tiles.json>|<name>=<tile fbx>=<tripo base color png>|...
                     |<name>=<block fbx>=@<tile name>|..."
  e.g. /Game/Environment/Grass|C:/work/char-pipeline/output/grass/tiles.json|SM_GrassTile_A=.../SM_GrassTile_A.fbx=.../basecolor.PNG

For each tile: <dest>/SM_GrassTile_X (180 square, origin in the middle of its top, no collision: the arena floor
underneath stays the floor), Textures/T_GrassTile_X_BaseColor and M_GrassTile_X: flat like the other props
(base color only, full roughness, no specular or metal) for the toon post process, the texture times "Tint" (from
tiles.json, so every tile's top averages the same green, times BRIGHTNESS), set up for instanced static meshes,
then meadow(): a touch less saturated and tinted by soft world-space patches that ignore tile edges.
Plus M_GrassUnder: that shared green, flat, for a plane laid just under the tiles so any hairline gap between them
shows grass instead of the floor.
Blocks (blender_grass_blocks.py, given as name=fbx=@<tile name>): SM_GrassBlock_X with M_GrassBlock_X, the same
grass as that tile (texture, tint, meadow) but keeping the mesh's own normals, so a cover block's top and sides shade apart.
"""
import json
import sys

import unreal

args = " ".join(sys.argv[1:]).split("|")
# Options as "opt:key=value" entries: brightness (BRIGHTNESS), desaturate (meadow's "Desaturate"), normals=mesh (keep
# the mesh's own shading instead of straight up: the dirt tiles' lumps should catch the light), under=0 (no M_GrassUnder)
options = dict(a[4:].split("=") for a in args[2:] if a.startswith("opt:"))
dest, info_path, entries = args[0], args[1], [a.split("=") for a in args[2:] if not a.startswith("opt:")]
info = json.load(open(info_path))
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()


def log(msg):
    unreal.log(f"GRASS {msg}")


if eal.does_directory_exist(dest):
    eal.delete_directory(dest)


def import_task(path, folder, name, options=None):
    task = unreal.AssetImportTask()
    task.filename = path
    task.destination_path = folder
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = False
    if options:
        task.options = options
    return task


def node(mat, cls, x, y, **props):
    n = mel.create_material_expression(mat, cls, x, y)
    for key, value in props.items():
        n.set_editor_property(key, value)
    return n


def scalar(mat, name, value, x, y):
    return node(mat, unreal.MaterialExpressionScalarParameter, x, y, parameter_name=name, default_value=value)


def color(mat, name, value, x, y):
    return node(mat, unreal.MaterialExpressionVectorParameter, x, y, parameter_name=name,
                default_value=unreal.LinearColor(value[0], value[1], value[2], 1.0))


def meadow(mat, grass):
    """Makes the grass read as one lawn instead of a grid of tiles: a little less saturated ("Desaturate"), and
    tinted by soft patches a few meters across that drift over the whole floor regardless of tile edges: noise on the
    world position ("PatchSize") between a darker, cooler "ShadePatch" and a lighter, warmer "SunPatch"."""
    desat = node(mat, unreal.MaterialExpressionDesaturation, -350, -300)
    mel.connect_material_expressions(grass, "", desat, "")
    mel.connect_material_expressions(scalar(mat, "Desaturate", float(options.get("desaturate", 0.25)), -550, -200), "", desat, "Fraction")
    position = node(mat, unreal.MaterialExpressionWorldPosition, -1100, 400)
    size = scalar(mat, "PatchSize", 900.0, -1100, 500)
    scaled = node(mat, unreal.MaterialExpressionDivide, -900, 450)
    mel.connect_material_expressions(position, "", scaled, "A")
    mel.connect_material_expressions(size, "", scaled, "B")
    # Two octaves of smooth noise, 0..1 (the noise's own scale stays 1: the divide above sets the patch size)
    noise = node(mat, unreal.MaterialExpressionNoise, -700, 450, scale=1.0, levels=2, output_min=0.0, output_max=1.0,
                 turbulence=False)
    mel.connect_material_expressions(scaled, "", noise, "Position")
    patch = node(mat, unreal.MaterialExpressionLinearInterpolate, -450, 300)
    mel.connect_material_expressions(color(mat, "ShadePatch", (0.72, 0.8, 0.82), -700, 200), "", patch, "A")
    mel.connect_material_expressions(color(mat, "SunPatch", (1.12, 1.05, 0.8), -700, 300), "", patch, "B")
    mel.connect_material_expressions(noise, "", patch, "Alpha")
    out = node(mat, unreal.MaterialExpressionMultiply, -150, -100)
    mel.connect_material_expressions(desat, "", out, "A")
    mel.connect_material_expressions(patch, "", out, "B")
    return out


def flat_material(name, base_color_node, flat_normals=True):
    """Lit, flat for the toon post process, ready for instanced meshes. base_color_node(mat) makes the grass color,
    which then goes through meadow(). flat_normals: see below (off for blocks, whose sides must shade as sides)."""
    mat = asset_tools.create_asset(name, dest, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property("used_with_instanced_static_meshes", True)
    mel.connect_material_property(meadow(mat, base_color_node(mat)), "", unreal.MaterialProperty.MP_BASE_COLOR)
    # Straight up, in world space: the blades' bumps would otherwise speckle the lawn with the toon post process's
    # crease ink and blotchy light bands; the texture already draws the blades
    if flat_normals:
        mat.set_editor_property("tangent_space_normal", False)
        up = node(mat, unreal.MaterialExpressionConstant3Vector, -250, 450, constant=unreal.LinearColor(0.0, 0.0, 1.0, 1.0))
        mel.connect_material_property(up, "", unreal.MaterialProperty.MP_NORMAL)
    for i, (prop, value) in enumerate(((unreal.MaterialProperty.MP_ROUGHNESS, 1.0),
                                       (unreal.MaterialProperty.MP_SPECULAR, 0.0),
                                       (unreal.MaterialProperty.MP_METALLIC, 0.0))):
        c = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -250, 100 + i * 100)
        c.set_editor_property("r", value)
        mel.connect_material_property(c, "", prop)
    mel.recompile_material(mat)
    return mat


# The toon post process brightens lit color (x1.4) and saturation; Tripo's grass came out neon, so all of it is dimmed
# (0.7 was still too light for the user; flat normals then light every pixel fully)
BRIGHTNESS = float(options.get("brightness", 0.45))
MESH_NORMALS = options.get("normals") == "mesh"

tiles = [e for e in entries if not e[2].startswith("@")]
blocks = [e for e in entries if e[2].startswith("@")]

tasks = []
for name, fbx, png in entries:
    ui = unreal.FbxImportUI()
    ui.automated_import_should_detect_type = False
    ui.import_mesh = True
    ui.import_as_skeletal = False
    ui.import_animations = False
    ui.import_materials = False
    ui.import_textures = False
    ui.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
    ui.static_mesh_import_data.set_editor_property("combine_meshes", True)
    ui.static_mesh_import_data.set_editor_property("auto_generate_collision", False)
    # Blocks bring their own axis-snapped normals (blender_grass_blocks.py)
    ui.static_mesh_import_data.set_editor_property("normal_import_method", unreal.FBXNormalImportMethod.FBXNIM_COMPUTE_NORMALS
                                                   if MESH_NORMALS else unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    tasks.append(import_task(fbx, dest, name, ui))
    if not png.startswith("@"):
        tasks.append(import_task(png, dest + "/Textures", name.replace("SM_", "T_") + "_BaseColor"))
asset_tools.import_asset_tasks(tasks)

averages = []
# Tiles first: blocks borrow their tile's texture and tint
for name, _, png in tiles + blocks:
    tile = png[1:] if png.startswith("@") else name
    mesh = unreal.load_asset(f"{dest}/{name}")
    tex = unreal.load_asset(f"{dest}/Textures/{tile.replace('SM_', 'T_')}_BaseColor")
    if mesh is None or tex is None:
        raise RuntimeError(f"{name}: import failed (mesh={mesh}, texture={tex})")
    tint = [c * BRIGHTNESS for c in info[tile]["tint"]]
    if tile == name:
        averages.append(info[name]["average"])

    def tinted(mat, tex=tex, tint=tint):
        sample = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -700, -100)
        sample.set_editor_property("texture", tex)
        param = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -700, 150)
        param.set_editor_property("parameter_name", "Tint")
        param.set_editor_property("default_value", unreal.LinearColor(tint[0], tint[1], tint[2], 1.0))
        mult = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -450, 0)
        mel.connect_material_expressions(sample, "RGB", mult, "A")
        mel.connect_material_expressions(param, "", mult, "B")
        return mult

    mat = flat_material(name.replace("SM_", "M_"), tinted, flat_normals=tile == name and not MESH_NORMALS)
    mesh.set_material(0, mat)
    b = mesh.get_bounds()
    log(f"{name}: {mesh.get_num_triangles(0)} triangles, tint {tint}, extent ({b.box_extent.x:.1f}, {b.box_extent.y:.1f}, {b.box_extent.z:.1f})")

shared = [sum(a[i] for a in averages) / len(averages) * BRIGHTNESS for i in range(3)]


def under_color(mat):
    p = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -450, 0)
    p.set_editor_property("parameter_name", "Color")
    p.set_editor_property("default_value", unreal.LinearColor(shared[0], shared[1], shared[2], 1.0))
    return p


if options.get("under", "1") != "0":
    flat_material("M_GrassUnder", under_color)
log(f"under color {[round(c, 4) for c in shared]}")
eal.save_directory(dest)
log("assets: " + ", ".join(sorted(a.split(".")[0].replace(dest + "/", "") for a in eal.list_assets(dest, recursive=True))))
log("DONE")
