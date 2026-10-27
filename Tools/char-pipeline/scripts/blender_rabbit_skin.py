"""Finish the cream rabbit's fitted Mixamo rig; run in Blender.

Usage: <fit.fbx> <basecolor> <out.fbx> <preview-prefix>
The much wider arms must not inherit the old narrow rabbit's torso weights.
Use the measured limb chains to skin those regions, keeping the donor bone axes.
"""
import math
from pathlib import Path
import sys
import bpy
import numpy as np
from mathutils import Vector

src, texture, out, preview = sys.argv[sys.argv.index('--') + 1:][:4]
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=src)
arm = next(o for o in bpy.data.objects if o.type == 'ARMATURE')
mesh = next(o for o in bpy.data.objects if o.type == 'MESH')
arm.animation_data_clear()
arm.data.pose_position = 'REST'
prefix = 'mixamorig:'
groups = {b.name: mesh.vertex_groups.get(b.name) or mesh.vertex_groups.new(name=b.name)
          for b in arm.data.bones}

def smooth(a, b, v):
    t = max(0., min(1., (v-a)/(b-a)))
    return t*t*(3.-2.*t)

def blend(a, b, t):
    return {a: 1-t, b: t}

def torso(p):
    chain = [('Hips', .24), ('Spine', .29), ('Spine1', .34),
             ('Spine2', .39), ('Neck', .445), ('Head', .48)]
    for (a, z0), (b, z1) in zip(chain, chain[1:]):
        if p.z < z1:
            return blend(a, b, smooth(z0, z1, p.z))
    return {'Head': 1.}

def chain_weights(p, side, names):
    pts = [arm.matrix_world @ arm.data.bones[prefix+side+n].head_local for n in names]
    arc = [0.]
    for a, b in zip(pts, pts[1:]):
        arc.append(arc[-1]+(b-a).length)
    nearest = None
    for i, (a, b) in enumerate(zip(pts, pts[1:])):
        ab = b-a
        t = max(0., min(1., (p-a).dot(ab)/ab.length_squared))
        d = (p-a-ab*t).length
        if nearest is None or d < nearest[0]:
            nearest = (d, arc[i]+t*ab.length)
    s = nearest[1]
    for i in range(1, len(names)-1):
        if s < arc[i]+.014:
            t = smooth(arc[i]-.014, arc[i]+.014, s)
            return {side+names[i-1]: 1-t, side+names[i]: t}
    return {side+names[-2]: 1.}

def add_mix(a, b, t):
    result = {n:w*(1-t) for n,w in a.items()}
    for n,w in b.items():
        result[n] = result.get(n, 0)+w*t
    return result

for v in mesh.data.vertices:
    p = mesh.matrix_world @ v.co
    side = 'Left' if p.x > 0 else 'Right'
    weights = torso(p)
    # A sloping armpit boundary follows the visible gap between torso and arms.
    border = min(.21, .135 + max(0., .40-p.z)*.55)
    arm_share = smooth(border-.012, border+.012, abs(p.x))
    arm_share *= smooth(.18, .205, p.z)*(1-smooth(.425, .47, p.z))
    if arm_share > 0:
        weights = add_mix(weights, chain_weights(p, side,
                          ['Arm','ForeArm','Hand','HandIndex1']), arm_share)
    leg_share = (1-smooth(.17, .225, p.z))*smooth(.012, .055, abs(p.x))
    if p.z < .13:
        leg_share = 1.
    if leg_share > 0:
        weights = add_mix(weights, chain_weights(p, side,
                          ['UpLeg','Leg','Foot','ToeBase','Toe_End']), leg_share)
    # The small central pompom follows the pelvis, not the upper spine.
    if p.y > .18 and .18 < p.z < .31 and abs(p.x) < .08:
        weights = {'Hips':1.}
    for group_index in [g.group for g in v.groups]:
        mesh.vertex_groups[group_index].remove([v.index])
    weights = sorted(weights.items(), key=lambda nw:-nw[1])[:4]
    total = sum(w for _,w in weights)
    for n,w in weights:
        if w > .001:
            groups[prefix+n].add([v.index], w/total, 'REPLACE')

for poly in mesh.data.polygons:
    poly.use_smooth = True

# A dedicated cream material for the tail prevents Tripo's shorts color bleeding
# into the pompom. Other faces keep their original UV texture and fur detail.
base = bpy.data.materials.new('RabbitBase')
base.use_nodes = True
bsdf = base.node_tree.nodes.get('Principled BSDF')
tex = base.node_tree.nodes.new('ShaderNodeTexImage')
tex.image = bpy.data.images.load(texture)
base.node_tree.links.new(tex.outputs['Color'], bsdf.inputs['Base Color'])
bsdf.inputs['Roughness'].default_value = .9
cream = bpy.data.materials.new('RabbitTailCream')
cream.diffuse_color = (.82,.73,.57,1)
cream.use_nodes = True
cream.node_tree.nodes['Principled BSDF'].inputs['Base Color'].default_value = (.82,.73,.57,1)
cream.node_tree.nodes['Principled BSDF'].inputs['Roughness'].default_value = 1.
mesh.data.materials.clear()
mesh.data.materials.append(base)
mesh.data.materials.append(cream)
tail_faces = 0
for poly in mesh.data.polygons:
    p = mesh.matrix_world @ poly.center
    if p.y > .18 and .18 < p.z < .31 and abs(p.x) < .08:
        poly.material_index = 1
        tail_faces += 1
