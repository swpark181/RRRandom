import bpy
from pathlib import Path
bpy.ops.wm.read_factory_settings(use_empty=True)
p=next(Path('C:/work/RRRandom/Tools/char-pipeline/input/fox_sword_advance/multistage').glob('*.fbx'))
bpy.ops.import_scene.fbx(filepath=str(p))
a=next(o for o in bpy.data.objects if o.type=='ARMATURE')
print('WORLD',a.matrix_world)
for n in ['Hips','Spine','Spine1','Spine2','RightShoulder','RightArm','RightForeArm','RightHand','LeftUpLeg','LeftLeg','LeftFoot','LeftToeBase','RightUpLeg','RightLeg','RightFoot','RightToeBase']:
 b=a.data.bones['mixamorig:'+n]
 print('REST',n,tuple(round(v,3) for v in b.head_local),tuple(round(v,3) for v in b.tail_local))
for f in [0,8,16,24]:
 bpy.context.scene.frame_set(f)
 print('POSE',f,[(n,tuple(round(v*100,2) for v in (a.matrix_world@a.pose.bones['mixamorig:'+n].matrix).translation)) for n in ['Hips','RightHand','LeftFoot','RightFoot']])
