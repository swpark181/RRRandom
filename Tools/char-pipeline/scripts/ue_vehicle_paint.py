"""Give the vehicles a glossy painted look (round 91: "탈것들은 리플렉션·빛 효과 필수"). Run inside Unreal:

  UnrealEditor-Cmd <project> -run=pythonscript -script=ue_vehicle_paint.py

Makes /Game/Vehicles/M_VehiclePaint, a glossy car paint: the Tripo base color texture ("BaseColor") times "Tint",
"Roughness", "Metallic" and "Specular" (low roughness, so the sky and the town show in it with Lumen reflections), and
"Glow" (emissive, the base color times it). (A clear coat would be nicer, but this engine's Python can't wire its inputs.)
Then for each vehicle mesh /Game/Vehicles/SM_<X> an instance MI_<X>Paint with its own T_<X>_BaseColor and its kind's
finish (FINISH below), set on every slot of the mesh (the haetae robot: every SM_StarRobot* part).
The old M_<X> (matte, made by ue_import_props.py) stays for going back: set it on the mesh again.
"""
import unreal

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
DEST = "/Game/Vehicles"

# Each vehicle's finish: roughness, metallic, specular
CANDY = (0.18, 0.08, 0.7)
FINISH = {
    "Kickboard": (0.22, 0.0, 0.65),
    "Motorbike": (0.16, 0.3, 0.7),
    "Car": CANDY,
    "StarChest": CANDY,
    "StarArms": CANDY,
    "StarLegs": CANDY,
    # The haetae robot's parts: more like enamelled metal
    "StarRobot": (0.22, 0.4, 0.65),
    "PlaneWood": (0.3, 0.0, 0.55),
    "PlaneFighter": (0.18, 0.35, 0.7),
    "PlaneSpaceship": (0.15, 0.5, 0.7),
}


def log(msg):
    unreal.log(f"PAINT {msg}")


def node(mat, cls, x, y, **props):
    n = mel.create_material_expression(mat, cls, x, y)
    for key, value in props.items():
        n.set_editor_property(key, value)
    return n


def scalar(mat, name, value, y):
    return node(mat, unreal.MaterialExpressionScalarParameter, -500, y, parameter_name=name, default_value=value)


def make_master():
    path = f"{DEST}/M_VehiclePaint"
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    mat = asset_tools.create_asset("M_VehiclePaint", DEST, unreal.Material, unreal.MaterialFactoryNew())
    default_tex = unreal.load_asset("/Engine/EngineResources/DefaultTexture")
    sample = node(mat, unreal.MaterialExpressionTextureSampleParameter2D, -900, 0, parameter_name="BaseColor", texture=default_tex)
    tint = node(mat, unreal.MaterialExpressionVectorParameter, -900, -220, parameter_name="Tint", default_value=unreal.LinearColor(1.0, 1.0, 1.0, 1.0))
    color = node(mat, unreal.MaterialExpressionMultiply, -600, -60)
    mel.connect_material_expressions(sample, "RGB", color, "A")
    mel.connect_material_expressions(tint, "", color, "B")
    mel.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    glow = scalar(mat, "Glow", 0.0, 520)
    emissive = node(mat, unreal.MaterialExpressionMultiply, -250, 480)
    mel.connect_material_expressions(color, "", emissive, "A")
    mel.connect_material_expressions(glow, "", emissive, "B")
    mel.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    for prop, name, value, y in ((unreal.MaterialProperty.MP_ROUGHNESS, "Roughness", 0.2, 120),
                                 (unreal.MaterialProperty.MP_METALLIC, "Metallic", 0.1, 200),
                                 (unreal.MaterialProperty.MP_SPECULAR, "Specular", 0.7, 280)):
        mel.connect_material_property(scalar(mat, name, value, y), "", prop)
    mel.recompile_material(mat)
    eal.save_asset(mat.get_path_name())
    log("M_VehiclePaint made")
    return mat


master = make_master()
for kind, finish in FINISH.items():
    mesh = unreal.load_asset(f"{DEST}/SM_{kind}") if eal.does_asset_exist(f"{DEST}/SM_{kind}") else None
    tex_path = f"{DEST}/Textures/T_{kind}_BaseColor"
    if not mesh or not eal.does_asset_exist(tex_path):
        log(f"{kind}: no mesh or texture, skipped")
        continue
    mi_path = f"{DEST}/MI_{kind}Paint"
    if eal.does_asset_exist(mi_path):
        eal.delete_asset(mi_path)
    mi = asset_tools.create_asset(f"MI_{kind}Paint", DEST, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, master)
    mel.set_material_instance_texture_parameter_value(mi, "BaseColor", unreal.load_asset(tex_path))
    for name, value in zip(("Roughness", "Metallic", "Specular"), finish):
        mel.set_material_instance_scalar_parameter_value(mi, name, value)
    mel.update_material_instance(mi)
    eal.save_asset(mi.get_path_name())
    meshes = [mesh]
    # The robot's parts all share M_StarRobot: every SM_StarRobot* mesh
    if kind == "StarRobot":
        meshes = [unreal.load_asset(p) for p in eal.list_assets(DEST, recursive=False) if "/SM_StarRobot" in p]
        meshes = [m for m in meshes if isinstance(m, unreal.StaticMesh)]
    for m in meshes:
        for i in range(len(m.get_editor_property("static_materials"))):
            m.set_material(i, mi)
        eal.save_asset(m.get_path_name())
    log(f"{kind}: {len(meshes)} mesh(es) -> MI_{kind}Paint {finish}")
log("DONE")
