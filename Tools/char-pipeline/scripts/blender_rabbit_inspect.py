"""Inspect the user's Mixamo rabbit and render texture/pose evidence without changing it."""
import json, math, sys
from pathlib import Path
import bpy
from mathutils import Vector

src, tex, output = sys.argv[sys.argv.index('--')+1:][:3]
out=Path(output);out.mkdir(parents=True,exist_ok=True)
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=src)
arm=next(o for o in bpy.data.objects if o.type=='ARMATURE')
meshes=[o for o in bpy.data.objects if o.type=='MESH']
arm.animation_data_clear();arm.data.pose_position='REST'
points=[o.matrix_world@v.co for o in meshes for v in o.data.vertices]
lo=Vector(tuple(min(p[i] for p in points) for i in range(3)))
hi=Vector(tuple(max(p[i] for p in points) for i in range(3)))
height=hi.z-lo.z;mid=(lo+hi)/2
bones={b.name.split(':')[-1]:b for b in arm.data.bones}
hips=(arm.matrix_world@bones['Hips'].head_local).z*100
report={'source':src,'height_cm':height*100,'hips_cm':hips,'bones':len(bones),
        'bone_names':list(bones),'bounds_m':[list(lo),list(hi)]}
(out/'rig_measure.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print('RABBITINSPECT '+json.dumps(report))
image=bpy.data.images.load(tex)
for mesh in meshes:
    mat=bpy.data.materials.new(mesh.name+'_View');mat.use_nodes=True
    bsdf=mat.node_tree.nodes.get('Principled BSDF')
    texture=mat.node_tree.nodes.new('ShaderNodeTexImage');texture.image=image
    mat.node_tree.links.new(texture.outputs['Color'],bsdf.inputs['Base Color'])
    bsdf.inputs['Roughness'].default_value=1
    mesh.data.materials.clear();mesh.data.materials.append(mat)
    for poly in mesh.data.polygons:poly.material_index=0
scene=bpy.context.scene;scene.render.engine='BLENDER_EEVEE'
scene.render.resolution_x=scene.render.resolution_y=800
scene.render.resolution_percentage=100
scene.world=bpy.data.worlds.new('World');scene.world.use_nodes=True
scene.world.node_tree.nodes['Background'].inputs[0].default_value=(.68,.68,.68,1)
scene.world.node_tree.nodes['Background'].inputs[1].default_value=.65
scene.view_settings.view_transform='Standard'
scene.render.image_settings.file_format='PNG'
cam=bpy.data.objects.new('Camera',bpy.data.cameras.new('Camera'));scene.collection.objects.link(cam)
cam.data.type='ORTHO';cam.data.ortho_scale=height*1.12;scene.camera=cam
sun=bpy.data.objects.new('Key',bpy.data.lights.new('Key','SUN'));scene.collection.objects.link(sun);sun.data.energy=2
for name,yaw in [('front',0),('back',180),('side',90)]:
    r=math.radians(yaw)
    cam.location=Vector((mid.x+math.sin(r)*height*5,mid.y-math.cos(r)*height*5,mid.z))
    cam.rotation_euler=(mid-cam.location).to_track_quat('-Z','Y').to_euler()
    sun.rotation_euler=(math.radians(35),0,r+math.radians(-20))
    scene.render.filepath=str(out/(name+'.png'));bpy.ops.render.render(write_still=True)
    if name=='back':
        center=Vector((mid.x,mid.y,lo.z+height*.38))
        cam.location=Vector((center.x,center.y+height*5,center.z))
        cam.rotation_euler=(center-cam.location).to_track_quat('-Z','Y').to_euler()
        cam.data.ortho_scale=height*.45
        scene.render.filepath=str(out/'shirt_back.png');bpy.ops.render.render(write_still=True)
        cam.data.ortho_scale=height*1.12
print('RABBITINSPECT renders complete')
