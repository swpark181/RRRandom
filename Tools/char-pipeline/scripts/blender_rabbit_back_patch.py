"""Project an ImageGen-retouched rear cloth view into the existing UV atlas.

Only the visible rear torso's purple pixels change. Use the edited/original
render ratio to remove view lighting before baking. Preserve the user's Mixamo
bones and weights, front print, fur, cuffs, collar, belt and shorts.
"""
import json, sys
from pathlib import Path
import bpy
import numpy as np
from mathutils import Vector

root=Path(__file__).resolve().parents[1]
src=root/'input/rabbit_smart_tpose'
out=root/'output/rabbit_tpose_game'
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=str(src/'mixamo/rabbit_mixamo_original.fbx'))
arm=next(o for o in bpy.data.objects if o.type=='ARMATURE')
arm.animation_data_clear();arm.data.pose_position='REST'
meshes=[o for o in bpy.data.objects if o.type=='MESH']
pts=[m.matrix_world@v.co for m in meshes for v in m.data.vertices]
lo=np.min(np.array(pts),axis=0);hi=np.max(np.array(pts),axis=0)
height=hi[2]-lo[2];mid=(lo+hi)/2

def load(path):
    im=bpy.data.images.load(str(path));w,h=im.size
    a=np.empty(w*h*4,dtype=np.float32);im.pixels.foreach_get(a)
    return im,a.reshape(h,w,4)

image,base=load(src/'rabbit_tpose_basecolor.png')
_,before=load(out/'before/shirt_back.png')
_,after=load(src/'shirt_back_edit.png')
result=base.copy();h,w=base.shape[:2];mask=np.zeros((h,w),np.float32)

def sample(a,uv):
    x=np.clip(uv[:,0],0,1)*(a.shape[1]-1);y=np.clip(uv[:,1],0,1)*(a.shape[0]-1)
    ix=x.astype(int);iy=y.astype(int);jx=np.minimum(ix+1,a.shape[1]-1);jy=np.minimum(iy+1,a.shape[0]-1)
    fx=(x-ix)[:,None];fy=(y-iy)[:,None]
    return (a[iy,ix,:3]*(1-fx)+a[iy,jx,:3]*fx)*(1-fy)+(a[jy,ix,:3]*(1-fx)+a[jy,jx,:3]*fx)*fy

def smooth(a,b,x):
    t=np.clip((x-a)/(b-a),0,1);return t*t*(3-2*t)

def purple(a):return (a[:,0]>a[:,1]*1.12)&(a[:,2]>a[:,1]*1.08)&(a[:,0]>.08)

for mesh in meshes:
    me=mesh.data;me.calc_loop_triangles();uv=me.uv_layers.active.data
    co=np.array([mesh.matrix_world@v.co for v in me.vertices])
    nt=mesh.matrix_world.to_3x3().inverted().transposed()
    for tri in me.loop_triangles:
        normal=(nt@tri.normal).normalized()
        if normal.y<.12:continue
        world=co[list(tri.vertices)]
        if world[:,2].max()<lo[2]+height*.30 or world[:,2].min()>lo[2]+height*.48:continue
        uvs=np.array([uv[i].uv[:] for i in tri.loops])*[w,h]
        x0,y0=np.maximum(np.floor(uvs.min(0)).astype(int)-1,0)
        x1,y1=np.minimum(np.ceil(uvs.max(0)).astype(int)+1,[w,h])
        if x1<=x0 or y1<=y0:continue
        ys,xs=np.mgrid[y0:y1,x0:x1];px=xs.ravel()+.5;py=ys.ravel()+.5
        (ax,ay),(bx,by),(cx,cy)=uvs
        den=(by-cy)*(ax-cx)+(cx-bx)*(ay-cy)
        if abs(den)<1e-9:continue
        a=((by-cy)*(px-cx)+(cx-bx)*(py-cy))/den
        b=((cy-ay)*(px-cx)+(ax-cx)*(py-cy))/den;c=1-a-b
        valid=(a>=0)&(b>=0)&(c>=0)
        if not valid.any():continue
        xx=xs.ravel()[valid];yy=ys.ravel()[valid]
        pos=np.stack([a[valid],b[valid],c[valid]],1)@world
        z=(pos[:,2]-lo[2])/height;x=np.abs(pos[:,0]-mid[0])/height
        blend=smooth(.30,.32,z)*(1-smooth(.455,.478,z))*(1-smooth(.145,.18,x))
        blend*=smooth(.12,.48,np.full(len(pos),normal.y))
        view=np.stack([.5-(pos[:,0]-mid[0])/(height*.45),.5+(pos[:,2]-(lo[2]+height*.38))/(height*.45)],1)
        old=sample(before,view);new=sample(after,view);raw=base[yy,xx,:3]
        blend*=purple(raw)&purple(old)&purple(new)
        ratio=np.clip(new/np.maximum(old,.025),.60,1.65)
        color=np.clip(raw*ratio,0,1)
        result[yy,xx,:3]=raw*(1-blend[:,None])+color*blend[:,None]
        mask[yy,xx]=np.maximum(mask[yy,xx],blend)

