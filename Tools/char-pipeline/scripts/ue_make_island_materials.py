"""Stand-in materials for the voxel island level, until Tripo blocks replace them (run inside Unreal).

Usage: UnrealEditor-Cmd <project> -run=pythonscript "-script=ue_make_island_materials.py <dest>"
  e.g. /Game/Environment/Island

All lit and flat for the toon post process (full roughness, no specular or metal), set up for instanced static meshes,
colors are parameters to tune in the editor:
- M_IslandRock: per instance a mix of "ColorA" and "ColorB" by the instance's custom data 0 (0..1, random per block),
  so stacked cubes read as separate rocks in a cliff.
- M_IslandSand: "Color", with soft world-space blotches ("PatchSize") a little darker.
- M_IslandWood: "Color", darker every "PlankWidth" along X (gaps between planks).
- M_IslandWater: "Deep" with "Light" ripples drifting over it (world-space noise moving with time) and a little glow
  ("Glow") so it reads as water under the toon bands; the waterfall columns use it too.
"""
import sys

import unreal

dest = " ".join(sys.argv[1:]).strip()
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
U = unreal


def log(msg):
    unreal.log(f"ISLAND {msg}")


class Graph:
    def __init__(self, name):
        path = f"{dest}/{name}"
        if eal.does_asset_exist(path):
            eal.delete_asset(path)
        self.mat = asset_tools.create_asset(name, dest, U.Material, U.MaterialFactoryNew())
        self.mat.set_editor_property("used_with_instanced_static_meshes", True)
        self.mat.set_editor_property("used_with_static_lighting", False)

    def node(self, cls, x, y, **props):
        n = mel.create_material_expression(self.mat, cls, x, y)
        for key, value in props.items():
            n.set_editor_property(key, value)
        return n

    def color(self, name, value, x, y):
        return self.node(U.MaterialExpressionVectorParameter, x, y, parameter_name=name,
                         default_value=U.LinearColor(value[0], value[1], value[2], 1.0))

    def scalar(self, name, value, x, y):
        return self.node(U.MaterialExpressionScalarParameter, x, y, parameter_name=name, default_value=value)

    def op(self, cls, a, b, x, y, a_out="", **props):
        n = self.node(cls, x, y, **props)
        mel.connect_material_expressions(a, a_out, n, "A")
        if b is not None:
            mel.connect_material_expressions(b, "", n, "B")
        return n

    def one(self, cls, a, x, y, **props):
        n = self.node(cls, x, y, **props)
        mel.connect_material_expressions(a, "", n, "")
        return n

    def power(self, base, exponent, x, y):
        n = self.node(U.MaterialExpressionPower, x, y)
        mel.connect_material_expressions(base, "", n, "Base")
        mel.connect_material_expressions(exponent, "", n, "Exponent")
        return n

    def lerp(self, a, b, alpha, x, y):
        n = self.node(U.MaterialExpressionLinearInterpolate, x, y)
        mel.connect_material_expressions(a, "", n, "A")
        mel.connect_material_expressions(b, "", n, "B")
        mel.connect_material_expressions(alpha, "", n, "Alpha")
        return n

    def noise(self, position, x, y, levels=2):
        n = self.node(U.MaterialExpressionNoise, x, y, scale=1.0, levels=levels, output_min=0.0, output_max=1.0,
                      turbulence=False)
        mel.connect_material_expressions(position, "", n, "Position")
        return n

    def world_over(self, size_param, x, y):
        return self.op(U.MaterialExpressionDivide, self.node(U.MaterialExpressionWorldPosition, x - 200, y), size_param, x, y)

    def finish(self, base, emissive=None):
        mel.connect_material_property(base, "", U.MaterialProperty.MP_BASE_COLOR)
        if emissive is not None:
            mel.connect_material_property(emissive, "", U.MaterialProperty.MP_EMISSIVE_COLOR)
        for i, (prop, value) in enumerate(((U.MaterialProperty.MP_ROUGHNESS, 1.0), (U.MaterialProperty.MP_SPECULAR, 0.0),
                                           (U.MaterialProperty.MP_METALLIC, 0.0))):
            c = self.node(U.MaterialExpressionConstant, 200, 200 + i * 100, r=value)
            mel.connect_material_property(c, "", prop)
        mel.recompile_material(self.mat)
        log(f"made {self.mat.get_path_name()}")


# Rock: two browns mixed per block
g = Graph("M_IslandRock")
mix = g.node(U.MaterialExpressionPerInstanceCustomData, -500, 200, data_index=0, const_default_value=0.5)
g.finish(g.lerp(g.color("ColorA", (0.20, 0.09, 0.035), -500, -200), g.color("ColorB", (0.34, 0.16, 0.06), -500, 0), mix, -200, 0))

# Sand: warm, with soft darker blotches
g = Graph("M_IslandSand")
blotch = g.noise(g.world_over(g.scalar("PatchSize", 400.0, -900, 300), -700, 300), -500, 300)
shade = g.lerp(g.node(U.MaterialExpressionConstant, -500, 450, r=0.85), g.node(U.MaterialExpressionConstant, -500, 520, r=1.0),
               blotch, -300, 350)
g.finish(g.op(U.MaterialExpressionMultiply, g.color("Color", (0.45, 0.32, 0.15), -500, -100), shade, -100, 0))

# Wood: planks along Y, a darker gap every PlankWidth along X
g = Graph("M_IslandWood")
along = g.node(U.MaterialExpressionComponentMask, -700, 300, r=True, g=False, b=False)
mel.connect_material_expressions(g.world_over(g.scalar("PlankWidth", 45.0, -1100, 300), -900, 300), "", along, "")
frac = g.one(U.MaterialExpressionFrac, along, -500, 300)
# |sin| so the power never sees a negative base
gap = g.power(g.one(U.MaterialExpressionAbs, g.one(U.MaterialExpressionSine, frac, -400, 300, period=1.0), -300, 300),
              g.node(U.MaterialExpressionConstant, -350, 420, r=0.25), -200, 300)
plank = g.lerp(g.node(U.MaterialExpressionConstant, -200, 450, r=0.55), g.node(U.MaterialExpressionConstant, -200, 520, r=1.0),
               g.one(U.MaterialExpressionSaturate, gap, -100, 300), 0, 350)
g.finish(g.op(U.MaterialExpressionMultiply, g.color("Color", (0.36, 0.19, 0.08), -500, -100), plank, 150, 0))

# Water: deep teal with lighter ripples drifting over it
g = Graph("M_IslandWater")
drift = g.op(U.MaterialExpressionMultiply, g.node(U.MaterialExpressionTime, -1100, 500), g.scalar("Drift", 0.25, -1100, 600), -900, 550)
moving = g.op(U.MaterialExpressionAdd, g.world_over(g.scalar("RippleSize", 250.0, -1300, 300), -1100, 300), drift, -800, 400)
ripple = g.power(g.noise(moving, -600, 400, levels=3), g.scalar("RippleSharpness", 4.0, -600, 550), -400, 400)
water = g.lerp(g.color("Deep", (0.0, 0.11, 0.17), -400, -200), g.color("Light", (0.2, 0.6, 0.7), -400, 0),
               g.one(U.MaterialExpressionSaturate, ripple, -250, 400), -100, 0)
glow = g.op(U.MaterialExpressionMultiply, water, g.scalar("Glow", 0.04, -100, 200), 100, 100)
g.finish(water, glow)

eal.save_directory(dest)
log("DONE")
