"""Measures Mixamo animations of the fox (run inside Blender): blender -b --factory-startup --python blender_anim_speed.py -- <fbx> ...

Per file: length, and the ground speed the feet show (the planted foot slides back at the speed the body would move):
the median velocity of whichever foot is lowest, over the frames where it is near the ground. Also the hips' own drift
(zero for an "In Place" download). Units are the FBX's centimeters on the Mixamo rig (the fox mesh at scale 1).
Also prints the direction from the right hand to the left hand at the middle frame (where a two-handed gun points),
as (side, forward, up) with forward = the way the character faces.
"""
import statistics
import sys

import bpy

for path in sys.argv[sys.argv.index("--") + 1:]:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=path)
    arm = next(o for o in bpy.data.objects if o.type == "ARMATURE")
    act = arm.animation_data.action
    f0, f1 = int(act.frame_range[0]), int(act.frame_range[1])
    fps = bpy.context.scene.render.fps
    sc = bpy.context.scene

    def bone(name):
        return next(b for b in arm.pose.bones if b.name.endswith(name))

    feet = [bone("LeftToeBase"), bone("RightToeBase")]
    hips = bone("Hips")
    lhand, rhand = bone("LeftHand"), bone("RightHand")
    # Blender meters on an armature scaled 0.01: world units * 100 = cm
    k = 100.0
    pts, hp = [], []
    for f in range(f0, f1 + 1):
        sc.frame_set(f)
        pts.append([(arm.matrix_world @ b.head).copy() for b in feet])
        hp.append((arm.matrix_world @ hips.head).copy())
    low = min(min(p[0].z, p[1].z) for p in pts)
    vx, vy = [], []
    for i in range(len(pts) - 1):
        j = 0 if pts[i][0].z < pts[i][1].z else 1
        if pts[i][j].z - low > 0.012:
            continue
        d = (pts[i + 1][j] - pts[i][j]) * fps * k
        vx.append(d.x)
        vy.append(d.y)
    mid = (f0 + f1) // 2
    sc.frame_set(mid)
    g = (arm.matrix_world @ lhand.head) - (arm.matrix_world @ rhand.head)
    g.normalize()
    # Mixamo characters face Blender's -Y after import
    print(f"ANIM {path.split('/')[-1]}: {(f1 - f0) / fps:.2f} s, feet ground speed "
          f"({-statistics.median(vx) if vx else 0:.0f}, {-statistics.median(vy) if vy else 0:.0f}) cm/s from {len(vx)} frames, "
          f"hips drift ({(hp[-1] - hp[0]).x * k:.0f}, {(hp[-1] - hp[0]).y * k:.0f}), "
          f"hands dir side {g.x:.2f} fwd {-g.y:.2f} up {g.z:.2f}")
