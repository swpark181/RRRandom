# Cinematic scouting (2026-11-19, fox vs tiger blocking): runs inside a -game session (-ExecCmds="py <this>").
# Looks straight down on Dice Town from high up and takes HighResShots: the whole town, then a grid of closer tiles
# (RR_SCOUT_GRID, default 2 = 2x2), then any extra views in RR_SCOUT_VIEWS ("x,y,z,pitch,yaw,fov;..."), then quits.
# Logs the town bounds and every shot's camera as CINESCOUT lines, so a spot in a picture can be turned back into world X/Y.
import os
import time

import unreal

start = time.time()
state = {"stage": 0, "next": 0.0, "shots": [], "cam": None}


def world():
    for w in unreal.ObjectIterator(unreal.World):
        try:
            if unreal.GameplayStatics.get_player_controller(w, 0):
                return w
        except Exception:
            pass
    return None


def plan(w):
    towns = unreal.GameplayStatics.get_all_actors_of_class(w, unreal.RRRandomDiceTown)
    if not towns:
        return False
    origin, extent = towns[0].get_actor_bounds(False)
    unreal.log(f"CINESCOUT town origin {origin.x:.0f},{origin.y:.0f},{origin.z:.0f} extent {extent.x:.0f},{extent.y:.0f},{extent.z:.0f}")
    half = float(os.environ.get("RR_SCOUT_HALF", "0")) or max(extent.x, extent.y)
    fov = 30.0
    import math
    tan = math.tan(math.radians(fov / 2.0))
    # Whole town: the picture is 16:9, so fit the town's size to the picture's height (X up the picture)
    shots = [("all", origin.x, origin.y, half / tan + 3000.0, -90.0, 0.0, fov * 16.0 / 9.0)]
    n = int(os.environ.get("RR_SCOUT_GRID", "2"))  # 0: no whole-town shot, no grid, only RR_SCOUT_VIEWS
    cell = 2.0 * half / max(n, 1)
    for i in range(n):
        for j in range(n):
            cx = origin.x + half - cell * (i + 0.5)
            cy = origin.y - half + cell * (j + 0.5)
            shots.append((f"tile_{i}{j}", cx, cy, (cell * 0.5) / tan + 2500.0, -90.0, 0.0, fov * 16.0 / 9.0))
    for k, v in enumerate(p for p in os.environ.get("RR_SCOUT_VIEWS", "").split(";") if p.strip()):
        x, y, z, pitch, yaw, f = (float(a) for a in v.split(","))
        shots.append((f"view_{k}", x, y, z, pitch, yaw, f))
    state["shots"] = shots
    return True


def tick(dt):
    t = time.time() - start
    w = world()
    if not w or t < state["next"]:
        return
    pc = unreal.GameplayStatics.get_player_controller(w, 0)
    if not pc:
        return
    if state["stage"] == 0:
        if t < 10.0 or not plan(w):
            return
        unreal.SystemLibrary.execute_console_command(w, "RRStudioCam 0", pc)
        unreal.SystemLibrary.execute_console_command(w, "RRStudioCamOff", pc)
        for c in (x for x in os.environ.get("RR_SCOUT_CMDS", "").split(";") if x.strip()):
            unreal.SystemLibrary.execute_console_command(w, c, pc)
        if os.environ.get("RR_SCOUT_GRID") == "0":
            state["shots"] = state["shots"][1:]
        cams = unreal.GameplayStatics.get_all_actors_of_class(w, unreal.CameraActor)
        state["cam"] = cams[0] if cams else None
        state["stage"] = 1
        state["i"] = 0
        return
    cam = state["cam"]
    if state["stage"] == 1:
        if state["i"] >= len(state["shots"]):
            unreal.log("CINESCOUT done")
            unreal.SystemLibrary.execute_console_command(w, "quit", pc)
            state["stage"] = 3
            return
        name, x, y, z, pitch, yaw, fov = state["shots"][state["i"]]
        cam.set_actor_location(unreal.Vector(x, y, z), False, False)
        cam.set_actor_rotation(unreal.Rotator(0.0, pitch, yaw), False)
        cam.camera_component.set_field_of_view(fov)
        cam.camera_component.set_editor_property("constrain_aspect_ratio", False)
        pc.set_view_target_with_blend(cam, 0.0)
        unreal.log(f"CINESCOUT shot {state['i']} {name} cam {x:.0f},{y:.0f},{z:.0f} pitch {pitch} yaw {yaw} hfov {fov:.1f}")
        state["stage"] = 2
        state["next"] = t + 3.0  # let streaming and far LOD settle
        return
    if state["stage"] == 2:
        unreal.SystemLibrary.execute_console_command(w, "HighResShot 2560x1440", pc)
        state["i"] += 1
        state["stage"] = 1
        state["next"] = t + 2.0


unreal.log("CINESCOUT armed")
unreal.register_slate_post_tick_callback(tick)
