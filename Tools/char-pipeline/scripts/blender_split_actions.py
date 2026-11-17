# Round 101: a Tripo Animate export with several motions in one FBX (Mixamo bone names) -> one FBX per motion (armature and
# mesh, that one action only) plus a contact sheet of each (8 frames across, seen from the front-right and from the side).
# usage: blender -b --factory-startup --python blender_split_actions.py -- <in fbx> <out dir> "<key>=<name>;<key>=<name>..."
#   <key>: a piece of the action's name (its prompt's first words), <name>: the file name to write (<name>.fbx, <name>.png)
import math
import os
import sys

import bpy
from mathutils import Vector

args = sys.argv[sys.argv.index("--") + 1:]
src, out_dir, pairs = args[0], args[1], [p.split("=") for p in args[2].split(";") if p]
os.makedirs(out_dir, exist_ok=True)

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=src, automatic_bone_orientation=False)
scene = bpy.context.scene
arm = next(o for o in bpy.data.objects if o.type == "ARMATURE")
meshes = [o for o in bpy.data.objects if o.type == "MESH"]
if arm.animation_data is None:
    arm.animation_data_create()

# The stage: a camera in front and to the right, another at the side; flat studio light, textures shown
scene.render.engine = "BLENDER_WORKBENCH"
scene.display.shading.light = "STUDIO"
scene.display.shading.color_type = "TEXTURE"
scene.render.resolution_x = 320
scene.render.resolution_y = 340
scene.render.film_transparent = False
scene.world = bpy.data.worlds.new("w")
scene.world.color = (0.82, 0.84, 0.88)
cams = {}
for name, yaw in (("front", 25.0), ("side", 90.0)):
    cam = bpy.data.cameras.new(name)
    cam.type = "ORTHO"
    cam.ortho_scale = 1.15
    obj = bpy.data.objects.new(name, cam)
    scene.collection.objects.link(obj)
    way = Vector((math.sin(math.radians(yaw)), -math.cos(math.radians(yaw)), 0.12)).normalized()
    obj.location = Vector((0.0, 0.0, 0.3)) + way * 6.0
    obj.rotation_euler = (Vector((0.0, 0.0, 0.3)) - obj.location).to_track_quat("-Z", "Y").to_euler()
    cams[name] = obj

for key, name in pairs:
    action = next((a for a in bpy.data.actions if key.lower() in a.name.lower()), None)
    if action is None:
        print("SPLIT missing", key)
        continue
    arm.animation_data.action = action
    first, last = int(round(action.frame_range[0])), int(round(action.frame_range[1]))
    scene.frame_start, scene.frame_end = first, last
    # The sheet
    shots = []
    for cam_name, cam in cams.items():
        scene.camera = cam
        for i in range(8):
            frame = first + round((last - first) * i / 7)
            scene.frame_set(frame)
            path = os.path.join(out_dir, f"_{name}_{cam_name}_{i}.png")
            scene.render.filepath = path
            bpy.ops.render.render(write_still=True)
            shots.append(path)
    print("SPLIT sheet", name, " ".join(shots))
    # The one-motion FBX (the other actions unlinked for the export)
    others = [a for a in bpy.data.actions if a is not action]
    for a in others:
        a.use_fake_user = False
    bpy.ops.object.select_all(action="DESELECT")
    arm.select_set(True)
    for m in meshes:
        m.select_set(True)
    bpy.context.view_layer.objects.active = arm
    bpy.ops.export_scene.fbx(filepath=os.path.join(out_dir, name + ".fbx"), use_selection=True, add_leaf_bones=False,
                             bake_anim=True, bake_anim_use_all_bones=True, bake_anim_use_nla_strips=False,
                             bake_anim_use_all_actions=False, bake_anim_force_startend_keying=True, bake_anim_step=1.0,
                             bake_anim_simplify_factor=0.0, path_mode="COPY", embed_textures=False)
    print("SPLIT wrote", name, first, last)