clean=bpy.data.images.new('RabbitClean',w,h,alpha=True)
clean.colorspace_settings.name=image.colorspace_settings.name
clean.pixels.foreach_set(result.ravel());clean.filepath_raw=str(src/'rabbit_tpose_basecolor_clean.png')
clean.file_format='PNG';clean.save()
mask_image=bpy.data.images.new('BackPatchMask',w,h,alpha=True)
m=np.ones((h,w,4),np.float32);m[:,:,:3]=mask[:,:,None]
mask_image.colorspace_settings.name='Non-Color';mask_image.pixels.foreach_set(m.ravel())
mask_image.filepath_raw=str(out/'back_patch_mask.png');mask_image.file_format='PNG';mask_image.save()

hand_checks={}
for mesh in meshes:
    # Preserve the downloaded skin exactly; only the render material changes.
    mat=bpy.data.materials.new('RabbitClean');mat.use_nodes=True
    node=mat.node_tree.nodes.new('ShaderNodeTexImage');node.image=clean
    bsdf=mat.node_tree.nodes.get('Principled BSDF');mat.node_tree.links.new(node.outputs['Color'],bsdf.inputs['Base Color'])
    bsdf.inputs['Roughness'].default_value=1
    mesh.data.materials.clear();mesh.data.materials.append(mat)
    for poly in mesh.data.polygons:poly.material_index=0;poly.use_smooth=True
    groups={g.index:g.name.split(':')[-1] for g in mesh.vertex_groups}
    for side,sign in [('Left',1),('Right',-1)]:
        vs=[v for v in mesh.data.vertices if sign*(mesh.matrix_world@v.co).x>height*.35]
        torso=[sum(g.weight for g in v.groups if groups[g.group] in {'Hips','Spine','Spine1','Spine2','Neck','Head'}) for v in vs]
        hand_checks[side]={'vertices':len(vs),'max_torso_weight':max(torso,default=0),'mean_torso_weight':float(np.mean(torso)) if torso else 0}
    mesh.select_set(True)
arm.select_set(True);bpy.context.view_layer.objects.active=arm
bpy.ops.export_scene.fbx(filepath=str(src/'mixamo/rabbit_SK.fbx'),use_selection=True,
    object_types={'ARMATURE','MESH'},add_leaf_bones=False,bake_anim=False,
    primary_bone_axis='Y',secondary_bone_axis='X',use_armature_deform_only=False,
    mesh_smooth_type='FACE',path_mode='STRIP')
report={'changed_texels':int(np.count_nonzero(mask>0)),'max_atlas_delta':float(np.max(np.abs(result-base))),
        'outside_mask_max_delta':float(np.max(np.abs(result-base)[mask==0])),
        'hips_cm':(arm.matrix_world@next(b for b in arm.data.bones if b.name.endswith(':Hips')).head_local).z*100,
        'bones':len(arm.data.bones),'hands':hand_checks}
(out/'back_patch.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
assert report['changed_texels']>1000
assert report['outside_mask_max_delta']==0
print('RABBITPATCH '+json.dumps(report))
