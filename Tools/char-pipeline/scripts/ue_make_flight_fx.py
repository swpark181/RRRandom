"""Materials for the flight effects (URRRandomFlightEffectsComponent; run inside Unreal).

Usage: UnrealEditor-Cmd <project> -run=pythonscript "-script=ue_make_flight_fx.py"

/Game/Effects/Flight:
- M_BlobShadow: a deferred decal, a soft dark disc under every brawler ("Opacity"; the component shrinks and fades it
  the higher the brawler flies, so height reads from above).
- M_DustRing: a deferred decal, a pale ring of dust thrown out on take-off and landing ("Ring" 0..1 grows the ring
  out from the middle, "Opacity" fades it).
- M_WindStreak: unlit, translucent, additive white streaks left in the air behind a flyer, soft at the ends and sides;
  each instance's first custom float is its opacity.
"""
import unreal

U = unreal
eal = U.EditorAssetLibrary
mel = U.MaterialEditingLibrary
FOLDER = "/Game/Effects/Flight"


def new_material(name):
    path = f"{FOLDER}/{name}"
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    return path, U.AssetToolsHelpers.get_asset_tools().create_asset(name, FOLDER, U.Material, U.MaterialFactoryNew())


def builder(mat):
    def node(cls, x, y, **props):
        n = mel.create_material_expression(mat, cls, x, y)
        for key, value in props.items():
            n.set_editor_property(key, value)
        return n

    def link(a, a_out, b, b_in):
        if not mel.connect_material_expressions(a, a_out, b, b_in):
            raise RuntimeError(f"could not connect into {b_in}")

    def custom(x, y, desc, code, names, out=U.CustomMaterialOutputType.CMOT_FLOAT1):
        c = node(U.MaterialExpressionCustom, x, y, output_type=out, description=desc, code=code)
        ins = []
        for n in names:
            ci = U.CustomInput()
            ci.set_editor_property("input_name", n)
            ins.append(ci)
        c.set_editor_property("inputs", ins)
        return c

    return node, link, custom


# Blob shadow
path, mat = new_material("M_BlobShadow")
mat.set_editor_property("material_domain", U.MaterialDomain.MD_DEFERRED_DECAL)
mat.set_editor_property("blend_mode", U.BlendMode.BLEND_TRANSLUCENT)
node, link, custom = builder(mat)
uv = node(U.MaterialExpressionTextureCoordinate, -800, 200)
opacity = node(U.MaterialExpressionScalarParameter, -800, 350, parameter_name="Opacity", default_value=0.45)
alpha = custom(-450, 250, "Blob", "// A soft disc: dark in the middle, fading out to the edge\n"
               "float R = length(UV - 0.5) * 2.0;\n"
               "return saturate(Opacity * (1.0 - smoothstep(0.55, 1.0, R)));", ("UV", "Opacity"))
link(uv, "", alpha, "UV")
link(opacity, "", alpha, "Opacity")
color = node(U.MaterialExpressionConstant3Vector, -450, 0, constant=U.LinearColor(0.02, 0.025, 0.03, 1.0))
mel.connect_material_property(color, "", U.MaterialProperty.MP_BASE_COLOR)
mel.connect_material_property(alpha, "", U.MaterialProperty.MP_OPACITY)
rough = node(U.MaterialExpressionConstant, -450, 450, r=1.0)
mel.connect_material_property(rough, "", U.MaterialProperty.MP_ROUGHNESS)
mel.recompile_material(mat)
eal.save_asset(path)
unreal.log(f"FLIGHTFX made {path}")

# Dust ring
path, mat = new_material("M_DustRing")
mat.set_editor_property("material_domain", U.MaterialDomain.MD_DEFERRED_DECAL)
mat.set_editor_property("blend_mode", U.BlendMode.BLEND_TRANSLUCENT)
node, link, custom = builder(mat)
uv = node(U.MaterialExpressionTextureCoordinate, -800, 200)
opacity = node(U.MaterialExpressionScalarParameter, -800, 350, parameter_name="Opacity", default_value=0.7)
ring = node(U.MaterialExpressionScalarParameter, -800, 450, parameter_name="Ring", default_value=0.5)
alpha = custom(-450, 250, "Dust ring",
               "// A puffy ring around radius Ring (of the decal's half width), lumpy all round\n"
               "float2 D = (UV - 0.5) * 2.0;\n"
               "float R = length(D);\n"
               "float A = atan2(D.y, D.x);\n"
               "float Lumps = 0.75 + 0.25 * sin(A * 7.0 + Ring * 3.0) * sin(A * 3.0 - 1.3);\n"
               "float Width = lerp(0.18, 0.32, Ring);\n"
               "float Band = 1.0 - smoothstep(0.0, Width, abs(R - Ring * 0.85));\n"
               "return saturate(Opacity * Band * Lumps * step(R, 1.0));", ("UV", "Opacity", "Ring"))
link(uv, "", alpha, "UV")
link(opacity, "", alpha, "Opacity")
link(ring, "", alpha, "Ring")
color = node(U.MaterialExpressionConstant3Vector, -450, 0, constant=U.LinearColor(0.86, 0.8, 0.68, 1.0))
mel.connect_material_property(color, "", U.MaterialProperty.MP_BASE_COLOR)
mel.connect_material_property(alpha, "", U.MaterialProperty.MP_OPACITY)
rough = node(U.MaterialExpressionConstant, -450, 450, r=1.0)
mel.connect_material_property(rough, "", U.MaterialProperty.MP_ROUGHNESS)
mel.recompile_material(mat)
eal.save_asset(path)
unreal.log(f"FLIGHTFX made {path}")

# Wind streak
path, mat = new_material("M_WindStreak")
mat.set_editor_property("blend_mode", U.BlendMode.BLEND_ADDITIVE)
mat.set_editor_property("shading_model", U.MaterialShadingModel.MSM_UNLIT)
mat.set_editor_property("two_sided", True)
mat.set_editor_property("used_with_instanced_static_meshes", True)
node, link, custom = builder(mat)
uv = node(U.MaterialExpressionTextureCoordinate, -800, 200)
data = node(U.MaterialExpressionPerInstanceCustomData, -800, 350, data_index=0)
glow = custom(-450, 250, "Streak",
              "// Along the streak (U): thin at the tail, fuller toward the head; across (V): a soft line\n"
              "float Along = smoothstep(0.0, 0.7, UV.x) * (1.0 - smoothstep(0.85, 1.0, UV.x));\n"
              "float Across = 1.0 - smoothstep(0.1, 0.5, abs(UV.y - 0.5));\n"
              "return saturate(Along * Across * Fade);", ("UV", "Fade"))
link(uv, "", glow, "UV")
link(data, "", glow, "Fade")
color = node(U.MaterialExpressionConstant3Vector, -450, 0, constant=U.LinearColor(0.75, 0.85, 1.0, 1.0))
lit = node(U.MaterialExpressionMultiply, -200, 100)
link(color, "", lit, "A")
link(glow, "", lit, "B")
mel.connect_material_property(lit, "", U.MaterialProperty.MP_EMISSIVE_COLOR)
mel.recompile_material(mat)
eal.save_asset(path)
unreal.log(f"FLIGHTFX made {path}")