print('RABBITSKIN tail faces', tail_faces)

# Bake that local correction into the original atlas so Unreal can keep a single
# material slot, including the existing hit flash and fur edge parameters.
image = tex.image
w, h = image.size
pixels = np.array(image.pixels[:], dtype=np.float32).reshape(h,w,4)
mask = np.zeros((h,w), dtype=bool)
uvs = mesh.data.uv_layers.active.data
for poly in mesh.data.polygons:
    if poly.material_index != 1:
        continue
    coords = [np.array(uvs[i].uv[:]) * np.array([w-1,h-1]) for i in poly.loop_indices]
    for k in range(1,len(coords)-1):
        a,b,c = coords[0],coords[k],coords[k+1]
        lo = np.maximum(0,np.floor(np.minimum(np.minimum(a,b),c)).astype(int))
        hi = np.minimum([w-1,h-1],np.ceil(np.maximum(np.maximum(a,b),c)).astype(int))
        yy,xx = np.mgrid[lo[1]:hi[1]+1,lo[0]:hi[0]+1]
        def cross(p,q):
            return (xx-p[0])*(q[1]-p[1])-(yy-p[1])*(q[0]-p[0])
        e0,e1,e2 = cross(a,b),cross(b,c),cross(c,a)
        inside = ((e0>=0)&(e1>=0)&(e2>=0)) | ((e0<=0)&(e1<=0)&(e2<=0))
        mask[lo[1]:hi[1]+1,lo[0]:hi[0]+1] |= inside
for _ in range(3):
    mask |= np.roll(mask,1,0)|np.roll(mask,-1,0)|np.roll(mask,1,1)|np.roll(mask,-1,1)
pixels[mask,:3] = np.array([.82,.73,.57])
image.pixels.foreach_set(pixels.ravel())
image.filepath_raw = str(Path(out).with_name('rabbit_cream_clean.png'))
image.file_format = 'PNG'
image.save()
for poly in mesh.data.polygons:
    poly.material_index = 0
mesh.data.materials.pop(index=1)

bpy.ops.export_scene.fbx(filepath=out, object_types={'ARMATURE','MESH'}, add_leaf_bones=False,
    bake_anim=False, primary_bone_axis='Y', secondary_bone_axis='X',
    use_armature_deform_only=False, mesh_smooth_type='FACE', path_mode='STRIP')

scene = bpy.context.scene
scene.render.engine = 'CYCLES'
scene.cycles.samples = 12
scene.render.resolution_x = scene.render.resolution_y = 640
scene.view_settings.view_transform = 'Standard'
scene.world = bpy.data.worlds.new('Studio')
scene.world.use_nodes = True
scene.world.node_tree.nodes['Background'].inputs[0].default_value = (.65,.65,.68,1)
scene.world.node_tree.nodes['Background'].inputs[1].default_value = .7
light = bpy.data.objects.new('Key', bpy.data.lights.new('Key','AREA'))
scene.collection.objects.link(light)
light.location = (-2,-3,4)
light.rotation_euler = (Vector((0,0,.5))-light.location).to_track_quat('-Z','Y').to_euler()
light.data.energy = 250
light.data.shape = 'DISK'
light.data.size = 3
cam = bpy.data.objects.new('Camera', bpy.data.cameras.new('Camera'))
scene.collection.objects.link(cam)
scene.camera = cam
cam.data.type = 'ORTHO'
cam.data.ortho_scale = 1.15
arm.data.pose_position = 'POSE'
poses = {'rest':[], 'pose':[('LeftArm','x',-60),('RightArm','x',-60),
    ('LeftForeArm','z',40),('RightForeArm','z',-40),('LeftUpLeg','x',-45),
    ('LeftLeg','x',60),('RightUpLeg','x',20),('Spine1','x',20),('Head','y',30)]}
for pose, turns in poses.items():
    for bone in arm.pose.bones:
        bone.rotation_mode = 'XYZ'
        bone.rotation_euler = (0,0,0)
    for name, axis, deg in turns:
        setattr(arm.pose.bones[prefix+name].rotation_euler,axis,math.radians(deg))
    bpy.context.view_layer.update()
    for view, angle in [('front',0),('side',90),('back',180)]:
        yaw = math.radians(angle)
        cam.location = (math.sin(yaw)*3,-math.cos(yaw)*3,.5)
        cam.rotation_euler = (Vector((0,0,.5))-cam.location).to_track_quat('-Z','Y').to_euler()
        scene.render.filepath = f'{preview}_{pose}_{view}.png'
        bpy.ops.render.render(write_still=True)
print('RABBITSKIN complete')
