"""Import the flight wing and its sparkle made by blender_wing.py and build their glowing materials (run inside Unreal).

Usage: UnrealEditor-Cmd <project> -run=pythonscript -dpcvars=Interchange.FeatureFlags.Import.FBX=0
           "-script=ue_import_wing.py <dest>|<wing fbx>|<sparkle fbx>"
  e.g. /Game/Effects/Wings|C:/work/char-pipeline/output/wings/SM_Wing.fbx|C:/work/char-pipeline/output/wings/SM_WingSparkle.fbx

Both materials are unlit, two-sided and opaque, so the toon post process leaves them as they are (no base color =
passed through) and they read as flat glowing shapes; values over 1 bloom.
- SM_Wing + M_Wing: one right wing (the game mirrors it for the left), no collision. Emissive =
  pink gradient root -> tip ("RootColor" -> "TipColor"), coverts lighter ("CovertColor", vertex green),
  a rim bright enough to bloom ("RimColor" x "RimGlow", vertex red), all times "Glow" with a shimmer running out
  along the wing ("ShimmerAmount", "ShimmerFrequency", "ShimmerSpeed"),
  plus twinkles: the wing's UV is cut into "SparkleDensityU" x "SparkleDensityV" cells, each with a random phase,
  flashing a soft dot ("SparkleSize") now and then ("SparkleSpeed" flashes a second, sharpened by "SparkleSharpness")
  in "SparkleColor" x "SparkleBrightness".
- SM_WingSparkle + M_WingSparkle: the four-pointed star the wings shed, drawn as instances (so the material is set up
  for instanced static meshes). Emissive "SparkleColor" with a white-hot core (vertex red), x "Brightness".
"""
import sys

import unreal

dest, wing_fbx, sparkle_fbx = " ".join(sys.argv[1:]).split("|")
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()


def log(msg):
    unreal.log(f"WING {msg}")


if eal.does_directory_exist(dest):
    eal.delete_directory(dest)


def import_mesh(fbx, name):
    ui = unreal.FbxImportUI()
    ui.automated_import_should_detect_type = False
    ui.import_mesh = True
    ui.import_as_skeletal = False
    ui.import_animations = False
    ui.import_materials = False
    ui.import_textures = False
    ui.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
    data = ui.static_mesh_import_data
    data.set_editor_property("combine_meshes", True)
    data.set_editor_property("auto_generate_collision", False)
    data.set_editor_property("vertex_color_import_option", unreal.VertexColorImportOption.REPLACE)
    task = unreal.AssetImportTask()
    task.filename = fbx
    task.destination_path = dest
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = False
    task.options = ui
    asset_tools.import_asset_tasks([task])
    mesh = unreal.load_asset(f"{dest}/{name}")
    if mesh is None:
        raise RuntimeError(f"import of {fbx} failed")
    return mesh


class Graph:
    """Small helpers for building one material's node graph."""

    def __init__(self, name, instanced=False):
        self.mat = asset_tools.create_asset(name, dest, unreal.Material, unreal.MaterialFactoryNew())
        self.mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
        self.mat.set_editor_property("two_sided", True)
        if instanced:
            self.mat.set_editor_property("used_with_instanced_static_meshes", True)

    def node(self, cls, x, y, **props):
        n = mel.create_material_expression(self.mat, cls, x, y)
        for key, value in props.items():
            n.set_editor_property(key, value)
        return n

    def color(self, name, value, x, y):
        return self.node(unreal.MaterialExpressionVectorParameter, x, y, parameter_name=name,
                         default_value=unreal.LinearColor(*value, 1.0))

    def scalar(self, name, value, x, y):
        return self.node(unreal.MaterialExpressionScalarParameter, x, y, parameter_name=name, default_value=value)

    def op(self, cls, a, b, x, y, a_out="", b_out="", **props):
        """Two-input node; b may be None when the node's constant (const_b, ...) is set through props."""
        n = self.node(cls, x, y, **props)
        mel.connect_material_expressions(a, a_out, n, "A")
        if b is not None:
            mel.connect_material_expressions(b, b_out, n, "B")
        return n

    def one(self, cls, a, x, y, a_out="", **props):
        n = self.node(cls, x, y, **props)
        mel.connect_material_expressions(a, a_out, n, "")
        return n

    def lerp(self, a, b, alpha, alpha_out, x, y):
        n = self.node(unreal.MaterialExpressionLinearInterpolate, x, y)
        mel.connect_material_expressions(a, "", n, "A")
        mel.connect_material_expressions(b, "", n, "B")
        mel.connect_material_expressions(alpha, alpha_out, n, "Alpha")
        return n

    def power(self, base, exponent, x, y):
        n = self.node(unreal.MaterialExpressionPower, x, y)
        mel.connect_material_expressions(base, "", n, "Base")
        mel.connect_material_expressions(exponent, "", n, "Exponent")
        return n

    def finish(self, emissive):
        mel.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        mel.recompile_material(self.mat)
        return self.mat


U = unreal
TWO_PI = 6.2831853

# ---------- M_Wing ----------
g = Graph("M_Wing")
uv = g.node(U.MaterialExpressionTextureCoordinate, -1800, 0)
span = g.one(U.MaterialExpressionComponentMask, uv, -1650, 0, r=True, g=False)
vc = g.node(U.MaterialExpressionVertexColor, -1650, 250)
time = g.node(U.MaterialExpressionTime, -1800, 500)

