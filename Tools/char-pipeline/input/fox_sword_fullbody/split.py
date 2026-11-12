import bpy
from pathlib import Path
bpy.ops.wm.read_factory_settings(use_empty=True)
root=Path('C:/work/RRRandom/Tools/char-pipeline/input/fox_sword_fullbody')
bpy.ops.import_scene.fbx(filepath=str(next((root/'combo').glob('*.fbx'))))
arm=next(o for o in bpy.data.objects if o.type=='ARMATURE')
print('ACTIONS', [(a.name, tuple(a.frame_range),[s.identifier for s in a.slots]) for a in bpy.data.actions])
for i,a in enumerate(list(bpy.data.actions)):
    arm.animation_data.action=a
    if a.slots: arm.animation_data.action_slot=a.slots[0]
    start,end=map(int,a.frame_range)
    bpy.context.scene.frame_start=start
    bpy.context.scene.frame_end=end
    print('MOTION',i,a.name,start,end)
    for f in range(start,end+1,4):
        bpy.context.scene.frame_set(f)
        names=['Hips','RightHand','LeftHand','Spine2']
        print('TRACE',f,[(n,tuple(round(v*100,1) for v in (arm.matrix_world @ arm.pose.bones['mixamorig:'+n].matrix).translation)) for n in names])
    bpy.ops.export_scene.fbx(filepath=str(root/f'motion_{i}.fbx'),use_selection=False,add_leaf_bones=False,bake_anim=True,bake_anim_use_all_actions=False,bake_anim_use_nla_strips=False,bake_anim_simplify_factor=0.0)

