"""Bake a connected advancing phrase from the Tripo Sword Diagonal Cuts take.

The generated take's first cut supplies the torso/shoulder motion. Re-time its
return into a rising cut, preserve shared boundary poses, and solve short planted
steps and a safe hand arc for the fox's short limbs. Translation is in-place;
CharacterMovement supplies the matching 10 cm source-rig advance per strike.
"""
import math
from pathlib import Path
import bpy
from mathutils import Vector, Matrix, Quaternion

root=Path('C:/work/RRRandom/Tools/char-pipeline/input/fox_sword_advance')
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=str(next((root/'multistage').glob('*.fbx'))))
arm=next(o for o in bpy.data.objects if o.type=='ARMATURE')
scene=bpy.context.scene
source=arm.animation_data.action
bones=arm.pose.bones
def b(n): return bones['mixamorig:'+n]
def update(): bpy.context.view_layer.update()
def smooth(t):
 t=max(0.,min(1.,t)); return t*t*(3-2*t)
def pos(n): return b(n).matrix.translation.copy()
def point(n,child,target):
 bone=b(n); mat=bone.matrix.copy()
 q=(pos(child)-mat.translation).rotation_difference(target-mat.translation)
 bone.matrix=Matrix.LocRotScale(mat.translation,q@mat.to_quaternion(),mat.to_scale())
 update()
def ik(a,c,e,target,pole):
 origin=pos(a); mid=pos(c); end=pos(e)
 l1=(mid-origin).length; l2=(end-mid).length
 v=target-origin; length=min(v.length,(l1+l2)*.985)
 axis=v.normalized(); along=(l1*l1-l2*l2+length*length)/(2*length)
 side=pole-origin; side=(side-axis*side.dot(axis)).normalized()
 joint=origin+axis*along+side*math.sqrt(max(0,l1*l1-along*along))
 point(a,c,joint); point(c,e,origin+axis*length)

# Snapshot the generated upper body before creating a replacement action.
samples=[]
for i in range(55):
 step=min(i//18,2); t=(i-step*18)/18
 swing=smooth((t-.12)/.68)
 source_frame=8+16*(swing if step!=1 else 1-swing)
 scene.frame_set(int(source_frame),subframe=source_frame%1)
 samples.append({p.name:p.matrix_basis.copy() for p in bones})

arm.animation_data_clear()
action=bpy.data.actions.new('Fox_Connected_Advancing_Saber')
baked=[]
rest_foot={s:b(s+'Foot').bone.matrix_local.to_quaternion() for s in ['Left','Right']}
for i in range(55):
 step=min(i//18,2); t=(i-step*18)/18
 for p in bones:
  p.rotation_mode='QUATERNION'; p.matrix_basis=samples[i][p.name]
 update()
 # Small weight transfer, no lateral travel. Feet advance in alternating order.
 hip=b('Hips'); hm=hip.matrix.copy()
 hm.translation=Vector((0,0,19.3-.55*math.sin(math.pi*t)**2))
 swing=smooth((t-.12)/.68)
 cut=swing if step!=1 else 1-swing
 turn=2*cut-1
 hip.matrix=Matrix.LocRotScale(hm.translation,Quaternion((0,0,1),math.radians(8*turn))@hm.to_quaternion(),hm.to_scale())
 update()
 chest=b('Spine2'); cm=chest.matrix.copy()
 chest.matrix=Matrix.LocRotScale(cm.translation,Quaternion((0,0,1),math.radians(10*turn))@cm.to_quaternion(),cm.to_scale())
 update()
 advance=10*(step+min(t/.8,1.))
 foot_y={'Left':-5.,'Right':5.}
 for j in range(step): foot_y['Right' if j%2==0 else 'Left']-=20
 moving='Right' if step%2==0 else 'Left'
 ft=smooth((t-.04)/.66)
 foot_y[moving]-=20*ft
 for side,x in [('Left',6.),('Right',-6.)]:
  lift=3.2*math.sin(math.pi*ft) if side==moving else 0
  target=Vector((x,foot_y[side]+advance,4.1+lift))
  ik(side+'UpLeg',side+'Leg',side+'Foot',target,Vector((x,-28,11)))
  mat=b(side+'Foot').matrix.copy()
  b(side+'Foot').matrix=Matrix.LocRotScale(mat.translation,rest_foot[side],mat.to_scale())
  b(side+'ToeBase').matrix_basis=Matrix.Identity(4)
  update()
 swing=smooth((t-.12)/.68)
 cut=swing if step!=1 else 1-swing
 high=Vector((-14,-9,38)); low=Vector((3,-11,29))
 hand=high.lerp(low,cut); hand.y-=5*math.sin(math.pi*cut)
 ik('RightArm','RightForeArm','RightHand',hand,Vector((-25,-5,27)))
 ik('LeftArm','LeftForeArm','LeftHand',Vector((10,-9,31)),Vector((23,-2,28)))
 baked.append({p.name:p.matrix_basis.copy() for p in bones})

arm.animation_data_create(); arm.animation_data.action=action
for i,pose in enumerate(baked):
 scene.frame_set(i)
 for p in bones:
  p.matrix_basis=pose[p.name]
  p.keyframe_insert(data_path='location',frame=i)
  p.keyframe_insert(data_path='rotation_quaternion',frame=i)
  p.keyframe_insert(data_path='scale',frame=i)

scene.render.fps=30
for step in range(3):
 scene.frame_start=step*18; scene.frame_end=(step+1)*18
 bpy.ops.export_scene.fbx(filepath=str(root/f'connected_{step+1}.fbx'),use_selection=False,
  add_leaf_bones=False,bake_anim=True,bake_anim_use_all_actions=False,
  bake_anim_use_nla_strips=False,bake_anim_simplify_factor=0.)
print('CONNECTED_DONE: 3 contiguous 0.6s cuts; shared boundary poses; 10cm forward steps')
