"""A flat picture of part of a Tripo model from a chosen direction, with its camera for blender_expression.py's bake
(2026-11-19: a mouth painted from straight ahead smears along the underside of a muzzle; painted from front-below,
square onto that surface, it wraps the muzzle properly).

  blender -b --factory-startup --python blender_view_render.py -- <fbx> <basecolor> <out.png> <out.json>
        [--pitch 38] [--yaw 0] [--cz 0.555] [--fwd 0.04] [--span 0.34] [--px 1024]
  --pitch degrees the camera sits BELOW the horizon looking up (0 = straight ahead), --yaw degrees to the model's left,
  --cz the target's height from the feet, --fwd how far behind the nose tip the target is, --span the picture's side
  (cz, fwd, span in body heights). The model stands on +Z and faces -Y.
"""
import json
import math
import sys

import bpy
from mathutils import Vector

argv = sys.argv[sys.argv.index("--") + 1:]
opt = {"pitch": 38.0, "yaw": 0.0, "cz": 0.555, "fwd": 0.04, "span": 0.34, "px": 1024}
rest = []
i = 0
while i < len(argv):
    if argv[i].startswith("--"):
        opt[argv[i][2:]] = float(argv[i + 1])
        i += 2
    else:
        rest.append(argv[i])
        i += 1
fbx, tex_path, out_png, out_json = rest[:4]
px = int(opt["px"])

bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
bpy.ops.import_scene.fbx(filepath=fbx)
mesh = max((o for o in bpy.data.objects if o.type == "MESH"), key=lambda o: len(o.data.polygons))
mw = mesh.matrix_world
verts = [mw @ v.co for v in mesh.data.vertices]
lo = Vector([min(v[k] for v in verts) for k in range(3)])
hi = Vector([max(v[k] for v in verts) for k in range(3)])
H = hi.z - lo.z
head = [v for v in verts if v.z > lo.z + H * 0.55]
cx = (min(v.x for v in head) + max(v.x for v in head)) / 2
snout = [v for v in verts if abs(v.x - cx) < H * 0.04 and lo.z + H * 0.48 < v.z < lo.z + H * 0.64]
tip = min(snout, key=lambda v: v.y)
target = Vector((cx, tip.y + H * opt["fwd"], lo.z + H * opt["cz"]))
p, yw = math.radians(opt["pitch"]), math.radians(opt["yaw"])
fwd = Vector((math.sin(yw) * math.cos(p), -math.cos(yw) * math.cos(p), -math.sin(p)))      # from the target to the camera
up = Vector((-math.sin(yw) * math.sin(p), math.cos(yw) * math.sin(p) * -1.0, math.cos(p)))
up = (up - fwd * up.dot(fwd)).normalized()

mat = bpy.data.materials.new("Flat")
mat.use_nodes = True
nt = mat.node_tree
nt.nodes.clear()
t = nt.nodes.new("ShaderNodeTexImage")
t.image = bpy.data.images.load(tex_path)
e = nt.nodes.new("ShaderNodeEmission")
o = nt.nodes.new("ShaderNodeOutputMaterial")
nt.links.new(t.outputs["Color"], e.inputs["Color"])
nt.links.new(e.outputs["Emission"], o.inputs["Surface"])
mesh.data.materials.clear()
mesh.data.materials.append(mat)

cam = bpy.data.objects.new("Cam", bpy.data.cameras.new("Cam"))
scene.collection.objects.link(cam)
scene.camera = cam
cam.data.type = "ORTHO"
cam.data.ortho_scale = H * opt["span"]
cam.location = target + fwd * H * 4
# -Z looks along -fwd, Y is the picture's up
right = up.cross(fwd).normalized()
from mathutils import Matrix
rot = Matrix((right, up, fwd)).transposed()
cam.rotation_euler = rot.to_euler()
engines = {x.identifier for x in bpy.types.RenderSettings.bl_rna.properties["engine"].enum_items}
scene.render.engine = "BLENDER_EEVEE_NEXT" if "BLENDER_EEVEE_NEXT" in engines else "BLENDER_EEVEE"
scene.render.resolution_x = scene.render.resolution_y = px
scene.view_settings.view_transform = "Standard"
world = bpy.data.worlds.new("W")
world.use_nodes = True
world.node_tree.nodes["Background"].inputs[0].default_value = (0.78, 0.79, 0.81, 1.0)
scene.world = world
scene.render.filepath = out_png
bpy.ops.render.render(write_still=True)
json.dump({"center": list(target), "forward": list(fwd), "up": list(up), "size": H * 0.3, "ortho": H * opt["span"],
           "cam": list(cam.location), "res": px}, open(out_json, "w"), indent=1)
print(f"VIEW {out_png}: target {tuple(round(c, 3) for c in target)}, pitch {opt['pitch']}, yaw {opt['yaw']}, span {H * opt['span']:.3f}")