# Colors: gradient along the span, lighter coverts, a rim bright enough to bloom
root = g.color("RootColor", (1.0, 0.45, 0.85), -1400, -500)
tip = g.color("TipColor", (0.75, 0.08, 0.55), -1400, -350)
covert = g.color("CovertColor", (1.0, 0.72, 0.92), -1400, -200)
rim = g.op(U.MaterialExpressionMultiply, g.color("RimColor", (1.0, 1.0, 1.0), -1400, -50),
           g.scalar("RimGlow", 1.6, -1400, 50), -1200, -50)
body = g.lerp(root, tip, span, "", -1100, -400)
body = g.lerp(body, covert, vc, "G", -900, -300)
body = g.lerp(body, rim, vc, "R", -700, -200)

# Brightness: Glow x (1 + ShimmerAmount x sin(U x ShimmerFrequency - Time x ShimmerSpeed)), a wave running out to the tip
wave = g.op(U.MaterialExpressionSubtract,
            g.op(U.MaterialExpressionMultiply, span, g.scalar("ShimmerFrequency", 3.0, -1400, 250), -1200, 250),
            g.op(U.MaterialExpressionMultiply, time, g.scalar("ShimmerSpeed", 1.5, -1400, 350), -1200, 350), -1050, 300)
shimmer = g.op(U.MaterialExpressionMultiply, g.one(U.MaterialExpressionSine, wave, -900, 300),
               g.scalar("ShimmerAmount", 0.2, -1050, 420), -750, 350)
brightness = g.op(U.MaterialExpressionMultiply, g.op(U.MaterialExpressionAdd, shimmer, None, -600, 350, const_b=1.0),
                  g.scalar("Glow", 1.0, -750, 450), -450, 350)
lit = g.op(U.MaterialExpressionMultiply, body, brightness, -300, -100)

# Twinkles: cells over the UV, each with its own random phase (the classic frac(sin(dot)) hash), flashing a soft dot
cells = g.op(U.MaterialExpressionMultiply, uv,
             g.op(U.MaterialExpressionAppendVector, g.scalar("SparkleDensityU", 14.0, -1800, 700),
                  g.scalar("SparkleDensityV", 5.0, -1800, 800), -1650, 750), -1500, 700)
cell = g.one(U.MaterialExpressionFloor, cells, -1350, 650)
local = g.op(U.MaterialExpressionSubtract, g.one(U.MaterialExpressionFrac, cells, -1350, 800), None, -1200, 800, const_b=0.5)
seed = g.op(U.MaterialExpressionDotProduct, cell, g.node(U.MaterialExpressionConstant2Vector, -1350, 700, r=12.9898, g=78.233),
            -1200, 650)
hashed = g.one(U.MaterialExpressionFrac,
               g.op(U.MaterialExpressionMultiply, g.one(U.MaterialExpressionSine, seed, -1050, 650, period=TWO_PI), None,
                    -900, 650, const_b=43758.5453), -750, 650)
phase = g.op(U.MaterialExpressionAdd, g.op(U.MaterialExpressionMultiply, time, g.scalar("SparkleSpeed", 0.7, -900, 750),
                                            -750, 750), hashed, -600, 700)
flash = g.power(g.one(U.MaterialExpressionSaturate, g.one(U.MaterialExpressionSine, phase, -450, 700), -300, 700),
                g.scalar("SparkleSharpness", 24.0, -300, 800), -150, 700)
dot = g.power(g.one(U.MaterialExpressionSaturate,
                    g.one(U.MaterialExpressionOneMinus,
                          g.op(U.MaterialExpressionDivide, g.one(U.MaterialExpressionLength, local, -1050, 850),
                               g.scalar("SparkleSize", 0.45, -1050, 950), -900, 900), -750, 900), -600, 900),
              g.node(U.MaterialExpressionConstant, -600, 1000, r=2.0), -450, 900)
twinkle = g.op(U.MaterialExpressionMultiply,
               g.op(U.MaterialExpressionMultiply, flash, dot, 0, 800),
               g.op(U.MaterialExpressionMultiply, g.color("SparkleColor", (1.0, 0.92, 1.0), -300, 950),
                    g.scalar("SparkleBrightness", 8.0, -300, 1100), -150, 1000), 150, 850)
wing_mat = g.finish(g.op(U.MaterialExpressionAdd, lit, twinkle, 300, 0))

# ---------- M_WingSparkle ----------
s = Graph("M_WingSparkle", instanced=True)
svc = s.node(U.MaterialExpressionVertexColor, -700, 100)
core = s.lerp(s.color("SparkleColor", (1.0, 0.3, 0.8), -700, -150), s.node(U.MaterialExpressionConstant3Vector, -700, 0,
                                                                         constant=unreal.LinearColor(1, 1, 1, 1)),
              svc, "R", -450, 0)
sparkle_mat = s.finish(s.op(U.MaterialExpressionMultiply, core, s.scalar("Brightness", 2.5, -450, 200), -200, 0))

wing = import_mesh(wing_fbx, "SM_Wing")
wing.set_material(0, wing_mat)
sparkle = import_mesh(sparkle_fbx, "SM_WingSparkle")
sparkle.set_material(0, sparkle_mat)
eal.save_directory(dest)

for mesh in (wing, sparkle):
    b = mesh.get_bounds()
    log(f"{mesh.get_name()}: triangles {mesh.get_num_triangles(0)}, slot -> {mesh.get_material(0).get_name()}, "
        f"origin ({b.origin.x:.1f}, {b.origin.y:.1f}, {b.origin.z:.1f}) extent ({b.box_extent.x:.1f}, {b.box_extent.y:.1f}, {b.box_extent.z:.1f})")
log("DONE")
