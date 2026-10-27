"""Stage the P2.0 candidate on the existing rabbit skeleton; never replace gameplay assets."""
from pathlib import Path
import unreal as u

root=Path(__file__).resolve().parents[1]
src=root/'input/rabbit_smart'
dest='/Game/Developers/RabbitSmartCompare'
eal=u.EditorAssetLibrary
at=u.AssetToolsHelpers.get_asset_tools()
mel=u.MaterialEditingLibrary
world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
u.SystemLibrary.execute_console_command(world,'Interchange.FeatureFlags.Import.FBX 0')
eal.make_directory(dest)

def constant(mat,prop,value):
    n=mel.create_material_expression(mat,u.MaterialExpressionConstant)
    n.set_editor_property('r',value)
    mel.connect_material_property(n,'',prop)

stage=u.load_asset(dest+'/M_CompareStage')
if not stage:stage=at.create_asset('M_CompareStage',dest,u.Material,u.MaterialFactoryNew())
mel.delete_all_material_expressions(stage)
c=mel.create_material_expression(stage,u.MaterialExpressionConstant3Vector)
c.set_editor_property('constant',u.LinearColor(.60,.58,.55,1))
mel.connect_material_property(c,'',u.MaterialProperty.MP_BASE_COLOR)
constant(stage,u.MaterialProperty.MP_ROUGHNESS,1.)
constant(stage,u.MaterialProperty.MP_SPECULAR,0.)
mel.recompile_material(stage)
eal.save_loaded_asset(stage)

fbx=src/'rabbit_smart_rig.fbx'
if fbx.exists():
    skeleton=u.load_asset('/Game/Characters/Rabbit/SK_Rabbit_Skeleton')
    assert skeleton
    before=u.AnimPoseExtensions.get_reference_pose(skeleton)
    ui=u.FbxImportUI()
    ui.automated_import_should_detect_type=False
    ui.import_as_skeletal=True
    ui.import_mesh=True
    ui.import_animations=False
    ui.import_materials=False
    ui.import_textures=False
    ui.create_physics_asset=False
    ui.mesh_type_to_import=u.FBXImportType.FBXIT_SKELETAL_MESH
    ui.skeleton=skeleton
    ui.skeletal_mesh_import_data.set_editor_property('update_skeleton_reference_pose',False)
    task=u.AssetImportTask()
    task.filename=str(fbx)
    task.destination_path=dest
    task.destination_name='SK_RabbitSmart'
    task.automated=True
    task.replace_existing=True
    task.save=True
    task.options=ui
    at.import_asset_tasks([task])
    mesh=u.load_asset(dest+'/SK_RabbitSmart')
    assert mesh and mesh.get_editor_property('skeleton')==skeleton
    task=u.AssetImportTask()
    task.filename=str(src/'rabbit_smart_basecolor.png')
    task.destination_path=dest
    task.destination_name='T_RabbitSmart_BaseColor'
    task.automated=True
    task.replace_existing=True
    task.save=True
    at.import_asset_tasks([task])
    tex=u.load_asset(dest+'/T_RabbitSmart_BaseColor')
    assert tex
    tex.set_editor_property('srgb',True)
    mat=eal.duplicate_asset('/Game/Characters/Rabbit/M_Rabbit',dest+'/M_RabbitSmart')
    if not mat:mat=u.load_asset(dest+'/M_RabbitSmart')
    # Both base colour and the fur colour mask sample the candidate's own atlas.
    for obj in u.ObjectIterator(u.MaterialExpressionTextureSample):
        if obj.get_path_name().startswith(mat.get_path_name()+':') and obj.get_editor_property('texture'):
            obj.set_editor_property('texture',tex)
            u.log('RABBITSMART replaced texture '+obj.get_name())
    mel.recompile_material(mat)
    mats=mesh.get_editor_property('materials')
    for m in mats:m.set_editor_property('material_interface',mat)
    mesh.set_editor_property('materials',mats)
    eal.save_directory(dest)
    after=u.AnimPoseExtensions.get_reference_pose(skeleton)
    for name in ('Hips','Head','LeftArm','LeftUpLeg'):
        a=u.AnimPoseExtensions.get_ref_bone_pose(before,name,u.AnimPoseSpaces.WORLD)
        b=u.AnimPoseExtensions.get_ref_bone_pose(after,name,u.AnimPoseSpaces.WORLD)
        assert (a.translation-b.translation).length()<.001,name
    u.log('RABBITSMART IMPORTED existing skeleton and reference pose preserved')
else:
    u.log('RABBITSMART STAGE MATERIAL ONLY')
