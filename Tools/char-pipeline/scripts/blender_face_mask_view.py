"""The cinematic body with its face mask, textured (body: base colour, mask: a face picture, its vertex colour R as
opacity), unlit, from the front and from 20 degrees above; and the mask alone in red over the grey body."""
import json
import math
import sys

import bpy
from mathutils import Vector

rig, base, face, cam_json, out = sys.argv[sys.argv.index("--") + 1:][:5]
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=rig)
cam = json.load(open(cam_json))
body = next(o for o in bpy.data.objects if o.type == "MESH")


def flat(name, img_path, alpha_from_vc):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    nt = m.node_tree
    nt.nodes.clear()
    t = nt.nodes.new("ShaderNodeTexImage")
    t.image = bpy.data.images.load(img_path)
    e = nt.nodes.new("ShaderNodeEmission")
    o = nt.nodes.new("ShaderNodeOutputMaterial")
    nt.links.new(t.outputs["Color"], e.inputs["Color"])
    if alpha_from_vc:
        vc = nt.nodes.new("ShaderNodeVertexColor")
        sep = nt.nodes.new("ShaderNodeSeparateColor")
        tr = nt.nodes.new("ShaderNodeBsdfTransparent")
        mix = nt.nodes.new("ShaderNodeMixShader")
        nt.links.new(vc.outputs["Color"], sep.inputs["Color"])
        nt.links.new(sep.outputs["Red"], mix.inputs["Fac"])
        nt.links.new(tr.outputs[0], mix.inputs[1])
        nt.links.new(e.outputs[0], mix.inputs[2])
        nt.links.new(mix.outputs[0], o.inputs["Surface"])
        if hasattr(m, "surface_render_method"):
            m.surface_render_method = "DITHERED"
        else:
            m.blend_method = "HASHED"
    else:
        nt.links.new(e.outputs[0], o.inputs["Surface"])
    return m


mats = body.data.materials
for i, m in enumerate(mats):
    mats[i] = flat("Face", face, True) if m.name.startswith("Face") else flat("Body", base, False)
scene = bpy.context.scene
scene.render.engine = "BLENDER_EEVEE_NEXT" if "BLENDER_EEVEE_NEXT" in [e.identifier for e in bpy.types.RenderSettings.bl_rna.properties["engine"].enum_items] else "BLENDER_EEVEE"
scene.view_settings.view_transform = "Standard"
scene.render.resolution_x = scene.render.resolution_y = 640
w = bpy.data.worlds.new("W")
w.color = (0.2, 0.2, 0.2)
scene.world = w
cd = bpy.data.cameras.new("C")
cd.type = "ORTHO"
cd.ortho_scale = cam["ortho"] * 0.6
c = bpy.data.objects.new("C", cd)
scene.collection.objects.link(c)
scene.camera = c
fwd = Vector(cam["forward"])
head = Vector(cam["center"]) + Vector((0, 0, -cam["size"] * 0.15))
for name, tilt, turn in (("front", 0, 0), ("above", 22, 0), ("side", 10, 35)):
    r, t = math.radians(turn), math.radians(tilt)
    d = Vector((fwd.x * math.cos(r) - fwd.y * math.sin(r), fwd.x * math.sin(r) + fwd.y * math.cos(r), 0)) * math.cos(t)
    d.z = math.sin(t)
    c.location = head + d * cam["size"] * 10
    c.rotation_euler = (-d).to_track_quat("-Z", "Y").to_euler()
    scene.render.filepath = f"{out}_{name}.png"
    bpy.ops.render.render(write_still=True)
print("MASKVIEW done")
