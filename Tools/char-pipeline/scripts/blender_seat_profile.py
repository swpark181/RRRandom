"""Height profile of each vehicle's top surface along its length, to find its seats (run inside Blender, -b).

Usage: blender -b --factory-startup --python blender_seat_profile.py -- <name>=<fbx>=<y1>,<y2>,... ...
  e.g. ... -- Car=C:/work/char-pipeline/output/vehicles/car2/SM_Car.fbx=0,22,-22

The FBX is one of blender_vehicles.py's (front on +X, pivot in the middle of the bottom). For each vehicle: its size
(cm), then for each Y (cm across; the seats are usually symmetric, so the sign hardly matters) the height of the first
surface hit straight down every 5 cm along X. A seat cushion is a low flat stretch just in front of a tall backrest
(round 71: the car's driver cushion -20 .. 20 at 52, backrest -40 .. -30 at 103). Those numbers go into the vehicle's
row in RRRandomVehicle.cpp (Handlings: SeatHeight / RearSeatHeight = cushion top, DriverSeatX / RearSeatX = where the
hips go, a little in front of the backrest), then check it in the game (capture_game_seq.ps1).
"""
import sys

import bpy
from mathutils import Vector

args = sys.argv[sys.argv.index("--") + 1:]
for spec in args:
    name, fbx, ys = spec.split("=")
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=fbx)
    obj = [o for o in bpy.data.objects if o.type == "MESH"][0]
    bpy.context.view_layer.update()
    pts = [obj.matrix_world @ v.co for v in obj.data.vertices]
    lo = Vector((min(p.x for p in pts), min(p.y for p in pts), min(p.z for p in pts)))
    hi = Vector((max(p.x for p in pts), max(p.y for p in pts), max(p.z for p in pts)))
    print(f"PROFILE {name} x {lo.x*100:.0f}..{hi.x*100:.0f} y {lo.y*100:.0f}..{hi.y*100:.0f} z {lo.z*100:.0f}..{hi.z*100:.0f}")
    inv = obj.matrix_world.inverted()
    for y in (float(v) for v in ys.split(",")):
        row = []
        x = lo.x
        while x <= hi.x + 1e-6:
            start = inv @ Vector((x, y / 100.0, hi.z + 1.0))
            direction = (inv.to_3x3() @ Vector((0, 0, -1))).normalized()
            hit, loc, normal, _ = obj.ray_cast(start, direction)
            z = (obj.matrix_world @ loc).z * 100 if hit else float("nan")
            row.append(f"{x*100:.0f}:{z:.0f}")
            x += 0.05
        print(f"PROFILE {name} y={y:.0f} " + " ".join(row))
